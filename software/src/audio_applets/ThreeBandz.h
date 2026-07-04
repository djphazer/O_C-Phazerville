// yo check this out
//

#include "../HemisphereAudioApplet.h"
#include "../Audio/AudioMixer.h"
#include "../Audio/AudioPassthrough.h"

// stereo only for now
//template <AudioChannels Channels>
class ThreeBandzApplet : public HemisphereAudioApplet {
public:
  static constexpr int BANDZ = 3;
  static constexpr int Channels = 2;

  enum ThreeBandCursor {
    //IN_GAIN,
    B1_GATE_THRESH,
    B1_COMP_THRESH,
    B1_OUT_GAIN,
    B1_LIMIT_THRESH,

    B1_SPLIT,

    B2_GATE_THRESH,
    B2_COMP_THRESH,
    B2_OUT_GAIN,
    B2_LIMIT_THRESH,

    B2_SPLIT,

    B3_GATE_THRESH,
    B3_COMP_THRESH,
    B3_OUT_GAIN,
    B3_LIMIT_THRESH,

    MAX_CURSOR = B3_LIMIT_THRESH
  };

  const char* applet_name() {
    return "3-Bandz";
  }

  void Start() {
    for (int ch = 0; ch < Channels; ++ch) {
      for (int b = 0; b < BANDZ; ++b) {
        complimit[ch][b] = GetComp();
        if (!complimit[ch][b]) return;

        PatchCable(*complimit[ch][b], 0, mixout[ch], b);
        //mixout[ch].gain(b, 1.0f);
        mixout[ch].gain(b, (b==1)?-1.0f:1.0f); // invert mid band
      }

      PatchCable(input, ch, filter[ch][0], 0); // low split
      PatchCable(filter[ch][0], 2, filter[ch][1], 0); // hi split

      PatchCable(filter[ch][0], 0, *complimit[ch][0], 0); // low band
      PatchCable(filter[ch][1], 0, *complimit[ch][1], 0); // mid band
      PatchCable(filter[ch][1], 2, *complimit[ch][2], 0); // hi band

      PatchCable(mixout[ch], 0, output, ch);

      filter[ch][0].resonance(0.70f);
      filter[ch][1].resonance(0.70f);
    }
    alloc_ok = true;
    SetParams();
  }

  void Unload() {
    for (int ch = 0; ch < Channels; ++ch) {
      for (int b = 0; b < BANDZ; ++b) {
        ReleaseComp(complimit[ch][b]);
      }
    }
    alloc_ok = false;
    AllowRestart();
  }

  void SetParams() {
    if (!alloc_ok) return;
    for (int ch = 0; ch < Channels; ++ch) {
      for (int i = 0; i < BANDZ; i++) {
        complimit[ch][i]->gate(gate_threshold[i] * 1.0f);
        complimit[ch][i]->compression(comp_threshold[i] * 1.0f);
        complimit[ch][i]->limit(limit_threshold[i] * 1.0f);
        if (makeupgain[i] < 0)
          complimit[ch][i]->autoMakeupGain();
        else
          complimit[ch][i]->makeupGain(makeupgain[i]);
      }

      filter[ch][0].frequency(splitfreq[0]);
      filter[ch][1].frequency(splitfreq[1]);
    }
  }

  void Controller() {
    if (!alloc_ok) return;
    for (int i = 0; i < BANDZ; i++) {
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
    const int page = (cursor > B1_SPLIT) + (cursor > B2_SPLIT);
    const char * const bandname[] = { "Low", "Mid", "Hi" };

    gfxRect(page*20, 10, 21, 3); // page tab

    gfxPrint(label_x, 14, bandname[page]);
    gfxPrint("G:");
    gfxStartCursor();
    if (gate_threshold[page] < LVL_MIN_DB)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", gate_threshold[page]);
    gfxEndCursor(cursor == B1_GATE_THRESH || cursor == B2_GATE_THRESH || cursor == B3_GATE_THRESH);

    gfxPrint(label_x, 25, "Comp:");
    gfxStartCursor();
    if (comp_threshold[page] >= 0)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", comp_threshold[page]);
    gfxEndCursor(cursor == B1_COMP_THRESH || cursor == B2_COMP_THRESH || cursor == B3_COMP_THRESH);

    gfxPrint(label_x, 35, "Gain:");
    gfxStartCursor();
    if (makeupgain[page] < 0)
      gfxPrint("auto");
    else
      graphics.printf("+%2ddB", makeupgain[page]);
    gfxEndCursor(cursor == B1_OUT_GAIN || cursor == B2_OUT_GAIN || cursor == B3_OUT_GAIN);

    gfxPrint(label_x, 45, "Lim: ");
    gfxStartCursor();
    if (limit_threshold[page] >= 0)
      graphics.printf("off");
    else
      graphics.printf("%3ddB", limit_threshold[page]);
    gfxEndCursor(cursor == B1_LIMIT_THRESH || cursor == B2_LIMIT_THRESH || cursor == B3_LIMIT_THRESH);

    if (page < 2) {
      gfxPrint(label_x, 55, "Hz: ");
      gfxStartCursor();
      gfxPrint(splitfreq[page]);
      gfxEndCursor(cursor == B1_SPLIT || cursor == B2_SPLIT);
    }
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }

    const int page = (cursor > B1_SPLIT) + (cursor > B2_SPLIT);
    switch (cursor) {
      case B1_GATE_THRESH:
      case B2_GATE_THRESH:
      case B3_GATE_THRESH:
        gate_threshold[page]
          = constrain(gate_threshold[page] + direction, LVL_MIN_DB - 1, 0);
        break;
      case B1_COMP_THRESH:
      case B2_COMP_THRESH:
      case B3_COMP_THRESH:
        comp_threshold[page] = constrain(comp_threshold[page] + direction, LVL_MIN_DB - 1, 0);
        break;
      case B1_LIMIT_THRESH:
      case B2_LIMIT_THRESH:
      case B3_LIMIT_THRESH:
        limit_threshold[page] = constrain(limit_threshold[page] + direction, LVL_MIN_DB - 1, 0);
        break;
      case B1_OUT_GAIN:
      case B2_OUT_GAIN:
      case B3_OUT_GAIN:
        makeupgain[page] = constrain(makeupgain[page] + direction, -1, 30);
        break;

      case B1_SPLIT:
        splitfreq[0] = constrain(splitfreq[0] + direction*50, 0, splitfreq[1]);
        break;

      case B2_SPLIT:
        splitfreq[1] = constrain(splitfreq[1] + direction*50, splitfreq[0], 20000);
        break;

      default:
        return;
        break;
    }

    SetParams();
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(
      gate_threshold[0],
      comp_threshold[0],
      limit_threshold[0],
      makeupgain[0],
      gate_threshold[1],
      comp_threshold[1],
      limit_threshold[1],
      makeupgain[1]
    );
    data[1] = PackPackables(
      gate_threshold[2], comp_threshold[2], limit_threshold[2], makeupgain[2],
      splitfreq[0], splitfreq[1]
    );
  }
  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(
      data[0],
      gate_threshold[0],
      comp_threshold[0],
      limit_threshold[0],
      makeupgain[0],
      gate_threshold[1],
      comp_threshold[1],
      limit_threshold[1],
      makeupgain[1]
    );
    UnpackPackables(
      data[1],
      gate_threshold[2], comp_threshold[2], limit_threshold[2], makeupgain[2],
      splitfreq[0], splitfreq[1]
    );
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
  int8_t gate_threshold[BANDZ] = {LVL_MIN_DB - 1, LVL_MIN_DB - 1, LVL_MIN_DB - 1};
  int8_t comp_threshold[BANDZ] = {-12, -12, -18};
  int8_t limit_threshold[BANDZ] = {-3, -6, -6};
  int8_t makeupgain[BANDZ] = {-1, -1, -1}; // negative means auto

  uint16_t splitfreq[2] = {250, 2500};

  bool alloc_ok = false;

  AudioPassthrough<Channels> input;
  AudioFilterStateVariable2 filter[Channels][2];
  AudioEffectDynamics* complimit[Channels][BANDZ];
  std::array<AudioMixer<BANDZ>, Channels> mixout;
  // TODO: maybe a final limiter after the mix?
  AudioPassthrough<Channels> output;
};

FLASHMEM void ThreeBandzApplet::View() {
  MainView();
}
FLASHMEM void ThreeBandzApplet::DrawFullScreen() {
  if (!alloc_ok) return;
  graphics.drawLine(64 - gfx_offset, 26, 127 - gfx_offset, 26, 3);
  for (int i = 0; i < BANDZ; i++) {
    const int x = 5 + i*20 + (64 - gfx_offset);
    const int h = Proportion(int(complimit[0][i]->get_total_gain()), 60, 30); // 60 dB == 30 px
    if (h > 0)
      graphics.drawRect(x, 26 - h, 10, h);
    else
      graphics.drawRect(x, 26, 10, -h);
  }
  View();
}
