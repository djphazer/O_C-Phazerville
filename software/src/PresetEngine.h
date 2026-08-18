#pragma once
// ---------------------------------------------------------------------------
// Whole-module preset engine for the 200e preset bus (Xenomorpher, T4.1).
//
// 30 slots (bus presets 0-29), each a full snapshot of module state:
//  PB_NN_G.CFG  global settings values + slot manifest (PhzConfig format)
//  PB_NN_A.BIN  the per-app chunk stream (OC::AppData serialization + header)
//  PB_NN_B.DAT  active Quadrants preset extracted from its bank (preset keys
//               remapped to preset 0) + bank globals + audio applet graph
//  PB_NN_S.DAT / PB_NN_C.DAT  Scenery / Captain MIDI file copies
//
// Slots live on the SD card when present, else LittleFS (myfs) — matching
// the bank-file preference. Calibration is per-module and never in a preset.
//
// RequestSave/RequestRecall are ISR-safe (volatile, last-wins); the work runs
// from Process() in loop() context. Recall applies LIVE: validate, stop the
// app ISR, restore chunks + globals, switch app, RESUME under
// AudioNoInterrupts. See docs in the plan / bring-up notes.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace OC {
namespace PresetEngine {

#if defined(ARDUINO_TEENSY41)

static constexpr int kNumSlots = 30;

void Init();                  // call after app_switcher.Init (boot)
void Process();               // call from loop(); runs pending save/recall

// ISR-safe triggers (from the preset bus or anywhere)
void RequestSave(uint8_t slot);
void RequestRecall(uint8_t slot);

// Direct operations; loop() context only.
bool SaveSlot(uint8_t slot);
bool RecallSlot(uint8_t slot);

// Quadrants recall interplay: after a recall, Resume() consumes this hint
// (>= 0 = load this bank number fresh from disk, preset 0) exactly once.
int ConsumeQuadrantsRecallHint();

// status for UI / debug
int8_t LastSlot();            // -1 = none yet
bool LastWasSave();
bool Busy();

#else  // non-T4.1 builds: inert stubs

static constexpr int kNumSlots = 30;
inline void Init() {}
inline void Process() {}
inline void RequestSave(uint8_t) {}
inline void RequestRecall(uint8_t) {}
inline bool SaveSlot(uint8_t) { return false; }
inline bool RecallSlot(uint8_t) { return false; }
inline int ConsumeQuadrantsRecallHint() { return -1; }
inline int8_t LastSlot() { return -1; }
inline bool LastWasSave() { return false; }
inline bool Busy() { return false; }

#endif

}  // namespace PresetEngine
}  // namespace OC
