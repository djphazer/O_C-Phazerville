#include "../src/Audio/synth_advanced_karplus.h"

// AdvKrpsStrngApplet — Advanced Karplus-Strong string synthesizer
//
// A MONO audio source applet built on AudioSynthAdvancedKarplus.
//
// Parameters:
//   Pitch     — base pitch (semitone cursor) + fine Hz cursor, V/Oct CV
//   Trigger   — DigitalInputMap: fires noteOn() after ADC lag settles
//   Decay     — perceptual decay time (pitch-compensated loop gain), CV-able
//   Brt       — brightness / filter cutoff (IIR α), CV-able
//   Bdy       — body resonance on excitation noise (bandpass Q), CV-able
//   Mix       — dry-input / synth blend, CV-able
//
// UI layout (5 rows, modelled after OscApplet):
//   y=15: [note name] [Hz]          [pitch CV icon]
//   y=25: Trg:        [trigger icon]
//   y=35: Dcy:  [0-100]             [decay CV icon]
//   y=45: Brt:  [0-100]             [brightness CV icon]
//   y=55: context-sensitive —
//           cursor ≤ BODY_CV  →  Bdy: [0-100]  [body CV icon]
//           cursor >  BODY_CV →  Mix: [0-100]%  [mix CV icon]
//
// Signal flow:
//   input_stream ──► output_mixer[0]      (dry bypass)
//   synth        ──► output_mixer[1]      (KS synthesis)
//   output_mixer ──► OutputStream()

class AdvKrpsStrngApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() { return "KrpsStrng"; }

  // --- Lifecycle ----------------------------------------------------------

  void Start() override {
    synth.Acquire();
    adc_lag_ = -1;

    PatchCable(input_stream, 0, output_mixer, 0);  // dry path
    PatchCable(synth,        0, output_mixer, 1);  // synth path

    output_mixer.gain(0, 1.0f - mix * 0.01f);
    output_mixer.gain(1, mix * 0.01f);
  }

  void Unload() override {
    synth.Release();
    AllowRestart();
  }

  // --- Controller (runs at OC ISR rate, ~16 kHz) -------------------------

  void Controller() override {
    // Pitch: base + V/Oct CV
    float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
    synth.setFrequency(freq);

    // Decay: parameter + CV offset, clamped [0, 1]
    float d = constrain(decay * 0.01f + decay_cv.InF(), 0.0f, 1.0f);
    synth.setDecay(d);

    // Brightness: parameter + CV offset
    float b = constrain(brightness * 0.01f + brightness_cv.InF(), 0.0f, 1.0f);
    synth.setBrightness(b);

    // Body: parameter + CV offset
    float bod = constrain(body * 0.01f + body_cv.InF(), 0.0f, 1.0f);
    synth.setBody(bod);

    // Mix: wet/dry blend
    float m = constrain(mix * 0.01f + mix_cv.InF(), 0.0f, 1.0f);
    output_mixer.gain(0, 1.0f - m);
    output_mixer.gain(1, m);

    // Trigger with ADC lag: pitch CV must settle before noteOn fires.
    // Per-instance countdown avoids sharing frame.adc_lag_countdown[io_offset]
    // with other AdvKrpsStrng instances on the same side (same hemisphere value).
    if (trig_cv.Clock()) adc_lag_ = HEMISPHERE_ADC_LAG;
    if (adc_lag_ > 0 && --adc_lag_ == 0) synth.noteOn(1.0f);
  }

  // --- View (64×64 display) ----------------------------------------------

  void View() override {
    // Row 1 — Pitch (two editable cursors + CV source)
    gfxStartCursor(1, 15);
    gfxPrintTuningIndicator(pitch);          // note name + tuning bar
    gfxEndCursor(cursor == PITCH);

    gfxStartCursor(11, 15);
    gfxPrintPitchHz(pitch);                  // e.g. " 261.6Hz"
    gfxEndCursor(cursor == PITCH_FINE);

    gfxStartCursor();
    gfxPrint(pitch_cv);
    gfxEndCursor(cursor == PITCH_CV, false, pitch_cv.InputName());

    // Row 2 — Trigger source
    gfxPrint(1, 25, "Trg:");
    gfxStartCursor(25, 25);
    gfxPrint(trig_cv);
    gfxEndCursor(cursor == TRIG_CV, false, trig_cv.InputName(), "Trigger");

    // Row 3 — Decay
    gfxPrint(1, 35, "Dcy:");
    gfxStartCursor(25, 35);
    graphics.printf("%3d", decay);
    gfxEndCursor(cursor == DECAY);
    gfxStartCursor();
    gfxPrint(decay_cv);
    gfxEndCursor(cursor == DECAY_CV, false, decay_cv.InputName());

    // Row 4 — Brightness
    gfxPrint(1, 45, "Brt:");
    gfxStartCursor(25, 45);
    graphics.printf("%3d", brightness);
    gfxEndCursor(cursor == BRIGHT);
    gfxStartCursor();
    gfxPrint(brightness_cv);
    gfxEndCursor(cursor == BRIGHT_CV, false, brightness_cv.InputName());

    // Row 5 — context-sensitive: Body (while editing Pitch/Trig/Decay/Brt/Body)
    //                           or Mix (while editing Mix)
    if (cursor <= BODY_CV) {
      gfxPrint(1, 55, "Bdy:");
      gfxStartCursor(25, 55);
      graphics.printf("%3d", body);
      gfxEndCursor(cursor == BODY);
      gfxStartCursor();
      gfxPrint(body_cv);
      gfxEndCursor(cursor == BODY_CV, false, body_cv.InputName());
    } else {
      gfxPrint(1, 55, "Mix:");
      gfxStartCursor(25, 55);
      graphics.printf("%3d%%", mix);
      gfxEndCursor(cursor == MIX);
      gfxStartCursor();
      gfxPrint(mix_cv);
      gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());
    }

    gfxDisplayInputMapEditor();
  }

  // --- Button / encoder --------------------------------------------------

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(PITCH_CV, pitch_cv),
          IndexedInput(TRIG_CV,  trig_cv),
          IndexedInput(DECAY_CV, decay_cv),
          IndexedInput(BRIGHT_CV, brightness_cv),
          IndexedInput(BODY_CV,  body_cv),
          IndexedInput(MIX_CV,   mix_cv)
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

    static constexpr int MAX_PITCH = 7 * 12 * 128;
    static constexpr int MIN_PITCH = -3 * 12 * 128;

    switch (cursor) {
      // Pitch: semitone steps (1 semitone = 128 units)
      case PITCH:
        pitch = constrain(pitch + direction * 128, MIN_PITCH, MAX_PITCH);
        break;
      // Fine pitch: ~3-cent steps (4 units ≈ 3 cents at A4)
      case PITCH_FINE:
        pitch = constrain(pitch + direction * 4, MIN_PITCH, MAX_PITCH);
        break;
      case PITCH_CV:
        pitch_cv.ChangeSource(direction);
        break;
      case TRIG_CV:
        trig_cv.ChangeSource(direction);
        break;
      case DECAY:
        decay = constrain(decay + direction, 0, 100);
        break;
      case DECAY_CV:
        decay_cv.ChangeSource(direction);
        break;
      case BRIGHT:
        brightness = constrain(brightness + direction, 0, 100);
        break;
      case BRIGHT_CV:
        brightness_cv.ChangeSource(direction);
        break;
      case BODY:
        body = constrain(body + direction, 0, 100);
        break;
      case BODY_CV:
        body_cv.ChangeSource(direction);
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

  // --- Serialisation ------------------------------------------------------

#define ADVKS_PARAMS pitch, decay, brightness, body, mix

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    uint16_t dummy = 0;
    data[0] = PackPackables(ADVKS_PARAMS);
    data[1] = PackPackables(pitch_cv, dummy, decay_cv, brightness_cv);
    data[2] = PackPackables(body_cv, mix_cv, trig_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    uint16_t dummy = 0;
    UnpackPackables(data[0], ADVKS_PARAMS);
    UnpackPackables(data[1], pitch_cv, dummy, decay_cv, brightness_cv);
    UnpackPackables(data[2], body_cv, mix_cv, trig_cv);
    if (dummy) trig_cv.Unpack(dummy);
    // Re-apply brightness so iir_alpha_ is recomputed on load
    synth.setBrightness(brightness * 0.01f);
  }

  // --- Audio routing ------------------------------------------------------

  AudioStream* InputStream()  override { return &input_stream;   }
  AudioStream* OutputStream() override { return &output_mixer;   }

protected:
  void SetHelp() override {}

private:
  // Cursor positions — order matches display top-to-bottom
  enum Cursor : int8_t {
    PITCH,       // semitone cursor  (note name indicator)
    PITCH_FINE,  // fine cursor      (Hz display)
    PITCH_CV,
    TRIG_CV,
    DECAY,
    DECAY_CV,
    BRIGHT,
    BRIGHT_CV,
    BODY,
    BODY_CV,
    MIX,
    MIX_CV,
  };

  int8_t cursor = PITCH;

  // Parameters (int8 0-100 for Decay/Brt/Bdy/Mix, int16 for Pitch)
  int16_t pitch      = 1 * 12 * 128;  // C4 default
  int8_t  decay      = 50;
  int8_t  brightness = 70;
  int8_t  body       = 30;
  int8_t  mix        = 100;             // 0 = fully transparent by default; turn up to add synthesis

  // Per-instance ADC lag counter (-1 = idle). Replaces StartADCLag/EndOfADCLag
  // to avoid sharing frame.adc_lag_countdown[io_offset] with other instances.
  int adc_lag_ = -1;

  // CV / trigger routing
  CVInputMap     pitch_cv;
  DigitalInputMap trig_cv;
  CVInputMap     decay_cv;
  CVInputMap     brightness_cv;
  CVInputMap     body_cv;
  CVInputMap     mix_cv;

  // Audio graph objects
  AudioPassthrough<MONO>        input_stream;   // dry bypass input
  AudioSynthAdvancedKarplus     synth;          // KS engine
  AudioMixer<2>                 output_mixer;   // dry/wet blend
};
