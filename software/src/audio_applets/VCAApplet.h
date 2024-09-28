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
      * (level * static_cast<float>(In(0)) / HEMISPHERE_MAX_INPUT_CV + offset);
    for (int i=0; i<Channels; i++) {
      amps[i].gain(gain);
    }
  }

  void View() {
    const int label_x = 1;
    const int param_x = 38;
    gfxPrint(label_x, 15, "Level:");
    gfxStartCursor(param_x, 15);
    graphics.printf("%3d%%", level);
    gfxEndCursor(cursor == 0);

    gfxPrint(label_x, 25, "Bias:");
    gfxStartCursor(param_x, 25);
    graphics.printf("%3d%%", offset);
    gfxEndCursor(cursor == 1);

    gfxPrint(label_x, 35, "Shape:");
    gfxStartCursor(param_x, 35);
    graphics.printf("%3d%%", shape);
    gfxEndCursor(cursor == 2);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 2);
      return;
    }
    switch (cursor) {
      case 0:
        level = constrain(level + direction, -200, 200);
        break;
      case 1:
        offset = constrain(offset + direction, -200, 200);
        break;
      case 2:
        shape = constrain(shape + direction, 0, 100);
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
  AudioChannels NumChannels() override {
    return Channels;
  }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int level = 100;
  int offset = 0;
  int shape = 0;

  AudioPassthrough<Channels> input;
  std::array<AudioAmplifier, Channels> amps;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;
};
