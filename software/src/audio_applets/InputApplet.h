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
    gfxPrint(1, 45, "Lvl:");
    gfxPrintDb(level);
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
        level = constrain(level + direction, LVL_MIN_DB - 1, LVL_MAX_DB);
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
    float lvl_scalar = level < LVL_MIN_DB ? 0.0f : dbToScalar(level);
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
  static const int LVL_MIN_DB = -90;
  static const int LVL_MAX_DB = 90;

  AudioPassthrough<Channels> input;
  AudioConnection in_conn[Channels * 2];
  AudioConnection cross_conn[Channels];
  AudioMixer4 mixer[Channels];
  AudioConnection out_conn[Channels];
  AudioPassthrough<Channels> output;

  AudioAnalyzePeak peakmeter[Channels];
  AudioConnection peakpatch[Channels];

  int8_t mono_mode = LEFT;
  int8_t level = 0;
  bool mixtomono = 0;

  enum SideMode { LEFT, RIGHT, MIXED, MODE_COUNT };
  enum { CHANNEL_MODE, IN_LEVEL, MAX_CURSOR = IN_LEVEL };
  int cursor = 0;

  void gfxPrintDb(int db) {
    if (db < LVL_MIN_DB) gfxPrint("    - ");
    else graphics.printf("%3ddB", db);
  }
};
