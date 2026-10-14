#pragma once

// F32-native: the four-VCA crossfade matrix and output mix run on float32
// blocks (AudioVCA_F32 / AudioMixerF32), so the equal-power/equal-amplitude
// pan curves reach the VCAs at full float precision instead of q15 and the
// crossfaded sum never requantizes to int16 between stages. The chain still
// sees int16 via the HemisphereAudioAppletF32 edge adapters. Params, ranges,
// and UI are unchanged.

#include "../Audio/AudioVCA_F32.h"
#include "../Audio/AudioMixerF32.h"
#include "../Audio/InterpolatingStreamF32.h"

class CrosspanApplet : public HemisphereAudioAppletF32 {
public:
  const char* applet_name() {
    return "Crosspan";
  }

  void Start() override {
    ForEachChannel(from) {
      attenuations[from].Method(INTERPOLATION_LINEAR);
      attenuations[from].Acquire();
      ForEachChannel(to) {
        mixers[from].gain(to, 1.0f);
        vcas[from][to].level(1.0f);
        vcas[from][to].bias(0.0f);
        vcas[from][to].rectify(true);
      }
    }

    PatchCableF32(InputF32(), 0, vcas[0][0], 0);
    PatchCableF32(InputF32(), 0, vcas[0][1], 0);
    PatchCableF32(InputF32(), 1, vcas[1][0], 0);
    PatchCableF32(InputF32(), 1, vcas[1][1], 0);

    PatchCableF32(attenuations[0], 0, vcas[0][0], 1);
    PatchCableF32(attenuations[0], 0, vcas[1][1], 1);
    PatchCableF32(attenuations[1], 0, vcas[0][1], 1);
    PatchCableF32(attenuations[1], 0, vcas[1][0], 1);

    PatchCableF32(vcas[0][0], 0, mixers[0], 0);
    PatchCableF32(vcas[0][1], 0, mixers[1], 0);
    PatchCableF32(vcas[1][0], 0, mixers[0], 1);
    PatchCableF32(vcas[1][1], 0, mixers[1], 1);

    PatchCableF32(mixers[0], 0, OutputF32(), 0);
    PatchCableF32(mixers[1], 0, OutputF32(), 1);

    AllowRestart();
  }

  void Unload() override {
    ForEachChannel(ch) attenuations[ch].Release();
  }

  void Controller() override {
    float total_crosspan = constrain(
      static_cast<float>(crosspan) * 0.01f + crosspan_cv.InF(), 0.0f, 1.0f
    );
    float out, in;
    if (xfade_shape == EQUAL_POWER) EqualPowerFade(out, in, total_crosspan);
    else {
      out = 1.0f - total_crosspan;
      in = total_crosspan;
    }
    attenuations[0].Push(out);
    attenuations[1].Push(in);
  }

  void View() override {
    gfxFrame(1, 28, 62, 8);

    int param_x = static_cast<int>(static_cast<float>(crosspan) * 0.01f * 62);
    gfxLine(param_x, 26, param_x, 38);

    int cv_x = constrain(param_x + crosspan_cv.InF() * 62, 1, 63);
    if (cv_x < param_x) gfxRect(cv_x, 30, param_x - cv_x, 4);
    else gfxRect(param_x, 30, cv_x - param_x, 4);
    if (cursor == 0) gfxCursor(1, 40, 62, 14);

    gfxStartCursor(28, 42);
    gfxPrint(crosspan_cv);
    gfxEndCursor(cursor == 1, false, crosspan_cv.InputName());

    gfxStartCursor(32 - 3 * 9, 55);
    gfxPrint(xfade_shape == EQUAL_POWER ? "Equal pow" : "Equal amp");
    gfxEndCursor(cursor == 2);

    gfxDisplayInputMapEditor();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(cursor, IndexedInput(1, crosspan_cv)))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 2);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case 0:
        crosspan = constrain(crosspan + direction, 0, 100);
        break;
      case 1:
        crosspan_cv.ChangeSource(direction);
        break;
      case 2:
        xfade_shape = static_cast<XfadeShape>(xfade_shape + direction);

      default:
        break;
    }
  }

  uint64_t OnDataRequest() override {
    return PackPackables(crosspan, crosspan_cv, pack<1>(xfade_shape));
  }

  void OnDataReceive(uint64_t data) override {
    UnpackPackables(data, crosspan, crosspan_cv, pack<1>(xfade_shape));
  }

protected:
  void SetHelp() override {}

private:
  enum XfadeShape : bool { EQUAL_POWER, EQUAL_AMPLITUDE };
  int8_t cursor = 0;
  int8_t crosspan = 0;
  CVInputMap crosspan_cv;
  XfadeShape xfade_shape;

  std::array<InterpolatingStreamF32<>, 2> attenuations;
  std::array<std::array<AudioVCA_F32, 2>, 2> vcas;
  std::array<AudioMixerF32<2>, 2> mixers;
};
