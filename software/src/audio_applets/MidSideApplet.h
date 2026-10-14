#pragma once

#include "extern/f32/AudioMixer_F32.h"

// F32-native: the M/S gain matrix runs in float32 end to end inside the
// applet; the chain still sees int16 via the base class edge adapters.
class MidSideApplet : public HemisphereAudioAppletF32 {
public:
  const char* applet_name() {
    return "Mid/Side";
  }

  void Start() override {
    ForEachChannel(to) {
      PatchCableF32(mixers[to], 0, OutputF32(), to);
      ForEachChannel(from) {
        PatchCableF32(InputF32(), from, mixers[to], from);
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
    SetGain(gain + direction);
  }

  uint64_t OnDataRequest() override {
    return PackPackables(gain);
  }

  void OnDataReceive(uint64_t data) override {
    UnpackPackables(data, gain);
    SetGain(gain);
  }

  void SetGain(int8_t new_gain) {
    gain = constrain(new_gain, -90, 90);
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

  std::array<AudioMixer4_F32, 2> mixers;
};
