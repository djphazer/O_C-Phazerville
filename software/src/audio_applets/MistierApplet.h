#pragma once

#include "../Audio/AudioEffectClouds.h"

extern "C" uint8_t external_psram_size;

// MistierApplet — Clouds-inspired live granular audio processor.
//
// Records audio into a 1-second PSRAM circular buffer and plays it back as a
// cloud of overlapping grains. Differences from MistApplet:
//   • Texture — continuous window morph: rect → triangle → Hann
//   • Density — centred at 0 (silence). CCW → regular periodic. CW → stochastic.
//   • Feedback (Fdb) — grain output fed back into the record buffer
//   • No fixed grain shapes — shape driven by Texture
//
// I/O:
//   Input → [AudioEffectClouds] → wet ──────────────────────────────┐
//   Input →                     → dry ─────────────────────────────┤ AudioMixer<2> → Output
//
// Freeze:
//   • AuxButton: latches/unlatches manual freeze (performance use, no cable needed)
//   • Frz input: assign a hardware gate via input map editor (cursor on Frz row → press button)
//   Both OR together. Frz row label inverts while latched.
//
// Parameters:
//   Page 1: Pos, Den, Sz, Spr, PSp
//   Page 2: Pitch, Fdb, Tex, Mix, Frz
//
template <AudioChannels Channels>
class MistierApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override { return "Misty"; }

    void Start() override {
        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].Start(this, ch, input_stream, output_stream);
        }
    }

    void Unload() override {
        for (auto& ch : channels) ch.Stop();
        AllowRestart();
    }

    void Controller() override {
        // ── CV-modulated effective parameters ──────────────────────────────────
        float eff_pos     = constrain(0.01f * pos     + pos_cv.InF(),              0.0f, 1.0f);
        // density UI 0–100: map to −20..+20 Hz (50=silence, >50=stochastic, <50=periodic)
        float eff_density = constrain(0.4f * (density - 50) + density_cv.InF() * 20.0f, -20.0f, 20.0f);
        float eff_size    = constrain(0.01f * size    + size_cv.InF() * 0.49f,     0.01f, 0.5f);
        float eff_texture = constrain(0.01f * texture + texture_cv.InF(),           0.0f, 1.0f);
        float eff_spray   = constrain(0.01f * spray   + spray_cv.InF(),             0.0f, 1.0f);

        // Pitch: semitones + V/Oct CV.
        float eff_semis = (float)pitch + (float)pitch_cv.In() / 128.0f;
        float eff_pitch = SemitonesToRatio(eff_semis);

        // Pitch spread: quadratic curve for fine control at low values.
        float eff_psprd_raw   = constrain(0.01f * psprd + psprd_cv.InF(), 0.0f, 1.0f);
        float eff_psprd_semis = 12.0f * eff_psprd_raw * eff_psprd_raw;

        // Feedback and wet/dry.
        float eff_feedback = constrain(0.01f * fdb + fdb_cv.InF(), 0.0f, 1.0f);
        float eff_mix      = constrain(0.01f * mix + mix_cv.InF(), 0.0f, 1.0f);

        // Freeze: hardware gate OR manual latch.
        bool frozen = freeze_input.Gate() || manual_freeze_;

        float dry_gain, wet_gain;
        EqualPowerFade(dry_gain, wet_gain, eff_mix);

        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].grain_stream.setPosition(eff_pos);
            channels[ch].grain_stream.setDensity(eff_density);
            channels[ch].grain_stream.setSize(eff_size);
            channels[ch].grain_stream.setSpray(eff_spray);
            channels[ch].grain_stream.setPitch(eff_pitch);
            channels[ch].grain_stream.setPitchSpread(eff_psprd_semis);
            channels[ch].grain_stream.setTexture(eff_texture);
            channels[ch].grain_stream.setFeedback(eff_feedback);
            channels[ch].grain_stream.setFreeze(frozen);
            channels[ch].mixer.gain(MistierChannel::DRY_CH, dry_gain);
            channels[ch].mixer.gain(MistierChannel::WET_CH, wet_gain);
        }
    }

    void View() override {
        if (!channels[0].grain_stream.IsReady()) {
            gfxPrint(1, 15, "No PSRAM");
            return;
        }

        // ── Grain activity bar (y=7) ──────────────────────────────────────────
        uint8_t active = channels[0].grain_stream.ActiveGrainCount();
        for (uint8_t i = 0; i < AudioEffectClouds::MAX_GRAINS; i++) {
            if (i < active) gfxPixel(1 + i, 7);
        }

        // ── Two pages ────────────────────────────────────────────────────────
        const bool pg2 = (cursor >= PITCH);

        if (!pg2) {
            // ── Page 1: Pos, Den, Sz, Spr, PSp (y=15/25/35/45/55) ──────────
            gfxPrint(1, 15, "Pos:");
            gfxStartCursor(); graphics.printf("%3d%%", pos); gfxEndCursor(cursor == POS);
            gfxStartCursor(); gfxPrint(pos_cv); gfxEndCursor(cursor == POS_CV, false, pos_cv.InputName());

            gfxPrint(1, 25, "Den:");
            gfxStartCursor();
            int8_t d_hz = (int8_t)(0.4f * (density - 50));
            if (d_hz >= 0) graphics.printf("+%2d", d_hz);
            else           graphics.printf("%3d", d_hz);
            gfxEndCursor(cursor == DENSITY);
            gfxStartCursor(); gfxPrint(density_cv); gfxEndCursor(cursor == DENSITY_CV, false, density_cv.InputName());

            gfxPrint(1, 35, "Sz:");
            gfxStartCursor(); graphics.printf("%3dms", size * 10); gfxEndCursor(cursor == SIZE);
            gfxStartCursor(); gfxPrint(size_cv); gfxEndCursor(cursor == SIZE_CV, false, size_cv.InputName());

            gfxPrint(1, 45, "Spr:");
            gfxStartCursor(); graphics.printf("%3d%%", spray); gfxEndCursor(cursor == SPRAY);
            gfxStartCursor(); gfxPrint(spray_cv); gfxEndCursor(cursor == SPRAY_CV, false, spray_cv.InputName());

            gfxPrint(1, 55, "PSp:");
            gfxStartCursor(); graphics.printf("%3d%%", psprd); gfxEndCursor(cursor == PSPRD);
            gfxStartCursor(); gfxPrint(psprd_cv); gfxEndCursor(cursor == PSPRD_CV, false, psprd_cv.InputName());
        } else {
            // ── Page 2: Pitch, Fdb, Tex, Mix, Frz (y=15/25/35/45/55) ────────

            // Pitch
            gfxPrint(1, 15, "Pt:");
            gfxStartCursor();
            if (pitch >= 0) graphics.printf("+%2d", pitch);
            else            graphics.printf("%3d", pitch);
            gfxEndCursor(cursor == PITCH);
            gfxStartCursor(); gfxPrint(pitch_cv); gfxEndCursor(cursor == PITCH_CV, false, pitch_cv.InputName());

            // Feedback
            gfxPrint(1, 25, "Fdb:");
            gfxStartCursor(); graphics.printf("%3d%%", fdb); gfxEndCursor(cursor == FDB);
            gfxStartCursor(); gfxPrint(fdb_cv); gfxEndCursor(cursor == FDB_CV, false, fdb_cv.InputName());

            // Texture
            gfxPrint(1, 35, "Tex:");
            gfxStartCursor(); graphics.printf("%3d%%", texture); gfxEndCursor(cursor == TEXTURE);
            gfxStartCursor(); gfxPrint(texture_cv); gfxEndCursor(cursor == TEXTURE_CV, false, texture_cv.InputName());

            // Mix
            gfxPrint(1, 45, "Mix:");
            gfxStartCursor(); graphics.printf("%3d%%", mix); gfxEndCursor(cursor == MIX);
            gfxStartCursor(); gfxPrint(mix_cv); gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

            // Freeze — label inverts while manual latch is active (print first, then invert)
            gfxPrint(1, 55, "Frz:");
            if (manual_freeze_) gfxInvert(1, 55, 20, 8);
            gfxStartCursor(); gfxPrint(freeze_input); gfxEndCursor(cursor == FREEZE, true, freeze_input.InputName());
        }

        // Page indicator
        gfxPrint(58, 56, pg2 ? "<" : ">");

        gfxDisplayInputMapEditor();
    }

    // AuxButton: latch/unlatch manual freeze (live performance, no cable needed).
    void AuxButton() override {
        manual_freeze_ ^= 1;
        CancelEdit();
    }

    void OnButtonPress() override {
        if (CheckEditInputMapPress(
                cursor,
                IndexedInput(POS_CV,      pos_cv),
                IndexedInput(DENSITY_CV,  density_cv),
                IndexedInput(SIZE_CV,     size_cv),
                IndexedInput(SPRAY_CV,    spray_cv),
                IndexedInput(PSPRD_CV,    psprd_cv),
                IndexedInput(PITCH_CV,    pitch_cv),
                IndexedInput(FDB_CV,      fdb_cv),
                IndexedInput(TEXTURE_CV,  texture_cv),
                IndexedInput(MIX_CV,      mix_cv),
                IndexedInput(FREEZE,      freeze_input)
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
            case POS:        pos     = constrain(pos     + direction,   0, 100); break;
            case POS_CV:     pos_cv.ChangeSource(direction);                      break;
            case DENSITY:    density = constrain(density + direction,   0, 100); break;
            case DENSITY_CV: density_cv.ChangeSource(direction);                  break;
            case SIZE:       size    = constrain(size    + direction,   1,  50); break;
            case SIZE_CV:    size_cv.ChangeSource(direction);                     break;
            case SPRAY:      spray   = constrain(spray   + direction,   0, 100); break;
            case SPRAY_CV:   spray_cv.ChangeSource(direction);                    break;
            case PSPRD:      psprd   = constrain(psprd   + direction,   0, 100); break;
            case PSPRD_CV:   psprd_cv.ChangeSource(direction);                    break;
            case PITCH:      pitch   = constrain(pitch   + direction, -12,  12); break;
            case PITCH_CV:   pitch_cv.ChangeSource(direction);                    break;
            case FDB:        fdb     = constrain(fdb     + direction,   0, 100); break;
            case FDB_CV:     fdb_cv.ChangeSource(direction);                      break;
            case TEXTURE:    texture = constrain(texture + direction,   0, 100); break;
            case TEXTURE_CV: texture_cv.ChangeSource(direction);                  break;
            case MIX:        mix     = constrain(mix     + direction,   0, 100); break;
            case MIX_CV:     mix_cv.ChangeSource(direction);                      break;
            case FREEZE:     freeze_input.ChangeSource(direction);                break;
            default: break;
        }
    }

#define MISTIER_PARAMS  pos, density, size, texture, pitch, psprd, fdb, mix
    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(MISTIER_PARAMS);
        data[1] = PackPackables(pos_cv, density_cv, size_cv, spray_cv);
        data[2] = PackPackables(pitch_cv, fdb_cv, texture_cv, mix_cv);
        data[3] = PackPackables(freeze_input, spray, psprd_cv);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], MISTIER_PARAMS);
        UnpackPackables(data[1], pos_cv, density_cv, size_cv, spray_cv);
        UnpackPackables(data[2], pitch_cv, fdb_cv, texture_cv, mix_cv);
        UnpackPackables(data[3], freeze_input, spray, psprd_cv);
    }
#undef MISTIER_PARAMS

    AudioStream* InputStream()  override { return &input_stream; }
    AudioStream* OutputStream() override { return &output_stream; }

protected:
    void SetHelp() override {}

private:
    enum Cursor : int8_t {
        // Page 1
        POS = 0, POS_CV,
        DENSITY, DENSITY_CV,
        SIZE, SIZE_CV,
        SPRAY, SPRAY_CV,
        PSPRD, PSPRD_CV,
        // Page 2
        PITCH, PITCH_CV,
        FDB, FDB_CV,
        TEXTURE, TEXTURE_CV,
        MIX, MIX_CV,
        FREEZE,
        CURSOR_LENGTH,
    };

    int8_t cursor = POS;

    // Parameters
    int8_t  pos      = 50;  // 0–100%
    CVInputMap pos_cv;
    int8_t  density  = 75;  // 0–100 (50=silence; 0.4*(val-50) Hz, >50=stochastic)
    CVInputMap density_cv;
    int8_t  size     = 15;  // 1–50 (×10ms = 10–500ms)
    CVInputMap size_cv;
    int8_t  spray    = 20;  // 0–100% position scatter
    CVInputMap spray_cv;
    int8_t  psprd    = 0;   // 0–100% pitch spread
    CVInputMap psprd_cv;
    int8_t  pitch    = 0;   // −12 to +12 semitones
    CVInputMap pitch_cv;
    int8_t  fdb      = 0;   // 0–100% grain feedback
    CVInputMap fdb_cv;
    int8_t  texture  = 50;  // 0–100% (0=rect, 50=tri, 100=Hann)
    CVInputMap texture_cv;
    int8_t  mix      = 80;  // 0–100% wet/dry
    CVInputMap mix_cv;
    DigitalInputMap freeze_input;

    bool manual_freeze_ = false;  // latched by AuxButton

    // Per-channel DSP struct.
    struct MistierChannel {
        static const uint8_t DRY_CH = 0;
        static const uint8_t WET_CH = 1;

        AudioEffectClouds grain_stream;
        AudioMixer<2>     mixer;

        MistierChannel()
            : grain_stream(
                external_psram_size
                    ? AudioEffectClouds::CLOUDS_BUFFER_SAMPLES
                    : AudioEffectClouds::CLOUDS_BUFFER_SAMPLES / 2)
        {}

        void Start(HemisphereAudioApplet* owner, int ch,
                   AudioStream& input, AudioStream& output) {
            grain_stream.Acquire();
            owner->PatchCable(input,       ch, grain_stream, 0);
            owner->PatchCable(input,       ch, mixer,        DRY_CH);
            owner->PatchCable(grain_stream, 0, mixer,        WET_CH);
            owner->PatchCable(mixer,        0, output,       ch);
        }

        void Stop() { grain_stream.Release(); }
    } channels[Channels];

    AudioPassthrough<Channels> input_stream;
    AudioPassthrough<Channels> output_stream;
};
