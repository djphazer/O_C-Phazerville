#pragma once

#include "HemisphereAudioApplet.h"
#include <Audio.h>
#include "AudioIO.h"

template <AudioChannels Channels>
class InputApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return Channels == MONO ? "Input" : "Inputs";
  }
  void Start() override {
    if (MONO == Channels) {
      in_conn[0].connect(OC::AudioIO::InputStream(), 0, mixer[0], 0);
      cross_conn[0].connect(OC::AudioIO::InputStream(), 1, mixer[0], 1);
      in_conn[1].connect(input, 0, mixer[0], 3);

      out_conn[0].connect(mixer[0], 0, output, 0);
      mixer[0].gain(1, 0.0);
    } else {
      for (int i = 0; i < Channels; ++i) {
        in_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[i], 0);
        cross_conn[i].connect(OC::AudioIO::InputStream(), i, mixer[1-i], 1);
        in_conn[i+2].connect(input, i, mixer[i], 3);

        out_conn[i].connect(mixer[i], 0, output, i);
        mixer[i].gain(1, 0.0);
      }
    }
  }
  void Controller() override {}
  void View() override {
    const char * const txt[] = {
      "Left", "Right", "Mixed"
    };
    gfxPrint(2, 15, txt[mono_mode]);
    if constexpr (Channels == STEREO) {
      gfxPrint("+Right");

      gfxPrint(2, 25, "Mix? ");
      gfxPrint(2, 35, mixtomono ? "Mono" : "Stereo" );
      gfxCursor(2, 44, 37);
    } else {
      gfxCursor(2, 24, 31);
    }
  }
  uint64_t OnDataRequest() override {
    return 0;
  }
  void OnDataReceive(uint64_t data) override {}
  void OnEncoderMove(int direction) override {
    if (!EditMode()) return;
    if (Channels == MONO) {
      mono_mode = constrain(mono_mode + direction, 0, MODE_COUNT-1);
      if (mono_mode == MIXED) {
        mixer[0].gain(0, 0.9);
        mixer[0].gain(1, 0.9);
      } else {
        mixer[0].gain(mono_mode, 1.0);
        mixer[0].gain(1 - mono_mode, 0.0);
      }
    } else {
      mixtomono = !mixtomono;
      mixer[0].gain(1, mixtomono? 1.0 : 0.0);
      mixer[1].gain(1, mixtomono? 1.0 : 0.0);
    }
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
  AudioPassthrough<Channels> input;
  AudioConnection in_conn[Channels*2];
  AudioConnection cross_conn[Channels];
  AudioMixer4 mixer[Channels];
  AudioConnection out_conn[Channels];
  AudioPassthrough<Channels> output;

  int mono_mode = LEFT;
  bool mixtomono = 0;

  enum SideMode {
    LEFT, RIGHT,
    MIXED,
    MODE_COUNT
  };
  enum {
    CHAN_SEL,
    MIX_TOGGLE,
    MAX_CURSOR = MIX_TOGGLE
  };
  int cursor;

};
