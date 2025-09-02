#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "dsputils_arm.h"
#include "hemisphere_audio_config.h"
#include "synth_waveform.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

class OscApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() override {
    return "Osc";
  }
  void Start() override {
    pwm_stream.Acquire();
    pwm_stream.Method(INTERPOLATION_LINEAR);
    mod_cv_stream.Acquire();
    mod_cv_stream.Method(INTERPOLATION_LINEAR);
    mod_vca.level(1.0f);
    mod_vca.rectify(true);
    SetModType(mod_type);
    SetModDepth(mod_depth);
    vca_cv.Acquire();
    vca_cv.Method(INTERPOLATION_LINEAR);
    vca.rectify(true);
  }

  void Unload() override {
    pwm_stream.Release();
    mod_cv_stream.Release();
    vca_cv.Release();
    AllowRestart();
  }

  void Controller() override {
    float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
    synth.frequency(freq);
    synth.amplitude(1.0f);
    pwm_stream.Push(
      float_to_q15(0.01f * static_cast<float>(pw * 2 - 100) + pw_cv.InF())
    );
    if (mod_cv.source) mod_cv_stream.Push(float_to_q15(mod_cv.InF()));

    float m
      = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);
    float gain = dbToScalar(level);
    if (level_cv.source) {
      vca.bias(0.0f);
      vca.level(gain);
      vca_cv.Push(float_to_q15(dbToScalar(-48 * (1.0f - level_cv.InF())))
      );
    } else {
      vca.bias(gain);
      vca.level(0.0f);
    }
    // There's a good chance of phase correlation if the incoming signal is
    // internal, so use equal amplitude
    mixer.gain(1, m);
    mixer.gain(0, 1.0f - m);
  }

  void View() override {
    gfxStartCursor(1, 15);
    gfxPrint(WAVEFORM_NAMES[waveform]);
    gfxEndCursor(cursor == WAVEFORM);

    if (WAVEFORMS[waveform] != WAVEFORM_SINE) {
      gfxStartCursor(1 + 3 * 6 + 2 * 6, 15);
      graphics.printf("%3d%%", pw);
      gfxEndCursor(cursor == PW);

      gfxStartCursor();
      gfxPrint(pw_cv);
      gfxEndCursor(cursor == PW_CV, false, pw_cv.InputName());
    }

    gfxStartCursor(1, 25);
    gfxPrintTuningIndicator(pitch);
    gfxEndCursor(cursor == OCTAVE);
    gfxStartCursor(11, 25);
    gfxPrintPitchHz(pitch);
    gfxEndCursor(cursor == PITCH);

    gfxStartCursor();
    gfxPrint(pitch_cv);
    gfxEndCursor(cursor == PITCH_CV, false, pitch_cv.InputName());

    gfxStartCursor(1, 35);
    gfxPrint(MOD_TYPE_NAMES[mod_type]);
    gfxEndCursor(cursor == MOD_TYPE);
    gfxPrint(":  ");

    gfxStartCursor();
    graphics.printf("%1d.%02d", SPLIT_INT_DEC(mod_depth * 0.01f, 100));
    gfxEndCursor(cursor == MOD_DEPTH);

    gfxStartCursor();
    gfxPrint(mod_cv);
    gfxEndCursor(cursor == MOD_CV, false, mod_cv.InputName());

    gfxPrint(1, 45, "Lvl:");
    gfxStartCursor();
    graphics.printf("%3ddB", level);
    gfxEndCursor(cursor == LEVEL);

    gfxStartCursor();
    gfxPrint(level_cv);
    gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

    gfxPrint(1, 55, "Mix: ");
    gfxStartCursor();
    graphics.printf("%3d%%", mix);
    gfxEndCursor(cursor == MIX);

    gfxStartCursor();
    gfxPrint(mix_cv);
    gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

    gfxDisplayInputMapEditor();
  }

#define OSC_PARAMS \
  pw, pitch, mod_depth, level, mix, pack<2>(waveform), pack<2>(mod_type)

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(OSC_PARAMS);
    data[1] = PackPackables(pw_cv, pitch_cv, mod_cv, level_cv);
    data[2] = PackPackables(mix_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], OSC_PARAMS);
    SetWaveform(waveform);
    SetModType(mod_type);
    SetModDepth(mod_depth);
    UnpackPackables(data[1], pw_cv, pitch_cv, mod_cv, level_cv);
    UnpackPackables(data[2], mix_cv);
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(cursor,
          IndexedInput(PITCH_CV, pitch_cv),
          IndexedInput(PW_CV, pw_cv),
          IndexedInput(MIX_CV, mix_cv),
          IndexedInput(MOD_CV, mod_cv),
          IndexedInput(LEVEL_CV, level_cv)
        ))
      return;
    CursorToggle();
  }

  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      do {
        MoveCursor(cursor, direction, MIX_CV);
      } while ((cursor == PW || cursor == PW_CV)
               && WAVEFORMS[waveform] == WAVEFORM_SINE);
      return;
    }
    if (EditSelectedInputMap(direction)) return;

    const int max_pitch = 7 * 12 * 128;
    const int min_pitch = -3 * 12 * 128;
    switch (cursor) {
      case WAVEFORM:
        SetWaveform(waveform + direction);
        break;
      case OCTAVE:
        pitch = constrain(pitch + direction * 1 * 128, min_pitch, max_pitch);
        break;
      case PW:
        pw = constrain(pw + direction, 0, 100);
        break;
      case PW_CV:
        pw_cv.ChangeSource(direction);
        break;
      case PITCH:
        pitch = constrain(pitch + direction * 4, min_pitch, max_pitch);
        break;
      case PITCH_CV:
        pitch_cv.ChangeSource(direction);
        break;
      case MOD_TYPE:
        SetModType(mod_type + direction);
        break;
      case MOD_DEPTH:
        SetModDepth(mod_depth + direction);
        break;
      case MOD_CV:
        mod_cv.ChangeSource(direction);
        break;
      case LEVEL:
        level = constrain(level + direction, -90, 90);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
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
    return &input_stream;
  }
  AudioStream* OutputStream() override {
    return &mixer;
  }

  void SetWaveform(int wf) {
    waveform = constrain(wf, 0, 2);
    synth.begin(WAVEFORMS[waveform]);
  }

  void SetModDepth(int depth) {
    mod_depth = constrain(depth, 0, MOD_DEPTH_MAX);
    mod_vca.bias(static_cast<float>(mod_depth) / MOD_DEPTH_MAX);
  }

  void SetModType(int t) {
    mod_type = static_cast<ModType>(constrain(t, FM, PM));
    switch (mod_type) {
      case FM:
        synth.frequencyModulation(FM_DEPTH);
        break;
      case PM:
        synth.phaseModulation(PM_DEPTH);
        break;
    };
  }

protected:
  void SetHelp() override {}

private:
  enum Cursor : int8_t {
    WAVEFORM,
    PW,
    PW_CV,
    OCTAVE,
    PITCH,
    PITCH_CV,
    MOD_TYPE,
    MOD_DEPTH,
    MOD_CV,
    LEVEL,
    LEVEL_CV,
    MIX,
    MIX_CV,
  };

  int8_t cursor = WAVEFORM;
  static constexpr int8_t WAVEFORMS[3]
    = {WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE, WAVEFORM_BANDLIMIT_PULSE};
  static constexpr char const* WAVEFORM_NAMES[3] = {"SIN", "TRI", "PLS"};
  static constexpr char const* MOD_TYPE_NAMES[2] = {"FM", "PM"};
  enum ModType : uint8_t { FM, PM };
  static const int MOD_DEPTH_MAX = 500;
  // 5 octaves; empirically about what Plaits max FM corresponds to
  static constexpr float FM_DEPTH = 5.0f;
  // 5 periods; for reference, parasite Warps uses 4 but I liked the ability
  // to go a bit deeper
  static constexpr float PM_DEPTH = 180.0f * 5.0f;

  uint8_t waveform;
  int8_t pw = 50;
  int16_t pitch = 1 * 12 * 128; // C4
  ModType mod_type = PM;
  uint16_t mod_depth = 0;
  int8_t level = 0; // dB
  int8_t mix = 100;

  CVInputMap pw_cv;
  CVInputMap pitch_cv;
  CVInputMap mod_cv;
  CVInputMap level_cv;
  CVInputMap mix_cv;

  AudioPassthrough<MONO> input_stream;
  InterpolatingStream<> pwm_stream;
  InterpolatingStream<> mod_cv_stream;
  AudioVCA mod_vca;
  AudioSynthWaveformModulated synth;
  InterpolatingStream<> vca_cv;
  AudioVCA vca;
  AudioMixer<2> mixer;

  AudioConnection input_to_mod_vca{input_stream, 0, mod_vca, 0};
  AudioConnection mod_cv_to_mod_vca{mod_cv_stream, 0, mod_vca, 1};
  AudioConnection mod_vca_to_synth{mod_vca, 0, synth, 0};
  AudioConnection in_conn{input_stream, 0, mixer, 0};
  AudioConnection pwm_conn{pwm_stream, 0, synth, 1};
  AudioConnection cv_to_vca{vca_cv, 0, vca, 1};
  AudioConnection synth_to_vca{synth, 0, vca, 0};
  AudioConnection vca_to_mixer{vca, 0, mixer, 1};
};
