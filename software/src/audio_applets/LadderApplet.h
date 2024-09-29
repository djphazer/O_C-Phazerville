#include "HemisphereAudioApplet.h"
#include "AudioPassthrough.h"
#include <Audio.h>

template <AudioChannels Channels>
class LadderApplet : public HemisphereAudioApplet {

public:
  const char* applet_name() {
    return "LadderLPF";
  }

  void Start() {
    for (int i=0; i < Channels; i++) {
      in_conns[i].connect(input, i, filters[i], 0);
      out_conns[i].connect(filters[i], 0, output, i);
    }
  }

  void Controller() {
    for (int i=0; i<Channels; i++) {
      filters[i].frequency(pitch);
      filters[i].resonance(0.01f * res);
    }

  }

  void View() {
    const int label_x = 1;
    const int param_x = 38;
    gfxPrint(label_x, 15, "Freq:");
    gfxStartCursor(param_x, 15);
    graphics.printf("%4d", pitch);
    gfxEndCursor(cursor == 0);

    gfxPrint(label_x, 25, "Res:");
    gfxStartCursor(param_x, 25);
    graphics.printf("%3d%%", res);
    gfxEndCursor(cursor == 1);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 1);
      return;
    }
    switch (cursor) {
      case 0:
        pitch = constrain(pitch + direction, 0, 0xffff);
        break;
      case 1:
        res = constrain(res + direction, 0, 180);
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
  int pitch = 440;
  int res = 0;

  AudioPassthrough<Channels> input;
  std::array<AudioFilterLadder, Channels> filters;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;
};
