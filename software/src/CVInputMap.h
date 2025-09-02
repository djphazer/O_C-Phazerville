#pragma once
// -- This entire header is meant to be included once in HSIOFrame.h

const int NUM_CV_INPUTS = CVMAP_MAX + 1;
//const int NUM_CV_INPUTS = ADC_CHANNEL_LAST * 2 + 1;
// We *could* reuse HS::input_quant for inputs, but easier to just do it
// independently, and semitone quantizers are lightwieght.
inline std::array<OC::SemitoneQuantizer, NUM_CV_INPUTS> cv_semitone_quants;

struct CVInputMap {
  int8_t source = 0;
  int8_t attenuversion = 60; // 60 is 100%
                             // max range is +/- 127 (448%)

  static constexpr size_t Size = 16; // Make this compatible with Packable

  // increments of 0.1%
  int Atten() {
    // exponential curve; 60 becomes 100.0%
    return 10 * attenuversion * abs(attenuversion) / 36;
  }
  int RawIn() {
    return source <= ADC_CHANNEL_LAST
      ? frame.inputs[source - 1]
      : (source - ADC_CHANNEL_LAST <= DAC_CHANNEL_LAST)
        ? frame.outputs[source - 1 - ADC_CHANNEL_LAST]
        : frame.MIDIState.mapping[source - ADC_CHANNEL_LAST - DAC_CHANNEL_LAST - 1].output;
  }

  int In(int default_value = 0) {
    if (!source) return default_value;
    return RawIn() * Atten() / 1000;
  }

  float InF(float default_value = 0.0f) {
    if (!source) return default_value;
    return 0.001f * Atten() * static_cast<float>(RawIn())
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
    return source <= ADC_CHANNEL_LAST + DAC_CHANNEL_LAST
      ? PARAM_MAP_ICONS + 8 * source
      : PhzIcons::midiIn;
  }

  uint16_t Pack() const {
    return (source & 0xFF) | (attenuversion << 8);
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

struct DigitalInputMap {
  enum DigitalSourceType {
    NONE,
    CLOCK,
    DIGITAL_INPUT,
    CV_INPUT,
    CV_OUTPUT,
    MIDI_MAP,
  };

  int8_t source = 0;
  int8_t division = 0; // -2 = /3, -1 = /2, 0 = x1, 1 = x2, 2 = x3...
  bool last_gate_state = true; // for detecting clocks
  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;
  static const int num_sources = TRIGMAP_MAX;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  void ChangeSource(int dir) {
    source = constrain(source + dir, -1, num_sources);
  }

  bool Gate() {
    switch (source_type()) {
      case CLOCK: {
        if (!clock_m.IsRunning()) return false;
        uint32_t ticks_since_beat = OC::CORE::ticks - clock_m.beat_tick;
        // TODO: implement division for ticks_per_beat
        uint32_t tick_phase
          = (ppqn * ticks_since_beat) % clock_m.ticks_per_beat;
        bool gate
          = tick_phase < internal_clocked_gate_pw * clock_m.ticks_per_beat;
        return gate;
      }
      case DIGITAL_INPUT:
      case CV_INPUT:
        return frame.gate_high[digital_input_index()];
        // gate_high is already calculated for ADC
        //return frame.inputs[cv_input_index()] > GATE_THRESHOLD;
      case CV_OUTPUT:
        return frame.outputs[cv_output_index()] > GATE_THRESHOLD;
      case MIDI_MAP:
        return frame.MIDIState.mapping[midi_map_index()].output > GATE_THRESHOLD;
      case NONE:
      default:
        return false;
    }
  }

  /**
   * Returns true on rising gate input. Will return true once and then go back
   * to false until the gate goes low again.
   **/
  bool Clock() {
    bool gate = Gate();
    bool tock = !last_gate_state && gate;
    last_gate_state = gate;
    return tock;
  }

  uint8_t const* Icon() const {
    switch (source_type()) {
      case CLOCK:
        return clock_m.cycle ? METRO_L_ICON : METRO_R_ICON;
      case DIGITAL_INPUT:
        return DIGITAL_INPUT_ICONS + digital_input_index() * 8;
      case CV_INPUT:
        return PARAM_MAP_ICONS + (1 + cv_input_index()) * 8;
      case CV_OUTPUT:
        return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + cv_output_index()) * 8;
      case MIDI_MAP:
        return PhzIcons::midiIn;
      case NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }
  char const* InputName() const {
    if (source == -1) return "CLK"; // TODO: division
    if (source > OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST + DAC_CHANNEL_LAST)
      return OC::Strings::cv_input_names_none[source - OC::DIGITAL_INPUT_LAST];
    return OC::Strings::trigger_input_names_none[source];
  }

  uint16_t Pack() const {
    return (source & 0xFF) | as_unsigned(division << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;
    division = extract_value<int8_t>(data >> 8);
  }

private:
  DigitalSourceType source_type() const {
    switch (source) {
      case -1:
        return CLOCK;
      case 0:
        return NONE;
      default: {
        if (source < 1 + OC::DIGITAL_INPUT_LAST)
          return DIGITAL_INPUT;

        if (source < 1 + OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST)
          return CV_INPUT;

        if (source < 1 + OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST + DAC_CHANNEL_LAST)
          return CV_OUTPUT;

        return MIDI_MAP;
      }
    }
  }

  inline int8_t digital_input_index() const {
    return source - 1;
  }
  inline int8_t cv_input_index() const {
    return source - 1 - OC::DIGITAL_INPUT_LAST;
  }
  inline int8_t cv_output_index() const {
    return source - 1 - OC::DIGITAL_INPUT_LAST - ADC_CHANNEL_LAST;
  }
  inline int8_t midi_map_index() const {
    return source - 1 - OC::DIGITAL_INPUT_LAST - ADC_CHANNEL_LAST - DAC_CHANNEL_LAST;
  }
};

// Let's PackingUtils know this is Packable as is.
constexpr DigitalInputMap& pack(DigitalInputMap& input) {
  return input;
}

extern DigitalInputMap trigmap[ADC_CHANNEL_LAST];
extern CVInputMap cvmap[ADC_CHANNEL_LAST];
