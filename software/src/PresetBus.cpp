// 200e preset-bus transport (LPI2C1 general-call slave). See PresetBus.h.
#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

#include <Arduino.h>
#include <Wire.h>
#include <imxrt.h>

#include "PresetBus.h"
#include "PresetBus200e.h"
#include "PresetEngine.h"
#include "OC_gpio.h"
#include "OC_core.h"
#include "PhzConfig.h"

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
  if (uint16_t(uint8_t(ring_w - ring_r)) >= kRingSize) {
    ring_ovf = true;  // drop; Task() poisons the frame
    return;
  }
  ring[ring_w & (kRingSize - 1)] = ev;
  ring_w = ring_w + 1;
}

// ---- LPI2C1 slave ISR ------------------------------------------------------
static void lpi2c1_slave_isr() {
  stats.isr_count++;
  const uint32_t status = LPI2C1_SSR;
  const uint32_t w1c = status & 0xF00;
  if (w1c) LPI2C1_SSR = w1c;

  if (status & LPI2C_SSR_RDF) {
    const uint32_t rx = LPI2C1_SRDR;
    if (rx & LPI2C_SRDR_SOF) {  // start of frame
      push_event(BUS200E_EV_START);
      stats.starts++;
    }
    push_event(rx & 0xFF);
    stats.bytes++;
  }
  if (status & (LPI2C_SSR_SDF | LPI2C_SSR_RSF)) {  // stop / repeated start
    push_event(BUS200E_EV_STOP);
    if (status & LPI2C_SSR_RSF) push_event(BUS200E_EV_START);
    stats.stops++;
  }
}

// ---- bus MIDI rings ---------------------------------------------------------
// RX: producer = Task() (loop, via parser callback), consumer = the active
// app's MIDI poll (Quadrants: loop; Captain: app ISR). One producer, one
// consumer -- plain SPSC.
// TX: producers = app ISR (engine sends) AND loop (thru), so pushes are
// briefly IRQ-masked. Consumer = Task() (loop).
static constexpr uint8_t kMidiRing = 32;  // power of two
static volatile uint32_t midi_rx_q[kMidiRing];
static volatile uint8_t midi_rx_w = 0;
static uint8_t midi_rx_r = 0;
static volatile uint32_t midi_tx_q[kMidiRing];
static volatile uint8_t midi_tx_w = 0;
static uint8_t midi_tx_r = 0;
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
}

static const Bus200eOps kOps = {
  cb_save, cb_recall,
  0, nullptr, nullptr, nullptr, nullptr,  // card transfers: phase 2
  cb_midi,
};

static bool tx_gate_open();  // defined with Task() below

// ---- commander mode: bus-wide preset commands -------------------------------
// One pending command, last wins (matches the engine's own request model).
static volatile int16_t pending_bcast = -1;  // (cmd << 8) | slot, cmd 01/02
static uint8_t bcast_tries = 0;
static uint32_t bcast_tx = 0, bcast_drop = 0;

FLASHMEM void BroadcastSave(uint8_t slot) {
  if (slot < 30) pending_bcast = (0x02 << 8) | slot;
}
FLASHMEM void BroadcastRecall(uint8_t slot) {
  if (slot < 30) pending_bcast = (0x01 << 8) | slot;
}

FLASHMEM static void pump_broadcast() {
  const int16_t cmd = pending_bcast;
  if (cmd < 0) return;
  if (!tx_gate_open()) return;

  // same long/PRIMO frame a preset manager sends. The slave stays ENABLED
  // during TX: if we lose arbitration the winner's frame (maybe the WPM's
  // one-shot recall) must still be heard; on success the parser drops our
  // own echo via Bus200eSuppressFrame.
  uint8_t f[5] = { 0x04, 0x00, 0x22, uint8_t(cmd >> 8), uint8_t(cmd & 0x1F) };

  Wire.beginTransmission(0);
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) Bus200eSuppressFrame(f, sizeof(f));

  if (err == 0) {
    pending_bcast = -1;
    bcast_tries = 0;
    bcast_tx++;
    // our slave never hears our own TX: dispatch locally so this module
    // saves/recalls in lockstep with the rest of the bus
    if ((cmd >> 8) == 0x02) PresetEngine::RequestSave(cmd & 0x1F);
    else PresetEngine::RequestRecall(cmd & 0x1F);
    if (verbose) Serial.printf("PresetBus: broadcast %s %d\n",
                               (cmd >> 8) == 0x02 ? "SAVE" : "RECALL",
                               cmd & 0x1F);
  } else if (++bcast_tries >= 50) {  // persistent contention: give up loudly
    pending_bcast = -1;
    bcast_tries = 0;
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
  // loop never calls this with interrupts already masked.)
  __disable_irq();
  if (uint8_t(midi_tx_w - midi_tx_r) >= kMidiRing) {
    stats.midi_tx_drop++;
  } else {
    midi_tx_q[midi_tx_w & (kMidiRing - 1)] =
        uint32_t(status) | (uint32_t(d1) << 8) | (uint32_t(d2) << 16);
    midi_tx_w = midi_tx_w + 1;
  }
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
  while (midi_tx_r != midi_tx_w && sent < 4) {
    if (!tx_gate_open()) return;

    const uint32_t v = midi_tx_q[midi_tx_r & (kMidiRing - 1)];
    // [08][00][22][0F][status|mask][00][d1][d2][00] -- 2WIRELESS long format
    uint8_t f[9] = { 0x08, 0x00, 0x22, 0x0F,
                     uint8_t(v & 0xFF), 0x00,
                     uint8_t((v >> 8) & 0xFF), uint8_t((v >> 16) & 0xFF),
                     0x00 };

    Wire.beginTransmission(0);
    Wire.write(f, sizeof(f));
    const uint8_t err = Wire.endTransmission();
    if (err == 0) Bus200eSuppressFrame(f, sizeof(f));

    if (err == 0) {
      midi_tx_r = midi_tx_r + 1;
      midi_tx_fails = 0;
      stats.midi_tx++;
      ++sent;
    } else {
      // arbitration loss etc: retry next Task() pass; give up eventually
      if (++midi_tx_fails >= 100) {
        midi_tx_r = midi_tx_r + 1;
        midi_tx_fails = 0;
        stats.midi_tx_drop++;
      }
      return;
    }
  }
}

// ---- QUERY reply (we briefly master the bus) -------------------------------
static uint8_t query_tries = 0;

FLASHMEM static void try_query_reply() {
  if (!tx_gate_open()) return;

  // [0A][22][ourAddr][13]["30.6"+3 spaces] — module identity / fw version
  uint8_t f[11] = { 0x0A, 0x22, Bus200eModuleAddress(), 0x13,
                    '3', '0', '.', '6', ' ', ' ', ' ' };

  Wire.beginTransmission(0);  // general call
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  if (err == 0) Bus200eSuppressFrame(f, sizeof(f));

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

  // persisted module address (GLOBALS.CFG must be the loaded map here)
  uint64_t addr = 0;
  if (PhzConfig::getValue(kAddrKey, addr) && addr > 0 && addr < 0x78)
    Bus200eSetModuleAddress((uint8_t)addr);

  // LPI2C1 slave engine, alongside the (polled) stock Wire master.
  // Wire.begin() has already gated the peripheral clock and set the pads.
  LPI2C1_SCR = LPI2C_SCR_RST;
  LPI2C1_SCR = 0;
  // GCEN: match the general call (0x00) — the bit neither Wire nor
  // teensy4_i2c ever sets, and the whole reason this block exists.
  // RXSTALL lets the peripheral clock-stretch if we fall behind.
  LPI2C1_SCFGR1 = LPI2C_SCFGR1_GCEN | LPI2C_SCFGR1_RXSTALL;
  LPI2C1_SCFGR2 = LPI2C_SCFGR2_FILTSDA(2) | LPI2C_SCFGR2_FILTSCL(2)
                | LPI2C_SCFGR2_DATAVD(3) | LPI2C_SCFGR2_CLKHOLD(2);
  LPI2C1_SAMR = LPI2C_SAMR_ADDR0(0);  // belt and braces alongside GCEN
  attachInterruptVector(IRQ_LPI2C1, lpi2c1_slave_isr);
  NVIC_SET_PRIORITY(IRQ_LPI2C1, 144);  // below CORE (80) and UI (128)
  NVIC_ENABLE_IRQ(IRQ_LPI2C1);
  LPI2C1_SIER = LPI2C_SIER_RDIE | LPI2C_SIER_SDIE | LPI2C_SIER_RSIE;
  LPI2C1_SCR = LPI2C_SCR_SEN | LPI2C_SCR_FILTEN;

  enabled = true;
  Serial.printf("PresetBus: slave listening on general call (module addr %02X)\n",
                Bus200eModuleAddress());
}

// Master-TX gate shared by every pump: quiet bus AND no card-transfer
// window open. A preset manager's FRAM backup/restore swallows every byte
// on the bus (receiveEvent in fram mode), so any TX during the window
// corrupts the user's backup; hold off until well past its 1s done-detect.
static bool tx_gate_open() {
  if (uint8_t(ring_w - ring_r) != 0) return false;
  if (millis() - last_rx_ms < 2) return false;
  const uint32_t t = Bus200eLastTransferMs();
  if (t && millis() - t < 1500) return false;
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

  if (ring_ovf) {
    ring_ovf = false;
    stats.ring_ovf++;
    Bus200eFeedEvent(BUS200E_EV_OVF);
  }

  bool got = false;
  while (ring_r != ring_w) {
    if (ring_ovf) {  // overflow mid-drain: poison before the gap parses
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

  if (Bus200eQueryPending()) try_query_reply();
  Bus200eTask();   // card-transfer job engine (self-clears with null hooks)
  pump_broadcast();
  pump_midi_tx();
  probe_wpm();
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
                  wpm_present ? "WPM" : "none",
                  dialect, d->frames_long, d->frames_short, wpm_probes);
    Serial.printf("bcast: tx=%lu drop=%lu pending=%d\n",
                  bcast_tx, bcast_drop, pending_bcast);
  }
  Serial.printf("isr=%lu starts=%lu stops=%lu bytes=%lu ring_ovf=%lu\n",
                stats.isr_count, stats.starts, stats.stops, stats.bytes,
                stats.ring_ovf);
  const Bus200eStats *ps = Bus200eGetStats();
  Serial.printf("frames=%lu dropped=%lu query_tx=%lu query_retry=%lu\n",
                ps->frames, ps->dropped, stats.query_replies, stats.query_retries);
  Serial.printf("midi: rx=%lu rx_ovf=%lu tx=%lu tx_drop=%lu\n",
                stats.midi_rx, stats.midi_rx_ovf, stats.midi_tx,
                stats.midi_tx_drop);
  Serial.printf("engine: last_slot=%d was_save=%d busy=%d\n",
                PresetEngine::LastSlot(), PresetEngine::LastWasSave(),
                PresetEngine::Busy());
  static const char *const opnames[] = {
    "none", "RECALL", "SAVE", "REMOTE_EN", "REMOTE_DIS", "POLL_DONE",
    "QUERY", "BACKUP", "RESTORE", "MIDI", "CLOCK", "UNKNOWN", "DROPPED",
  };
  const uint32_t total = Bus200eLogTotal();
  Serial.printf("decoded commands (%lu total, newest first):\n", total);
  Bus200eCmd c;
  for (uint32_t i = 0; i < 10 && Bus200eLogRead(i, &c); ++i) {
    Serial.printf("  %-10s arg=%u mod=%02X card=%02X off=%04X\n",
                  c.op <= 12 ? opnames[c.op] : "?", c.arg, c.mod_addr,
                  c.card_lo, c.mem_off);
  }
}

}  // namespace PresetBus
}  // namespace OC

#endif  // ARDUINO_TEENSY41 && PRESET_BUS
