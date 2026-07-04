#include "../src/Audio/effect_dynamics.h"

template <AudioChannels Channels>
class DynamicsApplet : public HemisphereAudioApplet {
public:

  enum DynamicsCursor {
    //IN_GAIN,
    GATE_THRESH,
    COMP_THRESH,
    OUT_GAIN,
    LIMIT_THRESH,

    MAX_CURSOR = LIMIT_THRESH
  };

  const char* applet_name() {
    return "Dynamics";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      complimit[i] = GetComp();
      if (!complimit[i]) return;

      PatchCable(input, i, *complimit[i], 0);
      PatchCable(*complimit[i], 0, output, i);
    }
    alloc_ok = true;
    SetParams();
  }

  void Unload() {
    for (auto& cl : complimit) ReleaseComp(cl);
    alloc_ok = false;
    AllowRestart();
  }

  void SetParams() {
    if (!alloc_ok) return;
    for (int i = 0; i < Channels; i++) {
      complimit[i]->gate(gate_threshold * 1.0f);
      complimit[i]->compression(comp_threshold * 1.0f);
      complimit[i]->limit(limit_threshold * 1.0f);
      if (makeupgain < 0)
        complimit[i]->autoMakeupGain();
      else
        complimit[i]->makeupGain(makeupgain);
    }
  }

  void Controller() {
    if (!alloc_ok) return;
    for (int i = 0; i < Channels; i++) {
      // TODO: connect modulated param values to stuff
    }
  }

  void DrawFullScreen() final;
  void View() final;

  void MainView() {
    if (!alloc_ok) {
      gfxPrint(2, 15, "Out of RAM!!");
      return;
    }

    const int label_x = 1;

    gfxPrint(label_x, 15, "Gate:");
    gfxStartCursor();
    if (gate_threshold < LVL_MIN_DB)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", gate_threshold);
    gfxEndCursor(cursor == GATE_THRESH);

    gfxPrint(label_x, 25, "Comp:");
    gfxStartCursor();
    if (comp_threshold >= 0)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", comp_threshold);
    gfxEndCursor(cursor == COMP_THRESH);

    gfxPrint(label_x, 35, "MakeUp:");
    gfxStartCursor(label_x, 45);
    if (makeupgain < 0)
      gfxPrint("auto");
    else
      graphics.printf("+%3ddB", makeupgain);
    gfxEndCursor(cursor == OUT_GAIN);

    gfxPrint(label_x, 55, "Lim: ");
    gfxStartCursor();
    if (limit_threshold >= 0)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", limit_threshold);
    gfxEndCursor(cursor == LIMIT_THRESH);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    switch (cursor) {
      case GATE_THRESH:
        gate_threshold
          = constrain(gate_threshold + direction, LVL_MIN_DB - 1, 0);
        break;
      case COMP_THRESH:
        comp_threshold = constrain(comp_threshold + direction, LVL_MIN_DB - 1, 0);
        break;
      case LIMIT_THRESH:
        limit_threshold = constrain(limit_threshold + direction, LVL_MIN_DB - 1, 0);
        break;
      case OUT_GAIN:
        makeupgain = constrain(makeupgain + direction, -1, 30);
        break;

      default:
        return;
        break;
    }

    SetParams();
  }

  uint64_t OnDataRequest() {
    return PackPackables(gate_threshold, comp_threshold, limit_threshold, makeupgain);
  }
  void OnDataReceive(uint64_t data) {
    UnpackPackables(data, gate_threshold, comp_threshold, limit_threshold, makeupgain);
    SetParams();
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
  int8_t gate_threshold = -70;
  int8_t comp_threshold = -6;
  int8_t limit_threshold = -1;
  int8_t makeupgain = 0; // negative means auto

  bool alloc_ok = false;

  // TODO: more params for attack/release of each stage, comp ratio & knee
  //
  // for reference, from effect_dynamics.h:
  //void gate(float threshold = -50.0f, float attack = MIN_T, float release = 0.3f, float hysterisis = 6.0f)
  //void compression(float threshold = -40.0f, float attack = MIN_T, float release = 0.5f, float ratio = 35.0f, float kneeWidth = 6.0f)
  //void makeupGain(float gain = 0.0f)
  //void limit(float threshold = -3.0f, float attack = MIN_T, float release = MIN_T)

  AudioPassthrough<Channels> input;
  std::array<AudioEffectDynamics*, Channels> complimit;
  AudioPassthrough<Channels> output;
};

template <AudioChannels Channels>
FLASHMEM void DynamicsApplet<Channels>::View() {
  MainView();
}

template <AudioChannels Channels>
FLASHMEM void DynamicsApplet<Channels>::DrawFullScreen() {
  if (!alloc_ok) return;
  graphics.drawLine(64 - gfx_offset, 26, 127 - gfx_offset, 26, 3);
  const int x = 25 + (64 - gfx_offset);
  const int h = Proportion(int(complimit[0]->get_total_gain()), 60, 30); // 60 dB == 30 px
  if (h > 0)
    graphics.drawRect(x, 26 - h, 10, h);
  else
    graphics.drawRect(x, 26, 10, -h);
  View();
}
