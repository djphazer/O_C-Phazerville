// Copyright (c) 2016 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef UTIL_MATH_H_
#define UTIL_MATH_H_

#include "util_macros.h"

// Simulated fixed floats by multiplying and dividing by powers of 2
#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
using simfloat = int32_t;
#endif

/* Proportion method using simfloat, useful for calculating scaled values given
 * a fractional value.
 *
 * Solves this:  numerator        ???
 *              ----------- = -----------
 *              denominator       max
 *
 * For example, to convert a parameter with a range of 1 to 100 into value scaled
 * to HEMISPHERE_MAX_CV, to be sent to the DAC:
 *
 * Out(ch, Proportion(value, 100, HEMISPHERE_MAX_CV));
 *
 */
constexpr int Proportion(const int numerator, const int denominator, const int max_value) {
    simfloat proportion = int2simfloat((int32_t)abs(numerator)) / (int32_t)denominator;
    int scaled = simfloat2int(proportion * max_value);
    return numerator >= 0 ? scaled : -scaled;
}

// Given a bipolar param value from -127 to +128, returns a scalar value in increments of 0.1%
constexpr int Atten(int8_t att) {
  // exponential curve; 60 becomes 100.0%
  return 10 * att * abs(att) / 36;
}

// Woo. Funky macro magic to avoid dividing by non-power-of-two.
// Essentially a quick fixed-point calculation, but only valid up to 2^exp

#define FAST_FP_DIV(n, div, exp) \
  (((n) * (((1 << exp) + 1) / div)) >> exp)

#define FAST_FP_MOD(n, div, exp) \
  ((n) - FAST_FP_DIV(n, div, exp) * div)

#define DIV_8(n, div) \
  FAST_FP_DIV(n, div, 8)

#define MOD_8(n, div) \
  FAST_FP_MOD(n, div, 8)

inline int32_t SSAT16(int32_t value) __attribute__((always_inline));
inline int32_t SSAT16(int32_t value) {
  int32_t result;
  __asm("ssat %0, %1, %2" : "=r" (result) : "I" (16),  "r" (value));
  return result;
}

inline uint32_t USAT16(uint32_t value) __attribute__((always_inline));
inline uint32_t USAT16(uint32_t value) {
  uint32_t result;
  __asm("usat %0, %1, %2" : "=r" (result) : "I" (16), "r" (value));
  return result;
}

inline uint32_t USAT16(int32_t value) __attribute__((always_inline));
inline uint32_t USAT16(int32_t value) {
  uint32_t result;
  __asm("usat %0, %1, %2" : "=r" (result) : "I" (16), "r" (value));
  return result;
}

static inline uint32_t multiply_u32xu32_rshift24(uint32_t a, uint32_t b) __attribute__((always_inline));
static inline uint32_t multiply_u32xu32_rshift24(uint32_t a, uint32_t b)
{
  uint32_t lo, hi;
  asm volatile("umull %0, %1, %2, %3" : "=r" (lo), "=r" (hi) : "r" (a), "r" (b));
  return (lo >> 24) | (hi << 8);
}

static inline uint32_t multiply_u32xu32_rshift(uint32_t a, uint32_t b, uint32_t shift) __attribute__((always_inline));
static inline uint32_t multiply_u32xu32_rshift(uint32_t a, uint32_t b, uint32_t shift)
{
  uint32_t lo, hi;
  asm volatile("umull %0, %1, %2, %3" : "=r" (lo), "=r" (hi) : "r" (a), "r" (b));
  return (lo >> shift) | (hi << (32 - shift));
}


static inline uint32_t uhadd16(uint32_t a, uint32_t b) __attribute__((always_inline, unused));
static inline uint32_t uhadd16(uint32_t a, uint32_t b)
{
  uint32_t out;
  asm volatile("uhadd16 %0, %1, %2" : "=r" (out) : "r" (a), "r" (b));
  return out;
}

template <typename T, T smoothing>
struct SmoothedValue {
  SmoothedValue() : value_(0) { }

  T value_;

  T value() const {
    return value_;
  }

  void push(T value) {
    value_ = (value_ * (smoothing - 1) + value) / smoothing;
  }

  void set(T value) {
    value_ = value;
  }
};

struct SlewedValue {
  static constexpr int EXTRA_PRECISION = 4;
  int target_, value_;

  void set(int val, bool override = false) {
    target_ = val << EXTRA_PRECISION;
    if (override) value_ = target_;
  }

  void push(uint8_t slew) {
    if (slew) {
      int diff = target_ - value_;
      int delta = 1;
      if (slew <= 50)
        delta += 250 - 4 * slew;
      else
        delta += 100 - slew;
      CONSTRAIN(delta, 0, abs(diff));
      if (diff < 0) delta = -delta;
      value_ += delta;
    } else
      value_ = target_;
  }

  // attenuverted getter
  int get(int8_t atten) const {
    // exponential curve; 60 becomes 100.0%; 127 == 448%
    return get() * Atten(atten) / 1000;

    // linear curve; 63 == 100%; 126 == 200%
    //return Proportion(atten, 63, get());
  }
  int get() const {
    return value_ >> EXTRA_PRECISION;
  }
  int get_target() const {
    return target_ >> EXTRA_PRECISION;
  }
};

#define SCALE8_16(x) ((((x + 1) << 16) >> 8) - 1)

inline int16_t InterpLinear16(int16_t start, int16_t end, uint16_t phase) {
  int32_t delta = (end - start) * phase;
  return (delta / 65535) + start;
}

#endif // UTIL_MATH_H_
