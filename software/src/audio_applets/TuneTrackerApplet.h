#pragma once

#include "../HemisphereAudioApplet.h"
#include "../src/Audio/SafeNoteFrequencyAnalyzer.h"

template <AudioChannels Channels>
class TuneTrackerApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "TuneTrk";
  }

  void Start() override {
    // Connect input to pitch analyzer (assuming mono input for pitch tracking)
    for (int i = 0; i < Channels; i++) {
        // using the passthru as our input
        PatchCable(passthru, i, note_freqs[i], 0);
        note_freqs[i].begin(0.14, 7000); // initializes the pitch tracking algo. threshold 0.1-0.2 for optimal error rates
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
      volts = constrain(-2.0f + log2f(freq / 16.35159783f), -3.0f, 6.0f); // C0 freq is 16.35159783f
      freq_available = freq > 0.0f;

      Out(0, volts * (12 << 7)); // I or K
    } else {
      // should we do anything when a pitch is not being read?
      freq_available = false;
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
    if (vf == 100) {
      vi += 1;
      vf = 0;
    }
    if (neg) {
      gfxPrint(label_x, 25, "-");
      gfxPrint(vi);
    } else {
      gfxPrint(label_x, 25, vi);
    }
    gfxPrint(".");
    if (vf < 10) gfxPrint("0");
    gfxPrint(vf);

    gfxPrint(label_x, 35, "CVOut: ");
    gfxPrint(OutputLabel(0));

    float prob = note_freqs[0].probability();
    int p_int = (int)prob;
    int p_frac = (int)((prob - p_int) * 100.0f + 0.5f);
    if (p_frac == 100) {
      p_int += 1;
      p_frac = 0;
    }
    gfxPrint(label_x, 45, p_int);
    gfxPrint(".");
    if (p_frac < 10) gfxPrint("0");
    gfxPrint(p_frac);

    gfxDisplayInputMapEditor(); // this bookends any GUI with editable CV inputs
  }
  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}

  // void OnButtonPress() override { }

  void OnEncoderMove(int direction) override { }

  AudioStream* InputStream() override {
    return &passthru;
  }
  AudioStream* OutputStream() override {
    return &passthru;
  };

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<Channels> passthru;

  std::array<SafeNoteFrequencyAnalyzer, Channels> note_freqs;

  float last_freq = 0.0f;
  bool freq_available = false;

  // For displaying frequency as text
  int int_part = 0;
  int frac_part = 0;

  float volts = 0.0f;
};
