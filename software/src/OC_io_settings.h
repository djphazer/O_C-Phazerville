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
#ifndef OC_IO_SETTINGS_H_
#define OC_IO_SETTINGS_H_

#include "OC_cv_utils.h"
#include "OC_DAC.h"
#include "OC_strings.h"
#include "util/util_settings.h"

namespace OC {

// Output value types
enum OutputMode {
  OUTPUT_MODE_PITCH,
  OUTPUT_MODE_GATE,
  OUTPUT_MODE_UNI,
  OUTPUT_MODE_RAW,
};

enum IO_SETTING {
  IO_SETTING_CV1_GAIN, IO_SETTING_CV1_FILTER, IO_SETTING_TR1, IO_SETTING_A_SCALING, IO_SETTING_A_TUNING,
  IO_SETTING_CV2_GAIN, IO_SETTING_CV2_FILTER, IO_SETTING_TR2, IO_SETTING_B_SCALING, IO_SETTING_B_TUNING,
  IO_SETTING_CV3_GAIN, IO_SETTING_CV3_FILTER, IO_SETTING_TR3, IO_SETTING_C_SCALING, IO_SETTING_C_TUNING,
  IO_SETTING_CV4_GAIN, IO_SETTING_CV4_FILTER, IO_SETTING_TR4, IO_SETTING_D_SCALING, IO_SETTING_D_TUNING,
  IO_SETTING_LAST
};

extern const char *const autotune_enable_strings[];
extern const char *const voltage_scalings[];

// TODO[PLD] Offset mult factors so 0 = no gain, or attenuvert?

class IOSettings: public settings::SettingsBase<IOSettings, IO_SETTING_LAST> {
public:
  static constexpr int kSettingsPerChannel = IO_SETTING_CV2_GAIN - IO_SETTING_CV1_GAIN;


  static inline IO_SETTING channel_setting(IO_SETTING setting, int channel) {
    return static_cast<IO_SETTING>(setting + channel * kSettingsPerChannel);
  }

  inline int input_gain(int channel) const {
    return get_value(channel_setting(IO_SETTING_CV1_GAIN, channel));
  }

  inline bool input_gain_enabled(int channel) const {
    return CVUtils::kMultOne != input_gain(channel);
  }

  inline bool adc_filter_enabled(int channel) const {
    return get_value(channel_setting(IO_SETTING_CV1_FILTER, channel));
  }

  OutputVoltageScaling get_output_scaling(int channel) const {
    return static_cast<OutputVoltageScaling>(get_value(channel_setting(IO_SETTING_A_SCALING, channel)));
  }

  inline bool autotune_data_enabled(int channel) const {
    return get_value(channel_setting(IO_SETTING_A_TUNING, channel));
  }

  uint32_t status_mask(int channel) const {
    return
      (input_gain_enabled(channel) ? 0x1 : 0x0) |
      (adc_filter_enabled(channel) ? 0x2 : 0x0) |
      (get_output_scaling(channel) ? 0x4 : 0x0) |
      (autotune_data_enabled(channel) ? 0x8 : 0x0);
  }

  uint32_t status_mask() const {
    return
      status_mask(0) |
      status_mask(1) << 8 |
      status_mask(2) << 16 |
      status_mask(3) << 24;
  }

  SETTINGS_ARRAY_DECLARE() {{
    { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
    { 0, 0, 1, "CV1 filter", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "", nullptr, settings::STORAGE_TYPE_NOP },
    { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "DAC calibr.", OC::autotune_enable_strings, settings::STORAGE_TYPE_U4 },

    { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
    { 0, 0, 1, "CV2 filter", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "", nullptr, settings::STORAGE_TYPE_NOP },
    { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "DAC calibr.", OC::autotune_enable_strings, settings::STORAGE_TYPE_U4 },

    { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
    { 0, 0, 1, "CV3 filter", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "", nullptr, settings::STORAGE_TYPE_NOP },
    { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "DAC calibr.", OC::autotune_enable_strings, settings::STORAGE_TYPE_U4 },

    { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
    { 0, 0, 1, "CV4 filter", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "", nullptr, settings::STORAGE_TYPE_NOP },
    { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "DAC calibr.", OC::autotune_enable_strings, settings::STORAGE_TYPE_U4 },
  }};
};


} // OC

#endif // OC_IO_SETTINGS_H_
