#pragma once
// -- This entire header is meant to be included once in HSIOFrame.h

struct CVInputMap {
  // upper 3 bits of source
  enum SourceType : uint8_t {
    TYPE_NONE = (0 << 5),
    TYPE_OLD1 = (1 << 5),
    TYPE_ADC = (2 << 5),
    TYPE_DAC = (3 << 5),
    TYPE_MIDI = (4 << 5),

    TYPE_HID = (5 << 5),
    TYPE_RESERVED0 = (6 << 5),
    TYPE_INTERNAL = (7 << 5), // for noise, LFOs, S&H, etc.
  };

  uint8_t source = 0; // 3 bits type | 5 bits index
  int8_t attenuversion = 60; // 60 is 100%
                             // max range is +/- 127 (448%)

  static const int num_sources = 3 * IO_CHANNEL_COUNT;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  const bool IsMidi() const {
    return source > ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT;
  }
  void AutoLearn() {
    if (IsMidi()) {
      frame.MIDIState.mapping[source - ADC_CHANNEL_LAST - DAC_CHANNEL_LAST - 1].AutoLearn();
    }
  }

  util::SemitoneQuantizer semitone_quant;

  SourceType source_type() const {
    return (SourceType)(source & (0x7 << 5)); // upper 3 bits
  }
  inline uint8_t index() const {
    return source & 0x1f; // lower 5 bits
  }

  int RawIn() const {
    switch (source_type()) {
      case TYPE_ADC:
        return frame.In(index());

      case TYPE_DAC:
        return frame.ViewOut(index());

      case TYPE_MIDI:
        return frame.MIDIState.mapping[index()].output;

      case TYPE_INTERNAL:
        // noise source
        if (source == 0xff) return random();
        // else, fall through to default
      default:
        return 0;
    }
  }

  int In(int default_value = 0) const {
    if (!source) return default_value;
    return RawIn() * Atten(attenuversion) / 1000;
  }

  float InF(float default_value = 0.0f) const {
    if (!source) return default_value;
    return 0.001f * Atten(attenuversion) * static_cast<float>(RawIn())
      / static_cast<float>((source_type() == TYPE_ADC)?HEMISPHERE_MAX_INPUT_CV:HEMISPHERE_MAX_CV);
  }

  int SemitoneIn(int default_value = 0) {
    return semitone_quant.Process(In(default_value));
  }

  int InRescaled(int max_value) const {
    return Proportion(In(), (source_type() == TYPE_ADC)?HEMISPHERE_MAX_INPUT_CV:HEMISPHERE_MAX_CV, max_value);
  }

  void ChangeSource(int dir) {
    uint8_t x = source + dir + 1;
    if (x > TYPE_ADC + num_sources) return;
    if (x > 1 && x <= TYPE_ADC) x = 1 + TYPE_ADC*(dir>0);
    source = uint8_t(x) - 1;
  }
  void RotateSource(int dir) {
    int x = source + dir + 1;
    if (x < 0) x = TYPE_ADC + num_sources;
    if (x > TYPE_ADC + num_sources) x = 0;
    if (x > 1 && x <= TYPE_ADC) x = 1 + TYPE_ADC*(dir>0);
    source = uint8_t(x) - 1;
  }

  void SetMidiMap(uint8_t midx) {
    source = TYPE_MIDI | (midx & 0x1f);
  }
  void SetInput(uint8_t idx) {
    source = TYPE_ADC | (idx & 0x1f);
  }
  void SetOutput(uint8_t idx) {
    source = TYPE_DAC | (idx & 0x1f);
  }

  const char * InputName() const {
    static char in_label[] = "C 1";

    if (!source) {
      return " - ";
    }
    const char type[] = {' ', '-', 'C', '#', 'M', 'G', '?', '?'};
    uint8_t idx = index();

    in_label[0] = type[source_type() >> 5];
    switch (source_type()) {
      case TYPE_DAC:
        in_label[1] = OC::Strings::capital_letters[idx][0];
        in_label[2] = ' ';
        break;

      case TYPE_MIDI:
      case TYPE_ADC:
        ++idx;
        if (idx / 10)
          in_label[1] = char('0' + idx / 10);
        else
          in_label[1] = ' ';
        in_label[2] = char('0' + idx % 10);
        break;

      default:
        in_label[1] = '*';
        in_label[2] = ' ';
        break;
    }
    return in_label;
  }

  uint8_t const* Icon() const {
    switch (source_type()) {
      case TYPE_NONE:
        return PARAM_MAP_ICONS + 0;
      case TYPE_INTERNAL:
        return MOD_ICON;
      case TYPE_MIDI:
        return PhzIcons::midiIn;

      case TYPE_ADC:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + index()) * 8;
      case TYPE_DAC:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + index()) * 8;

      default:
        return ZAP_ICON;
    }
  }

  uint16_t Pack() const {
    return (source & 0xFF) | (attenuversion << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;

    // migrate old data; zero is still disabled
    if (source && (source >> 5) < 2) {
      if (source <= ADC_CHANNEL_COUNT)
        SetInput(source - 1); // CV input
      else if (source <= DAC_CHANNEL_COUNT + ADC_CHANNEL_COUNT)
        SetOutput(source - ADC_CHANNEL_COUNT - 1); // DAC output
      else // if (source <= DAC_CHANNEL_COUNT + ADC_CHANNEL_COUNT + MIDIMAP_MAX)
        SetMidiMap(source - ADC_CHANNEL_COUNT - DAC_CHANNEL_COUNT - 1);
    }
    attenuversion = extract_value<int8_t>(data >> 8);
  }
};

// Let's PackingUtils know this is Packable as is.
constexpr CVInputMap& pack(CVInputMap& input) {
  return input;
}

struct DigitalInputMap {
  // upper 3 bits
  enum DigitalSourceType : uint8_t {
    NONE = 0,
    DEPRECATED = (1 << 5),

    CLOCK = (7 << 5), // -1 and -2 fall here

    DIGITAL_INPUT = (2 << 5),
    CV_INPUT = (3 << 5),
    CV_OUTPUT = (4 << 5),
    MIDI_MAP = (5 << 5),

    RESERVED = (6 << 5),
  };

  static const int ppqn = 4;
  static constexpr float GATE_PW_LOW = 0.49f;
  static constexpr float GATE_PW_HIGH = 0.51f;
  static const int num_sources = 4 * IO_CHANNEL_COUNT;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  // settings
  uint8_t source = 0;
  ClkDivMult div_mult;
  int8_t e_length, e_beats, e_rotate;

  // state
  bool last_gate_state = false; // for detecting clocks
  uint32_t clkcount = 0;

  void ChangeSource(int dir) {
    uint8_t x = source + dir + 2;

    if (x > DIGITAL_INPUT + num_sources + 1) return;
    if (x > 2 && x < DIGITAL_INPUT + 2) x = 2 + DIGITAL_INPUT * (dir > 0);

    source = uint8_t(x) - 2;
  }

  void SetGateInput(uint8_t idx) {
    source = DIGITAL_INPUT | (idx & 0x1f);
  }
  void SetInput(uint8_t idx) {
    source = CV_INPUT | (idx & 0x1f);
  }
  void SetOutput(uint8_t idx) {
    source = CV_OUTPUT | (idx & 0x1f);
  }
  void SetMidiMap(uint8_t midx) {
    source = MIDI_MAP | (midx & 0x1f);
  }

  void Reset(bool hard = false) {
    if (hard) div_mult.steps = 1;
    div_mult.Reset();
    last_gate_state = false;
    clkcount = 0;
  }

  bool Gate() const {
    switch (source_type()) {
      case CLOCK: {
        if (!clock_m.IsRunning()) return false;

        uint32_t ticks_since_beat = OC::CORE::ticks - clock_m.BeatTick();
        uint32_t tpb = clock_m.GetTempoTicks();
        int mult = (source == 0xfe) ? 1 : ppqn;

        // TODO: this doesn't work for larger divisions, because beat_tick gets reset
        //if (div_mult.steps < 0) tpb = tpb * -div_mult.steps;
        //if (div_mult.steps == 0) tpb = tpb;
        //if (div_mult.steps > 0) tpb = tpb / (div_mult.steps+1);

        uint32_t tick_phase = (mult * ticks_since_beat) % tpb;
        // hysteresis to guard against jitter from external clock sync
        // TODO: this only helps with falling edge
        uint32_t phase_edge = last_gate_state ? (GATE_PW_HIGH * tpb) : (GATE_PW_LOW * tpb);
        bool gate = tick_phase < phase_edge;
        return gate;
      }
      case DIGITAL_INPUT:
        return frame.gate_high[index()];
      case CV_INPUT:
        return frame.gate_high[DIGITAL_INPUT_COUNT + index()];
        // gate_high is already calculated for ADC
        //return frame.inputs[index()] > GATE_THRESHOLD;
      case CV_OUTPUT:
        return frame.ViewOut(index()) > GATE_THRESHOLD;
      case MIDI_MAP:
        return frame.MIDIState.mapping[index()].output > GATE_THRESHOLD;
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

    tock = div_mult.Tick(tock);

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
      case MIDI_MAP:
        return PhzIcons::midiIn;

      case DIGITAL_INPUT:
        if (index() < 4)
          return DIGITAL_INPUT_ICONS + index() * 8;
      case CV_INPUT:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + index()) * 8;
      case CV_OUTPUT:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + index()) * 8;

        return ZAP_ICON;

      case NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }
  char const* InputName() const {
    static char in_label[] = "T 1";
    const char type[] = {' ', '-', 'T', 'C', '#', 'M', '?', '*'};

    if (source == 0xff) return "CL4";
    if (source == 0xfe) return "CL1";
    if (!source) return " - ";

    in_label[0] = type[source_type() >> 5];
    uint8_t idx = index();
    switch (source_type()) {
      case CV_OUTPUT:
        in_label[1] = OC::Strings::capital_letters[idx][0];
        in_label[2] = ' ';
        break;

      case DIGITAL_INPUT:
      case CV_INPUT:
      case MIDI_MAP:
        ++idx;
        if (idx / 10)
          in_label[1] = char('0' + idx / 10);
        else
          in_label[1] = ' ';
        in_label[2] = char('0' + idx % 10);
        break;

      default:
        in_label[1] = '*';
        in_label[2] = ' ';
        break;
    }

    return in_label;
  }

  uint16_t Pack() const {
    return (source & 0xFF) | as_unsigned(div_mult.steps << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;

    // migrate old data; zero is still disabled; -1 and -2 are still valid
    if (source && (source >> 5) < 2) {
      if (source < 1 + DIGITAL_INPUT_COUNT)
        SetGateInput(source - 1);
      else if (source < 1 + DIGITAL_INPUT_COUNT + ADC_CHANNEL_LAST)
        SetInput(source - DIGITAL_INPUT_COUNT - 1);
      else if (source < 1 + DIGITAL_INPUT_COUNT + ADC_CHANNEL_LAST + DAC_CHANNEL_LAST)
        SetOutput(source - DIGITAL_INPUT_COUNT - ADC_CHANNEL_COUNT - 1);
      else
        SetMidiMap(source - DIGITAL_INPUT_COUNT - ADC_CHANNEL_COUNT - DAC_CHANNEL_COUNT - 1);
    }

    div_mult.Set(extract_value<int8_t>(data >> 8));
    if (0 == div_mult.steps) div_mult.steps = 1;
  }

private:
  DigitalSourceType source_type() const {
    return (DigitalSourceType)(source & (0x7 << 5)); // upper 3 bits
  }
  inline uint8_t index() const {
    return source & 0x1f;
  }
};

// Let's PackingUtils know this is Packable as is.
constexpr DigitalInputMap& pack(DigitalInputMap& input) {
  return input;
}

extern DigitalInputMap trigmap[ADC_CHANNEL_LAST];
extern CVInputMap cvmap[ADC_CHANNEL_LAST];
