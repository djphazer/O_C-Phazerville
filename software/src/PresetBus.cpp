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

extern volatile uint32_t loop_counter;

namespace OC {
namespace PresetBus {

static constexpr uint16_t kAddrKey = (8 << 8) | 0x10;

static constexpr uint16_t kRingSize = 256;
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
    ring_ovf = true;
    return;
  }
  if (depth + 1u > stats.ring_hw) stats.ring_hw = depth + 1u;
  ring[ring_w & (kRingSize - 1)] = ev;
  ring_w = ring_w + 1;
}

static volatile bool card_serving = false;
static volatile uint8_t card_addr7 = BUS200E_CARD_BASE;
static volatile bool in_card_txn = false;
static volatile bool card_tx_open = false;
static volatile uint16_t staged_addr = 0xFFFF;

static void lpi2c1_slave_isr() {
  stats.isr_count++;
  uint32_t status = LPI2C1_SSR;
  const uint32_t w1c = status & 0xF00;
  if (w1c) LPI2C1_SSR = w1c;

  if (card_serving && (status & LPI2C_SSR_AVF)) {
    const uint32_t sasr = LPI2C1_SASR;
    if (!(sasr & LPI2C_SASR_ANV)) staged_addr = sasr & 0x7FF;
  }

  if (status & LPI2C_SSR_RDF) {
    const uint32_t rx = LPI2C1_SRDR;
    if (rx & LPI2C_SRDR_SOF) {
      if (status & (LPI2C_SSR_SDF | LPI2C_SSR_RSF)) {
        if (in_card_txn) {
          if (card_tx_open) BusCardTxRewind();
          BusCardStop();
          in_card_txn = false;
          card_tx_open = false;
        } else {
          push_event(BUS200E_EV_STOP);
        }
        stats.stops++;
        status &= ~(uint32_t)(LPI2C_SSR_SDF | LPI2C_SSR_RSF);
      } else if (in_card_txn) {
        BusCardStop();
        in_card_txn = false;
        card_tx_open = false;
      }
      if (card_serving && (staged_addr >> 1) == card_addr7) {
        in_card_txn = true;
        BusCardStart(0);
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
  if (status & (LPI2C_SSR_SDF | LPI2C_SSR_RSF)) {
    if (in_card_txn) {
      if (card_tx_open) BusCardTxRewind();
      BusCardStop();
      if (status & LPI2C_SSR_SDF) in_card_txn = false;
    } else {
      push_event(BUS200E_EV_STOP);
      if (status & LPI2C_SSR_RSF) push_event(BUS200E_EV_START);
    }
    card_tx_open = false;
    stats.stops++;
  }
  if (status & LPI2C_SSR_TDF) {
    if (card_serving && (staged_addr >> 1) == card_addr7
        && (staged_addr & 1)) {
      if (!card_tx_open) {
        card_tx_open = true;
        in_card_txn = true;
        BusCardStart(1);
      }
      LPI2C1_STDR = BusCardTxByte();
    } else if (card_serving) {
      LPI2C1_STDR = 0xFF;
    }
  }
}

static constexpr uint8_t kMidiRing = 32;
static volatile uint32_t midi_rx_q[kMidiRing];
static volatile uint8_t midi_rx_w = 0;
static uint8_t midi_rx_r = 0;
static constexpr uint8_t kMidiRingRx = kMidiRing;
static constexpr uint8_t kMidiRingTx = MidiTxRing::kSize;
static MidiTxRing midi_tx;
static uint8_t midi_tx_fails = 0;

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

static void cb_query_reply(uint8_t from_addr, const uint8_t *ver, uint8_t n) {
  Bus200eMasterQueryReply(from_addr, ver, n);
  if (verbose) {
    Serial.printf("PresetBus: QUERY reply from %02X (%u bytes)\n", from_addr, n);
  }
}

static void cb_xfer_done(uint8_t from_addr) {
  Bus200eMasterXferDone(from_addr);
  if (verbose) Serial.printf("PresetBus: transfer done from %02X\n", from_addr);
}

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
  if (free_slot < 0) {
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
  0, nullptr, nullptr, nullptr, nullptr,
  cb_midi,
  cb_query_reply,
  cb_xfer_done,
  cb_load_ack,
};

static bool tx_gate_open();

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

  uint8_t f[5] = { 0x04, 0x00, 0x22, cmd.op, uint8_t(cmd.slot & 0x1F) };

  Wire.beginTransmission(0);
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) {
    bcast_dequeue();
    bcast_tx++;
    if (cmd.op == kCmdSave) PresetEngine::RequestSave(cmd.slot);
    else PresetEngine::RequestRecall(cmd.slot);
    Bus200eSuppressFrame(f, sizeof(f));
    drain_ring();
    if (verbose) Serial.printf("PresetBus: broadcast %s %d\n",
                               cmd.op == kCmdSave ? "SAVE" : "RECALL", cmd.slot);
  } else if (++bcast_tries >= 50) {
    bcast_dequeue();
    bcast_drop++;
    Serial.printf("PresetBus: broadcast dropped (err %d)\n", err);
  }
}

static bool wpm_present = false;
static uint8_t wpm_misses = 0;
static uint32_t wpm_last_probe_ms = 0;
static uint32_t wpm_probes = 0;

bool WpmPresent() { return wpm_present; }

FLASHMEM static void probe_wpm() {
  if (card_serving && card_addr7 == BUS200E_CARD_BASE) return;
  if (millis() - wpm_last_probe_ms < 5000) return;
  if (!tx_gate_open()) return;
  wpm_last_probe_ms = millis();
  wpm_probes++;

  Wire.beginTransmission(0x50);
  const uint8_t err = Wire.endTransmission();

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
  }
}

static uint8_t *card_image = nullptr;
static uint32_t card_flush_arm_ms = 0;
static uint32_t card_seen_writes = 0;
static uint32_t card_seen_reads = 0;
static uint32_t card_last_ms = 0;
static uint32_t card_last_us = 0;
static uint32_t card_gap_max_us = 0;
static bool card_burst_ours = false;
static constexpr const char *kCardFile = "PBCARD.BIN";
static uint8_t card_addr_lo = 0;

FLASHMEM static void slave_reconfig(bool serve, uint8_t addr7 = BUS200E_CARD_BASE) {
  LPI2C1_SCR = 0;
  LPI2C1_SIER = 0;
  uint32_t cfg1 = LPI2C_SCFGR1_GCEN | LPI2C_SCFGR1_RXSTALL;
  uint32_t samr = LPI2C_SAMR_ADDR0(0);
  uint32_t sier = LPI2C_SIER_RDIE | LPI2C_SIER_SDIE | LPI2C_SIER_RSIE;
  if (serve) {
    cfg1 |= LPI2C_SCFGR1_ADDRCFG(2)
          | LPI2C_SCFGR1_TXDSTALL;
    samr |= LPI2C_SAMR_ADDR1(addr7 & 0x7F);
    sier |= LPI2C_SIER_TDIE;
  }
  LPI2C1_SCFGR1 = cfg1;
  LPI2C1_SAMR = samr;
  LPI2C1_SIER = sier;
  LPI2C1_SCR = LPI2C_SCR_SEN | LPI2C_SCR_FILTEN;
}

static CardSectors card_sectors;

FLASHMEM static void card_image_flush(const char *why) {
  if (!card_image || !BusCardDirty()) return;
  if (card_burst_ours) {
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
  const uint32_t c0 = ARM_DWT_CYCCNT;

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
    ok = true;
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

  if (addr7 == BUS200E_CARD_BASE && wpm_present) {
    Serial.println("PresetBus: card serve REFUSED - WPM owns 0x50");
    return -2;
  }
  if (!card_image) card_image = (uint8_t *)malloc(BUSCARD_SIZE);
  if (!card_image) {
    Serial.println("PresetBus: card serve failed - no memory");
    return -3;
  }
  memset(card_image, 0xFF, BUSCARD_SIZE);
  File f = PhzConfig::myfs.open(kCardFile, FILE_READ);
  size_t got = 0;
  if (f) {
    got = f.read(card_image, BUSCARD_SIZE);
    f.close();
  }
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

static uint32_t master_now_ms() { return millis(); }
static int master_tx_gate_open() { return tx_gate_open() ? 1 : 0; }

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

static uint32_t master_card_activity() {
  const BusCardStats *cs = BusCardGetStats();
  if (!cs) return 0;
  return Bus200eMasterIsRestore() ? cs->bytes_read : cs->bytes_written;
}

static const Bus200eMasterOps kMasterOps = {
  master_now_ms, master_tx_gate_open, master_probe_card,
  master_send_frame, master_suppress_echo, master_card_activity,
};

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
static bool query_quiet = false;
void MasterQuerySetQuiet(bool on) { query_quiet = on; }
bool MasterQueryQuiet() { return query_quiet; }

static Bus200eQueryState query_reported = BUS200E_QUERY_IDLE;

FLASHMEM static void report_query() {
  const Bus200eQueryState s = Bus200eMasterQueryGetState();
  if (s == query_reported) return;
  query_reported = s;

  if (query_quiet) return;

  if (s == BUS200E_QUERY_DONE) {
    uint8_t v[BUS200E_QUERY_VER_MAX];
    const uint8_t n = Bus200eMasterQueryVersion(v, sizeof(v));
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

static void card_task() {
  if (!card_serving) return;
  const BusCardStats *cs = BusCardGetStats();
  const bool wrote = cs->bytes_written != card_seen_writes;
  if (wrote || cs->bytes_read != card_seen_reads) {
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

static uint32_t bbf_since_ms = 0;

FLASHMEM __attribute__((noinline)) static void bus_stuck_recover() {
  stats.bus_stuck++;
  Serial.println("PresetBus: bus stuck (BBF 3s, no RX) - recovering");

  Wire.begin();
  Wire.setClock(100000);
  delayMicroseconds(200);
  if (!(LPI2C1_MSR & LPI2C_MSR_BBF)) {
    stats.bus_recovered++;
    bbf_since_ms = 0;
    Serial.println("PresetBus: recovered (master engine reset)");
    return;
  }

  pinMode(18, OUTPUT_OPENDRAIN);
  pinMode(19, OUTPUT_OPENDRAIN);
  digitalWrite(18, HIGH);
  for (int i = 0; i < 9; ++i) {
    digitalWrite(19, LOW);
    delayMicroseconds(5);
    digitalWrite(19, HIGH);
    delayMicroseconds(5);
    if (digitalRead(18)) break;
  }
  digitalWrite(18, LOW);
  delayMicroseconds(5);
  digitalWrite(18, HIGH);
  delayMicroseconds(5);

  Wire.begin();
  Wire.setClock(100000);
  slave_reconfig(card_serving, card_addr7);
  in_card_txn = false;
  card_tx_open = false;
  Bus200eFeedEvent(BUS200E_EV_OVF);
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
  static uint32_t seen_isr = 0;
  const uint32_t isr_now = stats.isr_count;
  const bool slave_active = (isr_now != seen_isr);
  seen_isr = isr_now;
  const uint32_t xfer = Bus200eLastTransferMs();
  if (slave_active || now - last_rx_ms < 3000
      || (xfer && now - xfer < 5000)) {
    bbf_since_ms = now;
    return;
  }
  bus_stuck_recover();
}

void QueueMidiTx(uint8_t type, uint8_t channel, uint8_t d1, uint8_t d2) {
  if (!enabled) return;
  uint8_t status;
  if (type >= 0xF8) {
    status = type;
    d1 = d2 = 0;
  } else {
    const uint8_t mask = (channel >= 1 && channel <= 4)
                             ? uint8_t(0x8 >> (channel - 1)) : uint8_t(0xF);
    status = (type & 0xF0) | mask;
  }
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

FLASHMEM static void pump_midi_tx() {
  uint8_t sent = 0;
  uint32_t v = 0;
  for (;;) {
    __disable_irq();
    midi_tx.promote_realtime();
    __enable_irq();

    if (!midi_tx.peek(v) || sent >= 4) return;
    if (!tx_gate_open()) return;

    uint8_t f[9] = { 0x08, 0x00, 0x22, 0x0F,
                     uint8_t(v & 0xFF), 0x00,
                     uint8_t((v >> 8) & 0xFF), uint8_t((v >> 16) & 0xFF),
                     0x00 };

    Wire.beginTransmission(0);
    Wire.write(f, sizeof(f));
    const uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Bus200eSuppressFrame(f, sizeof(f));
      drain_ring();
    }

    if (err == 0) {
      midi_tx.pop();
      midi_tx_fails = 0;
      stats.midi_tx++;
      ++sent;
    } else {
      if (++midi_tx_fails >= 100) {
        midi_tx.pop();
        midi_tx_fails = 0;
        stats.midi_tx_drop = ++midi_tx.dropped;
      }
      return;
    }
  }
}

static uint8_t query_tries = 0;

FLASHMEM static void try_query_reply() {
  if (!tx_gate_open()) return;

  uint8_t f[5] = { 0x04, 0x22, Bus200eModuleAddress(), 0x1C, 0xFF };

  Wire.beginTransmission(0);
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) {
    Bus200eSuppressFrame(f, sizeof(f));
    drain_ring();
  }

  if (err == 0) {
    Bus200eClearQueryPending();
    query_tries = 0;
    stats.query_replies++;
    if (verbose) Serial.println("PresetBus: QUERY reply sent");
  } else {
    stats.query_retries++;
    if (++query_tries >= 5) {
      Bus200eClearQueryPending();
      query_tries = 0;
      if (verbose) Serial.printf("PresetBus: QUERY reply failed (%d)\n", err);
    }
  }
}

FLASHMEM void Init() {
  if (!I2C_Expansion) return;
  if (!OC::Buchla200eHardware()) return;

  Bus200eInit(&kOps);
  Bus200eMasterInit(&kMasterOps);

  uint64_t addr = 0;
  if (PhzConfig::getValue(kAddrKey, addr) && addr > 0 && addr < 0x78)
    Bus200eSetModuleAddress((uint8_t)addr);

  LPI2C1_SCR = LPI2C_SCR_RST;
  LPI2C1_SCR = 0;
  LPI2C1_SCFGR2 = LPI2C_SCFGR2_FILTSDA(2) | LPI2C_SCFGR2_FILTSCL(2)
                | LPI2C_SCFGR2_DATAVD(3) | LPI2C_SCFGR2_CLKHOLD(2);
  attachInterruptVector(IRQ_LPI2C1, lpi2c1_slave_isr);
  NVIC_SET_PRIORITY(IRQ_LPI2C1, 144);
  NVIC_ENABLE_IRQ(IRQ_LPI2C1);
  slave_reconfig(false);

  enabled = true;
  Serial.printf("PresetBus: slave listening on general call (module addr %02X)\n",
                Bus200eModuleAddress());
}

#define CARD_QUIET_MS          500
#define FOREIGN_XFER_WINDOW_MS 15000
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

  static uint32_t rate_t0 = 0, rate_l0 = 0;
  if (millis() - rate_t0 >= 500) {
    loop_rate_hz = (loop_counter - rate_l0) * 1000 / (millis() - rate_t0);
    rate_t0 = millis();
    rate_l0 = loop_counter;
  }

  drain_ring();
  if (LPI2C1_MSR & LPI2C_MSR_BBF) last_bbf_ms = millis() ? millis() : 1;
  card_task();

  if (Bus200eQueryPending()) try_query_reply();
  Bus200eTask();
  Bus200eMasterTask();
  Bus200eMasterQueryTask();
  report_query();
  pump_broadcast();
  pump_midi_tx();
  probe_wpm();
  bus_stuck_check();
}

bool Enabled() { return enabled; }
bool RemoteEnabled() { return Bus200eRemoteEnabled(); }

FLASHMEM void SetModuleAddress(uint8_t a) {
  Bus200eSetModuleAddress(a);
  PhzConfig::setValue(kAddrKey, Bus200eModuleAddress());
}

FLASHMEM void SetModuleAddressRuntime(uint8_t a) {
  Bus200eSetModuleAddress(a);
}
uint8_t ModuleAddress() { return Bus200eModuleAddress(); }
const Stats &GetStats() { return stats; }
void SetVerbose(bool on) { verbose = on; }
bool Verbose() { return verbose; }

FLASHMEM void DebugDump() {
  Serial.println("--- PresetBus ---");
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

}
}

#endif
