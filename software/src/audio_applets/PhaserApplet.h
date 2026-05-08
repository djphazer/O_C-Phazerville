// Phazer applet and DSP effect
// Authored by Ivan Cohen
// modified by djphazer

#include "../src/Audio/effect_phaser.h"

// MONO only... for now
class PhazerApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "Phazer";
  }
  void Start() override {
    if (!phaser && OC::CORE::FreeRam() > (int)sizeof(AudioEffectPhazer)) {
      phaser = new AudioEffectPhazer();
    }
    if (!phaser) return;

    PatchCable(input, 0, dry_wet_mixer, 1);
    PatchCable(input, 0, *phaser, 0);
    PatchCable(*phaser, 0, dry_wet_mixer, 0);

    dry_wet_mixer.gain(1, 1.0f);
  }

  void Controller() override {
    if (!phaser) {
      return;
    }

    phaser->setDepth(0.01f * depth + depth_cv.InF());
    phaser->setFeedback(0.01f * feedback + feedback_cv.InF());

    // rate * rate == centihertz
    float rate_mod = constrain(rate + rate_cv.InF() * MAX_RATE, 1, MAX_RATE);
    rate_mod *= rate_mod;
    phaser->setRate(0.01f * rate_mod);

    float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);

    dry_wet_mixer.gain(0, m);
    dry_wet_mixer.gain(1, 1.0f - m);
  }

  void View() override {
    if (!phaser) {
      gfxPrint(1, 15, "Out Of RAM !!!");
      return;
    };
    gfxPrint(1, 15, "Depth:");
    gfxStartCursor();
    graphics.printf("%3d%%", depth);
    gfxEndCursor(cursor == DEPTH);

    gfxStartCursor();
    gfxPrint(depth_cv);
    gfxEndCursor(cursor == DEPTH_CV, false, depth_cv.InputName());

    gfxPrint(1, 25, "Feed:");
    gfxStartCursor();
    graphics.printf("%3d%%", feedback);
    gfxEndCursor(cursor == FEED);

    gfxStartCursor();
    gfxPrint(feedback_cv);
    gfxEndCursor(cursor == FEED_CV, false, feedback_cv.InputName());

    gfxPrint(1, 35, "Hz:");
    gfxStartCursor();
    int rmod = rate * rate;
    graphics.printf("%2d.%02d", rmod / 100, rmod % 100);
    gfxEndCursor(cursor == RATE);

    gfxStartCursor();
    gfxPrint(rate_cv);
    gfxEndCursor(cursor == RATE_CV, false, rate_cv.InputName());

    gfxPrint(1, 45, "Mix:");
    gfxStartCursor();
    graphics.printf("%3d%%", mix);
    gfxEndCursor(cursor == MIX);

    gfxStartCursor();
    gfxPrint(mix_cv);
    gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

    gfxDisplayInputMapEditor();
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(mix, depth, feedback, rate);
    data[1] = PackPackables(mix_cv, depth_cv, feedback_cv, rate_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], mix, depth, feedback, rate);
    UnpackPackables(data[1], mix_cv, depth_cv, feedback_cv, rate_cv);
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(MIX_CV, mix_cv),
          IndexedInput(DEPTH_CV, depth_cv),
          IndexedInput(FEED_CV, feedback_cv),
          IndexedInput(RATE_CV, rate_cv)
        ))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MIX_CV);
      return;
    }

    if (EditSelectedInputMap(direction)) return;

    switch (cursor) {
      case DEPTH:
        depth = constrain(depth + direction, 0, 100);
        break;
      case DEPTH_CV:
        depth_cv.ChangeSource(direction);
        break;
      case FEED:
        feedback = constrain(feedback + direction, 1, 99);
        break;
      case FEED_CV:
        feedback_cv.ChangeSource(direction);
        break;
      case RATE:
        rate = constrain(rate + direction, 1, MAX_RATE);
        break;
      case RATE_CV:
        rate_cv.ChangeSource(direction);
        break;
      case MIX:
        mix = constrain(mix + direction, 0, 100);
        break;
      case MIX_CV:
        mix_cv.ChangeSource(direction);
        break;
      default:
        break;
    }
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &dry_wet_mixer;
  }

protected:
  void SetHelp() override {}

private:
  enum Cursor : int8_t {
    DEPTH,
    DEPTH_CV,
    FEED,
    FEED_CV,
    RATE,
    RATE_CV,
    MIX,
    MIX_CV
  };

  static constexpr int MAX_RATE = 100;

  int8_t cursor = DEPTH;
  AudioPassthrough<MONO> input;

  AudioEffectPhazer* phaser;
  AudioMixer<2> dry_wet_mixer;

  uint8_t mix = 50; // 0 to 100 %
  uint8_t depth = 50; // 0 to 100%
  uint8_t feedback = 50; // 1 to 99 %
  uint16_t rate = 1; // squared, centi-Hz

  CVInputMap mix_cv;
  CVInputMap depth_cv;
  CVInputMap feedback_cv;
  CVInputMap rate_cv;
};
