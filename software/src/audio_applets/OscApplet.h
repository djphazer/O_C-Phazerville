#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include <Audio.h>

class OscApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "Osc";
  }
  void Start() override {}
  void Controller() override {
    float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
    synth.begin(1.0f, freq, waveforms[waveform]);
    synth.pulseWidth(0.01f * pw + pw_cv.InF());
  }
  void View() override {
    gfxStartCursor(1, 15);
    gfxPrint(waveform_names[waveform]);
    gfxEndCursor(cursor == 0);

    gfxStartCursor(1, 25);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == 1);

    gfxStartCursor();
    gfxPrintIcon(pitch_cv.Icon());
    gfxEndCursor(cursor == 2);

    gfxPrint(1, 35, "PW:  ");
    gfxStartCursor();
    graphics.printf("%3d%%", pw);
    gfxEndCursor(cursor == 3);

    gfxStartCursor();
    gfxPrintIcon(pw_cv.Icon());
    gfxEndCursor(cursor == 4);
  }

  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 5);
      return;
    }
    switch (cursor) {
      case 0:
        waveform = constrain(waveform + direction, 0, 2);
        break;
      case 1:
        pitch = constrain(pitch + direction * 16, -3 * 12 * 128, 7 * 12 * 128);
        break;
      case 2:
        pitch_cv.ChangeSource(direction);
        break;
      case 3:
        pw = constrain(pw + direction, 0, 100);
        break;
      case 4:
        pw_cv.ChangeSource(direction);
        break;
    }
  }

  AudioStream* InputStream() override {
    return nullptr;
  }
  AudioStream* OutputStream() override {
    return &synth;
  }
  // AudioChannels NumChannels() override { return MONO; }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  const int8_t waveforms[3]
    = {WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE, WAVEFORM_PULSE};
  char const* waveform_names[3] = {"SINE", "TRIANGLE", "PULSE"};

  int8_t waveform;
  int16_t pitch = 1 * 12 * 128; // C4
  CVInput pitch_cv;
  int8_t pw = 50;
  CVInput pw_cv;
  AudioSynthWaveform synth;
};
