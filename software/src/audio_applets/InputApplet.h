#pragma once

#include "AudioIO.h"
#include "HSUtils.h"
#include "PackingUtils.h"
#include "HemisphereAudioApplet.h"
#include <Audio.h>

template <AudioChannels Channels>
class InputApplet : public HemisphereAudioApplet {
public:
  const uint64_t applet_id() override {
    return strhash("Input");
  }

  const char* applet_name() override {
    return Channels == MONO ? (mono_mode == LEFT ? "Input L" : "Input R")
                            : "Inputs";
  }
  void Start() override {
    if (MONO == Channels) {
      mono_mode = hemisphere;
      in_conn[0].connect(OC::AudioIO::InputStream(), 0, mixer[0], 0);
      cross_conn[0].connect(OC::AudioIO::InputStream(), 1, mixer[0], 1);
      in_conn[1].connect(input, 0, mixer[0], 2);

      out_conn[0].connect(mixer[0], 0, output, 0);

      peakpatch[0].connect(OC::AudioIO::InputStream(), 0, peakmeter[0], 0);

      mixer[0].gain(2, 1.0); // passthru
    } else {
      for (int i = 0; i < Channels; ++i) {
        in_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[i], 0);
        cross_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[1 - i], 1);
        in_conn[i + 2].connect(input, i, mixer[i], 2);

        out_conn[i].connect(mixer[i], 0, output, i);

        peakpatch[i].connect(OC::AudioIO::InputStream(), i, peakmeter[i], 0);

        mixer[i].gain(2, 1.0); // passthru
      }
    }
  }
  void Controller() override {
    UpdateMix();
  }
  void View() override {
    const char* const txt[] = {"Left", "Right", "Mixed"};
    gfxPrint(3, 15, txt[mono_mode]);
    if constexpr (Channels == STEREO) {
      gfxPrint("+Right");

      gfxPrint(10, 25, "Mix? ");
      gfxPrint(10, 35, mixtomono ? "Mono" : "Stereo");
      if (cursor == CHANNEL_MODE) gfxCursor(10, 43, 37);
    } else {
      if (cursor == CHANNEL_MODE) gfxCursor(3, 23, 31);
    }
    gfxPrint(1, 45, "Lvl:");
    gfxPrintDb(level);
    if (cursor == IN_LEVEL) gfxCursor(26, 53, 26);
    gfxStartCursor();
    gfxPrint(level_cv);
    gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

    for (int ch = 0; ch < Channels; ++ch) {
      if (peakmeter[ch].available()) {
        int peaklvl = peakmeter[ch].read() * 64;
        gfxInvert(ch*61, 64 - peaklvl, 3, peaklvl);
      }
    }

    gfxDisplayInputMapEditor();
  }
  uint64_t OnDataRequest() override {
    return PackPackables(pack<4>(mono_mode), pack(level), pack<1>(mixtomono), level_cv);
  }
  void OnDataReceive(uint64_t data) override {
    UnpackPackables(data, pack<4>(mono_mode), pack(level), pack<1>(mixtomono), level_cv);
  }

  // *****
  void OnButtonPress() override {
    if (CheckEditInputMapPress(cursor, IndexedInput(LEVEL_CV, level_cv)))
      return;
    CursorToggle();
  }
  // *****
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case IN_LEVEL:
        level = constrain(level + direction, LVL_MIN_DB - 1, LVL_MAX_DB);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
        break;
      case CHANNEL_MODE:
        if (Channels == MONO) {
          if (cursor == CHANNEL_MODE) {
            mono_mode = constrain(mono_mode + direction, 0, MODE_COUNT - 1);
          }
        } else {
          mixtomono = !mixtomono;
        }
        break;
    }
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

  void UpdateMix() {
    float lvl_scalar = level < LVL_MIN_DB ? 0.0f : dbToScalar(level) * level_cv.InF(1.0f);
    if (Channels == MONO) {
      if (mono_mode == MIXED) {
        mixer[0].gain(0, lvl_scalar);
        mixer[0].gain(1, lvl_scalar);
      } else {
        mixer[0].gain(mono_mode, lvl_scalar);
        mixer[0].gain(1 - mono_mode, 0.0);
      }
    } else {
      mixer[0].gain(0, lvl_scalar);
      mixer[0].gain(1, mixtomono ? lvl_scalar : 0.0);
      mixer[1].gain(0, lvl_scalar);
      mixer[1].gain(1, mixtomono ? lvl_scalar : 0.0);
    }
  }

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<Channels> input;
  AudioConnection in_conn[Channels * 2];
  AudioConnection cross_conn[Channels];
  AudioMixer<3> mixer[Channels];
  AudioConnection out_conn[Channels];
  AudioPassthrough<Channels> output;

  AudioAnalyzePeak peakmeter[Channels];
  AudioConnection peakpatch[Channels];

  int8_t mono_mode = LEFT;
  int8_t level = 0;
  CVInputMap level_cv;
  bool mixtomono = 0;

  enum SideMode { LEFT, RIGHT, MIXED, MODE_COUNT };
  enum { CHANNEL_MODE, IN_LEVEL, LEVEL_CV, MAX_CURSOR = LEVEL_CV };
  int cursor = 0;
};
