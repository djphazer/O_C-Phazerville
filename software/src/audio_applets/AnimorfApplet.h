#pragma once
// "Animorf" - an emulation of the Moog MF-105M MIDI MuRF:
// 8 resonant bandpass filters animated by a pattern sequencer, with
// per-band level sliders, envelope morphing, drive, and array sweep.
//
// Controls:
//   Mix    0..100%        (AuxButton toggles Send mode)
//   Ptn    0..15          0 = static (no animation), 15 = random
//   Mode   Mids / Bass    filter bank voicing
//   Rate   0.1..20.0 Hz   animation clock; external clock overrides
//   Env    -100..+100     envelope speed; negative = reverse swells
//   Drv    0..100%        input drive into soft clipper
//   Swp    -24..+24       array frequency sweep in semitones
//   Bands  8 sliders, drawn as a live bar graph (active bands invert)
//
// Mix, Rate, Env and Sweep have CV input maps; the clock input steps the
// pattern directly (internal clock resumes ~2s after the last pulse).

#include "../Audio/effect_animorf.h"

class AnimorfApplet : public HemisphereAudioAppletF32 {
public:
  const char* applet_name() {
    return "Animorf";
  }

  void Start() {
    morfer.begin();
    morfer.setMonoSum(Channels() == MONO);
    ForEachChannel(ch) {
      if (Channels() == STEREO || ch == 0) {
        PatchCableF32(InputF32(), ch, InputStream(), ch); // converter

        PatchCable(InputStream(), ch, morfer, ch);
        PatchCable(morfer, ch, wetdry[ch], WD_WET_CH);
        PatchCable(InputStream(), ch, wetdry[ch], WD_DRY_CH);
        PatchCable(wetdry[ch], 0, OutputStream(), ch);

        PatchCableF32(OutputStream(), ch, OutputF32(), ch); // converter
      }
    }
    clock_source.Clear(); // none by default; internal rate runs free
  }

  void Unload() {
    morfer.end();
    AllowRestart();
  }

  void Controller() {
    ++ticks_since_clock;
    if (clock_source.Clock()) {
      morfer.stepNow();
      ticks_since_clock = 0;
    }
    // suppress the internal clock while an external one is active (~2s)
    morfer.setInternalStepping(ticks_since_clock > 33333);

    const float mix01 =
      constrain(0.01f * wet + mix_cv.InF(), 0.0f, 1.0f);
    const float rate_hz =
      constrain(0.1f * rate_x10 + 10.0f * rate_cv.InF(), 0.05f, 30.0f);
    const float env01 =
      constrain(0.01f * env + env_cv.InF(), -1.0f, 1.0f);
    const float sweep01 =
      constrain(sweep * (1.0f / 24.0f) + sweep_cv.InF(), -1.0f, 1.0f);

    morfer.setPattern(pattern);
    morfer.setBassMode(bass_mode);
    morfer.setRate(rate_hz);
    morfer.setEnv(env01);
    morfer.setDrive(0.01f * drive);
    morfer.setSweep(sweep01);
    for (int i = 0; i < 8; ++i) morfer.setBandLevel(i, 0.01f * bands[i]);

    float dry_gain, wet_gain;
    EqualPowerFade(dry_gain, wet_gain, mix01);
    ForEachChannel(ch) {
      if (Channels() == STEREO || ch == 0) {
        wetdry[ch].gain(WD_WET_CH, wet_gain);
        wetdry[ch].gain(WD_DRY_CH, send_mode ? 1.0f : dry_gain);
      }
    }
  }

  void View() {
    const int row = RowOfCursor();
    int scroll = row - (kVisibleRows - 1);
    if (scroll < 0) scroll = 0;
    if (scroll > ROWS - kVisibleRows) scroll = ROWS - kVisibleRows;
    int y = 15;
    for (int r = scroll; r < scroll + kVisibleRows && r < ROWS; ++r, y += 10) {
      DrawRow(r, y);
    }
    gfxDisplayInputMapEditor();
  }

  void AuxButton() override {
    if (cursor == MIX) send_mode ^= 1;
    CancelEdit();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(MIX_CV, mix_cv),
          IndexedInput(RATE_CV, rate_cv),
          IndexedInput(ENV_CV, env_cv),
          IndexedInput(SWEEP_CV, sweep_cv)
        ))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
      return;
    }
    if (EditSelectedInputMap(direction)) return;

    knob_accel += direction - direction * (millis_since_turn / 10);
    if (direction * knob_accel <= 0) knob_accel = direction;
    CONSTRAIN(knob_accel, -100, 100);

    switch (cursor) {
      case MIX:
        wet = constrain(wet + direction, 0, 100);
        break;
      case MIX_CV:
        mix_cv.ChangeSource(direction);
        break;
      case PATTERN:
        pattern = constrain(pattern + direction, 0, 15);
        break;
      case MODE:
        bass_mode ^= 1;
        break;
      case RATE:
        rate_x10 = constrain(rate_x10 + knob_accel, 1, 200);
        break;
      case RATE_CV:
        rate_cv.ChangeSource(direction);
        break;
      case CLOCK_SRC:
        clock_source.ChangeSource(direction);
        break;
      case ENV:
        env = constrain(env + direction, -100, 100);
        break;
      case ENV_CV:
        env_cv.ChangeSource(direction);
        break;
      case DRIVE:
        drive = constrain(drive + direction, 0, 100);
        break;
      case SWEEP:
        sweep = constrain(sweep + direction, -24, 24);
        break;
      case SWEEP_CV:
        sweep_cv.ChangeSource(direction);
        break;
      default:
        if (cursor >= BAND1 && cursor <= BAND8) {
          int8_t& b = bands[cursor - BAND1];
          b = constrain(b + knob_accel, 0, 100);
        }
        break;
    }
    millis_since_turn = 0;
  }

#define ANIMORFER_MAIN_PARAMS \
  wet, env, drive, sweep, rate_x10, pack<4>(pattern), pack<1>(bass_mode), \
  pack<1>(send_mode)

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(ANIMORFER_MAIN_PARAMS);
    data[1] = PackPackables(mix_cv, rate_cv, env_cv, sweep_cv);
    data[2] = PackPackables(
      bands[0], bands[1], bands[2], bands[3], bands[4], bands[5], bands[6],
      bands[7]
    );
    data[3] = PackPackables(clock_source);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], ANIMORFER_MAIN_PARAMS);
    UnpackPackables(data[1], mix_cv, rate_cv, env_cv, sweep_cv);
    UnpackPackables(
      data[2], bands[0], bands[1], bands[2], bands[3], bands[4], bands[5],
      bands[6], bands[7]
    );
    UnpackPackables(data[3], clock_source);
  }

protected:
  void SetHelp() {}

private:
  enum Cursor {
    MIX,
    MIX_CV,
    PATTERN,
    MODE,
    RATE,
    RATE_CV,
    CLOCK_SRC,
    ENV,
    ENV_CV,
    DRIVE,
    SWEEP,
    SWEEP_CV,
    BAND1, BAND2, BAND3, BAND4, BAND5, BAND6, BAND7, BAND8,
    CURSOR_LENGTH,
  };

  static const int ROWS = 7;
  static const int kVisibleRows = 5;
  static const uint8_t WD_DRY_CH = 0;
  static const uint8_t WD_WET_CH = 1;

  int RowOfCursor() const {
    switch (cursor) {
      case MIX: case MIX_CV: return 0;
      case PATTERN: case MODE: return 1;
      case RATE: case RATE_CV: case CLOCK_SRC: return 2;
      case ENV: case ENV_CV: return 3;
      case DRIVE: return 4;
      case SWEEP: case SWEEP_CV: return 5;
      default: return 6;
    }
  }

  void DrawRow(int r, int y) {
    const int rx = 63 - 8;
    switch (r) {
      case 0:
        gfxPrint(1, y, send_mode ? "Snd:" : "Mix:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%3d%%", wet);
        gfxEndCursor(cursor == MIX, true);
        gfxStartCursor();
        gfxPrint(mix_cv);
        gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());
        break;
      case 1:
        gfxPrint(1, y, "Pt:");
        gfxStartCursor();
        if (pattern == 0) gfxPrint("St");
        else if (pattern == 15) gfxPrint("Rn");
        else graphics.printf("%2d", pattern);
        gfxEndCursor(cursor == PATTERN);
        gfxPrint(" ");
        gfxStartCursor();
        gfxPrint(bass_mode ? "Bass" : "Mids");
        gfxEndCursor(cursor == MODE);
        break;
      case 2: {
        gfxPrint(1, y, "Rt:");
        const int rt = rate_x10; // tenths of Hz
        gfxStartCursor();
        graphics.printf("%2d.%01d", rt / 10, rt % 10);
        gfxEndCursor(cursor == RATE);
        gfxStartCursor();
        gfxPrint(rate_cv);
        gfxEndCursor(cursor == RATE_CV, false, rate_cv.InputName());
        gfxStartCursor();
        gfxPrint(clock_source);
        gfxEndCursor(cursor == CLOCK_SRC);
        break;
      }
      case 3:
        gfxPrint(1, y, "Env:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%4d", env);
        gfxEndCursor(cursor == ENV);
        gfxStartCursor();
        gfxPrint(env_cv);
        gfxEndCursor(cursor == ENV_CV, false, env_cv.InputName());
        break;
      case 4:
        gfxPrint(1, y, "Drv:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%3d%%", drive);
        gfxEndCursor(cursor == DRIVE);
        break;
      case 5:
        gfxPrint(1, y, "Swp:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%4d", sweep);
        gfxEndCursor(cursor == SWEEP);
        gfxStartCursor();
        gfxPrint(sweep_cv);
        gfxEndCursor(cursor == SWEEP_CV, false, sweep_cv.InputName());
        break;
      case 6:
        // live band bars: height = slider level, inverted while sounding
        for (int i = 0; i < 8; ++i) {
          const int x = 2 + i * 7;
          const int h = 1 + (bands[i] * 6) / 100; // 1..7 px
          gfxRect(x, y + 7 - h, 5, h);
          if (morfer.bandEnv(i) > 0.1f) gfxInvert(x, y - 1, 5, 9);
          if (cursor == BAND1 + i) gfxCursor(x, y + 8, 5);
        }
        break;
    }
  }

  int cursor = MIX;

  // parameters
  int8_t wet = 60;         // 0..100
  int8_t env = 30;         // -100..100, negative = reverse swells
  int8_t drive = 20;       // 0..100
  int8_t sweep = 0;        // -24..24 semitones
  uint8_t rate_x10 = 20;   // 1..200 -> 0.1..20.0 Hz
  int8_t pattern = 1;      // 0..15
  bool bass_mode = false;
  bool send_mode = false;
  int8_t bands[8] = {100, 100, 100, 100, 100, 100, 100, 100};

  CVInputMap mix_cv;
  CVInputMap rate_cv;
  CVInputMap env_cv;
  CVInputMap sweep_cv;
  DigitalInputMap clock_source;
  uint32_t ticks_since_clock = 0;

  int16_t knob_accel = 0;
  elapsedMillis millis_since_turn;

  AudioEffectAnimorf morfer;
  AudioMixer<2> wetdry[STEREO];
};
