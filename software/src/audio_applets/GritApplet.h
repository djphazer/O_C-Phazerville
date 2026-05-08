#pragma once

#include "../Audio/AudioEffectGrit.h"

// GritApplet — 4-mode distortion / lo-fi processor.
//
// Modes (encoder-selected, no CV):
//   CLIP  — hard clipping
//   SAT   — soft saturation (rational approx)
//   CRUSH — bit-depth reduction
//   DECI  — sample-rate decimation
//
// Parameters (each with assignable CV input):
//   Drv  — input gain before distortion
//   Amt  — effect depth (threshold / knee / bit depth / hold)
//   Tone — post-distortion 1-pole lowpass (0%=dark, 100%=open)
//   Mix  — wet/dry blend (equal-power)
//
// Signal flow (per channel):
//   input ──► AudioEffectGrit ──► output
//   (wet/dry mix is handled inside AudioEffectGrit)
//
// AuxButton: cycle mode (same as encoder on Mode row).

template <AudioChannels Channels>
class GritApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() { return "Grit"; }

    void Start() override {
        for (int ch = 0; ch < Channels; ch++) {
            grit[ch].Acquire();
            PatchCable(input_stream,  ch, grit[ch], 0);
            PatchCable(grit[ch],       0, output_stream, ch);
        }
    }

    void Unload() override {
        for (int ch = 0; ch < Channels; ch++) grit[ch].Release();
        AllowRestart();
    }

    void Controller() override {
        float eff_drive = 1.0f + constrain(0.01f * drive + drv_cv.InF(), 0.0f, 1.0f) * 9.0f; // 1–10×
        float eff_amt   = constrain(0.01f * amt  + amt_cv.InF(),  0.0f, 1.0f);
        float eff_tone  = constrain(0.01f * tone + tone_cv.InF(), 0.0f, 1.0f);
        float eff_mix   = constrain(0.01f * mix  + mix_cv.InF(),  0.0f, 1.0f);

        // Tone: 0%→~800 Hz, 100%→~20 kHz (log curve via powf at control rate)
        float tone_hz    = 800.0f * powf(25.0f, eff_tone);
        float tone_coeff = 1.0f - expf(-2.0f * M_PI * tone_hz / AUDIO_SAMPLE_RATE_EXACT);

        float wet_gain, dry_gain;
        EqualPowerFade(dry_gain, wet_gain, eff_mix);

        for (int ch = 0; ch < Channels; ch++) {
            grit[ch].setMode(mode_);
            grit[ch].setDrive(eff_drive);
            grit[ch].setAmt(eff_amt);
            grit[ch].setToneCoeff(tone_coeff);
            grit[ch].setMix(wet_gain, dry_gain);
        }
    }

    void View() override {
        static const char* const MODE_NAMES[] = { "CLIP", "SAT ", "CRUS", "DECI" };

        // Row 1 (y=15): Mode
        gfxPrint(1, 15, "Mode:");
        gfxStartCursor(31, 15);
        gfxPrint(MODE_NAMES[mode_]);
        gfxEndCursor(cursor == MODE);

        // Row 2 (y=25): Drive
        gfxPrint(1, 25, "Drv:");
        gfxStartCursor(25, 25);
        graphics.printf("%3d%%", drive);
        gfxEndCursor(cursor == DRIVE);
        gfxStartCursor();
        gfxPrint(drv_cv);
        gfxEndCursor(cursor == DRIVE_CV, false, drv_cv.InputName());

        // Row 3 (y=35): Amount (label changes per mode)
        static const char* const AMT_LABELS[] = { "Thr:", "Kne:", "Bit:", "Dec:" };
        gfxPrint(1, 35, AMT_LABELS[mode_]);
        gfxStartCursor(25, 35);
        graphics.printf("%3d%%", amt);
        gfxEndCursor(cursor == AMT);
        gfxStartCursor();
        gfxPrint(amt_cv);
        gfxEndCursor(cursor == AMT_CV, false, amt_cv.InputName());

        // Row 4 (y=45): Tone
        gfxPrint(1, 45, "Ton:");
        gfxStartCursor(25, 45);
        graphics.printf("%3d%%", tone);
        gfxEndCursor(cursor == TONE);
        gfxStartCursor();
        gfxPrint(tone_cv);
        gfxEndCursor(cursor == TONE_CV, false, tone_cv.InputName());

        // Row 5 (y=55): Mix
        gfxPrint(1, 55, "Mix:");
        gfxStartCursor(25, 55);
        graphics.printf("%3d%%", mix);
        gfxEndCursor(cursor == MIX);
        gfxStartCursor();
        gfxPrint(mix_cv);
        gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

        gfxDisplayInputMapEditor();
    }

    // AuxButton: cycle mode
    void AuxButton() override {
        mode_ = (mode_ + 1) % 4;
        CancelEdit();
    }

    void OnButtonPress() override {
        if (CheckEditInputMapPress(
                cursor,
                IndexedInput(DRIVE_CV, drv_cv),
                IndexedInput(AMT_CV,   amt_cv),
                IndexedInput(TONE_CV,  tone_cv),
                IndexedInput(MIX_CV,   mix_cv)
            ))
            return;
        CursorToggle();
    }

    void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
            return;
        }
        if (EditSelectedInputMap(direction)) return;

        switch (cursor) {
            case MODE:     mode_  = (mode_ + 4 + direction) % 4; break;
            case DRIVE:    drive  = constrain(drive  + direction, 0, 100); break;
            case DRIVE_CV: drv_cv.ChangeSource(direction);                  break;
            case AMT:      amt    = constrain(amt    + direction, 0, 100); break;
            case AMT_CV:   amt_cv.ChangeSource(direction);                  break;
            case TONE:     tone   = constrain(tone   + direction, 0, 100); break;
            case TONE_CV:  tone_cv.ChangeSource(direction);                 break;
            case MIX:      mix    = constrain(mix    + direction, 0, 100); break;
            case MIX_CV:   mix_cv.ChangeSource(direction);                  break;
            default: break;
        }
    }

#define GRIT_PARAMS mode_, drive, amt, tone, mix
    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(GRIT_PARAMS);
        data[1] = PackPackables(drv_cv, amt_cv, tone_cv, mix_cv);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], GRIT_PARAMS);
        UnpackPackables(data[1], drv_cv, amt_cv, tone_cv, mix_cv);
        mode_ = constrain(mode_, 0, 3);
    }
#undef GRIT_PARAMS

    AudioStream* InputStream()  override { return &input_stream; }
    AudioStream* OutputStream() override { return &output_stream; }

protected:
    void SetHelp() override {}

private:
    enum Cursor : int8_t {
        MODE = 0,
        DRIVE, DRIVE_CV,
        AMT,   AMT_CV,
        TONE,  TONE_CV,
        MIX,   MIX_CV,
        CURSOR_LENGTH,
    };

    int8_t cursor = MODE;

    // Parameters
    int8_t    mode_  = AudioEffectGrit::MODE_SAT;  // default: soft saturation
    int8_t    drive  = 20;   // 0–100% → 1–10× gain
    CVInputMap drv_cv;
    int8_t    amt    = 50;   // 0–100%
    CVInputMap amt_cv;
    int8_t    tone   = 80;   // 0–100% → ~800 Hz–20 kHz
    CVInputMap tone_cv;
    int8_t    mix    = 100;  // 0–100% wet/dry
    CVInputMap mix_cv;

    AudioEffectGrit          grit[Channels];
    AudioPassthrough<Channels> input_stream;
    AudioPassthrough<Channels> output_stream;
};
