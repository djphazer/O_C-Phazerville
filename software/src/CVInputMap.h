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

  static constexpr SourceType ordered_types[5] = {
    TYPE_NONE,
    TYPE_ADC,
    TYPE_DAC,
    TYPE_MIDI,
    TYPE_INTERNAL,
  };

private:
  uint8_t source = 0; // 3 bits type | 5 bits index
public:
  int8_t attenuversion = 60; // 60 is 100%
                             // max range is +/- 127 (448%)

  static const int num_sources = 3 * IO_CHANNEL_COUNT;

  static constexpr size_t Size = 16; // Make this compatible with Packable

  void AutoLearn() {
    if (source_type() == TYPE_MIDI) {
      frame.MIDIState.mapping[index()].AutoLearn();
      frame.MIDIState.UpdateMidiChannelFilter();
    }
  }

  util::SemitoneQuantizer semitone_quant;

  const bool enabled() const { return source != 0; }
  SourceType source_type() const {
    return (SourceType)(source & (0x7 << 5)); // upper 3 bits
  }
  inline uint8_t index() const {
    return source & 0x1f; // lower 5 bits
  }
  constexpr int channel_count(SourceType t) const {
    switch(t) {
      default:
        return 0;
      case TYPE_NONE:
        return 1;
      case TYPE_ADC:
        // strictly hardware inputs
        return ADC_CHANNEL_COUNT;
      case TYPE_DAC:
        // This one can have virtual signals.
        // 2 outputs per applet slot, including Clock and Audio Applets...
        return 2 * APPLET_CURSOR_COUNT;
      case TYPE_MIDI:
        return MIDIMAP_MAX;
      case TYPE_INTERNAL:
        return 1;
    }
  }

  int RawIn() const {
    switch (source_type()) {
      case TYPE_ADC:
        return frame.In(index());

      case TYPE_DAC:
        return frame.ViewOut(index());

      case TYPE_MIDI:
        return frame.MIDIState.mapping[index()].ViewOut();

      case TYPE_INTERNAL:
        // noise source
        if (index() == 0)
          return int(random(ONE_OCTAVE * HS::octave_max * 2)) - ONE_OCTAVE * HS::octave_max;
        // TODO: LFOs and other basic stuff
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

  bool ChangeSourceType(int dir) {
    const int count = sizeof(ordered_types);
    int tidx = 0;
    while (source_type() != ordered_types[tidx] && tidx < count) ++tidx;
    if (tidx >= count) tidx = 0;
    tidx += (dir>0?1:-1);
    CONSTRAIN(tidx, 0, count - 1);
    int sidx = constrain(index(), 0, channel_count(ordered_types[tidx]) - 1);

    SourceType oldtype = source_type();
    source = ordered_types[tidx] | sidx;
    return oldtype != source_type();
  }
  void ChangeSource(int dir) {
    int sidx = index() + dir;
    if (sidx < 0) {
      if (ChangeSourceType(-1))
        sidx = channel_count(source_type()) - 1;
    }
    if (sidx >= channel_count(source_type())) {
      if (ChangeSourceType(1))
        sidx = 0;
    }
    CONSTRAIN(sidx, 0, channel_count(source_type()) - 1);
    source = source_type() | uint8_t(sidx);
  }

  void SetMidiMap(uint8_t midx) {
    CONSTRAIN(midx, 0, channel_count(TYPE_MIDI) - 1);
    source = TYPE_MIDI | (midx & 0x1f);
  }
  void SetInput(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_ADC) - 1);
    source = TYPE_ADC | (idx & 0x1f);
  }
  void SetOutput(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_DAC) - 1);
    source = TYPE_DAC | (idx & 0x1f);
  }
  void SetInternal(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_INTERNAL) - 1);
    source = TYPE_INTERNAL | (idx & 0x1f);
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
          return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_COUNT + index()) * 8;

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
    // enforce constrain for presets saved with v2.0-alpha
    if (source_type() == TYPE_INTERNAL) SetInternal(index());

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
    TYPE_NONE = 0,
    DEPRECATED = (1 << 5),

    TYPE_DIGITAL_INPUT = (2 << 5),
    TYPE_ADC = (3 << 5),
    TYPE_DAC = (4 << 5),
    TYPE_MIDI_MAP = (5 << 5),

    RESERVED = (6 << 5),
    TYPE_INTERNAL = (7 << 5), // -1 and -2 fall here
  };

  static constexpr DigitalSourceType ordered_types[6] = {
    TYPE_NONE,
    TYPE_DIGITAL_INPUT,
    TYPE_ADC,
    TYPE_DAC,
    TYPE_MIDI_MAP,
    TYPE_INTERNAL,
  };

  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;

  static constexpr size_t Size = 32; // Make this compatible with Packable

  // settings
private:
  uint8_t source = 0;
public:
  ClkDivMult div_mult;
  int8_t e_length, e_beats, e_rotate;

  // state
  bool last_gate_state = false; // for detecting clocks
  uint32_t clkcount = 0;

  bool ChangeSourceType(int dir) {
    const int count = sizeof(ordered_types);
    int tidx = 0;
    while (source_type() != ordered_types[tidx] && tidx < count) ++tidx;
    if (tidx >= count) tidx = 0;
    tidx += (dir>0?1:-1);
    CONSTRAIN(tidx, 0, count - 1);
    int sidx = constrain(index(), 0, channel_count(ordered_types[tidx]) - 1);

    DigitalSourceType oldtype = source_type();
    source = ordered_types[tidx] | sidx;
    return oldtype != source_type();
  }
  void ChangeSource(int dir) {
    int sidx = index() + dir;
    if (sidx < 0) {
      if (ChangeSourceType(-1))
        sidx = channel_count(source_type()) - 1;
    }
    if (sidx >= channel_count(source_type())) {
      if (ChangeSourceType(1))
        sidx = 0;
    }
    CONSTRAIN(sidx, 0, channel_count(source_type()) - 1);
    source = source_type() | uint8_t(sidx);
  }

  void SetGateInput(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_DIGITAL_INPUT) - 1);
    source = TYPE_DIGITAL_INPUT | (idx & 0x1f);
  }
  void SetInput(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_ADC) - 1);
    source = TYPE_ADC | (idx & 0x1f);
  }
  void SetOutput(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_DAC) - 1);
    source = TYPE_DAC | (idx & 0x1f);
  }
  void SetMidiMap(uint8_t midx) {
    CONSTRAIN(midx, 0, channel_count(TYPE_MIDI_MAP) - 1);
    source = TYPE_MIDI_MAP | (midx & 0x1f);
  }
  void SetClockSource(uint8_t idx) {
    CONSTRAIN(idx, 0, channel_count(TYPE_INTERNAL) - 1);
    source = TYPE_INTERNAL | (idx & 0x1f);
  }

  void Reset(bool hard = false) {
    if (hard) div_mult.steps = 1;
    div_mult.Reset();
    last_gate_state = false;
    clkcount = 0;
  }
  void Clear() {
    source = 0;
    Reset(true);
  }

  bool Gate() const {
    switch (source_type()) {
      case TYPE_INTERNAL:
        // Formerly hardcoded clock sources.
        // Now, the ClockSetup applet provides virtual outputs I/J/K/L
        return frame.ViewOut(DAC_CHANNEL_COUNT + index()) > GATE_THRESHOLD;

      case TYPE_DIGITAL_INPUT:
        return frame.gate_high[index()];
      case TYPE_ADC:
        return frame.gate_high[DIGITAL_INPUT_COUNT + index()];
        // gate_high is already calculated for ADC
        //return frame.inputs[index()] > GATE_THRESHOLD;
      case TYPE_DAC:
        return frame.ViewOut(index()) > GATE_THRESHOLD;
      case TYPE_MIDI_MAP:
        return frame.MIDIState.mapping[index()].ViewOut() > GATE_THRESHOLD;
      case TYPE_NONE:
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
        tock = EuclideanFilter(e_length + 1, e_beats, e_rotate, clkcount);
      ++clkcount;
    }

    return tock;
  }

  uint8_t const* Icon() const {
    switch (source_type()) {
      case TYPE_INTERNAL:
        return (clkcount & 1) ? METRO_L_ICON : METRO_R_ICON;
      case TYPE_MIDI_MAP:
        return PhzIcons::midiIn;

      case TYPE_DIGITAL_INPUT:
        if (index() < 4)
          return DIGITAL_INPUT_ICONS + index() * 8;
      case TYPE_ADC:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + index()) * 8;
      case TYPE_DAC:
        if (index() < 8)
          return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_COUNT + index()) * 8;

        return ZAP_ICON;

      case TYPE_NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }
  char const* InputName() const {
    static char in_label[] = "T 1";
    const char type[] = {' ', '-', 'T', 'C', '#', 'M', '?', '*'};
    const char* clktype[] = {"CL1", "CL4", "RUN", "RST"};

    if (!source) return " - ";

    in_label[0] = type[source_type() >> 5];
    uint8_t idx = constrain(index(), 0, channel_count(source_type()));
    switch (source_type()) {
      case TYPE_INTERNAL:
        return clktype[idx];

      case TYPE_DAC:
        in_label[1] = OC::Strings::capital_letters[idx][0];
        in_label[2] = ' ';
        break;

      case TYPE_DIGITAL_INPUT:
      case TYPE_ADC:
      case TYPE_MIDI_MAP:
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

  uint32_t Pack() const {
    return (source & 0xFF) | as_unsigned(div_mult.steps << 8) | (e_length & 0x1f) << 16 | (e_beats & 0x3f) << 21 | (e_rotate & 0x1f) << 27;
  }

  void Unpack(uint32_t data) {
    source = data & 0xFF;

    // migrate old data; zero is still disabled; -1 and -2 are still valid
    if (source && (source >> 5) < 2) {
      if (source < 1 + DIGITAL_INPUT_COUNT)
        SetGateInput(source - 1);
      else if (source < 1 + DIGITAL_INPUT_COUNT + ADC_CHANNEL_COUNT)
        SetInput(source - DIGITAL_INPUT_COUNT - 1);
      else if (source < 1 + DIGITAL_INPUT_COUNT + ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT)
        SetOutput(source - DIGITAL_INPUT_COUNT - ADC_CHANNEL_COUNT - 1);
      else
        SetMidiMap(source - DIGITAL_INPUT_COUNT - ADC_CHANNEL_COUNT - DAC_CHANNEL_COUNT - 1);
    }

    // migrate old clock sources -1 and -2
    if (source_type() == TYPE_INTERNAL) {
      if (0xff == source) // "CL4"
        SetClockSource(1);
      if (0xfe == source) // "CL1"
        SetClockSource(0);
    }

    div_mult.Set(extract_value<int8_t>(data >> 8));
    if (0 == div_mult.steps) div_mult.steps = 1;

    e_length = (data >> 16) & 0x1f; // 5 bits
    e_beats  = (data >> 21) & 0x3f; // 6 bits
    e_rotate = (data >> 27) & 0x1f; // 5 bits
  }

  const bool enabled() const { return source != 0; }
  DigitalSourceType source_type() const {
    return (DigitalSourceType)(source & (0x7 << 5)); // upper 3 bits
  }
  inline uint8_t index() const {
    return source & 0x1f;
  }
  const bool is_clock() const {
    return (source_type() == TYPE_INTERNAL) && (index() <= 1);
  }
  constexpr int channel_count(DigitalSourceType t) const {
    switch (t) {
      default: return 0;

      case TYPE_NONE:
        return 1;

      case TYPE_INTERNAL:
        return 4; // 1x, 4x, RUN, RESET

      case TYPE_DIGITAL_INPUT:
        return DIGITAL_INPUT_COUNT;
      case TYPE_ADC:
        // strictly hardware inputs
        return ADC_CHANNEL_COUNT;
      case TYPE_DAC:
        // This one can have virtual signals.
        // 2 outputs per applet slot, including Clock and Audio Applets...
        return 2 * APPLET_CURSOR_COUNT;
      case TYPE_MIDI_MAP:
        return MIDIMAP_MAX;
    }
  }
};

// Let's PackingUtils know this is Packable as is.
constexpr DigitalInputMap& pack(DigitalInputMap& input) {
  return input;
}

extern DigitalInputMap trigmap[ADC_CHANNEL_COUNT];
extern CVInputMap cvmap[ADC_CHANNEL_COUNT];
