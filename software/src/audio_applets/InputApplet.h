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
      in_conn[1].connect(input, 0, mixer[0], 3);

      out_conn[0].connect(mixer[0], 0, output, 0);

      peakpatch[0].connect(OC::AudioIO::InputStream(), 0, peakmeter[0], 0);
    } else {
      for (int i = 0; i < Channels; ++i) {
        in_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[i], 0);
        cross_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[1 - i], 1);
        in_conn[i + 2].connect(input, i, mixer[i], 3);

        out_conn[i].connect(mixer[i], 0, output, i);

        peakpatch[i].connect(OC::AudioIO::InputStream(), i, peakmeter[i], 0);
      }
    }
    UpdateMix();
  }
  void Controller() override {}
  void View() override {
    const char* const txt[] = {"Left", "Right", "Mixed"};
    gfxPrint(3, 15, txt[mono_mode]);
    if constexpr (Channels == STEREO) {
      gfxPrint("+Right");

      gfxPrint(10, 25, "Mix? ");
      gfxPrint(10, 35, mixtomono ? "Mono" : "Stereo");
      if (cursor == CHANNEL_MODE) gfxCursor(10, 43, 37);

      if (peakmeter[1].available()) {
        int peaklvl = peakmeter[1].read() * 64;
        gfxInvert(61, 64 - peaklvl, 3, peaklvl);
      }
    } else {
      if (cursor == CHANNEL_MODE) gfxCursor(3, 23, 31);
    }
    gfxPrint(2, 45, "Lvl:");
    gfxPrint(level);
    gfxPrint("%");
    if (cursor == IN_LEVEL) gfxCursor(26, 53, 26);

    if (peakmeter[0].available()) {
      int peaklvl = peakmeter[0].read() * 64;
      gfxInvert(0, 64 - peaklvl, 3, peaklvl);
    }
  }
  uint64_t OnDataRequest() override {
    return PackPackables(pack<4>(mono_mode), pack(level), pack<1>(mixtomono));
  }
  void OnDataReceive(uint64_t data) override {
    UnpackPackables(data, pack<4>(mono_mode), pack(level), pack<1>(mixtomono));
    UpdateMix();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    switch (cursor) {
      case IN_LEVEL:
        level = constrain(level + direction, 0, 200);
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

    // update mix levels on any param change
    UpdateMix();
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

  void UpdateMix() {
    if (Channels == MONO) {
      if (mono_mode == MIXED) {
        mixer[0].gain(0, level * 0.01f);
        mixer[0].gain(1, level * 0.01f);
      } else {
        mixer[0].gain(mono_mode, level * 0.01f);
        mixer[0].gain(1 - mono_mode, 0.0);
      }
    } else {
      mixer[0].gain(0, level * 0.01f);
      mixer[0].gain(1, mixtomono ? level * 0.01f : 0.0);
      mixer[1].gain(0, level * 0.01f);
      mixer[1].gain(1, mixtomono ? level * 0.01f : 0.0);
    }
  }

protected:
  void SetHelp() override {}

private:
  AudioPassthrough<Channels> input;
  AudioConnection in_conn[Channels * 2];
  AudioConnection cross_conn[Channels];
  AudioMixer4 mixer[Channels];
  AudioConnection out_conn[Channels];
  AudioPassthrough<Channels> output;

  AudioAnalyzePeak peakmeter[Channels];
  AudioConnection peakpatch[Channels];

  int8_t mono_mode = LEFT;
  int8_t level = 90;
  bool mixtomono = 0;

  enum SideMode { LEFT, RIGHT, MIXED, MODE_COUNT };
  enum { CHANNEL_MODE, IN_LEVEL, MAX_CURSOR = IN_LEVEL };
  int cursor = 0;
};
