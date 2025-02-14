#pragma once

#include "HSUtils.h"
#include "HemisphereApplet.h"
#include "dsputils.h"
#include "CVInputMap.h"
#include <AudioStream.h>
#include <cstdint>
#include <type_traits>

struct DigitalInputMap {
  enum DigitalSourceType {
    NONE,
    CLOCK,
    DIGITAL_INPUT,
    CV_OUTPUT,
  };

  int8_t source = 0;
  int8_t division = 0; // -2 = /3, -1 = /2, 0 = x1, 1 = x2, 2 = x3...
  bool last_gate_state = true; // for detecting clocks
  static const int ppqn = 4;
  static constexpr float internal_clocked_gate_pw = 0.5f;
  static const int num_sources = 2 + OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST;

  static constexpr size_t Size = 16; // Make this compatible with Packable

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
      case CV_OUTPUT:
        return PARAM_MAP_ICONS + (1 + ADC_CHANNEL_LAST + cv_output_index()) * 8;
      case NONE:
      default:
        return PARAM_MAP_ICONS + 0;
    }
  }

  uint16_t Pack() const {
    return source | as_unsigned(division << 8);
  }

  void Unpack(uint16_t data) {
    source = data & 0xFF;
    division = extract_value<int8_t>(data >> 8);
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

// Let's PackingUtils know this is Packable as is.
constexpr DigitalInputMap& pack(DigitalInputMap& input) {
  return input;
}

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
  static const uint_fast8_t CONFIG_SIZE = 4;
  // If applet_name() can return different things at different times, you
  // *must* override this or saving and loading won't work!
  virtual const uint64_t applet_id() {
    return strhash(applet_name());
  };
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;
  virtual void mainloop() {}

  virtual void OnDataReceive(uint64_t data) {
    Serial.println("Warning: default OnDataReceive() called; either override this or OnDataReceive(const std::<array<uint64_t, CONFIG_SIZE>& data)");
  }
  virtual uint64_t OnDataRequest() {
    Serial.println("Warning: default OnDataRequest() called; either override this or OnDataRequest(std::<array<uint64_t, CONFIG_SIZE>& data)");
    return 0;
  }
  virtual void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) {
    OnDataReceive(data[0]);
  }
  virtual void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) {
    data[0] = OnDataRequest();
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
  }

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
