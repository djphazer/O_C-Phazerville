#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>


// index being which virtual audio input (1-8), value being the CV
inline void WriteVirtualAudioCV(uint8_t index, float value01) {
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
        note_freqs[i].begin(0.1); // threshold (try 0.1–0.2)
    }
  }
  void Controller() override {
    // pitch tracking of audio input goes here
    if (note_freqs[0].available()) {
      float freq = note_freqs[0].read();
      last_freq = freq;
      freq_available = freq > 0.0f;
      // convert frequency to CV
      float volts = log2f(last_freq / 32.7032f); // -3V to +6V
      volts = constrain(volts, -3.0f, 6.0f);     // Clamp to O&C range
      float norm = (volts + 3.0f) / 9.0f;        // Normalize -3V..+6V to 0.0..1.0
      WriteVirtualAudioCV(pitch_cv_selection, norm);

    // should we do anything when a pitch is not being read?
    } else { freq_available = false; }
  }
  void View() override {
    // GUI here
    const int label_x = 1;

    // // Print the detected pitch at the top
    // gfxPrint(label_x, 2, (int)last_freq);
    // gfxPrint("Hz");
    // Lazy implementation of printing float value, as it is likely unreliable to do so
    int int_part = (int)last_freq;
    int frac_part = (int)((last_freq - int_part) * 100);
    if (frac_part < 0) frac_part = -frac_part; // handle negative frequencies, just in case

    gfxPrint(label_x, 15, int_part);
    gfxPrint(".");
    if (frac_part < 10) gfxPrint("0"); // leading zero for single-digit decimals
    gfxPrint(frac_part);
    gfxPrint(" Hz");


    gfxPrint(label_x, 25, "PitchCV:");
    gfxStartCursor();
    gfxPrint(pitch_cv_selection);
    gfxEndCursor(cursor == PITCH_CV_OUT);

    gfxPrint(label_x, 35, "PitchEnv:");
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

  float last_freq = 0.0f;
  bool freq_available = false;

};