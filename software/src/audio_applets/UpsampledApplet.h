#include "HSicons.h"
#include "HemisphereAudioApplet.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

class UpsampledApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "Upsampled";
  }

  void Start() override {
    interp_stream.Acquire();
    interp_stream.Method(static_cast<InterpolationMethod>(method));
  }

  void Unload() override {
    interp_stream.Release();
    AllowRestart();
  }

  void Controller() override {
    int16_t in = input.In();
    // 0.001 should put cutoff at ~2.7Hz for SR of 16666
    ONE_POLE(lp, in, 0.001f);

    if (ac_couple) in -= lp;
    interp_stream.Push(Clip16(0.01f * gain * scalar * in));
  }

  void View() override {
    gfxPrint(1, 15, "Source:");
    gfxStartCursor();
    gfxPrintIcon(input.Icon());
    gfxEndCursor(cursor == 0);

    gfxPrint(1, 25, "Interp:");
    gfxStartCursor();
    switch (method) {
      case INTERPOLATION_ZOH:
        gfxPrint("ZOH");
        break;
      case INTERPOLATION_LINEAR:
        gfxPrint("Lin");
        break;
      case INTERPOLATION_HERMITE:
        gfxPrint("Spl");
        break;
    }
    gfxEndCursor(cursor == 1);

    gfxPrint(1, 35, "Gain:");
    gfxStartCursor();
    graphics.printf("%4d%%", gain);
    gfxEndCursor(cursor == 2);

    gfxPrint(1, 45, "AC:    ");
    gfxStartCursor();
    gfxPrintIcon(ac_couple ? CHECK_ON_ICON : CHECK_OFF_ICON);
    gfxEndCursor(cursor == 3);
  }

  void OnButtonPress() override {
    if (cursor == 3) {
      ac_couple = !ac_couple;
    } else {
      CursorToggle();
    }
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, 3);
      return;
    }

    switch (cursor) {
      case 0:
        input.ChangeSource(direction);
        break;
      case 1:
        method += direction;
        CONSTRAIN(method, 0, 2);
        interp_stream.Method(static_cast<InterpolationMethod>(method));
        break;
      case 2:
        gain += direction;
        CONSTRAIN(direction, -999, 999);
        break;
    }
  }
  void OnDataReceive(uint64_t data) override {}
  uint64_t OnDataRequest() override {
    return 0;
  }

  AudioStream* InputStream() override {
    return nullptr;
  }
  AudioStream* OutputStream() override {
    return &interp_stream;
  };

protected:
  void SetHelp() override {}

private:
  InterpolatingStream<> interp_stream;
  CVInput input;
  float lp = 0.0f;
  static constexpr float scalar = -31267.0f / HEMISPHERE_MAX_CV;
  int cursor = 0;
  int8_t method = INTERPOLATION_HERMITE;
  int16_t gain = 100;
  boolean ac_couple = 0;
};
