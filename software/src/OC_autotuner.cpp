// Copyright 2019 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
// Adapted from code by Max Stadler
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

#include "OC_core.h"
#include "OC_autotuner.h"

namespace OC {

SETTINGS_ARRAY_DEFINE(AutotunerSettings);

#if defined(BUCHLA_4U) && !defined(IO_10V)
const char* const AT_steps[] = {
  "0.0V", "1.2V", "2.4V", "3.6V", "4.8V", "6.0V", "7.2V", "8.4V", "9.6V", "10.8V", " " 
};

#elif defined(IO_10V)
const char* const AT_steps[] = {
  "0.0V", "1.0V", "2.0V", "3.0V", "4.0V", "5.0V", "6.0V", "7.0V", "8.0V", "9.0V", " " 
};

#else
const char* const AT_steps[] = {
  "-3V", "-2V", "-1V", " 0V", "+1V", "+2V", "+3V", "+4V", "+5V", "+6V", " " 
};
#endif

const char *const reset_action_strings[] = {
  ">RESET", ">  USE"
};

const char *const status_action_strings[] = {
  ">  ARM", ">  RUN", "> STOP", ">   OK"
};

} // namespace OC
