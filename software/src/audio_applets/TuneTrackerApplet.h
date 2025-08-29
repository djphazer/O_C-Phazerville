#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

// ---- Replace with your actual VA writer (index: 0-based) ----
inline void WriteVA(uint8_t index, float value01) {
  // Example (uncomment & adapt for your tree):
  // HS::VAWrite(index, value01);
  (void)index; (void)value01; // placeholder
}

template <AudioChannels Channels>
class TuneTrackerApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "TuneTrackr";
  }
  void Start() override {
    cv_stream.Method(INTERPOLATION_LINEAR);
    cv_stream.Acquire();
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, vcas[i], 0);
      cv_conns[i].connect(cv_stream, 0, vcas[i], 1);
      out_conns[i].connect(vcas[i], 0, output, i);
    }
  }
  void Unload() {
    cv_stream.Release();
    AllowRestart();
  }
  void Controller() override {
    // pitch tracking of audio input goes here
  }
  void View() override {
    // GUI here
    const int label_x = 1;

    gfxPrint(label_x, 15, "PitchCV:");
    gfxStartCursor();
    gfxPrint(pitch_cv_selection);
    gfxEndCursor(cursor == PITCH_CV_OUT);

    gfxPrint(label_x, 25, "PitchEnv:");
    gfxStartCursor();
    gfxPrint(pitch_env_selection);
    gfxEndCursor(cursor == PITCH_ENV_OUT);
  }
  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {}
  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {}
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_MAX);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case PITCH_CV_OUT:
        break;
      case PITCH_ENV_OUT:
        break;
    }
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  };

protected:
  void SetHelp() override {}

private:
  enum TuneTrackerCursor {
    PITCH_CV_OUT,
    PITCH_ENV_OUT,

    CURSOR_MAX = PITCH_ENV_OUT
  };

  int cursor = 0;
  // correct this later to reflect some enum or list of available Virtual Audio CV ins
  int8_t pitch_cv_selection = 1; //VA1 
  int8_t pitch_env_selection = 2; //VA2

  InterpolatingStream<> cv_stream;
  AudioPassthrough<Channels> input;
  AudioPassthrough<Channels> output;
  
  std::array<AudioVCA, Channels> vcas;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> cv_conns;
  std::array<AudioConnection, Channels> out_conns;

};