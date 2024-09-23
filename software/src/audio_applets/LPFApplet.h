#include "HemisphereAudioApplet.h"

template <AudioChannels Channels>
class LpfApplet : public HemisphereAudioApplet {
public:
  const char *applet_name() { return "LPF"; }

  void Start() {}

  void Controller() {}

  void View() {
    gfxPrint(15, 0, "Freq:  ");
    gfxStartCursor();
    gfxPrint(level);
    gfxPrint("%");
    gfxEndCursor(cursor == 0);

    gfxPrint(25, 0, "Res:  ");
    gfxStartCursor();
    gfxPrint(level);
    gfxPrint("%");
    gfxEndCursor(cursor == 1);

    gfxPrint(35, 0, "Shape:  ");
    gfxStartCursor();
    gfxPrint(shape);
    gfxPrint("%");
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
      offset = constrain(level + direction, -200, 200);
      break;
    case 2:
      shape = constrain(shape + direction, 0, 100);
      break;
    }
  }

  uint64_t OnDataRequest() { return 0; }
  void OnDataReceive(uint64_t data) {}

  AudioStream *InputStream() override { return nullptr; }
  AudioStream *OutputStream() override { return nullptr; }
  AudioChannels NumChannels() override { return Channels; }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int level = 100;
  int offset = 0;
  int shape = 0;
};
