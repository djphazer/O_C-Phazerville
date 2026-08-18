#pragma once
// ---------------------------------------------------------------------------
// 200e preset-bus transport: LPI2C1 slave on the Xenomorpher's I2C header
// (pins 18=SDA / 19=SCL, shared with the stock Wire master).
//
// Neither stock WireIMXRT nor teensy4_i2c can listen on the I2C general
// call (SCFGR1[GCEN] unused in both) — and every 200e command broadcasts on
// address 0x00 — so this is a small direct-register slave block on LPI2C1.
// The LPI2C master and slave engines are independent; stock Wire's master
// (fully polled, no IRQ use) keeps working for ScanI2C and our QUERY reply.
//
// ISR work is minimal: bus bytes/START/STOP land in an SPSC event ring.
// Task() (loop context) drains the ring into the Bus200e parser, dispatches
// save/recall into the PresetEngine, and answers QUERY by mastering the
// reply frame when the bus has been quiet.
//
// HARDWARE PREREQUISITE: the bus is 5V with system-side pull-ups; the
// i.MX RT pins are NOT 5V tolerant. A bidirectional level shifter
// (PCA9306 / 2x BSS138) is mandatory between the header and EDAC pins 8/9.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace OC {
namespace PresetBus {

struct Stats {
  uint32_t isr_count;
  uint32_t starts;
  uint32_t stops;
  uint32_t bytes;
  uint32_t ring_ovf;       // events lost (frame poisoned downstream)
  uint32_t query_replies;
  uint32_t query_retries;
};

#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

void Init();               // call after Wire.begin() and PhzConfig load
void Task();               // call from loop()

bool Enabled();            // Init ran (I2C_Expansion hardware present)
bool RemoteEnabled();      // bus remote-enable state (parser)
void SetModuleAddress(uint8_t a);  // payload address; persisted by caller
void SetModuleAddressRuntime(uint8_t a);  // live only, no config write
uint8_t ModuleAddress();
const Stats &GetStats();
void DebugDump();          // print status + decoded-command ring to Serial
void SetVerbose(bool on);
bool Verbose();

#else

inline void Init() {}
inline void Task() {}
inline bool Enabled() { return false; }
inline bool RemoteEnabled() { return false; }
inline void SetModuleAddress(uint8_t) {}
inline void SetModuleAddressRuntime(uint8_t) {}
inline uint8_t ModuleAddress() { return 0; }
inline const Stats &GetStats() { static Stats s = {}; return s; }
inline void DebugDump() {}
inline void SetVerbose(bool) {}
inline bool Verbose() { return false; }

#endif

}  // namespace PresetBus
}  // namespace OC
