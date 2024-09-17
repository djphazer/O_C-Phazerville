#include "HemisphereAudioApplet.h"

template <size_t Side> class InputApplet : public HemisphereAudioApplet {
public:
  const char *applet_name() override { return Side == 0 ? "Input L" : "Input R"; }
  void Start() override {}
  void Controller() override {}
  void View() override {}
  uint64_t OnDataRequest() override { return 0; }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {}

  AudioStream *InputStream() override { return nullptr; }
  AudioStream *OutputStream() override { return nullptr; }
  AudioChannels NumChannels() override { return MONO; }

protected:
  void SetHelp() override {}
};
