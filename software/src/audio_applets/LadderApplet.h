#include "HemisphereAudioApplet.h"

template <AudioChannels Channels>
class LadderApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "LadderLPF";
  }

  void Start() {}

  void Controller() {}

  void View() {
    const int label_x = 1;
    const int param_x = 38;
    gfxPrint(label_x, 15, "Pitch:");
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
        res = constrain(res + direction, 0, 100);
        break;
    }
  }

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

  AudioStream* InputStream() override {
    return nullptr;
  }
  AudioStream* OutputStream() override {
    return nullptr;
  }
  AudioChannels NumChannels() override {
    return Channels;
  }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int pitch = 12 * 128;
  int res = 0;
};
