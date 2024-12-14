#include "HemisphereAudioApplet.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

template <AudioChannels Channels>
class VcaApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "VCA";
  }

  void Start() {
    cv_stream.Acquire();
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, muls[i], 0);
      cv_conns[i].connect(cv_stream, 0, muls[i], 1);
      mul_to_mixer[i].connect(muls[i], 0, bias_mixer[i], 0);
      bias_to_mixer[i].connect(input, i, bias_mixer[i], 1);
      out_conns[i].connect(bias_mixer[i], 0, output, i);
    }
  }

  void Unload() {
    cv_stream.Release();
    AllowRestart();
  }

  void Controller() {
    float cv = static_cast<float>(level_cv.In(HEMISPHERE_MAX_INPUT_CV))
      / HEMISPHERE_MAX_INPUT_CV * 32768.0f;
    for (int i = 0; i < Channels; i++) {
      cv_stream.Push(Clip16(cv));
      bias_mixer[i].gain(0, level * 0.01f);
      bias_mixer[i].gain(1, offset * 0.01f);
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
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS - 1);
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

protected:
  void SetHelp() override {}

private:
  const int NUM_PARAMS = 5;
  int cursor = 0;
  int level = 100;
  CVInput level_cv;
  int offset = 0;
  int shape = 0;
  CVInput shape_cv;

  AudioPassthrough<Channels> input;
  InterpolatingStream<> cv_stream;
  std::array<AudioEffectMultiply, Channels> muls;
  std::array<AudioMixer<2>, Channels> bias_mixer;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> cv_conns;
  std::array<AudioConnection, Channels> mul_to_mixer;
  std::array<AudioConnection, Channels> bias_to_mixer;
  std::array<AudioConnection, Channels> out_conns;
};
