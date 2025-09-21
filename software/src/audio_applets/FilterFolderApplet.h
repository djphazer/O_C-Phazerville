#include "HSUtils.h"
#include "HemisphereAudioApplet.h"

template <AudioChannels Channels>
class FilterFolderApplet : public HemisphereAudioApplet {

public:
  enum FiltFoldCursor {
    FOLD_AMT,
    FOLD_CV,
    FILTMODE,
    FILTER_FREQ,
    FILTER_FREQ_CV,
    FILTER_RES,
    FILTER_RES_CV,
    AMP,
    AMP_CV,

    CURSOR_MAX = AMP_CV
  };

  const char* applet_name() {
    return "Fold/MMF";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      PatchCable(input, i, filtfolder[i].folder, 0);
      filtfolder[i].Start(this);
      PatchCable(filtfolder[i].mixer, 0, output, i);
    }
  }

  void Controller() {
    const bool tiltmode = filtfolder[0].modesel > 3;
    const int bias = tiltmode ? tiltbias + res_cv.InRescaled(LVL_MAX_DB) : 0;

    for (int i = 0; i < Channels; i++) {
      filtfolder[i].filter.frequency(PitchToRatio(pitch + pitch_cv.In()) * C3);
      if (tiltmode) {
        filtfolder[i].filter.resonance(0.70);
      } else {
        filtfolder[i].filter.resonance(0.01f * (res + res_cv.InRescaled(500)));
      }
      filtfolder[i].AmpAndFold(
        0.01f * fold + fold_cv.InF(0.0f),
        dbToScalar(amplevel - abs(bias)) + amp_cv.InF(0.0f),
        bias
      );
    }
  }

  void View() {
    const char * const modename[] = {
      "", "+LPF", "+BPF", "+HPF", "+Tilt"
    };
    const int label_x = 1;
    int label_y = 15;

    gfxPrint(label_x, label_y, "Fld: ");
    gfxStartCursor();
    graphics.printf("%3d%%", fold);
    gfxEndCursor(cursor == FOLD_AMT);
    gfxStartCursor();
    gfxPrint(fold_cv);
    gfxEndCursor(cursor == FOLD_CV, false, fold_cv.InputName());

    label_y += 10;
    gfxIcon(label_x, label_y, PhzIcons::filter);
    gfxStartCursor(label_x + 9, label_y);
    gfxPrint("FOLD");
    gfxPrint(modename[filtfolder[0].modesel]);
    gfxEndCursor(cursor == FILTMODE);

    if (filtfolder[0].modesel) {
      label_y += 10;
      gfxStartCursor(label_x, label_y);
      gfxPrintPitchHz(pitch);
      gfxEndCursor(cursor == FILTER_FREQ);
      gfxStartCursor();
      gfxPrint(pitch_cv);
      gfxEndCursor(cursor == FILTER_FREQ_CV, false, pitch_cv.InputName());

      label_y += 10;
      if (filtfolder[0].modesel < 4) {
        gfxPrint(label_x, label_y, "Res: ");
        gfxStartCursor();
        graphics.printf("%3d%%", res);
        gfxEndCursor(cursor == FILTER_RES);
        gfxStartCursor();
        gfxPrint(res_cv);
        gfxEndCursor(cursor == FILTER_RES_CV, false, res_cv.InputName());
      } else {
        gfxPrint(label_x, label_y, "LF: ");
        gfxStartCursor();
        gfxPrintDb(tiltbias);
        gfxEndCursor(cursor == FILTER_RES);
        gfxStartCursor();
        gfxPrint(res_cv);
        gfxEndCursor(cursor == FILTER_RES_CV, false, res_cv.InputName());
      }
    }

    label_y += 10;
    gfxPrint(label_x, label_y, "Amp:");
    gfxStartCursor();
    gfxPrintDb(amplevel);
    gfxEndCursor(cursor == AMP);
    gfxStartCursor();
    gfxPrint(amp_cv);
    gfxEndCursor(cursor == AMP_CV, false, amp_cv.InputName());

    gfxDisplayInputMapEditor();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(FILTER_FREQ_CV, pitch_cv),
          IndexedInput(FILTER_RES_CV, res_cv),
          IndexedInput(FOLD_CV, fold_cv),
          IndexedInput(AMP_CV, amp_cv)
        ))
      return;
    CursorToggle();
  }
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      do {
        MoveCursor(cursor, direction, CURSOR_MAX);
      } while (0 == filtfolder[0].modesel && cursor >= FILTER_FREQ && cursor <= FILTER_RES_CV);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case FILTMODE:
        ChangeMode(direction);
        break;
      case FILTER_FREQ:
        pitch = constrain(pitch + direction * 16, -8 * 12 * 128, 8 * 12 * 128);
        break;
      case FILTER_FREQ_CV:
        pitch_cv.ChangeSource(direction);
        break;
      case FILTER_RES:
        if (filtfolder[0].modesel > 3)
          tiltbias = constrain(tiltbias + direction, LVL_MIN_DB, LVL_MAX_DB);
        else
          res = constrain(res + direction, 70, 500);
        break;
      case FILTER_RES_CV:
        res_cv.ChangeSource(direction);
        break;
      case FOLD_AMT:
        fold = constrain(fold + direction, 0, 400);
        break;
      case FOLD_CV:
        fold_cv.ChangeSource(direction);
        break;
      case AMP:
        amplevel = constrain(amplevel + direction, LVL_MIN_DB, LVL_MAX_DB);
        break;
      case AMP_CV:
        amp_cv.ChangeSource(direction);
        break;
    }
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(pitch, res, fold, amplevel, filtfolder[0].modesel);
    data[1] = PackPackables(pitch_cv, res_cv, fold_cv, amp_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], pitch, res, fold, amplevel, filtfolder[0].modesel);
    UnpackPackables(data[1], pitch_cv, res_cv, fold_cv, amp_cv);
    if (Channels == STEREO)
      filtfolder[1].modesel = filtfolder[0].modesel;
  }

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
  int16_t pitch = 1 * 12 * 128; // C4
  CVInputMap pitch_cv;
  int16_t res = 75;
  CVInputMap res_cv;
  int16_t fold = 6; // 0% is mute, 6% is dry
  CVInputMap fold_cv;
  int8_t amplevel = 0;
  CVInputMap amp_cv;
  int8_t tiltbias = 0; // dB

  struct FilterFolder {
    AudioEffectWaveFolder folder;
    AudioFilterStateVariable filter;
    AudioSynthWaveformDc drive;
    AudioMixer4 mixer;

    uint8_t modesel = 0; // 0 = BYPASS, 1 = LPF, 2 = BPF, 3 = HPF, 4 = Tilt

    void AmpAndFold(float foldF, float level, int tilt = 0) {
      drive.amplitude(foldF);
      for (int i = 0; i < 4; ++i) {
        float chanlvl = (i == modesel || (modesel > 3 && (i==1 || i==3))) * level;
        if (i==1) chanlvl *= dbToScalar(tilt) / 2;
        if (i==3) chanlvl *= -dbToScalar(-tilt) / 2;
        mixer.gain(i, chanlvl);
      }
    }

    void Start(HemisphereAudioApplet* owner) {
      owner->PatchCable(folder, 0, filter, 0);
      owner->PatchCable(folder, 0, mixer, 0);
      owner->PatchCable(filter, 0, mixer, 1);
      owner->PatchCable(filter, 1, mixer, 2);
      owner->PatchCable(filter, 2, mixer, 3);
      owner->PatchCable(drive, 0, folder, 1);
    }
  };

  AudioPassthrough<Channels> input;
  std::array<FilterFolder, Channels> filtfolder;
  AudioPassthrough<Channels> output;

  void ChangeMode(int dir) {
    uint8_t newmode = constrain(filtfolder[0].modesel + dir, 0, 4);
    for (int i = 0; i < Channels; i++) {
      filtfolder[i].modesel = newmode;
    }
  }
};
