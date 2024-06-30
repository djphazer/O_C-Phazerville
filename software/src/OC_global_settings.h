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

#ifndef OC_GLOBAL_SETTINGS_H_
#define OC_GLOBAL_SETTINGS_H_

#include "OC_DAC.h"
#include "OC_chords.h"
#include "OC_scales.h"
#include "OC_patterns.h"
#include "util/util_misc.h"
#include "enigma/TuringMachine.h"
#include "vector_osc/HSVectorOscillator.h"

namespace OC {

// Global settings are stored separately to actual app setings.
// The theory is that they might not change as often.
class GlobalSettings {
public:
  static constexpr uint32_t FOURCC = FOURCC<'O','C','S',2>::value;

  GlobalSettings() { }
  ~GlobalSettings() { }

  void Init();

  // NOTE These are used by load/save functions and might be hidden
  bool encoders_enable_acceleration;
  bool reserved0;
  bool reserved1;
  uint32_t reserved2;
  uint16_t current_app_id;

  // TODO[PLD] We should be able to use these structs directly to save space
  Scale user_scales[Scales::SCALE_USER_LAST];
  Pattern user_patterns[Patterns::PATTERN_USER_ALL];
  // These both occupy 160 bytes
#ifdef ENABLE_APP_CHORDS
  Chord user_chords[Chords::CHORDS_USER_LAST];
#else
  HS::TuringMachine user_turing_machines[HS::TURING_MACHINE_COUNT];
#endif
  HS::VOSegment user_waveforms[HS::VO_SEGMENT_COUNT];

  // NOTE These structs are used directly, instead of maintaining two separate
  // copies in memory.
  DAC::AutotuneCalibrationData autotune_calibration_data;

  DISALLOW_COPY_AND_ASSIGN(GlobalSettings);
};

extern GlobalSettings global_settings;

} // namespace OC

#endif // OC_GLOBAL_SETTINGS_H_
