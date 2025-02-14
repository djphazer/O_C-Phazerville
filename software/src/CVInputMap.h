#pragma once

#include "HemisphereApplet.h"

const int NUM_CV_INPUTS = ADC_CHANNEL_LAST * 2 + 1;
// We *could* reuse HS::input_quant for inputs, but easier to just do it
// independently, and semitone quantizers are lightwieght.
inline std::array<OC::SemitoneQuantizer, NUM_CV_INPUTS> cv_semitone_quants;

struct CVInputMap {
  int8_t source = 0;
  int8_t attenuversion = 100;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  int In(int default_value = 0) {
    if (!source) return default_value;
    return source <= ADC_CHANNEL_LAST
      ? frame.inputs[source - 1]
      : frame.outputs[source - 1 - ADC_CHANNEL_LAST];
  }

  float InF(float default_value = 0.0f) {
    if (!source) return default_value;
    return static_cast<float>(In())
      / static_cast<float>(HEMISPHERE_MAX_INPUT_CV);
  }

  int SemitoneIn(int default_value = 0) {
    return cv_semitone_quants[source].Process(In(default_value));
  }

  int InRescaled(int max_value) {
    return Proportion(In(), HEMISPHERE_MAX_INPUT_CV, max_value);
  }

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, NUM_CV_INPUTS - 1);
  }

  void RotateSource(int dir) {
    int x = source + dir;
    while(x < 0) x += NUM_CV_INPUTS;
    while(x >= NUM_CV_INPUTS) x -= NUM_CV_INPUTS;
    source = x;
  }

  char const* InputName() const {
    return OC::Strings::cv_input_names_none[source];
  }

  uint8_t const* Icon() const {
    return PARAM_MAP_ICONS + 8 * source;
  }

  uint16_t Pack() const {
    return source | (attenuversion << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;
    attenuversion = extract_value<int8_t>(data >> 8);
  }
};

// Let's PackingUtils know this is Packable as is.
constexpr CVInputMap& pack(CVInputMap& input) {
  return input;
}