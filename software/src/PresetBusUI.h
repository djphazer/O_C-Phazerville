#pragma once
#include <stdint.h>

namespace UI { struct Event; }

namespace OC {
namespace PresetBusUI {

#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

void Init();
bool Active();
void Enter();
void Exit();
bool HandleEvent(const UI::Event &);
void Draw();
void Task();

#else

inline void Init() {}
inline bool Active() { return false; }
inline void Enter() {}
inline void Exit() {}
inline bool HandleEvent(const UI::Event &) { return false; }
inline void Draw() {}
inline void Task() {}

#endif

}
}
