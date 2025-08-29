#pragma once
#include <stdint.h>

// Tune this count to your needs; 8 is a good starting point.
#ifndef VCV_NUM_CHANNELS
#define VCV_NUM_CHANNELS 8
#endif

namespace VirtualAudioCV {

// One float per channel; seq is used for lock-free, consistent reads.
struct Channel {
  volatile float   value;   // recommended range: -1..+1 or 0..1
  volatile uint32_t seq;    // seqlock counter (increment after write)
};

// Initialize all channels to 0 and seq to even.
void init();

// Audio-rate publish (callable from audio ISR / update())
inline void publish(int ch, float v);

// Non-blocking read (safe from CV/control loop). Returns a stable snapshot.
inline float read(int ch);

// Read with simple one-pole smoothing (alpha in 0..1; higher = snappier).
float read_smooth(int ch, float alpha);

} // namespace VirtualAudioCV