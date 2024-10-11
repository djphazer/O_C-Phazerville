#include "AudioPassthrough.h"
#include "HemisphereAudioApplet.h"
#include <Audio.h>

template <AudioChannels Channels>
class VcaApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "VCA";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, amps[i], 0);
      out_conns[i].connect(amps[i], 0, output, i);
    }
  }

  void Controller() {
    float gain = 0.01f
      * (level * static_cast<float>(level_cv.In()) / HEMISPHERE_MAX_INPUT_CV
         + offset);
    for (int i = 0; i < Channels; i++) {
      amps[i].gain(gain);
    }
  }

  void View() {
    gfxPrint(1, 15, "Lvl:");
    gfxStartCursor();
    graphics.printf("%4d%%", level);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrintIcon(level_cv.Icon());
    gfxEndCursor(cursor == 1);

    gfxPrint(1, 25, "Off:");
    gfxStartCursor();
    graphics.printf("%4d%%", offset);
    gfxEndCursor(cursor == 2);

    gfxPrint(1, 35, "Exp:");
    gfxStartCursor();
    graphics.printf("%4d%%", shape);
    gfxEndCursor(cursor == 3);
    gfxStartCursor();
    gfxPrintIcon(PARAM_MAP_ICONS + 8 * shape_cv.source);
    gfxEndCursor(cursor == 4);
    // graphics.printf("%4d%%", shape_cv);
    //
    //
    //
    //
    // // if (cursor >= NUM_PARAMS) {
    // //   ViewInputMappings();
    // //   return;
    // // }
    //
    // const int label_x = 1;
    // const int param_x = 38;
    // gfxPrint(label_x, 15, "Level:");
    // gfxStartCursor(param_x, 15);
    // graphics.printf("%3d%%", level);
    // gfxEndCursor(cursor == 0);
    //
    // gfxPrint(label_x, 25, "Bias:");
    // gfxStartCursor(param_x, 25);
    // graphics.printf("%3d%%", offset);
    // gfxEndCursor(cursor == 1);
    //
    // gfxPrint(label_x, 35, "Shape:");
    // gfxStartCursor(param_x, 35);
    // graphics.printf("%3d%%", shape);
    // gfxEndCursor(cursor == 2);
  }

  void ViewInputMappings() {
    int c = cursor - NUM_PARAMS;
    DisplayInputMap(level_cv, 0, c);
    DisplayInputMap(shape_cv, 1, c);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS + NumInputParams() - 1);
      return;
    }
    switch (cursor) {
      case 0:
        level = constrain(level + direction, -200, 200);
        break;
      case 1:
        level_cv.ChangeSource(direction);
        break;
      case 2:
        offset = constrain(offset + direction, -200, 200);
        break;
      case 3:
        shape = constrain(shape + direction, 0, 100);
        break;
      case 4:
        shape_cv.ChangeSource(direction);
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

  inline size_t NumInputParams() const {
    return 2;
  }

  ParamInput* GetInputParam(size_t i) {
    ParamInput* params[] = {&level_cv, &shape_cv};
    return params[i];
  }

protected:
  void SetHelp() override {}

private:
  const int NUM_PARAMS = 3;
  int cursor = 0;
  int level = 100;
  CVInput level_cv{"Level"};
  int offset = 0;
  int shape = 0;
  CVInput shape_cv{"Shape"};

  AudioPassthrough<Channels> input;
  std::array<AudioAmplifier, Channels> amps;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;
};
