#include <stdint.h>

// Provide fallback multiply helper if not available to avoid editing util_profiling.h.
// util_profiling.h calls multiply_u32xu32_rshift32(...) — define it here so the
// header can compile when included below.
#ifndef multiply_u32xu32_rshift32
static inline uint32_t multiply_u32xu32_rshift32(uint32_t a, uint32_t b) {
  return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
}
#endif

#include <Audio.h>
#include <math.h> // optional clamp
#include "HSIOFrame.h" 
#include "VirtualAudioCV.h"

//I WILL LIKELY EITHER DEPRECATE OR CHANGE THIS FILE'S FUNCTIONALITY
//VIRTUAL AUDIO CV INPUTS ARE DEFINED WITHIN HSIOFrame.h
//THEY WILL ALSO BE STORED SOMEHOW THROUGH THE IO-FRAME. STILL FIGURING THAT OUT
static constexpr int VACV_CHANNEL_COUNT = HS::VACV_CHANNEL_COUNT;

namespace VirtualAudioCV {

static Channel g_ch[VACV_CHANNEL_COUNT];
// Optional: a tiny per-channel smoother state for read_smooth()
static float g_smooth[VACV_CHANNEL_COUNT];

void init() {
  for (int i = 0; i < VACV_CHANNEL_COUNT; ++i) {
    g_ch[i].value = 0.0f;
    g_ch[i].seq   = 0u;    // even => stable
    snapshot[i] = 0;
    g_smooth[i]   = 0.0f;
  }
}

// --- Audio-rate writer ---
// seqlock pattern: bump seq to odd, write value, bump seq to even.
void update(int ch, float v) {
  if ((unsigned)ch >= VACV_CHANNEL_COUNT) return;

  // (Optional) quickly clamp to a sane range to avoid NaNs/Inf
  if (v != v) v = 0.0f; // NaN guard

  uint32_t s = g_ch[ch].seq;
  g_ch[ch].seq = s + 1u;     // mark "dirty" (odd)
  g_ch[ch].value = v;
  g_ch[ch].seq = s + 2u;     // mark "stable" (even)
}

// --- Control-rate reader ---
  // returns -1..+1 full-scale
  float read(size_t vacv_index) {
    // produce a normalized value here from the correct VACV channel.
    // e.g. if you compute sample -> value in -1..1, return it directly.
    // Avoid heavy locking; if you must access audio buffers, use a lock-free snapshot.
    return current_vacv_value[vacv_index]; // float in [-1, 1]
  }

float read_smooth(int ch, float alpha) {
  if ((unsigned)ch >= VACV_CHANNEL_COUNT) return 0.0f;
  if (alpha < 0.0f) alpha = 0.0f;
  if (alpha > 1.0f) alpha = 1.0f;

  float target = read(ch);
  float prev   = g_smooth[ch];
  float v      = prev + alpha * (target - prev);
  g_smooth[ch] = v;
  return v;
}

// convenience accessor
inline constexpr int channel_count() { return VACV_CHANNEL_COUNT; }

} // namespace VirtualAudioCV