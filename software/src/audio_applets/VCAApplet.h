#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

template <AudioChannels Channels>
class VcaApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return "VCA";
  }

  void Start() {
    // Less CPU than hermite, but I couldn't hear the difference whereas I could
    // with ZOH
    cv_stream.Method(INTERPOLATION_LINEAR);
    cv_stream.Acquire();
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, muls[i], 0);
      cv_conns[i].connect(cv_stream, 0, muls[i], 1);
      mul_to_mixer[i].connect(muls[i], 0, bias_mixer[i], 0);
      bias_to_mixer[i].connect(input, i, bias_mixer[i], 1);
      out_conns[i].connect(bias_mixer[i], 0, output, i);
    }
  }

  void Unload() {
    cv_stream.Release();
    AllowRestart();
  }

  void Controller() {
    float lin_cv = level_cv.InF(1.0f);
    if (rectify && lin_cv < 0.0f) lin_cv = 0.0f;
    float cv = 32768.0f * (shape > 0 ? varexp(0.3f * shape, lin_cv) : lin_cv);
    cv_stream.Push(Clip16(cv));
    float lvl_scalar = level < VCA_MIN_DB ? 0.0f : dbToScalar(level);
    float off_scalar = offset < VCA_MIN_DB ? 0.0f : dbToScalar(offset);
    for (int i = 0; i < Channels; i++) {
      bias_mixer[i].gain(0, lvl_scalar);
      bias_mixer[i].gain(1, off_scalar);
    }
  }

  void View() {
    gfxPrint(1, 15, "Lvl:");
    gfxStartCursor();
    gfxPrintDb(level);
    gfxEndCursor(cursor == 0);
    gfxStartCursor();
    gfxPrintIcon(level_cv.Icon());
    gfxEndCursor(cursor == 1);

    gfxPrint(1, 25, "Off:");
    gfxStartCursor();
    gfxPrintDb(offset);
    gfxEndCursor(cursor == 2);

    gfxPrint(1, 35, "Exp: ");
    gfxStartCursor();
    graphics.printf("%3d%%", shape);
    gfxEndCursor(cursor == 3);
    gfxStartCursor();
    gfxPrintIcon(PARAM_MAP_ICONS + 8 * shape_cv.source);
    gfxEndCursor(cursor == 4);

    gfxPrint(1, 45, "Rectify: ");
    gfxStartCursor();
    gfxPrintIcon(rectify ? CHECK_ON_ICON : CHECK_OFF_ICON);
    gfxEndCursor(cursor == 5);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS - 1);
      return;
    }
    switch (cursor) {
      case 0:
        level = constrain(level + direction, VCA_MIN_DB - 1.0f, VCA_MAX_DB);
        break;
      case 1:
        level_cv.ChangeSource(direction);
        break;
      case 2:
        offset = constrain(offset + direction, VCA_MIN_DB - 1.0f, VCA_MAX_DB);
        break;
      case 3:
        shape = constrain(shape + direction, 0, 100);
        break;
      case 4:
        shape_cv.ChangeSource(direction);
        break;
      case 5:
        rectify = !rectify;
        break;
    }
  }

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  static const int NUM_PARAMS = 6;
  // -90 = 15bits of depth so no point in going lower
  static const int VCA_MIN_DB = -90;
  static const int VCA_MAX_DB = 90;

  int8_t cursor = 0;
  int8_t level = 0;
  int8_t offset = VCA_MIN_DB - 1;
  int8_t shape = 0;
  CVInput level_cv;
  CVInput shape_cv;
  bool rectify = true;

  AudioPassthrough<Channels> input;
  InterpolatingStream<> cv_stream;
  std::array<AudioEffectMultiply, Channels> muls;
  std::array<AudioMixer<2>, Channels> bias_mixer;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> cv_conns;
  std::array<AudioConnection, Channels> mul_to_mixer;
  std::array<AudioConnection, Channels> bias_to_mixer;
  std::array<AudioConnection, Channels> out_conns;

  void gfxPrintDb(int db) {
    if (db < VCA_MIN_DB) gfxPrint("    - ");
    else graphics.printf("%3ddB", db);
  }

  // Gives variable curve exponent by controlling the base normalized to go from
  // 0 to 1 for powers 0 to 1.
  float varexp(float log2base, float power) {
    return (fastpow2(log2base * power) - 1.0f) / (fastpow2(log2base) - 1.0f);
  }
};
