#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include "Audio/SafeNoteFrequencyAnalyzer.h"
#include "Audio/VACVMap.h"
#include <Audio.h>


// index being which virtual audio input (1-8), value being the CV
inline void WriteVirtualAudioCV(uint8_t index, float value01) {
  VirtualAudioCV::set(index, value01);
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

    //handle VACV assignment and ownership
    if (!vacv_owner) vacv_owner = VACVRegistry::I().registerOwner();
    pitch_cv_selection.AttachOwner(vacv_owner);
    pitch_env_selection.AttachOwner(vacv_owner);
    // Ensure initial selections are actually claimed (if you want defaults)
    if (!pitch_cv_selection.IsNone()) pitch_cv_selection.Claim(pitch_cv_selection.Channel01());
    if (!pitch_env_selection.IsNone()) pitch_env_selection.Claim(pitch_env_selection.Channel01());

    // Connect input to pitch analyzer (assuming mono input for pitch tracking)
    for (int i = 0; i < Channels; i++) {
        // using the passthru as our input
        in_conns[i].connect(passthru, i, note_freqs[i], 0);
        note_freqs[i].begin(0.14, 2200); // initializes the pitch tracking algo. threshold 0.1-0.2 for optimal error rates
    }
  }
  void Unload() override {
    // Unloading, then reloading TuneTracker seems to leave additional buffers in memory that add up everytime you do so.
    // TODO: find a way to pause note_freq[] pitch tracking when not needed.
    //
    AudioNoInterrupts(); // ensure audio doesn't flow in while we are stopping YIN
    delay(5);
    for (int i = 0; i < Channels; i++) {
      note_freqs[i].end(); // call this before so we can flush audio blocks. failure to do so results in leaks
    }
    AudioInterrupts();

    // Release VA claims and ID so future applets can reuse channels
    pitch_cv_selection.DetachOwner();
    pitch_env_selection.DetachOwner();
    if (vacv_owner) { VACVRegistry::I().unregisterOwner(vacv_owner); vacv_owner = 0; }

    cv_stream.Release();
    AllowRestart();
  }
  void Controller() override {
    // CPU usage is tied to the instantiation of YIN pitch tracking rather than how many times we call read().
    // Attempts to limit calls of read() by analyzing signal RMS only (via AudioAnalyzeRMS) increased memory from addiitonal objects, 
    // and did not reduce CPU usage.
    // It appears that YIN will continue to read in the background after calling .begin(), keeping CPU usage up even when not needed.
    // TODO: find a way to pause note_freq[] pitch tracking when not needed.
    //
    // we're going to assume mono input (channel 0 only) for now, and to keep CPU usage down
    float freq = note_freqs[0].read();
      // if we have a new frequency, convert to CV and write to Virtual Audio CV output
    if (freq && last_freq != freq) {
        last_freq = freq;
        int_part = (int)last_freq;
        frac_part = (int)((last_freq - int_part) * 100);
        // convert frequency to CV. O_C C0 starts at -2 volts, so -2-6V should be expected for TuneTracker V/Oct range.
        volts = constrain(-2.0f + log2f(freq / 16.35159783f), -3.0f, 6.0f);  // C0 freq is 16.35159783f
        freq_available = freq > 0.0f;
        pitch_cv_selection.WriteVolts(volts, -3.0f, 6.0f);
      }
      
      // should we do anything when a pitch is not being read?
      else { freq_available = false;
    }
  } 
  void View() override {
    // GUI here
    const int label_x = 1;

    // Print the detected pitch at the top
    // Lazy implementation of printing float value, as it is likely unreliable to do so
    gfxPrint(label_x, 15, int_part);
    gfxPrint(".");
    if (frac_part < 10) gfxPrint("0"); // leading zero for single-digit decimals
    gfxPrint(frac_part);
    gfxPrint(" Hz");
    //gfxPrintPitchHz(freq);

    float v = volts;
    bool neg = v < 0.0f;
    float av = neg ? -v : v;
    int vi = (int)av;
    int vf = (int)((av - vi) * 100.0f + 0.5f);
    if (vf == 100) { vi += 1; vf = 0; }
    if (neg) { gfxPrint(label_x, 25, "-"); gfxPrint(vi); }
    else { gfxPrint(label_x, 25, vi); }
    gfxPrint(".");
    if (vf < 10) gfxPrint("0");
    gfxPrint(vf);

    gfxPrint(label_x, 35, "CVOut:  ");
    gfxStartCursor();
    gfxPrint(pitch_cv_selection.Name());
    gfxEndCursor(cursor == PITCH_CV_OUT, false, pitch_cv_selection.Name());

    // gfxPrint(label_x, 35, "EnvOut:  ");
    // gfxStartCursor();
    // gfxPrint(pitch_env_selection.Name());
    // gfxEndCursor(cursor == PITCH_ENV_OUT, false, pitch_env_selection.Name());

    gfxPrint(label_x, 45, "PitchVolt:");
    // gfxPrint(label_x, 55, pitch_cv_selection.ReadVolts());
    float rv = pitch_cv_selection.ReadVolts();
    bool rneg = rv < 0.0f;
    float arv = rneg ? -rv : rv;
    int rvi = (int)arv;
    int rvf = (int)((arv - rvi) * 100.0f + 0.5f);
    if (rvf == 100) { rvi += 1; rvf = 0; }
    if (rneg) { gfxPrint(label_x, 55, "-"); gfxPrint(rvi); }
    else { gfxPrint(label_x, 55, rvi); }
    gfxPrint(".");
    if (rvf < 10) gfxPrint("0");
    gfxPrint(rvf);

    gfxDisplayInputMapEditor(); // this bookends any GUI with editable CV inputs

   }
  uint64_t OnDataRequest() override {
    return 0;
    }
  void OnDataReceive(uint64_t data) override {}
  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(PITCH_CV_OUT, pitch_cv_selection),
          IndexedInput(PITCH_ENV_OUT, pitch_env_selection)
        ))
      return;
    CursorToggle();
  }
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_MAX);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case PITCH_CV_OUT:
        pitch_cv_selection.ChangeSource(direction); 
        break;
      case PITCH_ENV_OUT:
        pitch_env_selection.ChangeSource(direction); 
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
  VACVMap pitch_cv_selection{1}; //VA1 
  VACVMap pitch_env_selection{2}; //VA2

  InterpolatingStream<> cv_stream;
  AudioPassthrough<Channels> passthru;

  std::array<SafeNoteFrequencyAnalyzer, Channels> note_freqs;
  // change connections to pointers we control
  std::array<AudioConnection, Channels> in_conns;

  float last_freq = 0.0f;
  bool freq_available = false;

  // For displaying frequency as text
  int int_part = 0;
  int frac_part = 0;

  uint16_t vacv_owner = 0;
  float volts = 0.0f;

};