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
#include "OC_io_settings.h"
#include "OC_strings.h"
#include "OC_scales.h"
#include "OC_cv_utils.h"
#include "OC_pitch_utils.h"

namespace OC {

const char * const autotune_enable_strings[] = { "Dflt", "Auto" };
SETTINGS_ARRAY_DEFINE(IOSettings);

void OutputDesc::set_printf(OutputMode output_mode, const char *fmt, ...) {
  mode = output_mode;
  va_list args;
  va_start(args, fmt );
  vsnprintf(label, sizeof(label), fmt, args);
  va_end(args);
}

void InputDesc::set_printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt );
  vsnprintf(label, sizeof(label), fmt, args);
  va_end(args);
}

/*static*/ void IO::Read(IOFrame *ioframe, const IOSettings *io_settings)
{
  {
    DEBUG_PIN_SCOPE(OC_GPIO_DEBUG_PIN1);
    ioframe->digital_inputs.rising_edges = DigitalInputs::rising_edges();
    ioframe->digital_inputs.raised_mask = DigitalInputs::raised_mask();
  }
  {
    DEBUG_PIN_SCOPE(OC_GPIO_DEBUG_PIN1);
    for (int channel = ADC_CHANNEL_1; channel < ADC_CHANNEL_LAST; ++channel) {
      int32_t value = io_settings->adc_filter_enabled(channel)
        ? ADC::value(static_cast<ADC_CHANNEL>(channel))
        : ADC::unsmoothed_value(static_cast<ADC_CHANNEL>(channel));

      // TODO[PLD] Does it make sense to only attenuate on demand?
      // Or only provide one "normalized" value? This would have to be the pitch
      // value but this makes the scaling somewhat awkward ()

      // Attenuating before pitch scaling loses some resolution
      auto gain = io_settings->input_gain(channel);
      ioframe->cv.values[channel] = CVUtils::Attenuate(value, gain);
      ioframe->cv.pitch_values[channel] = CVUtils::Attenuate(ADC::value_to_pitch(value), gain);
    }
  }
}

template <DAC_CHANNEL channel>
static void IOFrameToChannel(const IOFrame *ioframe, const IOSettings *io_settings) 
{
  DEBUG_PIN_SCOPE(OC_GPIO_DEBUG_PIN1);
  auto value = ioframe->outputs.values[channel];
  switch(ioframe->outputs.modes[channel]) {
    case OUTPUT_MODE_PITCH:
      value = DAC::PitchToScaledDAC<channel>(
                  value, io_settings->get_output_scaling(channel),
                  io_settings->autotune_data_enabled(channel));
    break;
    case OUTPUT_MODE_GATE:  value = DAC::GateToDAC<channel>(value); break;
    case OUTPUT_MODE_UNI:   value += DAC::get_zero_offset(channel); break;
    case OUTPUT_MODE_RAW:   break;
  }
  DAC::set<channel>(value);
}

/*static*/ void IO::Write(IOFrame *ioframe, const IOSettings *io_settings) 
{
  IOFrameToChannel<DAC_CHANNEL_A>(ioframe, io_settings);
  IOFrameToChannel<DAC_CHANNEL_B>(ioframe, io_settings);
  IOFrameToChannel<DAC_CHANNEL_C>(ioframe, io_settings);
  IOFrameToChannel<DAC_CHANNEL_D>(ioframe, io_settings);
}

/*static*/ int32_t IO::pitch_rel_to_abs(int32_t pitch) {
  return pitch + PitchUtils::PitchFromOctave(DAC::kOctaveZero);
}

/*static*/ int32_t IO::pitch_scale(int32_t pitch, OutputVoltageScaling scaling) {
  return DAC::Scale(pitch, scaling);
}

/*static*/ int32_t IO::pitch_to_millivolts(int32_t pitch) {
  return (((pitch * DAC::kMillvoltsPerOctave * 10) / (12 << 7)) + 5) / 10;
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
