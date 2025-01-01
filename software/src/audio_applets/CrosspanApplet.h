#pragma once

#include "HemisphereAudioApplet.h"
#include "dspinst.h"
#include "dsputils_arm.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/AudioVCA.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

class CrosspanApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "Crosspan";
  }

  void Start() override {
    ForEachChannel(from) {
      attenuations[from].Method(INTERPOLATION_LINEAR);
      attenuations[from].Acquire();
      ForEachChannel(to) {
        mixers[from].gain(to, 1.0f);
        vcas[from][to].level(1.0f);
        vcas[from][to].bias(0.0f);
        vcas[from][to].rectify(true);
      }
    }
    AllowRestart();
  }

  void Unload() override {
    ForEachChannel(ch) attenuations[ch].Release();
  }

  void Controller() override {
    float total_crosspan = constrain(
      static_cast<float>(crosspan) * 0.01f + crosspan_cv.InF(), 0.0f, 1.0f
    );
    float out, in;
    EqualPowerFade(out, in, total_crosspan);
    attenuations[0].Push(float_to_q15(out));
    attenuations[1].Push(float_to_q15(in));
  }

  void View() override {
    gfxFrame(1, 28, 62, 8);

    int param_x = static_cast<int>(static_cast<float>(crosspan) * 0.01f * 62);
    gfxLine(param_x, 26, param_x, 38);

    int cv_x = constrain(param_x + crosspan_cv.InF() * 62, 1, 63);
    if (cv_x < param_x) gfxRect(cv_x, 30, param_x - cv_x, 4);
    else gfxRect(param_x, 30, cv_x - param_x, 4);
    if (cursor == 0) gfxCursor(1, 40, 62, 14);

    gfxStartCursor(28, 42);
    gfxPrintIcon(PARAM_MAP_ICONS + 8 * crosspan_cv.source, 9);
    gfxEndCursor(cursor == 1);
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 1);
      return;
    }
    switch (cursor) {
      case 0:
        crosspan = constrain(crosspan + direction, 0, 100);
        break;
      case 1:
      default:
        crosspan_cv.ChangeSource(direction);
        break;
    }
  }

  uint64_t OnDataRequest() override {
    return crosspan;
  }

  void OnDataReceive(uint64_t data) override {
    crosspan = static_cast<int8_t>(data);
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
  int8_t cursor = 0;
  int8_t crosspan = 0;
  CVInput crosspan_cv;

  AudioPassthrough<2> input;
  std::array<InterpolatingStream<>, 2> attenuations;
  std::array<std::array<AudioVCA, 2>, 2> vcas;
  std::array<AudioMixer<2>, 2> mixers;
  AudioPassthrough<2> output;

  AudioConnection in_to_ll{input, 0, vcas[0][0], 0};
  AudioConnection in_to_lr{input, 0, vcas[0][1], 0};
  AudioConnection in_to_rl{input, 1, vcas[1][0], 0};
  AudioConnection in_to_rr{input, 1, vcas[1][1], 0};

  AudioConnection fade_out_to_ll{attenuations[0], 0, vcas[0][0], 1};
  AudioConnection fade_out_to_rr{attenuations[0], 0, vcas[1][1], 1};
  AudioConnection fade_in_to_lr{attenuations[1], 0, vcas[0][1], 1};
  AudioConnection fade_in_to_rl{attenuations[1], 0, vcas[1][0], 1};

  AudioConnection ll_to_l_mixer{vcas[0][0], 0, mixers[0], 0};
  AudioConnection lr_to_r_mixer{vcas[0][1], 0, mixers[1], 0};
  AudioConnection rl_to_l_mixer{vcas[1][0], 0, mixers[0], 1};
  AudioConnection rr_to_r_mixer{vcas[1][1], 0, mixers[1], 1};

  AudioConnection l_mixer_to_out{mixers[0], 0, output, 0};
  AudioConnection r_mixer_to_out{mixers[1], 0, output, 1};
};
