#pragma once
#include "extern/fastapprox/fastexp.h"
#include "extern/fastapprox/fastlog.h"

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);

inline float RatioToSemitones(float ratio) {
  return 12.0f * fastlog2(ratio);
}

inline int16_t RatioToPitch(float ratio) {
  return static_cast<int16_t>(128.0f * RatioToSemitones(ratio));
}

inline float SemitonesToRatio(float semitones) {
  return fastpow2(semitones / 12.0f);
}

// TODO: This is less accurate the LUT used in tideslite so might want to just
// use those instead when performance isn't critical
inline float PitchToRatio(int16_t pitch) {
  return SemitonesToRatio(static_cast<float>(pitch) / 128.0f);
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

inline int16_t Clip16(float x) {
  if (x < -32768.0f) {
    return -32768;
  } else if (x > 32767.0f) {
    return 32767;
  } else {
    return static_cast<int16_t>(x);
  }
}

constexpr inline float InterpLinear(float x0, float x1, float t) {
  return t * (x1 - x0) + x0;
}

constexpr inline float InterpHermite(
  float xm1, float x0, float x1, float x2, float t
) {
  // https://github.com/pichenettes/stmlib/blob/d18def816c51d1da0c108236928b2bbd25c17481/dsp/dsp.h#L52
  const float c = (x1 - xm1) * 0.5f;
  const float v = x0 - x1;
  const float w = c + v;
  const float a = w + v + (x2 - x0) * 0.5f;
  const float b_neg = w + a;
  return ((((a * t) - b_neg) * t + c) * t + x0);
}
