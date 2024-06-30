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

#ifndef OC_IO_H_
#define OC_IO_H_

#include <array>

#include "OC_ADC.h"
#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_io_settings.h"

namespace OC {

static constexpr size_t kMaxIOLabelLength = 32;

// Structs to hold information about IO ports
struct OutputDesc {
  char label[kMaxIOLabelLength + 1] = "";
  OutputMode mode = OUTPUT_MODE_RAW;

  void set(const char *l, OutputMode m) {
    strncpy(label, l, sizeof(label));
    mode = m;
  }

  void set_printf(OutputMode output_mode, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
};

struct InputDesc {
  char label[kMaxIOLabelLength + 1] = "";

  void set_printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  void set(const char *l) {
    strncpy(label, l, sizeof(label));
  }
};

// Struct that apps can use to describe the function of the IO ports.
// NOTE it's generally assumed that there's 4 of each type in the settings
// menu for simplicity.
// What'd be really nice (if one wanted to rewrite even more of the apps...)
// would be a way of defining a function + description instead of handling all
// the moving parts individually.
struct IOConfig {
  InputDesc digital_inputs[DIGITAL_INPUT_LAST];
  InputDesc cv[ADC_CHANNEL_LAST];
  OutputDesc outputs[DAC_CHANNEL_LAST];

  void Reset() {
    memset(this, 0, sizeof(IOConfig));
  }
};

struct IOFrame;

class IO {
public:
  static void Read(IOFrame *, const IOSettings *);
  static void Write(IOFrame *, const IOSettings *);

  static int32_t pitch_rel_to_abs(int32_t pitch);
  static int32_t pitch_scale(int32_t pitch, OutputVoltageScaling scaling);
  static int32_t pitch_to_millivolts(int32_t pitch);
};

// Processing for apps works on a per-sample basis. An IOFrame encapuslates the
// IO data for a given frame, plus some helper functions to mimic the available
// functions in the direct drivers.
//
struct IOFrame {

  void Reset();

  struct {
    uint32_t rising_edges; // Rising edge detected since last frame
    uint32_t raised_mask;   // Last read state

    inline uint32_t triggered() const { return rising_edges; }

    template <DigitalInput input>
    inline uint32_t triggered() const { return rising_edges & DIGITAL_INPUT_MASK(input); }

    inline uint32_t triggered(DigitalInput input) const { return rising_edges & DIGITAL_INPUT_MASK(input); }

    template <DigitalInput input>
    inline bool raised() const { return raised_mask & DIGITAL_INPUT_MASK(input); }

    inline bool raised(DigitalInput input) const { return raised_mask & DIGITAL_INPUT_MASK(input); }

  } digital_inputs;

  struct {
    std::array<int32_t, ADC_CHANNEL_LAST> values;       // 10V = 2^12
    std::array<int32_t, ADC_CHANNEL_LAST> pitch_values; // 1V = 12 << 7 = 1536

    // Get CV value mapped for a given number of steps across the range
    // Round up/offset to move window and avoid "busy" toggling around 0
    // Works best of powers-of-two
    template <int32_t steps>
    constexpr int32_t ScaledValue(size_t channel) const {
      // TODO[PLD] size_t vs. ADC_CHANNEL
      // TODO[PLD] Ensure positive range allows for all values?
      // TODO[PLD] Rounding
      return (values[channel] * steps + (1 << 11) - 1) >> 12;
      //return (values[channel] * steps + (4096 / steps / 2 - 1)) >> 12;
    }

  } cv;

  struct {
    std::array<int32_t, DAC_CHANNEL_LAST> values;
    std::array<OutputMode, DAC_CHANNEL_LAST> modes;

    template <typename T>
    void set_pitch_values(const T& src)
    {
      std::copy(src, src + DAC_CHANNEL_LAST, std::begin(values));
      std::fill(std::begin(modes), std::end(modes), OUTPUT_MODE_PITCH);  
    }

    void set_pitch_value(size_t channel, int32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_PITCH;
    }

    void set_gate_value(size_t channel, bool raised)
    {
      values[channel] = raised ? 1 : 0;
      modes[channel] = OUTPUT_MODE_GATE;
    }

    void set_unipolar_value(size_t channel, int32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_UNI;
    }

    void set_raw_value(size_t channel, uint32_t value)
    {
      values[channel] = value;
      modes[channel] = OUTPUT_MODE_RAW;
    }

  } outputs;

};

} // namespace OC

#endif // OC_IO_H_
