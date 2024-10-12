#include "AudioPassthrough.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include <Audio.h>

template <AudioChannels Channels>
class LadderApplet : public HemisphereAudioApplet {

public:
  const char* applet_name() {
    return "LadderLPF";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, filters[i], 0);
      out_conns[i].connect(filters[i], 0, output, i);
    }
  }

  void Controller() {
    for (int i = 0; i < Channels; i++) {
      filters[i].frequency(PitchToRatio(pitch + pitch_cv.In()) * C3);
      filters[i].resonance(0.01f * (res + res_cv.InRescaled(100)));
      filters[i].inputDrive(0.01f * (gain + gain_cv.InRescaled(100)));
      filters[i].passbandGain(0.01f * pb_gain);
    }
  }

  void View() {
    const int label_x = 1;
    gfxStartCursor(label_x, 15);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrintIcon(pitch_cv.Icon());
    gfxEndCursor(cursor == 1);

    gfxPrint(label_x, 25, "Res: ");
    gfxStartCursor();
    graphics.printf("%3d%%", res);
    gfxEndCursor(cursor == 2);
    gfxStartCursor();
    gfxPrintIcon(res_cv.Icon());
    gfxEndCursor(cursor == 3);

    gfxPrint(label_x, 35, "Drv: ");
    gfxStartCursor();
    graphics.printf("%3d%%", gain);
    gfxEndCursor(cursor == 4);
    gfxStartCursor();
    gfxPrintIcon(gain_cv.Icon());
    gfxEndCursor(cursor == 5);

    gfxPrint(label_x, 45, "PBG: ");
    gfxStartCursor();
    graphics.printf("%3d", pb_gain);
    gfxEndCursor(cursor == 6);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 6);
      return;
    }
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

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int pitch = 1 * 12 * 128; // C4
  CVInput pitch_cv;
  int16_t res = 0;
  CVInput res_cv;
  int16_t gain = 100;
  CVInput gain_cv;
  int16_t pb_gain = 50;

  AudioPassthrough<Channels> input;
  std::array<AudioFilterLadder, Channels> filters;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;
};
