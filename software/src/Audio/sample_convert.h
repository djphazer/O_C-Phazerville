#pragma once
// Pure sample-format conversion helpers shared by the USB F32 bridge and
// host-side unit tests (no Arduino/AudioStream dependencies — keep it that
// way so software/test/test_sample_convert.cpp keeps compiling on the host).
#include <stdint.h>
#include <math.h>

namespace samplefmt {

inline int32_t sat24(int32_t v) {
  if (v > 8388607) return 8388607;
  if (v < -8388608) return -8388608;
  return v;
}

inline int32_t sat16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return v;
}

// 24-bit little-endian -> float in [-1, 1)
inline float f32_from_i24le(const uint8_t* src) {
  int32_t v = (int32_t)((uint32_t)src[0] << 8 | (uint32_t)src[1] << 16 | (uint32_t)src[2] << 24) >> 8;
  return (float)v * (1.0f / 8388608.0f);
}

// float -> 24-bit little-endian, saturating
inline void f32_to_i24le(float f, uint8_t* dst) {
  int32_t v = sat24((int32_t)lrintf(f * 8388608.0f));
  dst[0] = (uint8_t)(v & 255);
  dst[1] = (uint8_t)((v >> 8) & 255);
  dst[2] = (uint8_t)((v >> 16) & 255);
}

inline float f32_from_i16(int16_t s) {
  return (float)s * (1.0f / 32768.0f);
}

inline int16_t f32_to_i16(float f) {
  return (int16_t)sat16((int32_t)lrintf(f * 32768.0f));
}

} // namespace samplefmt
