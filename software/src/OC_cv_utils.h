// Copyright 2019 Patrick Dowling
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
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
#ifndef OC_CV_UTILS_H_
#define OC_CV_UTILS_H_

#include <Arduino.h>
#include <array>
#include <stdint.h>
#include "src/extern/dspinst.h"
#include "util/util_macros.h"
#include "util/util_math.h"

namespace OC {

//#define OC_CV_MULT_SHORTCUT1X

class CVUtils {
public:

  // 0.0 / 2.0 in 0.05 steps
  static constexpr float kMultRange = 2.f;
  static constexpr float kMultStepSize = 0.05f;

  static constexpr int kMultSteps = static_cast<int>(kMultRange / kMultStepSize);
  static constexpr int kMultOne = static_cast<int>(1.f / kMultStepSize) - 1;

  static const std::array<int32_t, kMultSteps> kMultMultipliers; // 16.16

  template <bool saturate = false>
  static inline int32_t Attenuate(int32_t cv, int factor) {
    // We can optimize the 1x path, but for performance testing it's useful to
    // do the computation always (also, smaller code size).
#ifdef OC_CV_MULT_SHORTCUT1X
    if (kMultOne != factor) {
#endif
      CONSTRAIN(factor, 0, kMultSteps - 1); 
      int32_t scaled = signed_multiply_32x16b(kMultMultipliers[factor], cv); // NOTE if mutlipliers end up negative, check values
      return saturate ? SSAT16(scaled) : scaled;
#ifdef OC_CV_MULT_SHORTCUT1X
    } else {
      return cv;
    }
#endif
  }
};

} // namespace OC

#endif // OC_CV_UTILS_H_
