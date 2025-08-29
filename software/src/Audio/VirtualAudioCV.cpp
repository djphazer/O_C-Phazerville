#include "VirtualAudioCV.h"
#include <Audio.h>
#include <math.h> // optional clamp

//I WILL LIKELY EITHER DEPRECATE OR CHANGE THIS FILE'S FUNCTIONALITY
//VIRTUAL AUDIO CV INPUTS ARE DEFINED WITHIN HSIOFrame.h
//THEY WILL ALSO BE STORED SOMEHOW THROUGH THE IO-FRAME. STILL FIGURING THAT OUT

namespace VirtualAudioCV {

static Channel g_ch[VCV_NUM_CHANNELS];

// Optional: a tiny per-channel smoother state for read_smooth()
static float g_smooth[VCV_NUM_CHANNELS];

void init() {
  for (int i = 0; i < VCV_NUM_CHANNELS; ++i) {
    g_ch[i].value = 0.0f;
    g_ch[i].seq   = 0u;    // even => stable
    g_smooth[i]   = 0.0f;
  }
}

// --- Audio-rate writer ---
// seqlock pattern: bump seq to odd, write value, bump seq to even.
inline void publish(int ch, float v) {
  if ((unsigned)ch >= VCV_NUM_CHANNELS) return;

  // (Optional) quickly clamp to a sane range to avoid NaNs/Inf
  if (v != v) v = 0.0f; // NaN guard

  uint32_t s = g_ch[ch].seq;
  g_ch[ch].seq = s + 1u;     // mark "dirty" (odd)
  g_ch[ch].value = v;
  g_ch[ch].seq = s + 2u;     // mark "stable" (even)
}

// --- Control-rate reader ---
inline float read(int ch) {
  if ((unsigned)ch >= VCV_NUM_CHANNELS) return 0.0f;

  for (;;) {
    uint32_t s1 = g_ch[ch].seq;
    float v     = g_ch[ch].value;
    uint32_t s2 = g_ch[ch].seq;
    if ((s1 == s2) && ((s2 & 1u) == 0u)) {
      return v; // consistent, even seq => stable read
    }
    // else: writer raced us; spin a tiny bit (usually 0–1 iterations)
  }
}

float read_smooth(int ch, float alpha) {
  if ((unsigned)ch >= VCV_NUM_CHANNELS) return 0.0f;
  if (alpha < 0.0f) alpha = 0.0f;
  if (alpha > 1.0f) alpha = 1.0f;

  float target = read(ch);
  float prev   = g_smooth[ch];
  float v      = prev + alpha * (target - prev);
  g_smooth[ch] = v;
  return v;
}

} // namespace VirtualAudioCV