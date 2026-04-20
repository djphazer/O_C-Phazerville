#pragma once

#include "../Audio/AudioEffectModalResonator.h"

// ModalResonatorApplet — modal synthesis resonator bank
//
// Inspired by Mutable Instruments Rings / Elements.  Takes any input signal as
// an "exciter" (trigger burst, noise, or external audio) and passes it through
// a bank of 12 parallel two-pole resonators tuned to a modal spectrum.
//
// Parameters (each with a CV input):
//   Freq      — fundamental pitch of the lowest resonator (V/Oct pitch CV)
//   Structure — inharmonicity: 0 = harmonic series, 100 = bell/bar spectrum
//   Bright    — mode amplitude taper: 100 = all modes equal, 0 = only mode 0
//   Damp      — decay time: 0 = metallic click, 100 = long sustain
//   Pos       — excitation-point comb (Rings-style): 0=off, 50=odd harmonics
//   Mix       — wet/dry blend
//
// Signal flow (per channel):
//   input ──► resonator (wet) ──► wet_dry_mixer ──► output
//   input ──────────────────────► wet_dry_mixer ─┘
//
// AuxButton: fires a manual noise-burst strike (test without a patched exciter).

template <AudioChannels Channels>
class ModalResonatorApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override { return "ModalRes"; }

    // --- Lifecycle -----------------------------------------------------------

    void Start() override {
        for (int ch = 0; ch < Channels; ch++)
            channels[ch].Start(this, ch, input_stream, output_stream);
    }

    void Unload() override {
        for (auto& ch : channels) ch.Stop();
        AllowRestart();
    }

    // --- Controller (~150 Hz, ISR context) -----------------------------------

    void Controller() override {
        // Pitch: base (128 units = 1 semitone) + V/Oct CV
        float freq_hz = PitchToRatio(pitch + freq_cv.In()) * C3;
        if (freq_hz < 20.0f)   freq_hz = 20.0f;
        if (freq_hz > 4000.0f) freq_hz = 4000.0f;

        float eff_structure = constrain(0.01f * structure + struct_cv.InF(),  0.0f, 1.0f);
        float eff_bright    = constrain(0.01f * brightness + bright_cv.InF(), 0.0f, 1.0f);
        float eff_damp      = constrain(0.01f * damping    + damp_cv.InF(),   0.0f, 1.0f);
        float eff_pos       = constrain(0.01f * position   + pos_cv.InF(),    0.0f, 1.0f);
        float eff_mix       = constrain(0.01f * mix        + mix_cv.InF(),    0.0f, 1.0f);

        float dry_gain, wet_gain;
        EqualPowerFade(dry_gain, wet_gain, eff_mix);

        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].resonator.updateCoeffs(
                freq_hz, eff_structure, eff_bright, eff_damp, eff_pos);
            channels[ch].wet_dry_mixer.gain(ResonatorChannel::DRY_CH, dry_gain);
            channels[ch].wet_dry_mixer.gain(ResonatorChannel::WET_CH, wet_gain);

            if (manual_strike_) {
                channels[ch].resonator.strike(1.0f);
            }
        }
        manual_strike_ = false;
    }

    // --- View (64×64 px) -----------------------------------------------------

    FLASHMEM void View() override {
        // VU bar: excitation level at y=7, always visible
        uint16_t peak = channels[0].resonator.readPeak();
        if (peak > 0) {
            int bar_w = (int)((uint32_t)peak * 56 / 32767) + 1;
            gfxRect(1, 7, bar_w, 4);
        }

        // Row 1 (y=15): pitch note + Hz + freq CV
        gfxStartCursor(1, 15);
        gfxPrintTuningIndicator(pitch);
        gfxEndCursor(cursor == FREQ);

        gfxStartCursor(11, 15);
        gfxPrintPitchHz(pitch);
        gfxEndCursor(cursor == FREQ_FINE);

        gfxStartCursor();
        gfxPrint(freq_cv);
        gfxEndCursor(cursor == FREQ_CV, false, freq_cv.InputName());

        // Row 2 (y=25): Structure
        gfxPrint(1, 25, "Str:");
        gfxStartCursor(25, 25);
        graphics.printf("%3d", structure);
        gfxEndCursor(cursor == STRUCT);
        gfxStartCursor();
        gfxPrint(struct_cv);
        gfxEndCursor(cursor == STRUCT_CV, false, struct_cv.InputName());

        // Row 3 (y=35): Brightness
        gfxPrint(1, 35, "Brt:");
        gfxStartCursor(25, 35);
        graphics.printf("%3d", brightness);
        gfxEndCursor(cursor == BRIGHT);
        gfxStartCursor();
        gfxPrint(bright_cv);
        gfxEndCursor(cursor == BRIGHT_CV, false, bright_cv.InputName());

        // Row 4 (y=45): Damping
        gfxPrint(1, 45, "Dmp:");
        gfxStartCursor(25, 45);
        graphics.printf("%3d", damping);
        gfxEndCursor(cursor == DAMP);
        gfxStartCursor();
        gfxPrint(damp_cv);
        gfxEndCursor(cursor == DAMP_CV, false, damp_cv.InputName());

        // Row 5 (y=55): context-sensitive — Pos while cursor ≤ POS_CV, else Mix
        if (cursor <= POS_CV) {
            gfxPrint(1, 55, "Pos:");
            gfxStartCursor(25, 55);
            graphics.printf("%3d", position);
            gfxEndCursor(cursor == POS);
            gfxStartCursor();
            gfxPrint(pos_cv);
            gfxEndCursor(cursor == POS_CV, false, pos_cv.InputName());
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

    // --- AuxButton: manual test strike ---------------------------------------

    FLASHMEM void AuxButton() override {
        manual_strike_ = true;
        CancelEdit();
    }

    // --- Button / encoder ----------------------------------------------------

    FLASHMEM void OnButtonPress() override {
        if (CheckEditInputMapPress(
                cursor,
                IndexedInput(FREQ_CV,   freq_cv),
                IndexedInput(STRUCT_CV, struct_cv),
                IndexedInput(BRIGHT_CV, bright_cv),
                IndexedInput(DAMP_CV,   damp_cv),
                IndexedInput(POS_CV,    pos_cv),
                IndexedInput(MIX_CV,    mix_cv)
            ))
            return;
        CursorToggle();
    }

    FLASHMEM void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, MIX_CV);
            return;
        }
        if (EditSelectedInputMap(direction)) return;

        static constexpr int MAX_PITCH = 7 * 12 * 128;
        static constexpr int MIN_PITCH = -3 * 12 * 128;

        switch (cursor) {
            case FREQ:
                pitch = constrain(pitch + direction * 128, MIN_PITCH, MAX_PITCH);
                break;
            case FREQ_FINE:
                pitch = constrain(pitch + direction * 4, MIN_PITCH, MAX_PITCH);
                break;
            case FREQ_CV:    freq_cv.ChangeSource(direction);   break;
            case STRUCT:     structure  = constrain(structure  + direction, 0, 100); break;
            case STRUCT_CV:  struct_cv.ChangeSource(direction);  break;
            case BRIGHT:     brightness = constrain(brightness + direction, 0, 100); break;
            case BRIGHT_CV:  bright_cv.ChangeSource(direction);  break;
            case DAMP:       damping    = constrain(damping    + direction, 0, 100); break;
            case DAMP_CV:    damp_cv.ChangeSource(direction);    break;
            case POS:        position   = constrain(position   + direction, 0, 100); break;
            case POS_CV:     pos_cv.ChangeSource(direction);     break;
            case MIX:        mix        = constrain(mix        + direction, 0, 100); break;
            case MIX_CV:     mix_cv.ChangeSource(direction);     break;
            default: break;
        }
    }

    // --- Persistence ---------------------------------------------------------

#define MODAL_PARAMS pitch, structure, brightness, damping, position, mix
    FLASHMEM void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(MODAL_PARAMS);
        data[1] = PackPackables(freq_cv, struct_cv, bright_cv, damp_cv);
        data[2] = PackPackables(pos_cv, mix_cv);
    }

    FLASHMEM void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], MODAL_PARAMS);
        UnpackPackables(data[1], freq_cv, struct_cv, bright_cv, damp_cv);
        UnpackPackables(data[2], pos_cv, mix_cv);
    }
#undef MODAL_PARAMS

    // --- Audio routing -------------------------------------------------------

    AudioStream* InputStream()  override { return &input_stream;  }
    AudioStream* OutputStream() override { return &output_stream; }

protected:
    FLASHMEM void SetHelp() override {}

private:
    // Cursor order matches display top-to-bottom
    enum Cursor : int8_t {
        FREQ = 0,
        FREQ_FINE,
        FREQ_CV,
        STRUCT,
        STRUCT_CV,
        BRIGHT,
        BRIGHT_CV,
        DAMP,
        DAMP_CV,
        POS,
        POS_CV,
        MIX,
        MIX_CV,
    };

    int8_t cursor = FREQ;

    // Parameters
    int16_t pitch      = 1 * 12 * 128;  // C4 default
    int8_t  structure  = 0;             // 0=harmonic, 100=inharmonic
    int8_t  brightness = 70;            // mode amplitude taper
    int8_t  damping    = 50;            // decay time
    int8_t  position   = 25;            // excitation point
    int8_t  mix        = 100;           // wet/dry %

    CVInputMap freq_cv;
    CVInputMap struct_cv;
    CVInputMap bright_cv;
    CVInputMap damp_cv;
    CVInputMap pos_cv;
    CVInputMap mix_cv;

    bool manual_strike_ = false;

    // Per-channel DSP struct
    struct ResonatorChannel {
        static const uint8_t DRY_CH = 0;
        static const uint8_t WET_CH = 1;

        AudioEffectModalResonator resonator;
        AudioMixer<2>             wet_dry_mixer;

        void Start(HemisphereAudioApplet* owner, int ch,
                   AudioStream& input, AudioStream& output) {
            resonator.Acquire();
            owner->PatchCable(input,         ch, resonator,      0);
            owner->PatchCable(input,         ch, wet_dry_mixer,  DRY_CH);
            owner->PatchCable(resonator,      0, wet_dry_mixer,  WET_CH);
            owner->PatchCable(wet_dry_mixer,  0, output,         ch);
        }

        void Stop() { resonator.Release(); }
    } channels[Channels];

    AudioPassthrough<Channels> input_stream;
    AudioPassthrough<Channels> output_stream;
};
