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

namespace OC {
namespace PresetBus {

// module-address persistence key (PRESETBUS_KEY namespace, GLOBALS.CFG)
static constexpr uint16_t kAddrKey = (8 << 8) | 0x10;

// ---- SPSC event ring: ISR producer, Task() consumer ------------------------
static constexpr uint8_t kRingSize = 64;  // power of two
static volatile uint16_t ring[kRingSize];
static volatile uint8_t ring_w = 0;
static uint8_t ring_r = 0;
static volatile bool ring_ovf = false;

static Stats stats;
static bool enabled = false;
static bool verbose = false;
static uint32_t last_rx_ms = 0;

static void push_event(uint16_t ev) {
  if (uint8_t(ring_w - ring_r) >= kRingSize) {
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

// ---- parser callbacks into the preset engine -------------------------------
static void cb_save(uint8_t slot) { PresetEngine::RequestSave(slot); }
static void cb_recall(uint8_t slot) { PresetEngine::RequestRecall(slot); }

static const Bus200eOps kOps = {
  cb_save, cb_recall,
  0, nullptr, nullptr, nullptr, nullptr,  // card transfers: phase 2
};

// ---- QUERY reply (we briefly master the bus) -------------------------------
static uint8_t query_tries = 0;

FLASHMEM static void try_query_reply() {
  // only with a quiet bus: not mid-frame, controller idle, >=2ms since RX
  if (uint8_t(ring_w - ring_r) != 0) return;
  if (millis() - last_rx_ms < 2) return;
  if (LPI2C1_MSR & LPI2C_MSR_BBF) return;  // bus busy

  // [0A][22][ourAddr][13]["30.6"+3 spaces] — module identity / fw version
  uint8_t f[11] = { 0x0A, 0x22, Bus200eModuleAddress(), 0x13,
                    '3', '0', '.', '6', ' ', ' ', ' ' };

  // suppress self-RX while we hold the bus
  LPI2C1_SCR &= ~LPI2C_SCR_SEN;
  Wire.beginTransmission(0);  // general call
  Wire.write(f, sizeof(f));
  const uint8_t err = Wire.endTransmission();
  LPI2C1_SCR |= LPI2C_SCR_SEN;

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

void Task() {
  if (!enabled) return;

  if (ring_ovf) {
    ring_ovf = false;
    stats.ring_ovf++;
    Bus200eFeedEvent(BUS200E_EV_OVF);
  }

  bool got = false;
  while (ring_r != ring_w) {
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
    const uint32_t t0 = OC::CORE::ticks;
    delay(5);
    Serial.printf("core_ticks=%lu delta5ms=%lu display_en=%d app_isr=%d app_loop=%d\n",
                  OC::CORE::ticks, OC::CORE::ticks - t0,
                  OC::CORE::display_update_enabled, OC::CORE::app_isr_enabled,
                  OC::CORE::app_loop_enabled);
  }
  Serial.printf("enabled=%d remote=%d module_addr=%02X verbose=%d\n",
                enabled, Bus200eRemoteEnabled(), Bus200eModuleAddress(), verbose);
  Serial.printf("isr=%lu starts=%lu stops=%lu bytes=%lu ring_ovf=%lu\n",
                stats.isr_count, stats.starts, stats.stops, stats.bytes,
                stats.ring_ovf);
  const Bus200eStats *ps = Bus200eGetStats();
  Serial.printf("frames=%lu dropped=%lu query_tx=%lu query_retry=%lu\n",
                ps->frames, ps->dropped, stats.query_replies, stats.query_retries);
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
