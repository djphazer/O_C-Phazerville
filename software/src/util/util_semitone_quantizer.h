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
#ifndef UTIL_SEMITONE_QUANTIZER_H_
#define UTIL_SEMITONE_QUANTIZER_H_

#include <stdint.h>

namespace util {

// H1200/A11Z are semitone based, so don't need to go "full quanty" for now.
// They still need some hysteresis though
class SemitoneQuantizer {
public:
  static constexpr int32_t kHysteresis = 16;

  SemitoneQuantizer() { }
  ~SemitoneQuantizer() { }

  void Init() {
    last_pitch_ = 0;
  }

  int32_t Process(int32_t pitch) {
    if ((pitch > last_pitch_ + kHysteresis) || (pitch < last_pitch_ - kHysteresis)) {
      last_pitch_ = pitch;
    } else {
      pitch = last_pitch_;
    }
    return (pitch + 63) >> 7;
  }

private:
  int32_t last_pitch_;
};
  
} // util

#endif // UTIL_SEMITONE_QUANTIZER_H_
