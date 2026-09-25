#pragma once
// ---------------------------------------------------------------------------
// Front-panel preset manager for the 200e preset bus, modeled on the Buchla
// 225e Preset Manager: a full-screen overlay entered by holding BOTH encoder
// buttons, with a big current-preset display, recall on the right encoder,
// store on a left-encoder long-press, and 225e-style last/next pulse inputs
// assignable to the module's trigger jacks (those stay live even when the
// overlay is closed — cycling recalls bus-wide, like the 225e's red jacks).
//
// All preset actions broadcast on the bus (commander mode), so the whole
// case changes preset together — this module included.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace UI { struct Event; }

namespace OC {
namespace PresetBusUI {

#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

void Init();                          // load trigger assignments (after PhzConfig)
bool Active();
void Enter();
void Exit();
bool HandleEvent(const UI::Event &);  // true = consumed (overlay active)
void Draw();                          // full-screen; call instead of app draw
void Task();                          // trigger polling; call from loop() always

#else

inline void Init() {}
inline bool Active() { return false; }
inline void Enter() {}
inline void Exit() {}
inline bool HandleEvent(const UI::Event &) { return false; }
inline void Draw() {}
inline void Task() {}

#endif

}  // namespace PresetBusUI
}  // namespace OC
