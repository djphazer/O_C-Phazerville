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

    // built-in VCA
    float gain = 0.01f
      * (level * static_cast<float>(level_cv.In(HEMISPHERE_MAX_INPUT_CV))
         / HEMISPHERE_MAX_INPUT_CV);
    mixer.gain(1, gain);
  }
  void View() override {
    gfxStartCursor(1, 15);
    gfxPrint(waveform_names[waveform]);
    gfxEndCursor(cursor == WAVEFORM);

    gfxStartCursor(1, 25);
    gfxPrintTuningIndicator(pitch);
    gfxEndCursor(cursor == OCTAVE);
    gfxStartCursor(11, 25);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == PITCH);

    gfxStartCursor();
    gfxPrintIcon(pitch_cv.Icon());
    gfxEndCursor(cursor == PITCH_CV);

    gfxPrint(1, 35, "PW:  ");
    gfxStartCursor();
    graphics.printf("%3d%%", pw);
    gfxEndCursor(cursor == PW);

    gfxStartCursor();
    gfxPrintIcon(pw_cv.Icon());
    gfxEndCursor(cursor == PW_CV);

    gfxPrint(1, 45, "Level:");
    gfxStartCursor();
    graphics.printf("%3d", level);
    gfxEndCursor(cursor == LEVEL);

    gfxStartCursor();
    gfxPrintIcon(level_cv.Icon());
    gfxEndCursor(cursor == LEVEL_CV);
  }

  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 7);
      return;
    }
    const int max_pitch = 7 * 12 * 128;
    const int min_pitch = -3 * 12 * 128;
    switch (cursor) {
      case WAVEFORM:
        waveform = constrain(waveform + direction, 0, 2);
        break;
      case OCTAVE:
        pitch = constrain(pitch + direction * 1 * 128, min_pitch, max_pitch);
        break;
      case PITCH:
        pitch = constrain(pitch + direction * 4, min_pitch, max_pitch);
        break;
      case PITCH_CV:
        pitch_cv.ChangeSource(direction);
        break;
      case PW:
        pw = constrain(pw + direction, 0, 100);
        break;
      case PW_CV:
        pw_cv.ChangeSource(direction);
        break;
      case LEVEL:
        level = constrain(level + direction, 0, 100);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
        break;
    }
  }

  AudioStream* InputStream() override {
    return &mixer;
  }
  AudioStream* OutputStream() override {
    return &mixer;
  }
  // AudioChannels NumChannels() override { return MONO; }

protected:
  void SetHelp() override {}

private:
  enum Cursor { WAVEFORM, OCTAVE, PITCH, PITCH_CV, PW, PW_CV, LEVEL, LEVEL_CV };

  int cursor = 0;
  const int8_t waveforms[3]
    = {WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE, WAVEFORM_PULSE};
  char const* waveform_names[3] = {"SINE", "TRIANGLE", "PULSE"};

  int8_t waveform;
  int16_t pitch = 1 * 12 * 128; // C4
  CVInput pitch_cv;
  int8_t pw = 50;
  CVInput pw_cv;
  int level = 75;
  CVInput level_cv;

  AudioSynthWaveform synth;
  AudioMixer4 mixer;
  AudioConnection synthConn{synth, 0, mixer, 1};
};
