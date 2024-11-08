#pragma once

#include "HSicons.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "Audio/AudioDelayExt.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include <Audio.h>

class DelayApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "Delay";
  }
  void Start() {
    delay.Acquire();

    for (int i = 0; i < 8; i++) {
      taps_conns[i].connect(delay, i, taps_mixer, i);
    }
    set_taps(taps);
  }

  void Unload() {
    delay.Release();
    AllowRestart();
  }

  void Controller() {
    clock_count++;
    if (clock_source.Clock()) {
      clock_base_secs = clock_count / 16666.0f;
      clock_count = 0;
    }

    // if (Clock(1)) frozen = !frozen;

    float d;
    switch (time_units) {
      case HZ:
        d = DelaySecsFromPitch(
          delay_time + delay_cv.Process(delay_time_cv.In())
        );
        break;
      case CLOCK:
        d = DelaySecsFromRatio(
          ratio + semitones_to_div(delay_time_cv.SemitoneIn())
        );
        break;
      default:
      case SECS:
        d = DelaySecsFromMs(delay_time + delay_cv.Process(delay_time_cv.In()));
        break;
    }
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

    if (frozen) {
      input_amp.gain(0.0f);
      delay.feedback(8, 1.0f);
      for (int tap = 0; tap < 8; tap++) {
        delay.feedback(tap, 0.0f);
      }
    } else {
      input_amp.gain(1.0f);
      float fb
        = constrain((0.01f * feedback + feedback_cv.InF()) / taps, 0.0, 2.0f);
      for (int tap = 0; tap < 9; tap++) {
        delay.feedback(tap, tap < taps ? fb : 0.0f);
      }
    }

    float w = constrain(0.01f * wet + wet_cv.InF(), 0.0f, 1.0f);
    CONSTRAIN(w, 0.0f, 1.0f);
    wet_dry_mixer.gain(WD_WET_CH, w);
    wet_dry_mixer.gain(WD_DRY_CH, 1.0f - w);
  }

  void View() {
    // gfxPrint(0, 15, delaySecs * 1000.0f, 0);
    int unit_x = 6 * 7 + 1;
    gfxPos(1, 15);

    switch (time_units) {
      case SECS:
        gfxStartCursor();
        graphics.printf("  %5d", delay_time);
        gfxEndCursor(cursor == TIME);

        gfxStartCursor(unit_x, 15);
        gfxPrint("ms");
        gfxEndCursor(cursor == TIME_UNITS);
        break;
      case HZ:
        gfxStartCursor();
        graphics.printf(
          "%5d.%01d", SPLIT_INT_DEC(1.0f / DelaySecsFromPitch(delay_time), 10)
        );
        gfxEndCursor(cursor == TIME);

        gfxStartCursor(unit_x, 15);
        gfxPrint(unit_x, 15, "Hz");
        gfxEndCursor(cursor == TIME_UNITS);
        break;
      case CLOCK:
        gfxStartCursor();
        gfxPrintIcon(clock_source.Icon());
        gfxEndCursor(cursor == CLOCK_SOURCE);
        gfxPrint(" ");

        gfxStartCursor();
        if (ratio < 0) {
          graphics.printf("X %d", -ratio + 1);
        } else {
          graphics.printf("/ %d", ratio + 1);
        }
        gfxEndCursor(cursor == TIME);

        gfxStartCursor(unit_x, 15);
        gfxPrintIcon(CLOCK_ICON);
        gfxEndCursor(cursor == TIME_UNITS);
        break;
    }

    gfxStartCursor(unit_x + 2 * 6, 15);
    gfxPrintIcon(delay_time_cv.Icon());
    gfxEndCursor(cursor == TIME_CV);

    int param_right_x = 63 - 8;
    gfxPrint(1, 25, "FB:");
    gfxStartCursor(param_right_x - 4 * 6, 25);
    graphics.printf("%3d%%", feedback);
    gfxEndCursor(cursor == FEEDBACK);

    gfxStartCursor();
    gfxPrintIcon(feedback_cv.Icon());
    gfxEndCursor(cursor == FEEDBACK_CV);

    // gfxIcon(54, 25, LOOP_ICON);
    // if (frozen) gfxInvert(54, 25, 8, 8);
    // if (cursor == FREEZE) gfxCursor(54, 32, 8);

    gfxPrint(1, 35, "Wet:");
    gfxStartCursor(param_right_x - 4 * 6, 35);
    graphics.printf("%3d%%", wet);
    gfxEndCursor(cursor == WET);

    gfxStartCursor();
    gfxPrintIcon(wet_cv.Icon());
    gfxEndCursor(cursor == WET_CV);

    gfxPrint(1, 45, "Taps:");
    gfxStartCursor(param_right_x - 2 * 6, 45);
    gfxPrint(taps);
    gfxEndCursor(cursor == TAPS);

    gfxStartCursor(1, 55);
    gfxPrint(delay_mod_type == CROSSFADE ? "Crossfade" : "Stretch  ");
    gfxEndCursor(cursor == TIME_MOD);
  }

  void OnButtonPress() {
    // if (cursor == FREEZE) {
    //   frozen = !frozen;
    // } else {
    CursorToggle();
    // }
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
      if (cursor == CLOCK_SOURCE && time_units != CLOCK)
        MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
      return;
    }

    knob_accel += direction - direction * (millis_since_turn / 10);
    if (direction * knob_accel <= 0) knob_accel = direction;
    CONSTRAIN(knob_accel, -100, 100);

    switch (cursor) {
      case TIME:
        switch (time_units) {
          case CLOCK:
            ratio = constrain(ratio + direction, -127, 127);
            break;
          case HZ:
            // Adjust by 1/8 semitone, and ensure we hit semitones.
            delay_time /= 16;
            delay_time += knob_accel;
            delay_time *= 16;

            // -8 * 12 * 128 -> ~1 Hz
            // 6 * 12 * 128 -> ~17kHz
            CONSTRAIN(delay_time, -8 * 12 * 128, 6 * 12 * 128);
            break;
          case SECS:
            delay_time += knob_accel;
            CONSTRAIN(
              delay_time, 1, static_cast<int>(MAX_DELAY_SECS * 1000) - 1
            );
            break;
        }
        break;
      case CLOCK_SOURCE:
        clock_source.ChangeSource(direction);
        break;
      case TIME_UNITS:
        time_units = constrain(time_units + direction, 0, TIME_REP_LENGTH - 1);
        break;
      case TIME_CV:
        delay_time_cv.ChangeSource(direction);
        break;
      case TIME_MOD:
        delay_mod_type += direction;
        CONSTRAIN(delay_mod_type, 0, 1);
        break;
      case FEEDBACK:
        feedback += direction;
        CONSTRAIN(feedback, 0, 100);
        break;
      case FEEDBACK_CV:
        feedback_cv.ChangeSource(direction);
        break;
      case WET:
        wet += direction;
        CONSTRAIN(wet, 0, 100);
        break;
      case WET_CV:
        wet_cv.ChangeSource(direction);
        break;
      case TAPS:
        set_taps(taps + direction);
        break;
      case CURSOR_LENGTH:
        break;
    }
    millis_since_turn = 0;
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, delay_loc, delay_time);
    Pack(data, time_rep_loc, time_units);
    Pack(data, ratio_loc, ratio);
    Pack(data, wet_loc, wet);
    Pack(data, fb_loc, feedback);
    Pack(data, taps_loc, taps - 1);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    if (data != 0) {
      delay_time = Unpack(data, delay_loc);
      time_units = Unpack(data, time_rep_loc);
      ratio = Unpack(data, ratio_loc);
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

  float DelaySecsFromPitch(int pitch) {
    return constrain(PitchToRatio(-pitch) / (C3 * 2), 0.0f, MAX_DELAY_SECS);
  }

  float DelaySecsFromMs(int ms) {
    return constrain(0.001f * ms, 0.0f, MAX_DELAY_SECS);
  }

  float DelaySecsFromRatio(int ratio) {
    return constrain(clock_base_secs * DelayRatio(ratio), 0.0f, MAX_DELAY_SECS);
  }

  float DelayRatio(int16_t ratio) {
    return ratio < 0 ? 1.0f / (-ratio + 1) : ratio + 1;
  }

protected:
  void SetHelp() {}

  void set_taps(size_t t) {
    taps = t;
    CONSTRAIN(taps, 1, 8);
    float tap_gain = 1.0f / taps;
    for (int i = 0; i < taps; i++)
      taps_mixer.gain(i, tap_gain);
    for (int i = taps; i < 8; i++)
      taps_mixer.gain(i, 0.0f);
  }

private:
  enum Cursor {
    CLOCK_SOURCE,
    TIME,
    TIME_UNITS,
    TIME_CV,
    FEEDBACK,
    FEEDBACK_CV,
    // FREEZE,
    WET,
    WET_CV,
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

  int delay_time = 500;
  CVInput delay_time_cv;
  int16_t ratio = 0;
  DigitalInput clock_source;
  uint8_t time_units = 0;
  // Only need 7 bits on these but the sign makes CONSTRAIN work
  int8_t feedback = 0;
  CVInput feedback_cv;
  int8_t wet = 50;
  CVInput wet_cv;
  int8_t taps = 1;
  int8_t delay_mod_type = CROSSFADE;

  static constexpr PackLocation delay_loc{0, 16};
  static constexpr PackLocation time_rep_loc{16, 3};
  static constexpr PackLocation ratio_loc{19, 8};
  static constexpr PackLocation delay_time_cv_loc{27, 5};
  static constexpr PackLocation wet_loc{32, 7};
  static constexpr PackLocation fb_loc{39, 7};
  static constexpr PackLocation taps_loc{46, 3};
  static constexpr PackLocation clock_source_loc{49, 5};
  static constexpr PackLocation feedback_cv_loc{54, 5};
  static constexpr PackLocation wet_cv_loc{59, 5};

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
  const uint8_t WD_WET_CH = 1;

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
  AudioMixer<8> taps_mixer;

  AudioMixer<2> wet_dry_mixer;
  AudioConnection wet_conn{taps_mixer, 0, wet_dry_mixer, WD_WET_CH};
  AudioConnection dry_conn{input_stream, 0, wet_dry_mixer, WD_DRY_CH};

  // [-4,-2]=>-1, [-1,1]=>0, [2,4]=>1, etc
  int16_t semitones_to_div(int16_t semis) {
    return (semis + 32767) / 3 - 10922;
  }
};
