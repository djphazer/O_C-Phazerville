#pragma once

#include "HemisphereApplet.h"
#include "dsputils.h"
#include <AudioStream.h>

const int NUM_CV_INPUTS = ADC_CHANNEL_LAST * 2 + 1;
// We *could* reuse HS::input_quant for inputs, but easier to just do it
// independently, and semitone quantizers are lightwieght.
OC::SemitoneQuantizer cv_semitone_quants[NUM_CV_INPUTS];

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

  int SemitoneIn(int default_value = 0) {
    return cv_semitone_quants[source].Process(In(default_value));
  }

  int InRescaled(int max_value) {
    return Proportion(In(), HEMISPHERE_MAX_INPUT_CV, max_value);
  }

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, NUM_CV_INPUTS - 1);
  }

  char const* InputName() const {
    return OC::Strings::cv_input_names_none[source];
  }

  uint8_t const* Icon() const {
    return PARAM_MAP_ICONS + 8 * source;
  }
};

struct DigitalInput {
  enum DigitalSourceType {
    NONE,
    CLOCK,
    DIGITAL_INPUT,
    CV_OUTPUT,
  };

  int8_t source = 0;
  bool last_gate_state = true; // for detecting clocks
  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;
  static const int num_sources = 2 + OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST;

  void ChangeSource(int dir) {
    source = constrain(source + dir, 0, num_sources);
  }

  bool Gate() {
    switch (source_type()) {
      case CLOCK: {
        uint32_t ticks_since_beat = OC::CORE::ticks - clock_m.beat_tick;
        uint32_t tick_phase
          = (ppqn * ticks_since_beat) % clock_m.ticks_per_beat;
        bool gate
          = tick_phase < internal_clocked_gate_pw * clock_m.ticks_per_beat;
        return gate;
      }
      case DIGITAL_INPUT:
        return frame.gate_high[digital_input_index()];
      case CV_OUTPUT:
        return frame.outputs[cv_output_index()] > GATE_THRESHOLD;
      case NONE:
      default:
        return false;
    }
  }

  /**
   * Returns to on rising gate input. Will return true once and then go back to
   * false until the gate goes low again.
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
      case CV_OUTPUT:
        return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + cv_output_index()) * 8;
      case NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }

private:
  DigitalSourceType source_type() const {
    switch (source) {
      case 0:
        return NONE;
      case 1:
        return CLOCK;
      default: {
        if (source < 2 + OC::DIGITAL_INPUT_LAST) {
          return DIGITAL_INPUT;
        } else {
          return CV_OUTPUT;
        }
      }
    }
  }

  inline int8_t digital_input_index() const {
    return source - 2;
  }

  inline int8_t cv_output_index() const {
    return source - 2 - OC::DIGITAL_INPUT_LAST;
  }
};

// For ascii strings of 9 characters or less, will just be the ascii bits
// concatenated together. More characters than that and the xor plus misaligned
// shifting should avoid collisions.
constexpr uint64_t strhash(const char* str) {
  uint64_t id = 0;
  for (const char* c = str; *c != '\0'; c++) {
    id = (id << 7) | (id >> (64 - 7));
    id ^= (*c);
  }
  return id;
}

enum AudioChannels {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  // If applet_name() can return different things at different times, you *must*
  // override this or saving and loading won't work!
  virtual const uint64_t applet_id() {
    return strhash(applet_name());
  };
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;
  virtual void mainloop() {}

  void gfxPrintTuningIndicator(int16_t pitch) {
    // TODO this assumes pitch = C, which might not be true for some applets
    int semitone = pitch / 128;
    int offset = pitch - semitone * 128;
    if (offset >= 64) {
      offset = offset - 128;
      semitone++;
    }
    semitone = ((semitone % 12) + 12) % 12;
    int y = gfxGetPrintPosY();
    int x = gfxGetPrintPosX();
    gfxPos(x + 1, y);
    gfxPrintIcon(NOTE_NAMES + semitone * 8, 9);
    int pxOffset = 7 - (offset / 16 + 4);
    if (offset == 0) gfxInvert(x, y, 10, 8);
    else gfxDottedLine(x, y + pxOffset, x + 10, y + pxOffset);
  }

  void gfxPrintPitchHz(int16_t pitch, float base_freq = C3) {
    float freq = PitchToRatio(pitch) * base_freq;
    int shiftedFreq = static_cast<int>(roundf(freq * 10));
    int int_part = shiftedFreq / 10;
    int dec = shiftedFreq % 10;
    if (int_part > 9999) graphics.printf("%6d", int_part);
    else graphics.printf("%4d.%01d", int_part, dec);
    gfxPrintIcon(HZ);
    // gfxPrint("Hz");
  }
};
