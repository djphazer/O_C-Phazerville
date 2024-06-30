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

#include "OC_cv_utils.h"
#include "util/util_templates.h"

namespace OC {

template <size_t ...Is>
constexpr std::array<int32_t, sizeof...(Is)> generate_multipliers(float step_size, util::index_sequence<Is...>) {
  return { static_cast<int32_t>(step_size * static_cast<float>(Is + 1) * 65536.f + .5f)... };
}

/*static*/
const std::array<int32_t, CVUtils::kMultSteps> CVUtils::kMultMultipliers =
  generate_multipliers(CVUtils::kMultStepSize, util::make_index_sequence<CVUtils::kMultSteps>::type());

} // namespace OC

/*
  {
  3277, 6554, 9830, 13107, 16384, 19661, 22938, 26214, 29491, 32768, 
  36045, 39322, 42598, 45875, 49152, 52429, 55706, 58982, 62259, 65536, 
  68813, 72090, 75366, 78643, 81920, 85197, 88474, 91750, 95027, 98304, 
  101581, 104858, 108134, 111411, 114688, 117964, 121242, 124518, 127795, 131072
};
*/
