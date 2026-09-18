#ifndef FADE_H_
#define FADE_H_

#include <math.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// The fade a declared persistence window uses.
//
// Some writes to the program flash cannot be avoided and cannot be made
// quick: the Teensy core masks interrupts for the whole erase/program
// because code executes from the same chip. Audio, USB, the display and the
// bus slave all stop for the duration. Left alone the DAC replays whatever
// the DMA buffer last held, which is heard as a click into a frozen tone.
//
// So an unavoidable write is wrapped in a window that takes the audio down
// to true silence first and brings it back afterwards. A raised cosine, not
// a straight line: the ends are what a listener hears, and this one leaves
// and arrives at zero slope. Endpoints are exact -- a fade that only nearly
// reaches zero leaves a DC step for the buffer-zeroing to turn into the very
// click the fade exists to avoid.
//
// BSP-free and host-tested (test/test_fade.cpp).
// ---------------------------------------------------------------------------
namespace Fade {

// Gain at position `t` through the fade, 0 = start (full), 1 = end (silent).
// Positions outside [0,1] clamp, so a caller that overshoots its own ramp
// cannot produce a gain above unity or below zero.
inline float raised_cosine(float t) {
  if (t <= 0.0f) return 1.0f;
  if (t >= 1.0f) return 0.0f;
  return 0.5f * (1.0f + cosf((float)M_PI * t));
}

// A ramp measured in whole milliseconds, which is what the window has to
// work in: it steps the gain from loop context between the audio blocks the
// ISR is still producing.
struct Ramp {
  uint32_t len_ms;
  bool to_silence;

  void start(uint32_t ms, bool silence) {
    len_ms = ms;
    to_silence = silence;
  }

  bool done(uint32_t elapsed_ms) const { return elapsed_ms >= len_ms; }

  float gain_at(uint32_t elapsed_ms) const {
    if (!len_ms) return to_silence ? 0.0f : 1.0f;
    const float t = (float)elapsed_ms / (float)len_ms;
    const float g = raised_cosine(t);
    return to_silence ? g : 1.0f - g;
  }
};

}  // namespace Fade

#endif  // FADE_H_
