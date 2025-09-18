#pragma once

#include <arm_math.h>

inline q15_t float_to_q15(float x) {
  x *= 32768.0f;
  x += x > 0.0f ? 0.5f : -0.5f;
  return (q15_t)(__SSAT((q31_t)(x), 16));
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

