#pragma once

#include "AudioDelayExt.h"
#include "AudioPassthrough.h"
#include "HSicons.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include <Audio.h>

class DelayApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "Delay";
  }
  void Start() {
    delay.Acquire();

    for (int i = 0; i < 4; i++) {
      taps_conns[i].connect(delay, i, taps_mixer1, i);
      int j = i + 4;
      taps_conns[j].connect(delay, j, taps_mixer2, i);
    }
  }

  void Unload() {
    delay.Release();
    AllowRestart();
  }

  void Controller() {
    clock_count++;
    if (Clock(0)) {
      clock_base_secs = clock_count / 16666.0f;
      clock_count = 0;
    }

    if (Clock(1)) frozen = !frozen;

    float d = DelaySecs(delay_time + delay_cv.Process(In(0)));
    for (int tap = 0; tap < taps; tap++) {
      float t = d * (tap + 1.0f) / taps;
      CONSTRAIN(d, 0.0f, MAX_DELAY_SECS);
      switch (delay_mod_type) {
        case CROSSFADE:
          delay.cf_delay(tap, t);
          break;
        case STRETCH:
          delay.delay(tap, t);
          break;
      }
    }

    float tap_gain = 1.0f / taps;
    if (frozen) {
      input_amp.gain(0.0f);
      delay.feedback(8, 1.0f);
      for (int tap = 0; tap < 8; tap++) {
        delay.feedback(tap, 0.0f);
      }
    } else {
      input_amp.gain(1.0f);
      float f = 0.01f * feedback * tap_gain;
      for (int tap = 0; tap < 9; tap++) {
        delay.feedback(tap, tap < taps ? f : 0.0f);
      }
    }

    for (int i = 0; i < 4; i++) {
      taps_mixer1.gain(i, i < taps ? tap_gain : 0.0f);
      int j = i + 4;
      taps_mixer2.gain(i, j < taps ? tap_gain : 0.0f);
    }

    float w = 0.01f * wet + InF(1);
    CONSTRAIN(w, 0.0f, 1.0f);
    wet_dry_mixer.gain(WD_WET_CH_1, w);
    wet_dry_mixer.gain(WD_WET_CH_2, w);
    wet_dry_mixer.gain(WD_DRY_CH, 1.0f - w);
  }

  void View() {
    // gfxPrint(0, 15, delaySecs * 1000.0f, 0);
    int unit_x = 6 * 8;
    switch (time_units) {
      case SECS:
        gfxPrint(0, 15, DelaySecs(delay_time) * 1000.0f, 0);
        gfxPrint(unit_x, 15, "ms");
        break;
      case HZ:
        gfxPrint(0, 15, 1.0f / DelaySecs(delay_time), 2);
        gfxPrint(unit_x, 15, "Hz");
        break;
      case CLOCK:
        float r = DelayRatio(delay_time);
        if (r < 1.0f) {
          gfxPrint(0, 15, "/ ");
          gfxPrint(roundf(1.0f / r), 0);
        } else {
          gfxPrint(0, 15, "X ");
          gfxPrint(roundf(r), 0);
        }
        gfxIcon(unit_x, 15, CLOCK_ICON);
        break;
    }
    if (cursor == TIME) gfxCursor(0, 23, 6 * 6);
    if (cursor == TIME_UNITS) gfxCursor(unit_x, 23, 2 * 6);

    gfxPrint(0, 25, "FB: ");
    gfxPos(60 - 5 * 6, 25);
    graphics.printf("%3d%%", feedback);
    if (cursor == FEEDBACK) gfxCursor(60 - 4 * 6, 32, 4 * 6);

    gfxIcon(54, 25, LOOP_ICON);
    if (frozen) gfxInvert(54, 25, 8, 8);
    if (cursor == FREEZE) gfxCursor(54, 32, 8);

    gfxPrint(0, 35, "Wet: ");
    gfxPos(60 - 5 * 6, 35);
    graphics.printf("%3d%%", wet);
    if (cursor == WET) gfxCursor(60 - 4 * 6, 42, 4 * 6);

    gfxPrint(0, 45, "Taps: ");
    gfxPrint(54, 45, taps);
    if (cursor == TAPS) gfxCursor(54, 52, 6);

    switch (delay_mod_type) {
      case CROSSFADE:
        gfxPrint(0, 55, "Crossfade");
        break;
      case STRETCH:
        gfxPrint(0, 55, "Stretch");
        break;
    }
    if (cursor == TIME_MOD) gfxCursor(0, 62, 9 * 6);
  }

  void OnButtonPress() {
    if (cursor == FREEZE) {
      frozen = !frozen;
    } else {
      CursorToggle();
    }
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
      return;
    }

    knob_accel += direction - direction * (millis_since_turn / 10);
    if (direction * knob_accel <= 0) knob_accel = direction;
    CONSTRAIN(knob_accel, -100, 100);

    switch (cursor) {
      case TIME:
        if (time_units == CLOCK) {
          delay_time += static_cast<float>(direction) / RATIO_SCALAR;
        } else {
          float cur_delay = DelaySecs(delay_time);
          while (DelaySecs(delay_time) == cur_delay)
            delay_time += knob_accel;
        }
        CONSTRAIN(delay_time, INT16_MIN, INT16_MAX);
        break;
      case TIME_UNITS:
        time_units += direction;
        CONSTRAIN(time_units, 0, TIME_REP_LENGTH - 1);
        break;
      case TIME_MOD:
        delay_mod_type += direction;
        CONSTRAIN(delay_mod_type, 0, 1);
        break;
      case FEEDBACK:
        feedback += direction;
        CONSTRAIN(feedback, 0, 100);
        break;
      case WET:
        wet += direction;
        CONSTRAIN(wet, 0, 100);
        break;
      case TAPS:
        taps += direction;
        CONSTRAIN(taps, 1, 8);
      case CURSOR_LENGTH:
        break;
    }
    millis_since_turn = 0;
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, delay_loc, delay_time);
    Pack(data, time_rep_loc, time_units);
    Pack(data, wet_loc, wet);
    Pack(data, fb_loc, feedback);
    Pack(data, taps_loc, taps - 1);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    if (data != 0) {
      delay_time = Unpack(data, delay_loc);
      time_units = Unpack(data, time_rep_loc);
      wet = Unpack(data, wet_loc);
      feedback = Unpack(data, fb_loc);
      taps = Unpack(data, taps_loc) + 1;
    }
  }

  AudioStream* InputStream() {
    return &input_stream;
  }

  AudioStream* OutputStream() {
    return &wet_dry_mixer;
  }

  float DelaySecs(int16_t raw) {
    // we need duration, not frequency: negating pitch gets that faster than
    // `1.0f /`
    float s;
    switch (time_units) {
      case HZ:
        s = (PitchToRatio(-raw)) / 220.0f;
        break;
      case CLOCK:
        s = clock_base_secs / (DelayRatio(raw));
        break;
      default:
      case SECS:
        s = 0.500f + raw / 1000.0f; // default to 500ms
        break;
    }
    CONSTRAIN(s, 0.0f, MAX_DELAY_SECS);
    return s;
  }

  // /4 per v
  static constexpr float RATIO_SCALAR = 4.0f / (HEMISPHERE_3V_CV / 3.0f);
  float DelayRatio(int16_t raw) {
    float ratio = roundf(raw * RATIO_SCALAR);
    if (ratio == 0.0f) return 1.0f;
    else if (ratio > 0.0f) return 1.0f / (1.0f + ratio);
    else return 1.0f - ratio;
  }

  int16_t RatioToDelay(float ratio) {
    if (ratio < 1.0f) {
      return static_cast<int16_t>((1.0f / ratio - 1.0f) / RATIO_SCALAR);
    } else {
      return static_cast<int16_t>((1.0f - ratio) / RATIO_SCALAR);
    }
  }

protected:
  void SetHelp() {}

private:
  enum Cursor {
    TIME,
    TIME_UNITS,
    FEEDBACK,
    FREEZE,
    WET,
    TAPS,
    TIME_MOD,
    CURSOR_LENGTH,
  };

  enum TimeUnits {
    SECS,
    CLOCK,
    HZ,
    TIME_REP_LENGTH,
  };

  enum TimeMod {
    CROSSFADE,
    STRETCH,
  };

  int cursor = TIME;

  // only uses 16 bits, but do 32 to make CONSTRAIN easier
  int32_t delay_time = 0;
  uint8_t time_units = 0;
  // Only need 7 bits on these but the sign makes CONSTRAIN work
  int8_t wet = 50;
  int8_t feedback = 0;
  int8_t taps = 1;
  int8_t delay_mod_type = CROSSFADE;

  static constexpr PackLocation delay_loc{0, 16};
  static constexpr PackLocation time_rep_loc{16, 4};
  static constexpr PackLocation wet_loc{32, 7};
  static constexpr PackLocation fb_loc{39, 7};
  static constexpr PackLocation taps_loc{46, 3};

  NoiseSuppressor delay_cv{
    0.0f,
    0.05f, // This needs checking against various sequencers and such
    // 16 determined empirically by checking typical range with static
    // voltages
    16.0f,
    64, // a little less than 4ms
  };

  uint32_t clock_count = 0;
  float clock_base_secs = 0.0f;

  boolean frozen = false;

  int16_t knob_accel = 0;
  elapsedMillis millis_since_turn;

  const uint8_t WD_DRY_CH = 0;
  const uint8_t WD_WET_CH_1 = 1;
  const uint8_t WD_WET_CH_2 = 2;

  // Uses 1MB of psram and gives just under 12 secs of delay time.
  static const size_t DELAY_LENGTH = 1024 * 512;
  static constexpr float MAX_DELAY_SECS = DELAY_LENGTH / AUDIO_SAMPLE_RATE;

  AudioPassthrough<MONO> input_stream;
  AudioAmplifier input_amp;
  // 9th tap is freeze head
  AudioDelayExt<DELAY_LENGTH, 9> delay;
  AudioConnection input_to_amp{input_stream, input_amp};
  AudioConnection amp_to_delay{input_amp, delay};
  AudioConnection taps_conns[8];
  AudioMixer4 taps_mixer1;
  AudioMixer4 taps_mixer2;

  AudioMixer4 wet_dry_mixer;
  AudioConnection wet_conn1{taps_mixer1, 0, wet_dry_mixer, WD_WET_CH_1};
  AudioConnection wet_conn2{taps_mixer2, 0, wet_dry_mixer, WD_WET_CH_2};
  AudioConnection dry_conn{input_stream, 0, wet_dry_mixer, WD_DRY_CH};
};
