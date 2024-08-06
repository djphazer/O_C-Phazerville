#pragma once

#include "extern/fastapprox/fastexp.h"
#include "extern/fastapprox/fastlog.h"

inline float RatioToSemitones(float ratio) { return 12.0f * fastlog2(ratio); }

inline int16_t RatioToPitch(float ratio) {
  return 128.0f * RatioToSemitones(ratio);
}

inline float SemitonesToRatio(float semitones) {
  return fastpow2(semitones / 12.0f);
}

inline float PitchToRatio(int16_t pitch) {
  return SemitonesToRatio(pitch / 128.0f);
}

// for convenience
const float Cn3 = 2.0439497f;
const float C0 = Cn3 * SemitonesToRatio(3.0f * 12.0f);
const float C3 = C0 * SemitonesToRatio(3.0f * 12.0f);

inline int32_t Clip16(int32_t x) {
  if (x < -32768) {
    return -32768;
  } else if (x > 32767) {
    return 32767;
  } else {
    return x;
  }
}
