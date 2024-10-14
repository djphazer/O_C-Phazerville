#pragma once

#include "HemisphereApplet.h"
#include "dsputils.h"
#include <AudioStream.h>

struct CVInput {
  int8_t source = 0;

  int In(int default_value = 0) {
    if (!source) return default_value;
    return source <= ADC_CHANNEL_LAST
      ? frame.inputs[source - 1]
      : frame.outputs[source - 1 - ADC_CHANNEL_LAST];
  }

  float InF(float default_value = 0.0f) {
    return In(static_cast<int>(default_value * HEMISPHERE_MAX_INPUT_CV))
      / static_cast<float>(HEMISPHERE_MAX_INPUT_CV);
  }

  int InRescaled(int max_value) {
    return Proportion(In(), HEMISPHERE_MAX_INPUT_CV, max_value);
  }

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, NumInputs() - 1);
  }

  int8_t NumInputs() const {
    return ADC_CHANNEL_LAST * 2 + 1;
  }

  char const* InputName() const {
    return OC::Strings::cv_input_names_none[source];
  }

  uint8_t const* Icon() const {
    return PARAM_MAP_ICONS + 8 * source;
  }
};

struct TrigInput {
  uint8_t source = 0;
};

enum AudioChannels {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;

  void gfxPrintPitchHz(int16_t pitch, float base_freq = C3) {
    float freq = PitchToRatio(pitch) * base_freq;
    int int_part = static_cast<int>(freq);
    int dec = static_cast<int>(10 * (freq - int_part));
    graphics.printf("%5d.%01d", int_part, dec);
    gfxPrint("Hz");
  }
};
