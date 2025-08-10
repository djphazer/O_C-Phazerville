#include "AudioStream.h"
#include "HemisphereAudioApplet.h"
#include "OC_gpio.h"
#include "record_queue.h"
#include "Audio/AudioPassthrough.h"

template <AudioChannels Channels>
class RecorderApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "REC";
  }
  void Start() override {
    for (int i = 0; i < Channels; i++) {
      rec_conns[i].connect(passthru, i, record_queue[i], 0);
    }
  }
  void Controller() override {
    if (EditMode()) {
      if (!file_) return;

      // TODO: write to file
      while (record_queue[0].available() && record_queue[1].available()) {
        file_.write( record_queue[i].readBuffer(), 256 );
        record_queue[i].freeBuffer();
      }
    }
  }
  void View() override {
    gfxPrint(20,20,"Hi.");

  }
  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}

  void OnButtonPress() override {
    if (!file_ && SDcard_Ready) {
      char filename[16];
      sprintf(filename, "%03d.WAV", filenum_);
      file_ = SD.open(filename);
    }
  }
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
  std::array<AudioRecordQueue, Channels> record_queue;
  std::array<AudioConnection, Channels> rec_conns;

  File file_;
  size_t filenum_ = 0;
  bool writing_ = false;
};
