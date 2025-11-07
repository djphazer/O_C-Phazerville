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
    }

    for (int i = 0; i < Channels; ++i) {
      ForEachChannel(ch) {
        attenuations[i*2 + ch].Method(INTERPOLATION_LINEAR);
        attenuations[i*2 + ch].Acquire();

        PatchCable(OC::AudioIO::InputStream(), ch, vcas[i*2 + ch], 0);
        PatchCable(attenuations[i*2 + ch], 0, vcas[i*2 + ch], 1);
        PatchCable(vcas[i*2 + ch], 0, mixer[i], ch);
      }
      PatchCable(passthru, i, mixer[i], 2);
      PatchCable(mixer[i], 0, output, i);

      PatchCable(OC::AudioIO::InputStream(), i, peakmeter[i], 0);

      mixer[i].gain(0, 1.0); // Left
      mixer[i].gain(1, 1.0); // Right
      mixer[i].gain(2, 1.0); // passthru
    }
  }
  void Unload() override {
    for (int i = 0; i < Channels; ++i) {
      ForEachChannel(ch) attenuations[i*2 + ch].Release();
    }
    AllowRestart();
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
    return &passthru;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

  void UpdateMix() {
    float lvl_scalar = level < LVL_MIN_DB ? 0.0f : dbToScalar(level) * level_cv.InF(1.0f);
    q15_t lvl = float_to_q15(lvl_scalar);

    if (Channels == MONO) {
      if (mono_mode == MIXED) {
        attenuations[0].Push(lvl);
        attenuations[1].Push(lvl);
      } else {
        attenuations[mono_mode].Push(lvl);
        attenuations[1-mono_mode].Push(0.0);
      }
    } else {
      attenuations[0].Push(lvl); // left to left
      attenuations[1].Push(mixtomono? lvl : 0.0); // right to left
      attenuations[2].Push(mixtomono? lvl : 0.0); // left to right
      attenuations[3].Push(lvl); // right to right
    }
  }

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<Channels> passthru;
  std::array<InterpolatingStream<>, Channels*2> attenuations;
  std::array<AudioVCA, Channels*2> vcas;
  AudioMixer<3> mixer[Channels];
  AudioPassthrough<Channels> output;

  AudioAnalyzePeak peakmeter[Channels];

  int8_t mono_mode = LEFT;
  int8_t level = 0;
  CVInputMap level_cv;
  bool mixtomono = 0;

  enum SideMode { LEFT, RIGHT, MIXED, MODE_COUNT };
  enum { CHANNEL_MODE, IN_LEVEL, LEVEL_CV, MAX_CURSOR = LEVEL_CV };
  int cursor = 0;
};
