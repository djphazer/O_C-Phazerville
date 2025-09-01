// src/Audio/VirtualAudioCV.cpp
#include "Audio/VirtualAudioCV.h"

// We store values as Q15 (0..65535). No heap, no audio blocks.
static volatile uint16_t s_q15[VirtualAudioCV::kChannels] = {0};

namespace VirtualAudioCV {

// Voltage/float conversion
static inline uint16_t f_to_q15(float v01) {
  if (v01 <= 0.0f) return 0;
  if (v01 >= 1.0f) return 65535u;
  return static_cast<uint16_t>(v01 * 65535.0f + 0.5f);
}
static inline float q15_to_f(uint16_t q) {
  return static_cast<float>(q) / 65535.0f;
}

void set(uint8_t ch, float v01) {
  if (ch >= kChannels) return;
  uint16_t q = f_to_q15(v01);
  __disable_irq();
  s_q15[ch] = q;
  __enable_irq();
}

float read(uint8_t ch) {
  if (ch >= kChannels) return 0.0f;
  uint16_t q;
  __disable_irq();
  q = s_q15[ch];
  __enable_irq();
  return q15_to_f(q);
}

// // O&C uses -3V..+6V on inputs; map that linearly to 0..1 to keep RawIn() scaling identical.
// void setVolts(uint8_t ch, float volts) {
//   // Clamp -3..+6 V, then map to 0..1
//   if (ch >= kChannels) return;
//   float v = volts;
//   if (v < -3.0f) v = -3.0f;
//   if (v >  6.0f) v =  6.0f;
//   set(ch, (v + 3.0f) / 9.0f);
// }

void clear(uint8_t ch) {
  if (ch >= kChannels) return;
  __disable_irq();
  s_q15[ch] = 0;
  __enable_irq();
}

void clearAll() {
  __disable_irq();
  for (uint8_t i = 0; i < kChannels; ++i) s_q15[i] = 0;
  __enable_irq();
}

} // namespace VirtualAudioCV