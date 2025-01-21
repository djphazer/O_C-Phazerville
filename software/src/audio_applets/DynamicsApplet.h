#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "../Audio/effect_dynamics.h"
#include <cstdint>

template <AudioChannels Channels>
class DynamicsApplet : public HemisphereAudioApplet {
public:

  enum DynamicsCursor {
    //IN_GAIN,
    GATE_THRESH,
    COMP_THRESH,
    LIMIT_THRESH,
    OUT_GAIN,

    MAX_CURSOR = OUT_GAIN
  };

  const char* applet_name() {
    return "Dynamics";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      complimit[i].Acquire();

      in_conns[i].connect(input, i, complimit[i], 0);
      out_conns[i].connect(complimit[i], 0, output, i);

      SetParams();
    }
  }

  void Unload() {
    for (auto& cl : complimit) cl.Release();
    AllowRestart();
  }

  void SetParams() {
    for (int i = 0; i < Channels; i++) {
      complimit[i].gate(gate_threshold * 1.0f);
      complimit[i].compression(comp_threshold * 1.0f);
      complimit[i].limit(limit_threshold * 1.0f);
      if (makeupgain < 0)
        complimit[i].autoMakeupGain();
      else
        complimit[i].makeupGain(makeupgain);
    }
  }

  void Controller() {
    for (int i = 0; i < Channels; i++) {
      // TODO: connect modulated param values to stuff
    }
  }

  void View() {
    const int label_x = 1;

    gfxPrint(label_x, 15, "Gate:");
    gfxStartCursor();
    graphics.printf("%3ddB", gate_threshold);
    gfxEndCursor(cursor == GATE_THRESH);

    gfxPrint(label_x, 25, "Comp:");
    gfxStartCursor();
    graphics.printf("%3ddB", comp_threshold);
    gfxEndCursor(cursor == COMP_THRESH);

    gfxPrint(label_x, 35, "Lim: ");
    gfxStartCursor();
    graphics.printf("%3ddB", limit_threshold);
    gfxEndCursor(cursor == LIMIT_THRESH);

    gfxPrint(label_x, 45, "MakeUp:");
    gfxStartCursor(label_x, 55);
    if (makeupgain < 0)
      gfxPrint("auto");
    else
      graphics.printf("%3ddB", makeupgain);
    gfxEndCursor(cursor == OUT_GAIN);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    switch (cursor) {
      case GATE_THRESH:
        gate_threshold = constrain(gate_threshold + direction, -100, 0);
        break;
      case COMP_THRESH:
        comp_threshold = constrain(comp_threshold + direction, -100, 0);
        break;
      case LIMIT_THRESH:
        limit_threshold = constrain(limit_threshold + direction, -100, 0);
        break;
      case OUT_GAIN:
        makeupgain = constrain(makeupgain + direction, -1, 60);
        break;

      default:
        return;
        break;
    }

    SetParams();
  }

  uint64_t OnDataRequest() {
    return PackByteAligned(gate_threshold, comp_threshold, limit_threshold, makeupgain);
  }
  void OnDataReceive(uint64_t data) {
    UnpackByteAligned(data, gate_threshold, comp_threshold, limit_threshold, makeupgain);
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
  // thresholds in db
  int8_t gate_threshold = -50;
  int8_t comp_threshold = -6;
  int8_t limit_threshold = -1;
  int8_t makeupgain = -1; // negative means auto

  // TODO: more params for attack/release of each stage, comp ratio & knee
  //
  // for reference, from effect_dynamics.h:
  //void gate(float threshold = -50.0f, float attack = MIN_T, float release = 0.3f, float hysterisis = 6.0f)
  //void compression(float threshold = -40.0f, float attack = MIN_T, float release = 0.5f, float ratio = 35.0f, float kneeWidth = 6.0f)
  //void limit(float threshold = -3.0f, float attack = MIN_T, float release = MIN_T)
  //void makeupGain(float gain = 0.0f)

  AudioPassthrough<Channels> input;
  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioEffectDynamics, Channels> complimit;
  std::array<AudioConnection, Channels> out_conns;
  AudioPassthrough<Channels> output;

};
