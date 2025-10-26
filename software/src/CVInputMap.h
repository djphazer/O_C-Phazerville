#pragma once
// -- This entire header is meant to be included once in HSIOFrame.h

#include "bjorklund.h"
#include "util/clkdivmult.h"
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
        ? frame.ViewOut(source - 1 - ADC_CHANNEL_LAST)
        : frame.MIDIState.mapping[source - ADC_CHANNEL_LAST - DAC_CHANNEL_LAST - 1].output;
  }

  int In(int default_value = 0) {
    if (!source) return default_value;
    return RawIn() * Atten() / 1000;
  }

  float InF(float default_value = 0.0f) {
    if (!source) return default_value;
    return 0.001f * Atten() * static_cast<float>(RawIn())
      / static_cast<float>((source <= ADC_CHANNEL_LAST)?HEMISPHERE_MAX_INPUT_CV:HEMISPHERE_MAX_CV);
  }

  int SemitoneIn(int default_value = 0) {
    return cv_semitone_quants[source].Process(In(default_value));
  }

  int InRescaled(int max_value) {
    return Proportion(In(), (source <= ADC_CHANNEL_LAST)?HEMISPHERE_MAX_INPUT_CV:HEMISPHERE_MAX_CV, max_value);
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

  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;
  static const int num_sources = TRIGMAP_MAX;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  // settings
  int8_t source = 0;
  ClkDivMult div_mult;
  int8_t e_length, e_beats, e_rotate;

  // state
  bool last_gate_state = false; // for detecting clocks
  uint32_t clkcount = 0;

  void ChangeSource(int dir) {
    source = constrain(source + dir, -2, num_sources);
  }

  void Reset() {
    div_mult.Reset();
    last_gate_state = false;
    clkcount = 0;
  }

  bool Gate() {
    switch (source_type()) {
      case CLOCK: {
        if (!clock_m.IsRunning()) return false;

        uint32_t ticks_since_beat = OC::CORE::ticks - clock_m.beat_tick;
        uint32_t tpb = clock_m.GetTempoTicks();
        int mult = (source == -2) ? 1 : ppqn;

        // TODO: this doesn't work for larger divisions, because beat_tick gets reset
        //if (div_mult.steps < 0) tpb = tpb * -div_mult.steps;
        //if (div_mult.steps == 0) tpb = tpb;
        //if (div_mult.steps > 0) tpb = tpb / (div_mult.steps+1);

        uint32_t tick_phase = (mult * ticks_since_beat) % tpb;
        bool gate = tick_phase < (internal_clocked_gate_pw * tpb);
        return gate;
      }
      case DIGITAL_INPUT:
      case CV_INPUT:
        return frame.gate_high[digital_input_index()];
        // gate_high is already calculated for ADC
        //return frame.inputs[cv_input_index()] > GATE_THRESHOLD;
      case CV_OUTPUT:
        return frame.ViewOut(cv_output_index()) > GATE_THRESHOLD;
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
    if (clock_m.auto_reset) Reset();
    bool gate = Gate();
    bool tock = !last_gate_state && gate;
    last_gate_state = gate;
    //if (source_type() != CLOCK) {
    tock = div_mult.Tick(tock);
    //}

    // process trigger filters here - Euclidean, etc
    if (tock) {
      if (e_length > 0)
        tock = EuclideanFilter(e_length, e_beats, e_rotate, clkcount);
      ++clkcount;
    }

    return tock;
  }

  uint8_t const* Icon() const {
    switch (source_type()) {
      case CLOCK:
        return (clkcount & 1) ? METRO_L_ICON : METRO_R_ICON;
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
    if (source == -1) return "CLK4";
    if (source == -2) return "CLK1";
    if (source > OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST + DAC_CHANNEL_LAST)
      return OC::Strings::cv_input_names_none[source - OC::DIGITAL_INPUT_LAST];
    return OC::Strings::trigger_input_names_none[source];
  }

  uint16_t Pack() const {
    return (source & 0xFF) | as_unsigned(div_mult.steps << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;
    div_mult.Set(extract_value<int8_t>(data >> 8));
    if (0 == div_mult.steps) div_mult.steps = 1;
  }

private:
  DigitalSourceType source_type() const {
    switch (source) {
      case -2:
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
