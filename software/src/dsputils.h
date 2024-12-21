#pragma once
#include "extern/fastapprox/fastexp.h"
#include "extern/fastapprox/fastlog.h"
#include <arm_math.h>

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

inline q15_t float_to_q15(float x) {
  x *= 32768.0f;
  x += x > 0.0f ? 0.5f : -0.5f;
  return (q15_t)(__SSAT((q31_t)(x), 16));
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

void InterHermiteQ15Vec(
  q15_t* xm1,
  q15_t* x0,
  q15_t* x1,
  q15_t* x2,
  q15_t* t,
  q15_t* out,
  size_t size
) {
  q15_t c[size], v[size], w[size], a[size], b_neg[size];

  arm_sub_q15(x1, xm1, c, size);
  arm_scale_q15(c, 16384, 0, c, size);
  arm_sub_q15(x0, x1, v, size);
  arm_add_q15(c, v, w, size);

  arm_sub_q15(x2, x0, a, size);
  arm_scale_q15(a, 16384, 0, a, size);
  arm_add_q15(a, v, a, size);
  arm_add_q15(a, w, a, size);

  arm_add_q15(w, a, b_neg, size);

  arm_mult_q15(a, t, out, size);
  arm_sub_q15(out, b_neg, out, size);
  arm_mult_q15(out, t, out, size);
  arm_add_q15(out, c, out, size);
  arm_mult_q15(out, t, out, size);
  arm_add_q15(out, x0, out, size);
}

inline float dbToScalar(float db) {
  // pow10(x) = exp(log(10) * x)
  return fastexp(2.302585092994046f * 0.05f * db);
}

inline float scalarToDb(float scalar) {
  // log10(x) = log(x) / log(10)
  return 20.0f * 0.43429448190325176f * fastlog(scalar);
}

// Coefficients to use when equally mixing n sources for equal power
static const float EQUAL_POWER_EQUAL_MIX[]{
  1.0f,
  1.0f,
  0.7071067811865475f,
  0.5773502691896258f,
  0.5f,
  0.4472135954999579f,
  0.4082482904638631f,
  0.3779644730092272f,
  0.353553390593273731f
};

// From https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/
// Could use a LUT, but meh
inline void EqualPowerFade(float& fade_out, float& fade_in, float t) {
  float mt = 1.0f - t;
  float a = t * mt;
  float b = a * (1.0f + 1.4186f * a);
  float c = b + t;
  float d = b + mt;
  fade_out = d * d;
  fade_in = c * c;
}

inline float EqualPowerFadeBetween(float from, float to, float t) {
  float out;
  float in;
  EqualPowerFade(out, in, t);
  return out * from + in * to;
}

// The curve Stages uses for its envelopes:
// https://github.com/pichenettes/eurorack/blob/08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4/stages/segment_generator.cc#L118
// Gives a nice sublog -> log -> lin -> exp -> supexp approximation for
// envelopes t *must* be between 0 and 1 curve: 0 -> supexp/accelerating, 0.5 ->
// linear, 1.0 -> sublog/decelerating
inline float WarpPhase(float t, float curve) {
  curve -= 0.5f;
  const bool flip = curve < 0.0f;
  if (flip) {
    t = 1.0f - t;
  }
  const float a = 128.0f * curve * curve;
  t = (1.0f + a) * t / (1.0f + a * t);
  if (flip) {
    t = 1.0f - t;
  }
  return t;
}

