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
#include <algorithm>
#include "OC_io.h"
#include "OC_strings.h"
#include "OC_scales.h"
#include "OC_cv_utils.h"

namespace OC {

SETTINGS_DECLARE(OC::InputSettings, OC::INPUT_SETTING_LAST) {
  { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "CV1 gain", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
  { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "CV2 gain", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
  { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "CV3 gain", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
  { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "CV4 gain", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
  { 0, 0, 1, "CV1 smoothing", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "CV2 smoothing", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "CV3 smoothing", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "CV4 smoothing", OC::Strings::off_on, settings::STORAGE_TYPE_U4 },
};

SETTINGS_DECLARE(OC::OutputSettings, OC::OUTPUT_SETTING_LAST) {
  { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "#A scaling", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
  { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "#B scaling", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
  { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "#C scaling", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
  { VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_1V_PER_OCT, VOLTAGE_SCALING_LAST - 1, "#D scaling", OC::voltage_scalings, settings::STORAGE_TYPE_U4 },
};

/*static*/ void IO::ReadDigitalInputs(IOFrame *ioframe)
{
  ioframe->digital_inputs.rising_edges = DigitalInputs::rising_edges();
  ioframe->digital_inputs.raised_mask = DigitalInputs::raised_mask();
}

/*static*/ void IO::ReadADC(IOFrame *ioframe, const InputSettings &input_settings)
{
  for (int channel = ADC_CHANNEL_1; channel < ADC_CHANNEL_LAST; ++channel) {
    int32_t val, pitch;
    if (input_settings.get_value(INPUT_SETTING_CV1_FILTER + channel)) {
      val = ADC::value(static_cast<ADC_CHANNEL>(channel));
      pitch = ADC::pitch_value(static_cast<ADC_CHANNEL>(channel));
    } else {
      val = ADC::raw_value(static_cast<ADC_CHANNEL>(channel));
      pitch = ADC::raw_pitch_value(static_cast<ADC_CHANNEL>(channel));
    }

    auto mult_factor = input_settings.get_value(INPUT_SETTING_CV1_GAIN + channel);

    ioframe->cv.values[channel] = CVUtils::Attenuate(val, mult_factor);
    ioframe->cv.pitch_values[channel] = CVUtils::Attenuate(pitch, mult_factor);
  }
}

template <DAC_CHANNEL channel>
static void IOFrameToChannel(const IOFrame *ioframe, const OutputSettings &output_settings)
{
  auto value = ioframe->outputs.values[channel];
  switch(ioframe->outputs.modes[channel]) {
    case OUTPUT_MODE_PITCH: value = DAC::PitchToScaledDAC<channel>(value, output_settings.get_output_scaling(channel)); break;
    case OUTPUT_MODE_GATE:  value = DAC::GateToDAC<channel>(value); break;
    case OUTPUT_MODE_UNI:   value += DAC::get_zero_offset(channel); break;
    case OUTPUT_MODE_RAW:   break;
  }
  DAC::set<channel>(value);
}

/*static*/ void IO::WriteDAC(IOFrame *ioframe, const OutputSettings &output_settings)
{
  IOFrameToChannel<DAC_CHANNEL_A>(ioframe, output_settings);
  IOFrameToChannel<DAC_CHANNEL_B>(ioframe, output_settings);
  IOFrameToChannel<DAC_CHANNEL_C>(ioframe, output_settings);
  IOFrameToChannel<DAC_CHANNEL_D>(ioframe, output_settings);
}

void IOFrame::Reset()
{
  digital_inputs.rising_edges = digital_inputs.raised_mask = 0U;

  std::fill(std::begin(cv.values), std::end(cv.values), 0);
  std::fill(std::begin(cv.pitch_values), std::end(cv.pitch_values), 0);

  std::fill(std::begin(outputs.values), std::end(outputs.values), 0);
  std::fill(std::begin(outputs.modes), std::end(outputs.modes), OUTPUT_MODE_PITCH);
}

}