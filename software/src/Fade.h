#ifndef FADE_H_
#define FADE_H_

#include <math.h>
#include <stdint.h>

namespace Fade {

inline float raised_cosine(float t) {
  if (t <= 0.0f) return 1.0f;
  if (t >= 1.0f) return 0.0f;
  return 0.5f * (1.0f + cosf((float)M_PI * t));
}

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

}

#endif
