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

#ifndef OC_PITCH_UTILS_H_
#define OC_PITCH_UTILS_H_

#include <stdint.h>

namespace OC {

// Wrap some useful pitch-replated functions that previously lived in the DAC
// classes.

class PitchUtils {
public:

  static constexpr int32_t kPitchMax = 120 << 7;

  static constexpr int32_t PitchFromOctave(int32_t octave)
  {
    return octave * 12 << 7;
  }

  static constexpr int32_t PitchFromSemitone(int32_t semi, int32_t octave)
  {
    return (semi + octave * 12) << 7;
  }

  static constexpr int32_t PitchAddOctaves(int32_t pitch, int32_t octave)
  {
    return pitch + (octave * 12 << 7);
  }

};

} // namespace OC

#endif // OC_PITCH_UTILS_H_
