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
    // Connect input to pitch analyzer (assuming mono input for pitch tracking)
    in_conn = new AudioConnection(passthru, 0, note_freq, 0);
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
  uint64_t OnDataRequest() override {
    return 0;
    }
  void OnDataReceive(uint64_t data) override {}
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
    return &passthru;
  }
  AudioStream* OutputStream() override {
    return &passthru;
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
  AudioPassthrough<Channels> passthru;

  AudioAnalyzeNoteFrequency note_freq;
  AudioConnection* in_conn = nullptr;

};