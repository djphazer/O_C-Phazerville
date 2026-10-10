#pragma once
// The float <-> integer boundary of the audio path, as pure arithmetic so it
// can be tested exhaustively (test/test_f32_int_convert.cpp).
//
// Extracted from extern/f32/AudioConvert_F32.h, which scaled F32->int16 by
// 32678 while scaling int16->F32 by 32768 -- a transposed digit inherited
// from upstream OpenAudio. That is usually described as a 0.9972x gain error
// (-0.0239 dB), which undersells it: because the two directions disagree,
// 65,535 of the 65,536 possible int16 values did not survive a codec round
// trip through the int16 applet bus intact. It is a per-sample quantisation
// error, not just a level offset.
//
// The naive correction is a bug, which is why this sat untouched for a
// while: the old code used ONE constant for both the scale and the clamp, so
// it was at least self-consistent and never overflowed. Raising the scale to
// 32768 while leaving a symmetric clamp sends full scale to 32768, which
// does not fit an int16 and wraps to -32768 -- an audible click at exactly
// the loudest moment, far worse than the gain error it fixes. The limits
// have to be asymmetric, matching the integer range itself.

#include <stdint.h>

namespace f32conv {

// int16 -> float, the direction that was always right.
// -32768 maps to exactly -1.0f.
inline float from_i16(int16_t v) { return (float)v / 32768.0f; }

// float -> int16. Unity scale, asymmetric clamp.
//
// 32768.0f is exactly representable, so +1.0f scales to exactly 32768.0f and
// is caught by the upper clamp; -1.0f scales to exactly -32768.0f, which IS
// a valid int16 and passes through untouched.
inline int16_t to_i16(float x) {
  const float s = x * 32768.0f;
  if (s >= 32767.0f) return 32767;
  if (s <= -32768.0f) return -32768;
  return (int16_t)s;
}

// float -> int32, for the 24-bit / high-low split path.
//
// The extra trap here: 2^31-1 is NOT representable as a float. The nearest
// floats either side of it are 2147483520 and 2147483648, so a clamp written
// as `min(s, 2147483647.0f)` silently becomes `min(s, 2147483648.0f)` and
// overflows exactly as before. The largest float strictly below 2^31 is
// 2147483520, and that is the honest positive limit. The negative side has
// no such problem: -2^31 is a power of two, exactly representable, and
// exactly INT32_MIN.
inline int32_t to_i32(float x) {
  const float s = x * 2147483648.0f;
  if (s >= 2147483520.0f) return 2147483520;
  if (s <= -2147483648.0f) return -2147483647 - 1;  // INT32_MIN, no <limits>
  return (int32_t)s;
}

}  // namespace f32conv
