#pragma once
#include <Arduino.h>

namespace VirtualAudioCV {

// Keep this internal; CVInputMap bounds with VACV_CHANNEL_COUNT in HSIOFrame.h
#if defined(ARDUINO_TEENSY41)
  static constexpr uint8_t kChannels = 8;
#else
  static constexpr uint8_t kChannels = 0;
#endif

// Write a normalized CV value [0..1] for channel ch
void set(uint8_t ch, float v01);

// Read back the normalized CV value [0..1] for channel ch
float read(uint8_t ch);

// Optional helpers (no-ops if ch out of range)
void setVolts(uint8_t ch, float volts);  // -3..+6 V mapped to 0..1 like CVInputMap does
void clear(uint8_t ch);
void clearAll();

} // namespace VirtualAudioCV