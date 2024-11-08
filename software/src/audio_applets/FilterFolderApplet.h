#include "HemisphereAudioApplet.h"

template <AudioChannels Channels>
class FilterFolderApplet : public HemisphereAudioApplet {

public:

  const char* applet_name() {
    return "Filt/Fold";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, filtfolder[i].filter, 0);
      out_conns[i].connect(filtfolder[i].mixer, 0, output, i);
    }
  }

  void Controller() {
    for (int i = 0; i < Channels; i++) {
      filtfolder[i].filter.frequency(PitchToRatio(pitch + pitch_cv.In()) * C3);
      filtfolder[i].filter.resonance(0.01f * (res + res_cv.InRescaled(500)));
      filtfolder[i].AmpAndFold(fold + fold_cv.InRescaled(100), amplevel + amp_cv.InRescaled(100));
    }
  }

  void View() {
    const int label_x = 1;
    gfxStartCursor(label_x, 15);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrintIcon(pitch_cv.Icon());
    gfxEndCursor(cursor == 1);

    gfxPrint(label_x, 25, "Res: ");
    gfxStartCursor();
    graphics.printf("%3d%%", res);
    gfxEndCursor(cursor == 2);
    gfxStartCursor();
    gfxPrintIcon(res_cv.Icon());
    gfxEndCursor(cursor == 3);

    gfxPrint(label_x, 35, "Fld: ");
    gfxStartCursor();
    graphics.printf("%3d%%", fold);
    gfxEndCursor(cursor == 4);
    gfxStartCursor();
    gfxPrintIcon(fold_cv.Icon());
    gfxEndCursor(cursor == 5);

    gfxPrint(label_x, 45, "Amp: ");
    gfxStartCursor();
    graphics.printf("%3d", amplevel);
    gfxEndCursor(cursor == 6);
    gfxStartCursor();
    gfxPrintIcon(amp_cv.Icon());
    gfxEndCursor(cursor == 7);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 7);
      return;
    }
    switch (cursor) {
      case 0:
        pitch = constrain(pitch + direction * 16, -8 * 12 * 128, 8 * 12 * 128);
        break;
      case 1:
        pitch_cv.ChangeSource(direction);
        break;
      case 2:
        res = constrain(res + direction, 70, 500);
        break;
      case 3:
        res_cv.ChangeSource(direction);
        break;
      case 4:
        fold = constrain(fold + direction, 0, 400);
        break;
      case 5:
        fold_cv.ChangeSource(direction);
        break;
      case 6:
        amplevel = constrain(amplevel + direction, 0, 100);
        break;
      case 7:
        amp_cv.ChangeSource(direction);
        break;
    }
  }

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int pitch = 1 * 12 * 128; // C4
  CVInput pitch_cv;
  int16_t res = 70;
  CVInput res_cv;
  int16_t fold = 100;
  CVInput fold_cv;
  int16_t amplevel = 50;
  CVInput amp_cv;

  struct FilterFolder {
    AudioFilterStateVariable filter;
    AudioEffectWaveFolder folder;
    AudioSynthWaveformDc drive;
    AudioMixer4 mixer;

    AudioConnection conn1{filter, 0, mixer, 0};
    AudioConnection conn2{filter, 0, folder, 0};
    AudioConnection conn3{folder, 0, mixer, 3};
    AudioConnection conn4{drive, 0, folder, 1};

    void AmpAndFold(int foldamt, int amplevel) {
      float foldF = 0.01f * foldamt;
      drive.amplitude(foldF);
      mixer.gain(0, 0.01f * amplevel * (1.0 - abs(foldF)));
      mixer.gain(3, foldF * 0.9);
    }
  };

  AudioPassthrough<Channels> input;
  std::array<FilterFolder, Channels> filtfolder;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;
};
