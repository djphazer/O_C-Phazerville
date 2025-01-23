#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
class MidSideApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "Mid/Side";
  }

  void Start() override {
    ForEachChannel(to) {
      out_conns[to].connect(mixers[to], 0, output, to);
      ForEachChannel(from) {
        conns[from][to].connect(input, from, mixers[to], from);
      }
    }
    SetGain(gain);
  }

  void Controller() override {}

  void View() override {
    gfxPos(32 - 5 * 3, 25);
    graphics.printf("%3ddB", gain);
  }

  void OnEncoderMove(int direction) override {
    SetGain(constrain(gain + direction, -90, 90));
  }

  uint64_t OnDataRequest() override {
    return PackPackables(gain);
  }

  void OnDataReceive(uint64_t data) override {
    UnpackPackables(data, gain);
    SetGain(gain);
  }

  AudioStream* InputStream() override {
    return &input;
  }

  AudioStream* OutputStream() override {
    return &output;
  }

  void SetGain(int8_t new_gain) {
    gain = new_gain;
    float scalar = dbToScalar(gain);
    mixers[0].gain(0, scalar);
    mixers[0].gain(1, scalar);
    mixers[1].gain(0, scalar);
    mixers[1].gain(1, -scalar);
  }

protected:

  void SetHelp() override {}

private:
  int8_t gain = -6; // in dB

  AudioPassthrough<2> input;
  std::array<AudioMixer<2>, 2> mixers;
  AudioPassthrough<2> output;

  std::array<std::array<AudioConnection, 2>, 2> conns;
  std::array<AudioConnection, 2> out_conns;
};
