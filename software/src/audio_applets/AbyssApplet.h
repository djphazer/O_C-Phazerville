#pragma once
// A huge reverb in the style of the Eventide Blackhole.
//
// Controls:
//   Mix   0..100%          (AuxButton toggles Send mode, like Delay)
//   Grv   -100..+100  gravity/decay; negative = inverse (swelling) mode
//   Size  0..100%          tank scale 0.35x..2.5x
//   Pre   0..500 ms        pre-delay
//   Mod   0..100%          tank modulation depth
//   Rate  0.05..3.00 Hz    modulation rate
//   Lo / Hi 0..10          in-loop low cut / high damping
//
// Mix, Gravity, Size and Mod have CV input maps.

#include "../Audio/effect_abyss.h"

class AbyssApplet : public HemisphereAudioAppletF32 {
public:
  const char* applet_name() {
    return "Abyss";
  }

  void Start() final;

  void Unload() {
    reverb.end();
    AllowRestart();
  }

  void Controller() {
    const float mix01 = constrain(0.01f * wet + mix_cv.InF(), 0.0f, 1.0f);
    const float grav = constrain(0.01f * gravity + grav_cv.InF(), -1.0f, 1.0f);
    const float size01 = constrain(0.01f * size + size_cv.InF(), 0.0f, 1.0f);
    const float mod01 = constrain(0.01f * mod_depth + mod_cv.InF(), 0.0f, 1.0f);

    reverb.setGravity(grav);
    reverb.setSize(size01);
    reverb.setModDepth(mod01);
    reverb.setModRate(0.05f * mod_rate);
    reverb.setPredelayMs(2.0f * predelay_2ms);
    reverb.setLoCut(0.1f * locut);
    reverb.setHiDamp(0.1f * hidamp);

    float dry_gain, wet_gain;
    EqualPowerFade(dry_gain, wet_gain, mix01);
    reverb.setInputGain(send_mode ? wet_gain : 1.0f);
    ForEachChannel(ch) {
      if (Channels() == STEREO || ch == 0) {
        if (send_mode) {
          // wet & dry pass thru at unity; Mix sets how much goes into the tank.
          wetdry[ch].gain(WD_WET_CH, 1.0f);
          wetdry[ch].gain(WD_DRY_CH, 1.0f);
        } else {
          wetdry[ch].gain(WD_WET_CH, wet_gain);
          wetdry[ch].gain(WD_DRY_CH, dry_gain);
        }
      }
    }
  }

  void View() final;

  void AuxButton() override {
    if (cursor == MIX) send_mode ^= 1;
    CancelEdit();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(MIX_CV, mix_cv),
          IndexedInput(GRAVITY_CV, grav_cv),
          IndexedInput(SIZE_CV, size_cv),
          IndexedInput(MOD_CV, mod_cv)
        ))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) final;

#define BLACKHOLE_PARAMS \
  wet, gravity, size, mod_depth, mod_rate, locut, hidamp, predelay_2ms

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(BLACKHOLE_PARAMS);
    data[1] = PackPackables(mix_cv, grav_cv, size_cv, mod_cv);
    data[2] = PackPackables(pack<1>(send_mode));
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], BLACKHOLE_PARAMS);
    UnpackPackables(data[1], mix_cv, grav_cv, size_cv, mod_cv);
    UnpackPackables(data[2], pack<1>(send_mode));
  }

protected:
  void SetHelp() {}

private:
  enum Cursor {
    MIX,
    MIX_CV,
    GRAVITY,
    GRAVITY_CV,
    SIZE,
    SIZE_CV,
    PREDELAY,
    MOD_DEPTH,
    MOD_CV,
    MOD_RATE,
    LO_CUT,
    HI_DAMP,
    CURSOR_LENGTH,
  };

  static const int ROWS = 8;
  static const int kVisibleRows = 5;
  static const uint8_t WD_DRY_CH = 0;
  static const uint8_t WD_WET_CH = 1;

  int RowOfCursor() const {
    switch (cursor) {
      case MIX: case MIX_CV: return 0;
      case GRAVITY: case GRAVITY_CV: return 1;
      case SIZE: case SIZE_CV: return 2;
      case PREDELAY: return 3;
      case MOD_DEPTH: case MOD_CV: return 4;
      case MOD_RATE: return 5;
      case LO_CUT: return 6;
      default: return 7;
    }
  }

  void DrawRow(int r, int y) {
    const int rx = 63 - 8; // right edge for values, like DelayApplet
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
        gfxPrint(1, y, "Grv:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%4d", gravity);
        gfxEndCursor(cursor == GRAVITY);
        gfxStartCursor();
        gfxPrint(grav_cv);
        gfxEndCursor(cursor == GRAVITY_CV, false, grav_cv.InputName());
        break;
      case 2:
        gfxPrint(1, y, "Siz:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%3d%%", size);
        gfxEndCursor(cursor == SIZE);
        gfxStartCursor();
        gfxPrint(size_cv);
        gfxEndCursor(cursor == SIZE_CV, false, size_cv.InputName());
        break;
      case 3:
        gfxPrint(1, y, "Pre:");
        gfxStartCursor(rx - 5 * 6, y);
        graphics.printf("%3dms", predelay_2ms * 2);
        gfxEndCursor(cursor == PREDELAY);
        break;
      case 4:
        gfxPrint(1, y, "Mod:");
        gfxStartCursor(rx - 4 * 6, y);
        graphics.printf("%3d%%", mod_depth);
        gfxEndCursor(cursor == MOD_DEPTH);
        gfxStartCursor();
        gfxPrint(mod_cv);
        gfxEndCursor(cursor == MOD_CV, false, mod_cv.InputName());
        break;
      case 5: {
        gfxPrint(1, y, "R:");
        const int chz = mod_rate * 5; // centi-Hz
        gfxStartCursor(rx - 6 * 6, y);
        graphics.printf("%d.%02dHz", chz / 100, chz % 100);
        gfxEndCursor(cursor == MOD_RATE);
        break;
      }
      case 6:
        gfxPrint(1, y, "Lo-cut:");
        gfxStartCursor();
        graphics.printf("%2d", locut);
        gfxEndCursor(cursor == LO_CUT);
        break;
      case 7:
        gfxPrint(1, y, "Damp:");
        gfxStartCursor();
        graphics.printf("%2d", hidamp);
        gfxEndCursor(cursor == HI_DAMP);
        break;
    }
  }

  int cursor = MIX;

  // parameters (Blackhole-ish defaults: big, slightly damped, gentle mod)
  int8_t wet = 50;           // 0..100
  int8_t gravity = 35;       // -100..100, negative = inverse
  int8_t size = 70;          // 0..100
  uint8_t predelay_2ms = 0;  // 0..250 -> 0..500 ms
  int8_t mod_depth = 25;     // 0..100
  int8_t mod_rate = 10;      // 1..60 -> 0.05..3.00 Hz
  int8_t locut = 2;          // 0..10
  int8_t hidamp = 3;         // 0..10
  bool send_mode = false;
  bool alloc_ok = false;

  CVInputMap mix_cv;
  CVInputMap grav_cv;
  CVInputMap size_cv;
  CVInputMap mod_cv;

  int16_t knob_accel = 0;
  elapsedMillis millis_since_turn;

  AudioEffectAbyssReverb reverb;
  AudioMixer<2> wetdry[STEREO];
};

FLASHMEM void AbyssApplet::View() {
  if (!alloc_ok) {
    gfxPrint(1, 15, "Out of RAM!!");
    return;
  }
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

FLASHMEM void AbyssApplet::Start() {
  alloc_ok = reverb.begin();
  reverb.setMonoSum(Channels() == MONO);
  ForEachChannel(ch) {
    if (Channels() == STEREO || ch == 0) {
      PatchCableF32(InputF32(), ch, InputStream(), ch); // converter

      PatchCable(InputStream(), ch, reverb, ch);
      PatchCable(reverb, ch, wetdry[ch], WD_WET_CH);
      PatchCable(InputStream(), ch, wetdry[ch], WD_DRY_CH);
      PatchCable(wetdry[ch], 0, OutputStream(), ch);

      PatchCableF32(OutputStream(), ch, OutputF32(), ch); // converter
    }
  }
}


FLASHMEM void AbyssApplet::OnEncoderMove(int direction) {
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
    case GRAVITY:
      gravity = constrain(gravity + direction, -100, 100);
      break;
    case GRAVITY_CV:
      grav_cv.ChangeSource(direction);
      break;
    case SIZE:
      size = constrain(size + direction, 0, 100);
      break;
    case SIZE_CV:
      size_cv.ChangeSource(direction);
      break;
    case PREDELAY:
      predelay_2ms = constrain(predelay_2ms + knob_accel, 0, 250);
      break;
    case MOD_DEPTH:
      mod_depth = constrain(mod_depth + direction, 0, 100);
      break;
    case MOD_CV:
      mod_cv.ChangeSource(direction);
      break;
    case MOD_RATE:
      mod_rate = constrain(mod_rate + direction, 1, 60);
      break;
    case LO_CUT:
      locut = constrain(locut + direction, 0, 10);
      break;
    case HI_DAMP:
      hidamp = constrain(hidamp + direction, 0, 10);
      break;
    case CURSOR_LENGTH:
      break;
  }
  millis_since_turn = 0;
}
