#pragma once

#include "../Audio/AudioEffectMist.h"

extern "C" uint8_t external_psram_size;

// MistApplet — live granular audio processor.
//
// Continuously records audio into a 1-second PSRAM circular buffer and generates
// overlapping grains (up to 16 per channel). Each grain is a windowed slice of
// the buffer played back at a configurable pitch. Wet/dry mix blends the granular
// cloud with the live (always-recording) input.
//
// I/O:
//   Input → [AudioEffectMist] → wet  ─┐
//   Input →                    → dry    ─┴→ AudioMixer<2> → Output
//
// Parameters (two pages, 4 per page):
//   Page 1: POS, DENSITY, SIZE, SPRAY
//   Page 2: PITCH, PSPRD, FREEZE, MIX
//
template <AudioChannels Channels>
class MistApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override { return "Mist"; }

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
        // CV-modulated effective parameter values.
        float eff_pos     = constrain(0.01f * pos     + pos_cv.InF(),             0.0f, 1.0f);
        float eff_density = constrain((float)density  + density_cv.InF() * 49.0f, 1.0f, 50.0f);
        float eff_size    = constrain(0.01f * size     + size_cv.InF()  * 0.49f,   0.01f, 0.5f);
        float eff_spray   = constrain(0.01f * spray    + spray_cv.InF(),           0.0f, 1.0f);

        // Pitch: base semitones + V/Oct CV (128 units = 1 semitone, same as PitchToRatio scale).
        // No range constraint — CV can push well beyond the ±12 st knob range.
        float eff_semis = (float)pitch + (float)pitch_cv.In() / 128.0f;
        float eff_pitch = SemitonesToRatio(eff_semis);

        // Pitch spread: 0–100% → 0–12 semitones via quadratic curve.
        float eff_psprd_raw   = constrain(0.01f * psprd + psprd_cv.InF(), 0.0f, 1.0f);
        float eff_psprd_semis = 12.0f * eff_psprd_raw * eff_psprd_raw;

        // Freeze: hardware gate OR latched manual freeze.
        bool frozen = freeze_input.Gate() || manual_freeze_;

        // Wet/dry gains.
        float eff_mix = constrain(0.01f * mix + mix_cv.InF(), 0.0f, 1.0f);
        float dry_gain, wet_gain;
        EqualPowerFade(dry_gain, wet_gain, eff_mix);

        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].grain_stream.setPosition(eff_pos);
            channels[ch].grain_stream.setDensity(eff_density);
            channels[ch].grain_stream.setSize(eff_size);
            channels[ch].grain_stream.setSpray(eff_spray);
            channels[ch].grain_stream.setPitch(eff_pitch);
            channels[ch].grain_stream.setPitchSpread(eff_psprd_semis);
            channels[ch].grain_stream.setFreeze(frozen);
            channels[ch].grain_stream.setShape((AudioEffectMist::GrainShape)shape);
            channels[ch].wet_dry_mixer.gain(MistChannel::DRY_CH, dry_gain);
            channels[ch].wet_dry_mixer.gain(MistChannel::WET_CH, wet_gain);
        }
    }

    void View() override {
        if (!channels[0].grain_stream.IsReady()) {
            gfxPrint(1, 15, "No PSRAM");
            return;
        }

        // ── Grain activity bar (y=7) ──────────────────────────────────────
        uint8_t active = channels[0].grain_stream.ActiveGrainCount();
        for (uint8_t i = 0; i < AudioEffectMist::MAX_GRAINS; i++) {
            if (i < active) gfxPixel(1 + i, 7);
        }

        // ── Two pages, 4 params each (y=15,25,35,45; 10px per row) ────────
        // Page 1 (cursor < PITCH): Pos, Den, Sz, Spr
        // Page 2 (cursor >= PITCH): Pt, PSp, Frz, Mix
        const bool pg2 = (cursor >= PITCH);

        if (!pg2) {
            gfxPrint(1, 15, "Pos:");
            gfxStartCursor(); graphics.printf("%3d%%", pos); gfxEndCursor(cursor == POS);
            gfxStartCursor(); gfxPrint(pos_cv); gfxEndCursor(cursor == POS_CV, false, pos_cv.InputName());

            gfxPrint(1, 25, "Den:");
            gfxStartCursor(); graphics.printf("%2dHz", density); gfxEndCursor(cursor == DENSITY);
            gfxStartCursor(); gfxPrint(density_cv); gfxEndCursor(cursor == DENSITY_CV, false, density_cv.InputName());

            gfxPrint(1, 35, "Sz:");
            gfxStartCursor(); graphics.printf("%3dms", size * 10); gfxEndCursor(cursor == SIZE);
            gfxStartCursor(); gfxPrint(size_cv); gfxEndCursor(cursor == SIZE_CV, false, size_cv.InputName());

            gfxPrint(1, 45, "Spr:");
            gfxStartCursor(); graphics.printf("%3d%%", spray); gfxEndCursor(cursor == SPRAY);
            gfxStartCursor(); gfxPrint(spray_cv); gfxEndCursor(cursor == SPRAY_CV, false, spray_cv.InputName());
        } else {
            gfxPrint(1, 15, "Pt:");
            gfxStartCursor();
            if (pitch >= 0) graphics.printf("+%2d", pitch);
            else            graphics.printf("%3d", pitch);
            gfxEndCursor(cursor == PITCH);
            gfxStartCursor(); gfxPrint(pitch_cv); gfxEndCursor(cursor == PITCH_CV, false, pitch_cv.InputName());

            gfxPrint(1, 25, "PSp:");
            gfxStartCursor(); graphics.printf("%3d%%", psprd); gfxEndCursor(cursor == PSPRD);
            gfxStartCursor(); gfxPrint(psprd_cv); gfxEndCursor(cursor == PSPRD_CV, false, psprd_cv.InputName());

            gfxPrint(1, 35, "Frz:");
            if (manual_freeze_) gfxInvert(1, 35, 24, 8);
            gfxStartCursor(); gfxPrint(freeze_input); gfxEndCursor(cursor == FREEZE, true, freeze_input.InputName());

            gfxPrint(1, 45, "Mix:");
            gfxStartCursor(); graphics.printf("%3d%%", mix); gfxEndCursor(cursor == MIX);
            gfxStartCursor(); gfxPrint(mix_cv); gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

            static const char* SHAPE_NAMES[] = { "Sine", "Tri ", "R-Up", "R-Dn" };
            gfxPrint(1, 55, "Shp:");
            gfxStartCursor(); gfxPrint(SHAPE_NAMES[shape]); gfxEndCursor(cursor == SHAPE);
        }

        // Page indicator — ">" (more ahead) on page 1, "<" (go back) on page 2
        gfxPrint(58, 56, pg2 ? "<" : ">");

        gfxDisplayInputMapEditor();
    }

    // AuxButton latches manual freeze for performance use without a patch cable.
    void AuxButton() override {
        manual_freeze_ ^= 1;
        CancelEdit();
    }

    void OnButtonPress() override {
        if (CheckEditInputMapPress(
                cursor,
                IndexedInput(POS_CV,     pos_cv),
                IndexedInput(DENSITY_CV, density_cv),
                IndexedInput(SIZE_CV,    size_cv),
                IndexedInput(SPRAY_CV,   spray_cv),
                IndexedInput(PITCH_CV,   pitch_cv),
                IndexedInput(PSPRD_CV,   psprd_cv),
                IndexedInput(FREEZE,     freeze_input),
                IndexedInput(MIX_CV,     mix_cv)
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
            case POS:        pos     = constrain(pos     + direction, 0, 100);  break;
            case POS_CV:     pos_cv.ChangeSource(direction);                     break;
            case DENSITY:    density = constrain(density + direction, 1, 50);   break;
            case DENSITY_CV: density_cv.ChangeSource(direction);                 break;
            case SIZE:       size    = constrain(size    + direction, 1, 50);   break;
            case SIZE_CV:    size_cv.ChangeSource(direction);                    break;
            case SPRAY:      spray   = constrain(spray   + direction, 0, 100);  break;
            case SPRAY_CV:   spray_cv.ChangeSource(direction);                   break;
            case PITCH:      pitch   = constrain(pitch   + direction, -12, 12); break;
            case PITCH_CV:   pitch_cv.ChangeSource(direction);                   break;
            case PSPRD:      psprd   = constrain(psprd   + direction, 0, 100);  break;
            case PSPRD_CV:   psprd_cv.ChangeSource(direction);                   break;
            case FREEZE:     freeze_input.ChangeSource(direction);               break;
            case MIX:        mix     = constrain(mix     + direction, 0, 100);  break;
            case MIX_CV:     mix_cv.ChangeSource(direction);                     break;
            case SHAPE:      shape   = constrain(shape   + direction, 0, 3);    break;
            default: break;
        }
    }

#define MIST_PARAMS  pos, density, size, spray, pitch, psprd, mix, shape
    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(MIST_PARAMS);
        data[1] = PackPackables(pos_cv, density_cv, size_cv);
        data[2] = PackPackables(spray_cv, pitch_cv, psprd_cv, mix_cv);
        data[3] = PackPackables(freeze_input);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], MIST_PARAMS);
        UnpackPackables(data[1], pos_cv, density_cv, size_cv);
        UnpackPackables(data[2], spray_cv, pitch_cv, psprd_cv, mix_cv);
        UnpackPackables(data[3], freeze_input);
    }
#undef MIST_PARAMS

    AudioStream* InputStream()  override { return &input_stream; }
    AudioStream* OutputStream() override { return &output_stream; }

protected:
    void SetHelp() override {}

private:
    enum Cursor : int8_t {
        POS = 0,
        POS_CV,
        DENSITY,
        DENSITY_CV,
        SIZE,
        SIZE_CV,
        SPRAY,
        SPRAY_CV,
        PITCH,
        PITCH_CV,
        PSPRD,
        PSPRD_CV,
        FREEZE,
        MIX,
        MIX_CV,
        SHAPE,
        CURSOR_LENGTH,
    };

    int8_t cursor = POS;

    // Parameters
    int8_t  pos     = 50;  // 0–100% (buffer depth)
    CVInputMap pos_cv;
    int8_t  density = 8;   // 1–50 Hz
    CVInputMap density_cv;
    int8_t  size    = 10;  // 1–50 (×10ms = 10–500ms)
    CVInputMap size_cv;
    int8_t  spray   = 20;  // 0–100% scatter
    CVInputMap spray_cv;
    int8_t  pitch   = 0;   // −12 to +12 semitones
    CVInputMap pitch_cv;
    int8_t  psprd   = 0;   // 0–100% pitch spread (0=none, 100=±1oct)
    CVInputMap psprd_cv;
    DigitalInputMap freeze_input;
    int8_t  mix     = 80;  // 0–100% wet
    CVInputMap mix_cv;
    int8_t  shape   = 1;   // 0=Sine, 1=Triangle, 2=Ramp-Up, 3=Ramp-Down

    bool manual_freeze_ = false;

    // Per-channel DSP struct (mirrors GlitchChannel pattern).
    struct MistChannel {
        static const uint8_t DRY_CH = 0;
        static const uint8_t WET_CH = 1;

        AudioEffectMist grain_stream;
        AudioMixer<2>       wet_dry_mixer;

        MistChannel()
            : grain_stream(
                external_psram_size
                    ? AudioEffectMist::MIST_BUFFER_SAMPLES
                    : AudioEffectMist::MIST_BUFFER_SAMPLES / 2)
        {}

        void Start(HemisphereAudioApplet* owner, int ch,
                   AudioStream& input, AudioStream& output) {
            grain_stream.Acquire();
            owner->PatchCable(input,        ch,  grain_stream,   0);
            owner->PatchCable(input,        ch,  wet_dry_mixer,  DRY_CH);
            owner->PatchCable(grain_stream, 0,   wet_dry_mixer,  WET_CH);
            owner->PatchCable(wet_dry_mixer, 0,  output,         ch);
        }

        void Stop() { grain_stream.Release(); }
    } channels[Channels];

    AudioPassthrough<Channels> input_stream;
    AudioPassthrough<Channels> output_stream;
};
