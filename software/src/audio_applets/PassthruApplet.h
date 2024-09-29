
#include "HemisphereAudioApplet.h"
#include "AudioPassthrough.h"

template <AudioChannels Channels>
class PassthruApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return " - ";
  }
  void Start() override {}
  void Controller() override {}
  void View() override {}
  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {}

  AudioStream* InputStream() override {
    return &passthru;
  }
  AudioStream* OutputStream() override {
    return &passthru;
  }

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<Channels> passthru;
};
