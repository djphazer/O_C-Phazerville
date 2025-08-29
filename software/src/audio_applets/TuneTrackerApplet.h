#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>


// index being which virtual audio input (1-8), value being the CV
inline void WriteVA(uint8_t index, float value01) {
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
    for (int i = 0; i < Channels; ++i) {
        // Dynamically allocate each AudioConnection
        in_conns[i].connect(passthru, i, note_freqs[i], 0);
        peak_conns[i].connect(passthru, i, peak_analyzers[i], 0);
        note_freqs[i].begin(0.1); // threshold (try 0.1–0.2)
    }
  }
  void Controller() override {
    // pitch tracking of audio input goes here
    if (note_freqs[0].available()) {
      float freq = note_freqs[0].read();
      last_freq = freq;
      freq_available = freq > 0.0f;
    } else {
    freq_available = false;
    }

    // Volume measurement
    if (peak_analyzers[0].available()) {
        last_peak = peak_analyzers[0].read();
    }
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

    gfxPrint(label_x, 35, "P(Hz):");
    gfxStartCursor();
    //graphics.printf("%4.2fHz", last_freq);
    gfxPrint((int)last_freq);
    gfxPrint("Hz");
    gfxEndCursor(false);

    gfxPrint(label_x, 45, "F:");
    gfxStartCursor();
    gfxPrint(freq_available ? "True" : "False");
    gfxEndCursor(false);

    gfxPrint(label_x, 55, "Vol:");
    gfxStartCursor();
    gfxPrint((int)(last_peak * 100)); // as percentage
    gfxEndCursor(false);

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
    FREQ_DISPLAY,

    CURSOR_MAX = PITCH_ENV_OUT
  };

  int cursor = 0;
  // correct this later to reflect some enum or list of available Virtual Audio CV ins
  int8_t pitch_cv_selection = 1; //VA1 
  int8_t pitch_env_selection = 2; //VA2

  InterpolatingStream<> cv_stream;
  AudioPassthrough<Channels> passthru;

  std::array<AudioAnalyzeNoteFrequency, Channels> note_freqs;
  std::array<AudioConnection, Channels> in_conns; 

  std::array<AudioAnalyzePeak, Channels> peak_analyzers;
  std::array<AudioConnection, Channels> peak_conns;

  int last_peak = 0;
  float last_freq = 0.0f;
  bool freq_available = false;

};