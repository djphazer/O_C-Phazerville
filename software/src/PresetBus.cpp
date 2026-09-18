// 200e preset-bus transport (LPI2C1 general-call slave). See PresetBus.h.
#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

#include <Arduino.h>
#include <Wire.h>
#include <imxrt.h>

#include <LittleFS.h>

#include "Buchla200eWriteGuard.h"
#include "Bus200eMaster.h"
#include "MidiTxRing.h"
#include "PresetBus.h"
#include "PresetBus200e.h"
#include "PresetBusCard.h"
#include "CardSectors.h"
#include "PresetEngine.h"
#include "OC_gpio.h"
#include "OC_core.h"
#include "PhzConfig.h"
#include "PresetOpQueue.h"

extern volatile uint32_t loop_counter;  // Main.cpp (global scope)

namespace OC {
namespace PresetBus {

// module-address persistence key (PRESETBUS_KEY namespace, GLOBALS.CFG)
static constexpr uint16_t kAddrKey = (8 << 8) | 0x10;

// ---- SPSC event ring: ISR producer, Task() consumer ------------------------
// 256 events (uint8 indices wrap exactly): a save/recall blocks Task()
// for 100s of ms of file I/O while the ISR keeps filling this - 64 was
// only ~6 frames of headroom against a chatty preset manager.
static constexpr uint16_t kRingSize = 256;  // power of two, max for uint8 idx
static volatile uint16_t ring[kRingSize];
static volatile uint8_t ring_w = 0;
static uint8_t ring_r = 0;
static volatile bool ring_ovf = false;

static Stats stats;
static bool enabled = false;
static bool verbose = false;
static uint32_t last_rx_ms = 0;

static void push_event(uint16_t ev) {
  const uint16_t depth = uint16_t(uint8_t(ring_w - ring_r));
  if (depth >= kRingSize) {
    ring_ovf = true;  // drop; Task() poisons the frame
    return;
  }
  if (depth + 1u > stats.ring_hw) stats.ring_hw = depth + 1u;
  ring[ring_w & (kRingSize - 1)] = ev;
  ring_w = ring_w + 1;
}

// ---- LPI2C1 slave ISR ------------------------------------------------------
// Card-serving routing state (see "0x50 card serving" below). While serving,
// SAMR additionally matches 0x50 and transactions addressed there are routed
// into the BusCard emulator instead of the general-call event ring. With
// card_serving false the ISR behaves byte-identically to the GC-only build:
// SASR is never read and no BusCard entry point is reachable.
static volatile bool card_serving = false;
// 7-bit address currently served (SAMR ADDR1), meaningful only while
// card_serving: 0x50 for ordinary/manual card serving, or 0x51 when
// MasterBackup()/MasterRestore() claimed the alternate candidate because a
// live WPM held 0x50 (see card_serve_enable_at() below). Read from the ISR,
// so volatile; a plain byte read/write is atomic on this core.
static volatile uint8_t card_addr7 = BUS200E_CARD_BASE;
static volatile bool in_card_txn = false;   // current txn targets card_addr7
static volatile bool card_tx_open = false;  // read leg started (stats only)
static volatile uint16_t staged_addr = 0xFFFF;  // latest SASR RADDR (addr<<1|R)

static void lpi2c1_slave_isr() {
  stats.isr_count++;
  uint32_t status = LPI2C1_SSR;
  const uint32_t w1c = status & 0xF00;
  if (w1c) LPI2C1_SSR = w1c;

  // Address phase: stage the received address (reading SASR clears AVF).
  // AVF needs no interrupt of its own: a write txn raises RDF and a read
  // txn raises TDF before any routing decision is consumed below.
  if (card_serving && (status & LPI2C_SSR_AVF)) {
    const uint32_t sasr = LPI2C1_SASR;
    if (!(sasr & LPI2C_SASR_ANV)) staged_addr = sasr & 0x7FF;
  }

  if (status & LPI2C_SSR_RDF) {
    const uint32_t rx = LPI2C1_SRDR;
    if (rx & LPI2C_SRDR_SOF) {  // first byte of a write txn: route it now
      // A STOP/repeated-start pending in this same pass belongs to the
      // PREVIOUS transaction (wire order: ...data STOP START addr SOF-byte).
      // Close the old one here and consume the flags, or the block below
      // would tear down the freshly opened transaction.
      if (status & (LPI2C_SSR_SDF | LPI2C_SSR_RSF)) {
        if (in_card_txn) {
          if (card_tx_open) BusCardTxRewind();  // unsent TDF prefetch
          BusCardStop();
          in_card_txn = false;
          card_tx_open = false;
        } else {
          push_event(BUS200E_EV_STOP);
        }
        stats.stops++;
        status &= ~(uint32_t)(LPI2C_SSR_SDF | LPI2C_SSR_RSF);
      } else if (in_card_txn) {  // lost STOP entirely: close the stale txn
        BusCardStop();
        in_card_txn = false;
        card_tx_open = false;
      }
      if (card_serving && (staged_addr >> 1) == card_addr7) {
        in_card_txn = true;
        BusCardStart(0);        // SOF byte implies a master write
      } else {
        push_event(BUS200E_EV_START);
        stats.starts++;
      }
    }
    if (in_card_txn) {
      BusCardRxByte(rx & 0xFF);
    } else {
      push_event(rx & 0xFF);
      stats.bytes++;
    }
  }
  if (status & (LPI2C_SSR_SDF | LPI2C_SSR_RSF)) {  // stop / repeated start
    if (in_card_txn) {
      // a read leg always has one unsent byte prefetched into STDR by the
      // TDF feeder; rewind so current-address chunked reads stay aligned
      // with a real 24xx address counter
      if (card_tx_open) BusCardTxRewind();
      BusCardStop();
      // on RSF the next leg re-routes via SOF (write) or TDF (read)
      if (status & LPI2C_SSR_SDF) in_card_txn = false;
    } else {
      push_event(BUS200E_EV_STOP);
      if (status & LPI2C_SSR_RSF) push_event(BUS200E_EV_START);
    }
    card_tx_open = false;
    stats.stops++;
  }
  // Slave transmit: normally only card_addr7 reads request TX data, but a
  // malformed/glitched read at another matched address would leave TDF
  // pending with TXDSTALL stretching SCL forever and TDIE storming this
  // ISR - so an unroutable TDF is fed 0xFF (floating-bus semantics).
  if (status & LPI2C_SSR_TDF) {
    if (card_serving && (staged_addr >> 1) == card_addr7
        && (staged_addr & 1)) {
      if (!card_tx_open) {   // reads have no SOF: open the txn here
        card_tx_open = true;
        in_card_txn = true;
        BusCardStart(1);
      }
      LPI2C1_STDR = BusCardTxByte();
    } else if (card_serving) {
      LPI2C1_STDR = 0xFF;    // release the stretch, advance no state
    }
  }
}

// ---- bus MIDI rings ---------------------------------------------------------
// RX: producer = Task() (loop, via parser callback), consumer = the active
// app's MIDI poll (Quadrants: loop; Captain: app ISR). One producer, one
// consumer -- plain SPSC.
// TX: producers = app ISR (engine sends) AND loop (thru), so pushes are
// briefly IRQ-masked. Consumer = Task() (loop).
// RX comes off the same slow wire it is parsed from and never needs depth.
// TX is the deep, coalescing one (MidiTxRing.h) - host-tested, because the
// wraparound arithmetic is exactly where silent queue bugs live.
static constexpr uint8_t kMidiRing = 32;
static volatile uint32_t midi_rx_q[kMidiRing];
static volatile uint8_t midi_rx_w = 0;
static uint8_t midi_rx_r = 0;
static constexpr uint8_t kMidiRingRx = kMidiRing;
static constexpr uint8_t kMidiRingTx = MidiTxRing::kSize;
static MidiTxRing midi_tx;
static uint8_t midi_tx_fails = 0;

// ---- parser callbacks into the preset engine -------------------------------
static void cb_save(uint8_t slot) { PresetEngine::RequestSave(slot); }
static void cb_recall(uint8_t slot) { PresetEngine::RequestRecall(slot); }

static void cb_midi(uint8_t status, uint8_t d1, uint8_t d2) {
  if (uint8_t(midi_rx_w - midi_rx_r) >= kMidiRing) {
    stats.midi_rx_ovf++;
    return;
  }
  midi_rx_q[midi_rx_w & (kMidiRing - 1)] =
      uint32_t(status) | (uint32_t(d1) << 8) | (uint32_t(d2) << 16);
  midi_rx_w = midi_rx_w + 1;
  stats.midi_rx++;
  const uint8_t d = uint8_t(midi_rx_w - midi_rx_r);
  if (d > stats.midi_rx_hw) stats.midi_rx_hw = d;
}

// Another module answering a QUERY. The reply is a general-call broadcast
// like any other bus frame, so it reaches us through the ordinary slave RX
// path; all this does is hand it to the master-side QUERY FSM, which decides
// whether it is the answer to the question WE asked (see Bus200eMaster.h).
static void cb_query_reply(uint8_t from_addr, const uint8_t *ver, uint8_t n) {
  Bus200eMasterQueryReply(from_addr, ver, n);
  if (verbose) {
    Serial.printf("PresetBus: QUERY reply from %02X (%u bytes)\n", from_addr, n);
  }
}

// A module saying it is done with the card -- the 251e's [04 22 5C 0A 5C]
// after a BACKUP. Same route as a QUERY reply; the master FSM decides
// whether it is talking about the job we have open.
static void cb_xfer_done(uint8_t from_addr) {
  Bus200eMasterXferDone(from_addr);
  if (verbose) Serial.printf("PresetBus: transfer done from %02X\n", from_addr);
}

// Which modules answered a preset load, and when. A module in polling mode
// masters [04 22 addr 03 xx] once per preset load; that is the only positive
// confirmation this bus offers that a broadcast RECALL was acted on. The
// table is small and last-wins by address: what a caller ever asks is "which
// addresses answered in the last N ms", never a history.
static constexpr int kLoadAckSlots = 12;
static struct { uint8_t addr; uint32_t ms; } load_ack[kLoadAckSlots];

static void cb_load_ack(uint8_t from_addr) {
  const uint32_t now = millis() ? millis() : 1;
  int free_slot = -1;
  for (int i = 0; i < kLoadAckSlots; ++i) {
    if (load_ack[i].ms && load_ack[i].addr == from_addr) {
      load_ack[i].ms = now;
      if (verbose) Serial.printf("PresetBus: %02X followed the recall\n", from_addr);
      return;
    }
    if (!load_ack[i].ms && free_slot < 0) free_slot = i;
  }
  if (free_slot < 0) {  // full: take the stalest entry
    free_slot = 0;
    for (int i = 1; i < kLoadAckSlots; ++i)
      if (load_ack[i].ms < load_ack[free_slot].ms) free_slot = i;
  }
  load_ack[free_slot].addr = from_addr;
  load_ack[free_slot].ms = now;
  if (verbose) Serial.printf("PresetBus: %02X followed the recall\n", from_addr);
}

bool LoadAckSeenSince(uint8_t addr, uint32_t ms_ago) {
  const uint32_t now = millis();
  for (int i = 0; i < kLoadAckSlots; ++i)
    if (load_ack[i].ms && load_ack[i].addr == addr && now - load_ack[i].ms <= ms_ago)
      return true;
  return false;
}

int LoadAckCountSince(uint32_t since_ms) {
  if (!since_ms) return 0;
  int n = 0;
  for (int i = 0; i < kLoadAckSlots; ++i)
    if (load_ack[i].ms && (int32_t)(load_ack[i].ms - since_ms) >= 0) ++n;
  return n;
}

static const Bus200eOps kOps = {
  cb_save, cb_recall,
  0, nullptr, nullptr, nullptr, nullptr,  // card transfers: phase 2
  cb_midi,
  cb_query_reply,
  cb_xfer_done,
  cb_load_ack,
};

static bool tx_gate_open();  // defined with Task() below

// Drain whatever the ISR has queued into `ring`, same loop Task() runs at
// the top of its own pass. Factored out so a frame-mastering helper below
// can force an immediate drain right after Bus200eSuppressFrame() -- the
// egress Wire.beginTransmission()/write()/endTransmission() call is itself
// on the same physical bus our slave listens on, so our own slave ISR
// queues that exact frame's bytes into `ring` as a "self-echo" *during* the
// blocking send (tx_gate_open() already requires the ring be empty before
// any TX starts, so nothing else can land in there in the meantime).
// Bus200eSuppressFrame()'s match window is only 50ms (parse_frame() expires
// it past that, on purpose, so a stale registration can never eat a later
// genuine identical frame). Task()'s own drain loop runs BEFORE pump_*(), so
// left alone the self-echo would sit undrained until the *next* Task()
// call -- for pump_broadcast() specifically, that next call can be
// arbitrarily far off: RequestSave()/RequestRecall() below is consumed by
// PresetEngine::Process() on the very next loop() iteration, which runs
// *before* Task() and blocks it for 100s of ms doing the actual LittleFS
// save/recall. By the time Task() finally got to drain the ring, the
// suppression had long expired and the self-echoed SAVE/RECALL frame was
// mistaken for a second, genuine bus command -- reproduced live: an
// un-suppressed self-echo re-armed pending_save and fired a second,
// unprompted SaveSlot() right after the first one had already finished.
// Draining right here, while the registration is still fresh, closes that.
static void drain_ring() {
  if (ring_ovf) {
    ring_ovf = false;
    stats.ring_ovf++;
    Bus200eFeedEvent(BUS200E_EV_OVF);
  }
  bool got = false;
  while (ring_r != ring_w) {
    if (ring_ovf) {
      ring_ovf = false;
      stats.ring_ovf++;
      Bus200eFeedEvent(BUS200E_EV_OVF);
    }
    const uint16_t ev = ring[ring_r & (kRingSize - 1)];
    ring_r = ring_r + 1;
    got = true;
    if (verbose) {
      if (ev & BUS200E_EV_START) Serial.print("\n[S] ");
      else if (ev & BUS200E_EV_STOP) Serial.print("[P]");
      else Serial.printf("%02X ", ev & 0xFF);
    }
    Bus200eFeedEvent(ev);
  }
  if (got) last_rx_ms = millis();
}

// ---- commander mode: bus-wide preset commands -------------------------------
// A short queue with the engine's own rule -- PresetOpQueue.h, the one
// header both use: a recall replaces a trailing recall (only the last one
// is ever heard), a save is never merged or replaced, because each one is
// a distinct "keep this" and losing any of them is data loss. This used to
// be one last-wins cell, and a NEXT pulse landing while a STORE was still
// waiting for the bus quietly replaced the save; the panel then announced
// "STORED" for an op that never went out. Ops are the wire command bytes.
// The queue is filled from loop context only.
static const uint8_t kCmdRecall = 0x01, kCmdSave = 0x02;
static PresetOpQueue<4, kCmdRecall> bcast_q;
static uint8_t bcast_tries = 0;
static uint32_t bcast_tx = 0, bcast_drop = 0;

FLASHMEM static void bcast_enqueue(uint8_t cmd, uint8_t slot) {
  if (slot >= 30) return;
  if (bcast_q.push(cmd, slot)) return;
  bcast_drop++;
  Serial.printf("PresetBus: broadcast queue full, %s %d refused\n",
                cmd == kCmdSave ? "SAVE" : "RECALL", slot);
}

FLASHMEM void BroadcastSave(uint8_t slot) { bcast_enqueue(kCmdSave, slot); }
FLASHMEM void BroadcastRecall(uint8_t slot) { bcast_enqueue(kCmdRecall, slot); }
bool BroadcastQueued() { return !bcast_q.empty(); }

FLASHMEM static void bcast_dequeue() {
  bcast_q.pop();
  bcast_tries = 0;
}

FLASHMEM static void pump_broadcast() {
  if (bcast_q.empty()) return;
  const auto cmd = bcast_q.front();
  if (!tx_gate_open()) return;

  // same long/PRIMO frame a preset manager sends. The slave stays ENABLED
  // during TX: if we lose arbitration the winner's frame (maybe the WPM's
  // one-shot recall) must still be heard; on success the parser drops our
  // own echo via Bus200eSuppressFrame.
  uint8_t f[5] = { 0x04, 0x00, 0x22, cmd.op, uint8_t(cmd.slot & 0x1F) };

  Wire.beginTransmission(0);
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) {
    bcast_dequeue();
    bcast_tx++;
    // the only thing our slave hears of our own TX is the self-echo, which
    // the drain below suppresses and drops: dispatch locally so this module
    // saves/recalls in lockstep with the rest of the bus. BEFORE the drain:
    // the wire order is the engine order. A manager frame that follows ours
    // on the wire could be sitting in the ring by the time the drain parses
    // it, and dispatching ours after it would run "recall 5, then save 3"
    // for a wire that said "save 3, then recall 5" -- preset 5's state
    // written into slot 3. Whether the ring can hold a whole frame that
    // fast is not something to depend on; the enqueue is instant, so the
    // suppression window is as fresh at the drain as it was.
    if (cmd.op == kCmdSave) PresetEngine::RequestSave(cmd.slot);
    else PresetEngine::RequestRecall(cmd.slot);
    Bus200eSuppressFrame(f, sizeof(f));
    drain_ring();  // consume our own self-echo while suppression is fresh
    if (verbose) Serial.printf("PresetBus: broadcast %s %d\n",
                               cmd.op == kCmdSave ? "SAVE" : "RECALL", cmd.slot);
  } else if (++bcast_tries >= 50) {  // persistent contention: give up loudly
    bcast_dequeue();
    bcast_drop++;
    Serial.printf("PresetBus: broadcast dropped (err %d)\n", err);
  }
}

// ---- WPM / preset-manager presence ------------------------------------------
// A preset manager is whoever ACKs slave address 0x50. Probe with an empty
// master write (the WPM's receiveEvent sees howMany==0: harmless) every few
// seconds when the bus is quiet. Hysteresis on the way out so one lost
// arbitration doesn't demote a live WPM.
static bool wpm_present = false;
static uint8_t wpm_misses = 0;
static uint32_t wpm_last_probe_ms = 0;
static uint32_t wpm_probes = 0;

bool WpmPresent() { return wpm_present; }

FLASHMEM static void probe_wpm() {
  // Only skip while WE ACK 0x50 ourselves (the probe would self-ACK). If a
  // MasterBackup/MasterRestore claimed the alternate 0x51 candidate instead
  // (because a live WPM already held 0x50), 0x50 tracking must keep running
  // -- that's the exact presence flag CardServeEnable()'s hard gate and
  // master_probe_card() below still key off of.
  if (card_serving && card_addr7 == BUS200E_CARD_BASE) return;
  if (millis() - wpm_last_probe_ms < 5000) return;
  if (!tx_gate_open()) return;
  wpm_last_probe_ms = millis();
  wpm_probes++;

  // addressed to 0x50: our general-call slave never matches it, so the
  // slave can stay enabled with no echo concern
  Wire.beginTransmission(0x50);
  const uint8_t err = Wire.endTransmission();  // 0 = ACK, 2 = NACK

  if (err == 0) {
    if (!wpm_present && verbose) Serial.println("PresetBus: WPM detected");
    wpm_present = true;
    wpm_misses = 0;
  } else if (err == 2) {
    if (wpm_present && ++wpm_misses >= 3) {
      wpm_present = false;
      wpm_misses = 0;
      if (verbose) Serial.println("PresetBus: WPM gone");
    }
  }  // arbitration loss etc: no evidence either way, try again later
}

// ---- 0x50 card serving -------------------------------------------------------
// Serve the storage-card address for WPM-less systems: 200e modules can then
// BACKUP/RESTORE against our 64K image (PBCARD.BIN on LittleFS) exactly as
// they would against a WPM or a real card.
//
// HARD GATE, in three layers: enabling is REFUSED while a real preset
// manager ACKs 0x50 (two card slaves on one bus corrupt each other's ACKs);
// the presence probe is suspended while serving (it would self-ACK and
// re-trip the gate); and an enable-time self-test (our polled master
// probing our own slave engine) reverts everything if the address-match
// hardware ACKs anything but exactly 0x50. Default off, never persisted:
// every boot starts NOT serving.
static uint8_t *card_image = nullptr;
static uint32_t card_flush_arm_ms = 0;
static uint32_t card_seen_writes = 0;
static uint32_t card_seen_reads = 0;
static uint32_t card_last_ms = 0;        // last byte moved through our card
static uint32_t card_last_us = 0;
static uint32_t card_gap_max_us = 0;     // longest inter-chunk gap this burst
// The write burst now dirtying the image arrived while one of OUR master
// jobs was open -- it is a bank we asked a module for, not a module backing
// itself up to us. See card_task() for why that decides whether it persists.
static bool card_burst_ours = false;
static constexpr const char *kCardFile = "PBCARD.BIN";
// The card_lo that produced card_addr7 (card_addr7 == BUS200E_CARD_BASE |
// card_addr_lo), so MasterBackup()/MasterRestore() can hand Bus200eMaster
// back whichever candidate is actually being served without recomputing it.
// Meaningful only while card_serving.
static uint8_t card_addr_lo = 0;

// SCFGR1/SAMR may only change while the slave engine is disabled. `addr7`
// is the 7-bit card address to claim as ADDR1 while serve is true (ignored
// otherwise); callers not claiming a specific address (Init()'s startup
// disable, bus_stuck_recover()'s rebuild) pass the module's currently-active
// card_addr7 or don't care.
FLASHMEM static void slave_reconfig(bool serve, uint8_t addr7 = BUS200E_CARD_BASE) {
  LPI2C1_SCR = 0;
  LPI2C1_SIER = 0;
  uint32_t cfg1 = LPI2C_SCFGR1_GCEN | LPI2C_SCFGR1_RXSTALL;
  uint32_t samr = LPI2C_SAMR_ADDR0(0);  // belt and braces alongside GCEN
  uint32_t sier = LPI2C_SIER_RDIE | LPI2C_SIER_SDIE | LPI2C_SIER_RSIE;
  if (serve) {
    cfg1 |= LPI2C_SCFGR1_ADDRCFG(2)   // match ADDR0 (7-bit) OR ADDR1 (7-bit)
          | LPI2C_SCFGR1_TXDSTALL;    // stretch SCL until STDR is fed
    samr |= LPI2C_SAMR_ADDR1(addr7 & 0x7F);
    sier |= LPI2C_SIER_TDIE;
  }
  LPI2C1_SCFGR1 = cfg1;
  LPI2C1_SAMR = samr;
  LPI2C1_SIER = sier;
  LPI2C1_SCR = LPI2C_SCR_SEN | LPI2C_SCR_FILTEN;
}

// What PBCARD.BIN holds, as of the last load or successful flush, tracked
// per 4 KB sector -- the granularity the write cost is actually paid in.
// This subsumes the whole-image CRC it replaced: "no sector changed" is
// the same answer as "the image is unchanged", for one pass over the 64 KB
// instead of two. Measured 2026-09-14: the two passes put a loop iteration
// at 5.25 ms, over the 5 ms budget, when a backup enabled card serving.
// See CardSectors.h.
static CardSectors card_sectors;

// Dirty means "a slave write landed", not "the bytes changed": a 251e BACKUP
// writes the bank in whether or not it differs from the last one, and most
// reads at the panel are re-reads of a module nobody touched. Rewriting the
// file then costs 1.1 s (2026-09-02: 1088 ms, 64 KB through XenoFS's 4 KB
// sectors; millis() saw 131 ms of it, the rest passed with interrupts off --
// see WallClock in PresetEngine.cpp) during which audio, USB, the display
// and the bus slave all stall, 3 s after every Read, for a file that already
// holds these bytes. A CRC of the image is a few ms with interrupts on, so
// compare first and write only on a change.
FLASHMEM static void card_image_flush(const char *why) {
  if (!card_image || !BusCardDirty()) return;
  if (card_burst_ours) {
    // A capture we commanded (the 200e app's Read, the console's m, the USB
    // bridge's verify pass). Every reader of it works from RAM --
    // MasterCardImage(), DumpCard, the bridge, the app's snapshots have their
    // own files -- and nothing looks for it in PBCARD.BIN after a reboot.
    // Writing it there cost a 1.1 s interrupts-off stall after every Read
    // AND overwrote whatever a WPM-less bus had backed up into us: the open
    // design question in PresetBus.h, answered by not writing.
    BusCardClearDirty();
    card_burst_ours = false;
    Serial.printf("PresetBus: card image kept in RAM, PBCARD.BIN untouched (%s)\n", why);
    return;
  }
  const CardSectors::Plan plan = card_sectors.plan(card_image, Buchla200eCrc32);
  if (!plan.whole_image && plan.count == 0) {
    BusCardClearDirty();
    Serial.printf("PresetBus: card image unchanged, not rewritten (%s)\n", why);
    return;
  }
  // DWT cycles, not millis(): systick is masked with everything else while
  // the flash programs, so millis() under-reports exactly the costly part.
  // One lap is fine here -- the counter wraps at ~7 s and a flush is ~1 s.
  const uint32_t c0 = ARM_DWT_CYCCNT;

  // Only the sectors that actually moved. A 259e's 990-byte bank touches one
  // of the sixteen; writing all of them cost 1088 ms with interrupts masked
  // for fifteen sectors of unchanged bytes. The whole-image path stays for
  // the cases where partial writing is not sound: nothing known about the
  // file yet, or a file that is not exactly BUSCARD_SIZE (a short or absent
  // one cannot be seeked into).
  bool whole = plan.whole_image;
  if (!whole) {
    File probe = PhzConfig::myfs.open(kCardFile, FILE_READ);
    const bool sized = probe && probe.size() == BUSCARD_SIZE;
    if (probe) probe.close();
    if (!sized) whole = true;
  }

  bool ok = false;
  int wrote = 0;
  if (whole) {
    File f = PhzConfig::myfs.open(kCardFile, FILE_WRITE_BEGIN);
    if (f) {
      ok = f.write(card_image, BUSCARD_SIZE) == BUSCARD_SIZE;
      f.close();
    }
    wrote = CardSectors::kSectors;
  } else if (plan.count == 0) {
    ok = true;   // the per-sector view agrees with the file; nothing to do
  } else {
    File f = PhzConfig::myfs.open(kCardFile, FILE_WRITE);
    if (f) {
      ok = true;
      for (int i = 0; i < CardSectors::kSectors && ok; ++i) {
        if (!plan.dirty[i]) continue;
        const uint32_t off = CardSectors::offset_of(i);
        ok = f.seek(off) &&
             f.write(card_image + off, CardSectors::kSectorBytes) ==
                 CardSectors::kSectorBytes;
        ++wrote;
      }
      f.close();
    }
  }

  if (ok) {
    BusCardClearDirty();
    card_sectors.adopt(card_image, Buchla200eCrc32);
  } else {
    card_sectors.forget();
  }
  Serial.printf("PresetBus: card image %s, %d of %d sectors in %lu ms wall (%s)\n",
                ok ? "saved" : "SAVE FAILED", wrote, (int)CardSectors::kSectors,
                (unsigned long)((ARM_DWT_CYCCNT - c0) / (F_CPU_ACTUAL / 1000)), why);
}

// Parameterized enable: `card_lo` selects which candidate address to claim
// (BUS200E_CARD_BASE | card_lo). CardServeEnable() (the public API, the
// console 'e' toggle, and everything that predates foreign-module capture)
// always passes 0 -- it emulates the canonical card address other modules
// expect and must stay exactly as gated/tested as before. MasterBackup()/
// MasterRestore() are the only callers that ever pass something else, after
// Bus200eMasterFindFreeCard() has already picked a candidate nothing else
// is ACKing.
FLASHMEM static int card_serve_enable_at(bool on, uint8_t card_lo) {
  if (!enabled) return -1;
  if (on == (bool)card_serving) return 0;

  if (!on) {
    card_serving = false;
    in_card_txn = false;
    card_tx_open = false;
    slave_reconfig(false);
    card_image_flush("disable");
    Serial.println("PresetBus: card serving off");
    return 0;
  }

  const uint8_t addr7 = (uint8_t)((BUS200E_CARD_BASE | (card_lo & 0x7F)) & 0x7F);

  // THE gate for the canonical card address: never contest a live preset
  // manager (two card slaves on one bus corrupt each other's ACKs). This is
  // untouched from before -- card_lo 0 (address 0x50) still refuses on
  // wpm_present exactly as it always did. A non-zero candidate has no
  // cached presence flag of its own (unlike wpm_present for 0x50): callers
  // reaching here with one are expected to have just probed it fresh via
  // Bus200eMasterFindFreeCard(); the self-test below is the final,
  // authoritative check regardless of which address was requested.
  if (addr7 == BUS200E_CARD_BASE && wpm_present) {
    Serial.println("PresetBus: card serve REFUSED - WPM owns 0x50");
    return -2;
  }
  if (!card_image) card_image = (uint8_t *)malloc(BUSCARD_SIZE);
  if (!card_image) {
    Serial.println("PresetBus: card serve failed - no memory");
    return -3;
  }
  memset(card_image, 0xFF, BUSCARD_SIZE);  // blank card = erased EEPROM
  File f = PhzConfig::myfs.open(kCardFile, FILE_READ);
  size_t got = 0;
  if (f) {
    got = f.read(card_image, BUSCARD_SIZE);
    f.close();
  }
  // Only a whole file is known to match the image; a short or missing one
  // leaves the CRC unknown so the first flush always writes.
  // One pass over the image, not two: adopt() computes the per-sector CRCs
  // and that is the whole record of what the file holds.
  if (got == BUSCARD_SIZE) card_sectors.adopt(card_image, Buchla200eCrc32);
  else card_sectors.forget();
  BusCardInit(card_image, BUSCARD_SIZE);
  card_seen_writes = 0;
  card_seen_reads = 0;
  card_flush_arm_ms = 0;
  card_burst_ours = false;
  card_addr7 = addr7;
  card_addr_lo = card_lo & 0x7F;
  slave_reconfig(true, addr7);
  card_serving = true;

  // Self-test through the wire: the polled Wire master and our slave engine
  // share the pads, so an empty write to addr7 must self-ACK and a probe of
  // an unrelated address must still NACK (a mislaid SAMR ADDR1 field would
  // ACK the wrong address -- fail safe by reverting). 0x29 is fixed and
  // outside the {0x50, 0x51} candidate range, so it's never the address
  // under test either way.
  delayMicroseconds(200);
  Wire.beginTransmission(addr7);
  const uint8_t at_card = Wire.endTransmission();
  Wire.beginTransmission(0x29);
  const uint8_t at_other = Wire.endTransmission();
  if (at_card != 0 || at_other == 0) {
    card_serving = false;
    in_card_txn = false;
    card_tx_open = false;
    slave_reconfig(false);
    Serial.printf("PresetBus: card serve self-test FAILED (%02X=%u 0x29=%u), reverted\n",
                  addr7, at_card, at_other);
    return -4;
  }
  Serial.printf("PresetBus: serving %02X (%luK card image)\n", addr7,
                (unsigned long)(BUSCARD_SIZE / 1024));
  return 0;
}

FLASHMEM int CardServeEnable(bool on) { return card_serve_enable_at(on, 0); }

bool CardServing() { return card_serving; }

// ---- foreign-module BACKUP/RESTORE (transient master; new) -----------------
// Adapter wiring Bus200eMaster (BSP-free FSM, see Bus200eMaster.h/.cpp) onto
// this file's real Wire master and BusCard stats. Every op here is a thin
// call into machinery this file already has and already masters the bus
// with elsewhere (tx_gate_open, Bus200eSuppressFrame, the polled Wire
// master) -- no new hardware surface, just new orchestration.
// UNVERIFIED against a live BACKUP/RESTORE exchange: see Bus200eMaster.h.

static uint32_t master_now_ms() { return millis(); }
static int master_tx_gate_open() { return tx_gate_open() ? 1 : 0; }

// The canonical card address (0x50) reuses the cached WPM-presence flag
// rather than a fresh probe: it already answers "is something else ACKing
// 0x50 right now", at no extra bus traffic/risk -- same as CardServeEnable's
// own gate. Every other candidate (today only 0x51, MasterBackup()'s
// fallback for reaching a foreign module past a live WPM) has no cached
// flag of its own, so probe it fresh; fail closed (report claimed) if the
// bus isn't quiet enough to probe safely right now rather than risk it. If
// we're already the one serving addr7 ourselves, the probe below self-ACKs
// (same mechanism the enable-time self-test relies on) and correctly comes
// back "claimed".
static int master_probe_card(uint8_t addr7) {
  if (addr7 == BUS200E_CARD_BASE) return wpm_present ? 1 : 0;
  if (!tx_gate_open()) return 1;
  Wire.beginTransmission(addr7);
  return Wire.endTransmission() == 0 ? 1 : 0;
}

static int master_send_frame(const uint8_t *bytes, uint8_t n) {
  Wire.beginTransmission(0);
  Wire.write(bytes, n);
  return Wire.endTransmission();
}

static void master_suppress_echo(const uint8_t *bytes, uint8_t n) {
  Bus200eSuppressFrame(bytes, n);
}

// Which BusCardStats counter reflects "the target is touching our card"
// depends on job direction: a BACKUP has the target WRITING into our card
// (bytes_written), a RESTORE has it READING (bytes_read). Bus200eMaster
// already knows its own job's direction; asking it back here is simpler
// than threading a second flag through the ops table.
static uint32_t master_card_activity() {
  const BusCardStats *cs = BusCardGetStats();
  if (!cs) return 0;
  return Bus200eMasterIsRestore() ? cs->bytes_read : cs->bytes_written;
}

static const Bus200eMasterOps kMasterOps = {
  master_now_ms, master_tx_gate_open, master_probe_card,
  master_send_frame, master_suppress_echo, master_card_activity,
};

// Candidates tried, in order: 0x50 first (the canonical/expected card
// address), then 0x51 -- lets a capture still succeed with a live WPM
// occupying 0x50 (the whole point of this fix: that used to mean the only
// way to test was physically unplugging the WPM). Add more here if a bus
// ever has both 0x50 and 0x51 contested.
static const uint8_t kMasterCardCandidates[] = { 0x00, 0x01 };

FLASHMEM int MasterBackup(uint8_t mod_addr) {
  uint8_t card_lo = card_addr_lo;
  if (!card_serving) {
    if (!Bus200eMasterFindFreeCard(kMasterCardCandidates,
                                    sizeof(kMasterCardCandidates), &card_lo))
      return -BUS200E_MASTER_ERR_NO_FREE_CARD;
    const int err = card_serve_enable_at(true, card_lo);
    if (err != 0) return -BUS200E_MASTER_ERR_NO_FREE_CARD;
  }
  return Bus200eMasterBackup(mod_addr, card_lo);
}

FLASHMEM int MasterRestore(uint8_t mod_addr) {
  if (!card_serving) return -BUS200E_MASTER_ERR_BAD_ARGS;
  return Bus200eMasterRestore(mod_addr, card_addr_lo);
}

Bus200eMasterState MasterState() { return Bus200eMasterGetState(); }
Bus200eMasterError MasterError() { return Bus200eMasterLastError(); }
bool MasterTransferring() {
  const Bus200eMasterState st = Bus200eMasterGetState();
  const bool in_flight = (st != BUS200E_MASTER_IDLE &&
                          st != BUS200E_MASTER_DONE &&
                          st != BUS200E_MASTER_FAILED);
  return in_flight;
}
uint8_t *MasterCardImage() { return card_serving ? card_image : nullptr; }
void MasterReset() { Bus200eMasterReset(); }

// ---- module identification (QUERY; transient master; new) ------------------
// Thinner than MasterBackup by design: a QUERY claims no card address and
// needs none of card_serve_enable_at()'s gating, so there is nothing to set
// up first -- it is one mastered frame out (through the same kMasterOps
// adapter above) and one reply frame back in through the ordinary slave RX
// path (cb_query_reply near the top of this file).
FLASHMEM int MasterQuery(uint8_t mod_addr) {
  return Bus200eMasterQuery(mod_addr);
}
bool QueryReplyReady() {
  return Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE;
}
uint8_t MasterQueryVersion(uint8_t *out, uint8_t cap) {
  return Bus200eMasterQueryVersion(out, cap);
}
Bus200eQueryState MasterQueryState() { return Bus200eMasterQueryGetState(); }
Bus200eMasterError MasterQueryError() { return Bus200eMasterQueryLastError(); }
void MasterQueryReset() { Bus200eMasterQueryReset(); }
// Declared here rather than beside report_query() below, which uses it: the
// accessors are part of the API block and the definition has to precede them.
static bool query_quiet = false;
void MasterQuerySetQuiet(bool on) { query_quiet = on; }
bool MasterQueryQuiet() { return query_quiet; }

// One-shot console report the moment a QUERY resolves. The whole point of
// the command is the answer, and at the bench it lands asynchronously (the
// reply is a separate bus frame arriving some milliseconds later), so
// printing it here beats making the operator poll 'b' at the right instant.
// Edge-triggered on the FSM's state, so it prints exactly once per query.
static Bus200eQueryState query_reported = BUS200E_QUERY_IDLE;

FLASHMEM static void report_query() {
  const Bus200eQueryState s = Bus200eMasterQueryGetState();
  if (s == query_reported) return;
  query_reported = s;

  // Still edge-tracked above, so leaving quiet mode does not dump a backlog.
  if (query_quiet) return;

  if (s == BUS200E_QUERY_DONE) {
    uint8_t v[BUS200E_QUERY_VER_MAX];
    const uint8_t n = Bus200eMasterQueryVersion(v, sizeof(v));
    // A real module's payload is one opaque 0xFF byte, not text (see
    // PresetBus200e.h), so only quote a string when there is one to quote.
    bool printable = false;
    for (uint8_t i = 0; i < n; ++i)
      if (v[i] >= 0x20 && v[i] < 0x7F) printable = true;
    Serial.printf("PresetBus: module %02X answered", Bus200eMasterQueryModAddr());
    if (printable) {
      Serial.print(" \"");
      for (uint8_t i = 0; i < n; ++i)
        Serial.printf("%c", (v[i] >= 0x20 && v[i] < 0x7F) ? (char)v[i] : '.');
      Serial.print("\"");
    }
    Serial.print(" (hex:");
    for (uint8_t i = 0; i < n; ++i) Serial.printf(" %02X", v[i]);
    Serial.println(")");
  } else if (s == BUS200E_QUERY_FAILED) {
    const Bus200eMasterError e = Bus200eMasterQueryLastError();
    Serial.printf("PresetBus: QUERY %02X failed - %s (stray replies: %lu)\n",
                  Bus200eMasterQueryModAddr(),
                  e == BUS200E_MASTER_ERR_SEND_TIMEOUT ? "never got a quiet bus"
                  : e == BUS200E_MASTER_ERR_NO_RESPONSE ? "no reply"
                  : "bad request",
                  (unsigned long)Bus200eMasterQueryStrayReplies());
  }
}

// Flush the image 3s after a write burst goes quiet (a module BACKUP is a
// stream of write transactions; don't thrash LittleFS mid-transfer).
//
// ...and never while a master job is open, even a quiet one. The flush is
// a 64 KB LittleFS write (1.1 s, interrupts masked across each sector erase), and
// the one window the 3 s quiet rule does not cover is the start of the NEXT
// job: a Read tapped again 2 s after the last one lands the flush exactly
// as the command goes out and the 251e begins writing back. Our slave would
// clock-stretch it through the erases; whether a 251e waits that long is
// not known, and PresetEngine::Process defers a bus RECALL for the same
// reason. The image stays dirty and flushes once the job closes.
//
// Whose bytes these are decides what happens then. A module backing itself
// up into us on a WPM-less bus (CardServeEnable, the console's k) is the
// card's whole purpose and must survive a power cycle, so that burst is
// written to PBCARD.BIN. A burst that lands while one of our own master
// jobs is open is a bank WE asked for, kept in RAM only -- see
// card_image_flush(). Sticky across the burst: one observation inside the
// job is enough, since nothing else can be writing to us while it runs.
static void card_task() {
  if (!card_serving) return;
  const BusCardStats *cs = BusCardGetStats();
  const bool wrote = cs->bytes_written != card_seen_writes;
  if (wrote || cs->bytes_read != card_seen_reads) {
    // Either direction: tx_gate_open() holds our master off the bus while a
    // module is chunking into or out of us. The gap statistic is what sized
    // that hold (b prints it); a gap over a second is the burst boundary.
    const uint32_t now_us = micros();
    const uint32_t gap = now_us - card_last_us;
    if (card_last_us && gap < 1000000) {
      if (gap > card_gap_max_us) card_gap_max_us = gap;
    } else {
      card_gap_max_us = 0;
    }
    card_last_us = now_us;
    card_last_ms = millis() ? millis() : 1;
    card_seen_reads = cs->bytes_read;
  }
  if (wrote) {
    card_seen_writes = cs->bytes_written;
    card_flush_arm_ms = millis();
    if (MasterTransferring()) card_burst_ours = true;
  } else if (card_flush_arm_ms && BusCardDirty()
             && millis() - card_flush_arm_ms > 3000
             && !MasterTransferring()) {
    card_flush_arm_ms = 0;
    card_image_flush("write burst done");
  }
}

// ---- bus-stuck watchdog ------------------------------------------------------
// A multi-master bus eventually gets wedged: a master dies mid-transaction
// and leaves SDA low, so BBF never clears and every TX gate stays shut.
// Declare "stuck" only after BBF has been continuously set for 3s with ZERO
// slave RX in that window (real 200e transfers never hold the bus that long
// and always produce RX). Recovery is staged: reset our own master engine
// first (an arbitration-loss wedge on our side is the cheap, likely cause),
// then the classic 9-SCL-pulse + STOP release, then full re-init.
static uint32_t bbf_since_ms = 0;

FLASHMEM __attribute__((noinline)) static void bus_stuck_recover() {
  stats.bus_stuck++;
  Serial.println("PresetBus: bus stuck (BBF 3s, no RX) - recovering");

  // stage 1: kick our master engine
  Wire.begin();
  Wire.setClock(100000);
  delayMicroseconds(200);
  if (!(LPI2C1_MSR & LPI2C_MSR_BBF)) {
    stats.bus_recovered++;
    bbf_since_ms = 0;
    Serial.println("PresetBus: recovered (master engine reset)");
    return;
  }

  // stage 2: bit-bang 9 SCL pulses with SDA released so a slave stuck
  // mid-byte can finish shifting, then generate a STOP by hand.
  // (Pins 18=SDA / 19=SCL run through the level shifter; open-drain only.)
  pinMode(18, OUTPUT_OPENDRAIN);
  pinMode(19, OUTPUT_OPENDRAIN);
  digitalWrite(18, HIGH);
  for (int i = 0; i < 9; ++i) {
    digitalWrite(19, LOW);
    delayMicroseconds(5);
    digitalWrite(19, HIGH);
    delayMicroseconds(5);
    if (digitalRead(18)) break;  // SDA released: done
  }
  digitalWrite(18, LOW);   // STOP: SDA low->high while SCL high
  delayMicroseconds(5);
  digitalWrite(18, HIGH);
  delayMicroseconds(5);

  // stage 3: hand the pads back to LPI2C and rebuild both engines
  Wire.begin();
  Wire.setClock(100000);
  slave_reconfig(card_serving, card_addr7);
  in_card_txn = false;
  card_tx_open = false;
  Bus200eFeedEvent(BUS200E_EV_OVF);  // poison anything half-parsed
  bbf_since_ms = 0;
  if (!(LPI2C1_MSR & LPI2C_MSR_BBF)) {
    stats.bus_recovered++;
    Serial.println("PresetBus: recovered (SCL pulse + STOP)");
  } else {
    Serial.println("PresetBus: still stuck after recovery (hardware?)");
  }
}

static inline void bus_stuck_check() {
  if (!(LPI2C1_MSR & LPI2C_MSR_BBF)) {
    bbf_since_ms = 0;
    return;
  }
  const uint32_t now = millis();
  if (!bbf_since_ms) {
    bbf_since_ms = now;
    return;
  }
  if (now - bbf_since_ms < 3000) return;
  // "Stuck" must mean NO activity of any kind, or we'd fire mid-transfer:
  // - GC frames drain through the ring (last_rx_ms)
  // - card-serving traffic hits only the ISR (isr_count), never the ring
  // - a WPM<->third-party 0x50 transfer is invisible to our slave, but the
  //   GC command that opened it is parsed: honor the card-transfer holdoff
  static uint32_t seen_isr = 0;
  const uint32_t isr_now = stats.isr_count;
  const bool slave_active = (isr_now != seen_isr);
  seen_isr = isr_now;
  const uint32_t xfer = Bus200eLastTransferMs();
  if (slave_active || now - last_rx_ms < 3000
      || (xfer && now - xfer < 5000)) {
    bbf_since_ms = now;  // busy, not stuck
    return;
  }
  bus_stuck_recover();
}

// ---- bus MIDI public API ----------------------------------------------------

void QueueMidiTx(uint8_t type, uint8_t channel, uint8_t d1, uint8_t d2) {
  if (!enabled) return;
  // channel 1-4 -> 200e bus lines A(0x8) B(0x4) C(0x2) D(0x1), else all
  uint8_t status;
  if (type >= 0xF8) {
    status = type;
    d1 = d2 = 0;
  } else {
    const uint8_t mask = (channel >= 1 && channel <= 4)
                             ? uint8_t(0x8 >> (channel - 1)) : uint8_t(0xF);
    status = (type & 0xF0) | mask;
  }
  // pushes come from both the app ISR and loop: mask IRQs around the ring.
  // (Unconditional re-enable is fine — ISRs run with PRIMASK clear, and the
  // loop never calls this with interrupts already masked.) The ring folds
  // continuous controllers into a pending one of the same stream; see
  // MidiTxRing.h for why, and test_miditxring.cpp for the proof.
  __disable_irq();
  midi_tx.push(status, d1, d2);
  stats.midi_tx_drop = midi_tx.dropped;
  stats.midi_tx_merged = midi_tx.merged;
  stats.midi_tx_hw = midi_tx.high_water;
  __enable_irq();
}

bool ReadMidiRx(uint8_t &status, uint8_t &d1, uint8_t &d2) {
  if (midi_rx_r == midi_rx_w) return false;
  const uint32_t v = midi_rx_q[midi_rx_r & (kMidiRing - 1)];
  midi_rx_r = midi_rx_r + 1;
  status = v & 0xFF;
  d1 = (v >> 8) & 0xFF;
  d2 = (v >> 16) & 0xFF;
  return true;
}

// master queued MIDI frames onto the bus; quiet-gated like the QUERY reply
FLASHMEM static void pump_midi_tx() {
  uint8_t sent = 0;
  uint32_t v = 0;
  for (;;) {
    // Clock first. Every frame ahead of a tick costs about a millisecond of
    // quiet-gated bus, so a burst of notes or controllers queued before it
    // arrives delays the tick by that many milliseconds -- and a tick's
    // whole value is when it lands. IRQ-masked because QueueMidiTx runs in
    // ISR context and its coalescing writes into the same ring body; the
    // unconditional re-enable is fine here for the reason QueueMidiTx
    // gives, this is loop context.
    __disable_irq();
    midi_tx.promote_realtime();
    __enable_irq();

    if (!midi_tx.peek(v) || sent >= 4) return;
    if (!tx_gate_open()) return;

    // [08][00][22][0F][status|mask][00][d1][d2][00] -- 2WIRELESS long format
    uint8_t f[9] = { 0x08, 0x00, 0x22, 0x0F,
                     uint8_t(v & 0xFF), 0x00,
                     uint8_t((v >> 8) & 0xFF), uint8_t((v >> 16) & 0xFF),
                     0x00 };

    Wire.beginTransmission(0);
    Wire.write(f, sizeof(f));
    const uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Bus200eSuppressFrame(f, sizeof(f));
      // drain our own self-echo now: tx_gate_open() refuses to send again
      // (or SaveSlot/RecallSlot's caller-side use of this same pending-op
      // pattern would risk the same stale-suppression race pump_broadcast()
      // hit) while the ring holds unconsumed bytes.
      drain_ring();
    }

    if (err == 0) {
      midi_tx.pop();
      midi_tx_fails = 0;
      stats.midi_tx++;
      ++sent;
    } else {
      // arbitration loss etc: retry next Task() pass; give up eventually
      if (++midi_tx_fails >= 100) {
        midi_tx.pop();
        midi_tx_fails = 0;
        stats.midi_tx_drop = ++midi_tx.dropped;
      }
      return;
    }
  }
}

// ---- QUERY reply (we briefly master the bus) -------------------------------
static uint8_t query_tries = 0;

FLASHMEM static void try_query_reply() {
  if (!tx_gate_open()) return;

  // [04][22][ourAddr][1C][FF] — module identity ACK, byte-for-byte the shape
  // a real 251e (0x5C) and the module at 0x28 were captured answering with
  // at the bench. This used to be an 11-byte [0A][22][addr][13]["30.6   "]
  // frame of our own invention: no module on a real bus speaks that, so a
  // preset manager had no reason to recognize us. See PresetBus200e.h.
  uint8_t f[5] = { 0x04, 0x22, Bus200eModuleAddress(), 0x1C, 0xFF };

  Wire.beginTransmission(0);  // general call
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) {
    Bus200eSuppressFrame(f, sizeof(f));
    drain_ring();  // consume our own self-echo while suppression is fresh
  }

  if (err == 0) {
    Bus200eClearQueryPending();
    query_tries = 0;
    stats.query_replies++;
    if (verbose) Serial.println("PresetBus: QUERY reply sent");
  } else {
    stats.query_retries++;
    if (++query_tries >= 5) {  // give up; the manager will re-poll
      Bus200eClearQueryPending();
      query_tries = 0;
      if (verbose) Serial.printf("PresetBus: QUERY reply failed (%d)\n", err);
    }
  }
}

// ---- public API ------------------------------------------------------------

FLASHMEM void Init() {
  if (!I2C_Expansion) return;  // no I2C header on this hardware

  Bus200eInit(&kOps);
  Bus200eMasterInit(&kMasterOps);

  // persisted module address (GLOBALS.CFG must be the loaded map here)
  uint64_t addr = 0;
  if (PhzConfig::getValue(kAddrKey, addr) && addr > 0 && addr < 0x78)
    Bus200eSetModuleAddress((uint8_t)addr);

  // LPI2C1 slave engine, alongside the (polled) stock Wire master.
  // Wire.begin() has already gated the peripheral clock and set the pads.
  // GCEN (set in slave_reconfig): match the general call (0x00) — the bit
  // neither Wire nor teensy4_i2c ever sets, the whole reason this block
  // exists. RXSTALL lets the peripheral clock-stretch if we fall behind.
  LPI2C1_SCR = LPI2C_SCR_RST;
  LPI2C1_SCR = 0;
  LPI2C1_SCFGR2 = LPI2C_SCFGR2_FILTSDA(2) | LPI2C_SCFGR2_FILTSCL(2)
                | LPI2C_SCFGR2_DATAVD(3) | LPI2C_SCFGR2_CLKHOLD(2);
  attachInterruptVector(IRQ_LPI2C1, lpi2c1_slave_isr);
  NVIC_SET_PRIORITY(IRQ_LPI2C1, 144);  // below CORE (80) and UI (128)
  NVIC_ENABLE_IRQ(IRQ_LPI2C1);
  slave_reconfig(false);  // GC-only; card serving is opt-in per boot

  enabled = true;
  Serial.printf("PresetBus: slave listening on general call (module addr %02X)\n",
                Bus200eModuleAddress());
}

// Master-TX gate shared by every pump: quiet bus AND no card transfer in
// progress, ours or anyone's.
//
// A card transfer is one module mastering chunk after chunk (a 259e: 45
// writes of 22 bytes; a 251e: 63120 bytes over 7 s), and between chunks the
// bus is idle -- BBF clear, nothing in our ring -- for longer than this gate
// used to need. So the 5 s WPM presence probe fired between two chunks of a
// 259e BACKUP into our card, arbitrated the module's next START off the
// bus (0x50 beats 0x51 by the last address bit), and the bank arrived 33
// bytes short: 957 of 990, one chunk gone and another cut off, on a read
// the app then showed as good (bench 2026-09-02, ~6% of 259e reads by the
// probe's duty cycle; a 251e read is exposed for its whole 7 s). Three
// holds close it:
//   - our own master job, from its command until DONE (the FSM never
//     transmits in those states, so it cannot gate itself out);
//   - any byte through our served card within the last CARD_QUIET_MS,
//     either direction -- a module backing itself up into us on a WPM-less
//     bus is the same traffic without a job of ours around it;
//   - a manager's BACKUP/RESTORE command seen on the general call: the
//     transfer itself runs at 0x50, invisible to our slave, so for
//     FOREIGN_XFER_WINDOW_MS after the command any bus activity at all
//     (BBF, sampled every Task() pass) is taken as that transfer still
//     running, on top of the flat 1.5 s that covered its done-detect.
//     A WPM in card mode swallows every byte on the bus as FRAM data, so
//     a TX inside the window corrupts the user's backup, not just a chunk.
#define CARD_QUIET_MS          500
#define FOREIGN_XFER_WINDOW_MS 15000   // matches the master's HARD_CAP
#define FOREIGN_XFER_GAP_MS    20
static uint32_t last_bbf_ms = 0;

FLASHMEM static bool tx_gate_open() {
  if (uint8_t(ring_w - ring_r) != 0) return false;
  const uint32_t now = millis();
  if (now - last_rx_ms < 2) return false;
  const Bus200eMasterState ms = Bus200eMasterGetState();
  if (ms == BUS200E_MASTER_WAIT_ACTIVITY || ms == BUS200E_MASTER_TRANSFERRING)
    return false;
  if (card_last_ms && now - card_last_ms < CARD_QUIET_MS) return false;
  const uint32_t t = Bus200eLastTransferMs();
  if (t && now - t < 1500) return false;
  if (t && now - t < FOREIGN_XFER_WINDOW_MS && last_bbf_ms
      && now - last_bbf_ms < FOREIGN_XFER_GAP_MS)
    return false;
  if (LPI2C1_MSR & LPI2C_MSR_BBF) return false;
  return true;
}

static uint32_t loop_rate_hz = 0;

void Task() {
  if (!enabled) return;
  Bus200eSetNow(millis());

  // rolling main-loop rate (the number behind "feels sluggish")
  static uint32_t rate_t0 = 0, rate_l0 = 0;
  if (millis() - rate_t0 >= 500) {
    loop_rate_hz = (loop_counter - rate_l0) * 1000 / (millis() - rate_t0);
    rate_t0 = millis();
    rate_l0 = loop_counter;
  }

  drain_ring();
  if (LPI2C1_MSR & LPI2C_MSR_BBF) last_bbf_ms = millis() ? millis() : 1;
  card_task();   // before the pumps: tx_gate_open() reads what it stamps

  if (Bus200eQueryPending()) try_query_reply();
  Bus200eTask();   // card-transfer job engine (self-clears with null hooks)
  Bus200eMasterTask();  // foreign-module BACKUP/RESTORE orchestration (new)
  Bus200eMasterQueryTask();  // foreign-module QUERY orchestration (new).
  report_query();            // Runs AFTER drain_ring() above, so a reply that
                             // arrived this pass is already captured and the
                             // result prints on the same tick it lands.
  pump_broadcast();
  pump_midi_tx();
  probe_wpm();
  bus_stuck_check();
}

bool Enabled() { return enabled; }
bool RemoteEnabled() { return Bus200eRemoteEnabled(); }

FLASHMEM void SetModuleAddress(uint8_t a) {
  Bus200eSetModuleAddress(a);
  // persist into GLOBALS.CFG (caller ensures the default map is loaded,
  // or accepts that the key rides along in the current map)
  PhzConfig::setValue(kAddrKey, Bus200eModuleAddress());
}

// live-edit path (Settings UI): takes effect on the bus immediately,
// caller persists later via SetModuleAddress under the right config map
FLASHMEM void SetModuleAddressRuntime(uint8_t a) {
  Bus200eSetModuleAddress(a);
}
uint8_t ModuleAddress() { return Bus200eModuleAddress(); }
const Stats &GetStats() { return stats; }
void SetVerbose(bool on) { verbose = on; }
bool Verbose() { return verbose; }

FLASHMEM void DebugDump() {
  Serial.println("--- PresetBus ---");
  // CORE ISR liveness: two tick samples 5ms apart (16.67kHz => ~83 delta)
  {
    Serial.printf("loop rate ~%lu Hz\n", (unsigned long)loop_rate_hz);
    const uint32_t t0 = OC::CORE::ticks;
    delay(5);
    Serial.printf("core_ticks=%lu delta5ms=%lu display_en=%d app_isr=%d app_loop=%d\n",
                  OC::CORE::ticks, OC::CORE::ticks - t0,
                  OC::CORE::display_update_enabled, OC::CORE::app_isr_enabled,
                  OC::CORE::app_loop_enabled);
  }
  Serial.printf("enabled=%d remote=%d module_addr=%02X verbose=%d\n",
                enabled, Bus200eRemoteEnabled(), Bus200eModuleAddress(), verbose);
  {
    const Bus200eStats *d = Bus200eGetStats();
    const char *dialect = (d->frames_long || d->frames_short)
        ? (d->frames_long >= d->frames_short ? "v1/long" : "v2/short")
        : "unknown";
    Serial.printf("wpm=%s owner_0x50=%s dialect=%s (long=%lu short=%lu) probes=%lu\n",
                  wpm_present ? "present" : "absent",
                  wpm_present ? "WPM"
                  : (card_serving && card_addr7 == BUS200E_CARD_BASE) ? "US(card)"
                  : "none",
                  dialect, d->frames_long, d->frames_short, wpm_probes);
    if (card_serving || BusCardAttached()) {
      const BusCardStats *cs = BusCardGetStats();
      Serial.printf("card: serving=%d addr=%02X dirty=%d ptr=%04lX w_txn=%lu r_txn=%lu wr=%lu rd=%lu gap=%luus\n",
                    (int)card_serving, card_addr7, BusCardDirty(),
                    (unsigned long)BusCardPointer(),
                    cs->txns_write, cs->txns_read,
                    cs->bytes_written, cs->bytes_read,
                    (unsigned long)card_gap_max_us);
    }
    Serial.printf("bcast: tx=%lu drop=%lu queued=%u\n",
                  bcast_tx, bcast_drop, bcast_q.size());
  }
  Serial.printf("isr=%lu starts=%lu stops=%lu bytes=%lu ring_ovf=%lu\n",
                stats.isr_count, stats.starts, stats.stops, stats.bytes,
                stats.ring_ovf);
  Serial.printf("hw: ring=%lu/%u midi_rx=%lu/%u midi_tx=%lu/%u | stuck=%lu recovered=%lu\n",
                stats.ring_hw, kRingSize, stats.midi_rx_hw, kMidiRingRx,
                stats.midi_tx_hw, kMidiRingTx, stats.bus_stuck,
                stats.bus_recovered);
  Serial.printf("midi tx ring: merged=%lu promoted=%lu dropped=%lu\n",
                midi_tx.merged, midi_tx.promoted, midi_tx.dropped);
  const Bus200eStats *ps = Bus200eGetStats();
  Serial.printf("frames=%lu dropped=%lu query_tx=%lu query_retry=%lu\n",
                ps->frames, ps->dropped, stats.query_replies, stats.query_retries);
  Serial.printf("midi: rx=%lu rx_ovf=%lu tx=%lu tx_drop=%lu tx_merged=%lu\n",
                stats.midi_rx, stats.midi_rx_ovf, stats.midi_tx,
                stats.midi_tx_drop, stats.midi_tx_merged);
  Serial.printf("engine: last_slot=%d was_save=%d busy=%d\n",
                PresetEngine::LastSlot(), PresetEngine::LastWasSave(),
                PresetEngine::Busy());
  {
    static const char *const mstates[] = {
      "IDLE", "FINDING_CARD", "SENDING", "WAIT_ACTIVITY",
      "TRANSFERRING", "DONE", "FAILED",
    };
    static const char *const merrs[] = {
      "NONE", "BUSY", "BAD_ARGS", "NO_FREE_CARD", "SEND_TIMEOUT", "NO_RESPONSE",
    };
    const Bus200eMasterState ms = Bus200eMasterGetState();
    const Bus200eMasterError me = Bus200eMasterLastError();
    Serial.printf("master: state=%s error=%s mod=%02X card_lo=%02X restore=%d bytes=%lu acked=%d\n",
                  ms <= 6 ? mstates[ms] : "?", me <= 5 ? merrs[me] : "?",
                  Bus200eMasterModAddr(), Bus200eMasterCardAddr(),
                  Bus200eMasterIsRestore(),
                  (unsigned long)Bus200eMasterBytesTransferred(),
                  Bus200eMasterAcked());
  }
  {
    static const char *const qstates[] = {
      "IDLE", "SENDING", "WAITING", "DONE", "FAILED",
    };
    static const char *const merrs[] = {
      "NONE", "BUSY", "BAD_ARGS", "NO_FREE_CARD", "SEND_TIMEOUT", "NO_RESPONSE",
    };
    const Bus200eQueryState qs = Bus200eMasterQueryGetState();
    const Bus200eMasterError qe = Bus200eMasterQueryLastError();
    uint8_t v[BUS200E_QUERY_VER_MAX];
    const uint8_t vn = Bus200eMasterQueryVersion(v, sizeof(v));
    Serial.printf("query: state=%s error=%s mod=%02X stray=%lu ver=\"",
                  qs <= 4 ? qstates[qs] : "?", qe <= 5 ? merrs[qe] : "?",
                  Bus200eMasterQueryModAddr(),
                  (unsigned long)Bus200eMasterQueryStrayReplies());
    for (uint8_t i = 0; i < vn; ++i)
      Serial.printf("%c", (v[i] >= 0x20 && v[i] < 0x7F) ? (char)v[i] : '.');
    Serial.println("\"");
  }
  static const char *const opnames[] = {
    "none", "RECALL", "SAVE", "REMOTE_EN", "REMOTE_DIS", "POLL_DONE",
    "QUERY", "BACKUP", "RESTORE", "MIDI", "CLOCK", "UNKNOWN", "DROPPED",
    "QRY_REPLY", "XFER_DONE", "LOAD_ACK",
  };
  const uint32_t total = Bus200eLogTotal();
  Serial.printf("decoded commands (%lu total, newest first):\n", total);
  Bus200eCmd c;
  for (uint32_t i = 0; i < 10 && Bus200eLogRead(i, &c); ++i) {
    Serial.printf("  %-10s arg=%u mod=%02X card=%02X off=%04X\n",
                  c.op <= BUS200E_OP_XFER_DONE ? opnames[c.op] : "?",
                  c.arg, c.mod_addr,
                  c.card_lo, c.mem_off);
  }
}

FLASHMEM void DumpCard() {
  if (!card_serving || !card_image) {
    Serial.println("PresetBus: no card image (not serving)");
    return;
  }
  // Bus200eMasterBytesTransferred() -- NOT BusCardGetStats()->bytes_written
  // -- is the extent of the last completed master job specifically:
  // bytes_written/bytes_read are lifetime-cumulative counters across every
  // transaction the card slave has ever served (see PresetBusCard.h), so
  // capturing the same module twice back to back would otherwise double the
  // reported length on the second dump even though the image content is
  // byte-identical (990 real bytes, then 990 more of misleadingly-included
  // 0xFF filler).
  const uint32_t n = Bus200eMasterBytesTransferred();
  if (n == 0) {
    Serial.println("PresetBus: no completed master transfer yet (0 bytes)");
    return;
  }
  Serial.printf("PresetBus: card image dump, %lu bytes (last master transfer)\n",
                (unsigned long)n);
  for (uint32_t off = 0; off < n && off < BUSCARD_SIZE; off += 16) {
    Serial.printf("%04lX:", (unsigned long)off);
    for (uint32_t i = off; i < off + 16 && i < n && i < BUSCARD_SIZE; ++i)
      Serial.printf(" %02X", card_image[i]);
    Serial.println();
  }
}

}  // namespace PresetBus
}  // namespace OC

#endif  // ARDUINO_TEENSY41 && PRESET_BUS
