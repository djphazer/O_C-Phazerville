#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "Audio/AudioPassthrough.h"
#include <Audio.h>

template <AudioChannels Channels>
class LadderApplet : public HemisphereAudioApplet {

public:
  const char* applet_name() {
    return "LadderLPF";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      PatchCable(input, i, filters[i], 0);
      PatchCable(filters[i], 0, output, i);
    }
  }

  void Controller() {
    for (int i = 0; i < Channels; i++) {
      filters[i].frequency(PitchToRatio(pitch + pitch_cv.In()) * C3);
      filters[i].resonance(0.01f * res + res_cv.InF());
      filters[i].inputDrive(0.01f * gain + gain_cv.InF());
      filters[i].passbandGain(0.01f * pb_gain);
    }
  }

  void View() override {
    const int label_x = 1;
    gfxStartCursor(label_x, 15);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrint(pitch_cv);
    gfxEndCursor(cursor == 1, false, pitch_cv.InputName());

    gfxPrint(label_x, 25, "Res: ");
    gfxStartCursor();
    graphics.printf("%3d%%", res);
    gfxEndCursor(cursor == 2);
    gfxStartCursor();
    gfxPrint(res_cv);
    gfxEndCursor(cursor == 3, false, res_cv.InputName());

    gfxPrint(label_x, 35, "Drv: ");
    gfxStartCursor();
    graphics.printf("%3d%%", gain);
    gfxEndCursor(cursor == 4);
    gfxStartCursor();
    gfxPrint(gain_cv);
    gfxEndCursor(cursor == 5, false, gain_cv.InputName());

    gfxPrint(label_x, 45, "PBG: ");
    gfxStartCursor();
    graphics.printf("%3d", pb_gain);
    gfxEndCursor(cursor == 6);

    gfxDisplayInputMapEditor();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(1, pitch_cv),
          IndexedInput(3, res_cv),
          IndexedInput(5, gain_cv)
        ))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 6);
      return;
    }
    if(EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case 0:
        pitch = constrain(pitch + direction * 16, -8 * 12 * 128, 8 * 12 * 128);
        break;
      case 1:
        pitch_cv.ChangeSource(direction);
        break;
      case 2:
        res = constrain(res + direction, 0, 180);
        break;
      case 3:
        res_cv.ChangeSource(direction);
        break;
      case 4:
        gain = constrain(gain + direction, 0, 400);
        break;
      case 5:
        gain_cv.ChangeSource(direction);
        break;
      case 6:
        pb_gain = constrain(pb_gain + direction, 0, 50);
        break;
    }
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) {
    data[0] = PackPackables(pitch, res, gain, pb_gain);
    data[1] = PackPackables(pitch_cv, res_cv, gain_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) {
    UnpackPackables(data[0], pitch, res, gain, pb_gain);
    UnpackPackables(data[1], pitch_cv, res_cv, gain_cv);
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  int8_t cursor = 0;
  int16_t pitch = 1 * 12 * 128; // C4
  int16_t res = 0;
  int16_t gain = 100;
  int16_t pb_gain = 50;
  CVInputMap pitch_cv;
  CVInputMap res_cv;
  CVInputMap gain_cv;

  AudioPassthrough<Channels> input;
  std::array<AudioFilterLadder, Channels> filters;
  AudioPassthrough<Channels> output;
};
