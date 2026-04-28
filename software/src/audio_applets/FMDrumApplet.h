#pragma once

#include "synth_waveform.h"
#include "synth_whitenoise.h"
#include "../src/Audio/filter_variable2.h"

class FMDrumApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override { return "FMDrum"; }

    AudioStream* InputStream()  override { return &input_stream; }
    AudioStream* OutputStream() override { return &output_mixer; }

    FLASHMEM void Start() override {
        // Acquire interpolating streams
        fm_idx_stream.Acquire();
        fm_idx_stream.Method(INTERPOLATION_LINEAR);
        amp_env_stream.Acquire();
        amp_env_stream.Method(INTERPOLATION_LINEAR);
        noise_env_stream.Acquire();
        noise_env_stream.Method(INTERPOLATION_LINEAR);

        // Modulator: simple sine, phase-resettable
        modulator.begin(WAVEFORM_SINE);
        modulator.amplitude(1.0f);

        // Carrier: PM synthesis, max depth 1800 degrees = 5π rad
        carrier.begin(WAVEFORM_SINE);
        carrier.amplitude(1.0f);
        carrier.phaseModulation(1800.0f);

        // mod_vca: scales modulator by FM index envelope
        mod_vca.bias(0.0f);
        mod_vca.level(1.0f);
        mod_vca.rectify(true);

        // amp_vca: scales carrier by amplitude envelope
        amp_vca.bias(0.0f);
        amp_vca.level(1.0f);
        amp_vca.rectify(true);

        // Noise: white → fixed ~1kHz HPF → VCA
        noise_gen.amplitude(1.0f);
        noise_hpf.frequency(1000.0f);
        noise_hpf.resonance(0.707f);
        noise_hpf.octaveControl(0.0f);

        noise_vca.bias(0.0f);
        noise_vca.level(1.0f);
        noise_vca.rectify(true);

        // Output mixer: gain[0] and gain[2] fixed, gain[1] updated in Controller
        output_mixer.gain(0, 1.0f);
        output_mixer.gain(1, 0.0f);
        output_mixer.gain(2, 1.0f);

        // --- Cable routing ---
        // FM chain
        PatchCable(modulator,       0, mod_vca,      0);
        PatchCable(fm_idx_stream,   0, mod_vca,      1);
        PatchCable(mod_vca,         0, carrier,      0);  // PM mod input

        // Carrier → amp envelope → mixer ch0
        PatchCable(carrier,         0, amp_vca,      0);
        PatchCable(amp_env_stream,  0, amp_vca,      1);
        PatchCable(amp_vca,         0, output_mixer, 0);

        // Noise chain: noise → HPF → noise VCA → mixer ch1
        PatchCable(noise_gen,       0, noise_hpf,    0);
        PatchCable(noise_hpf,       2, noise_vca,    0);  // output 2 = HP
        PatchCable(noise_env_stream,0, noise_vca,    1);
        PatchCable(noise_vca,       0, output_mixer, 1);

        // Passthrough: audio in → mixer ch2 (level via mixer gain)
        PatchCable(input_stream,    0, output_mixer, 2);

        // Prime streams to silence
        fm_idx_stream.Push(float_to_q15(0.0f));
        amp_env_stream.Push(float_to_q15(0.0f));
        noise_env_stream.Push(float_to_q15(0.0f));
    }

    FLASHMEM void Unload() override {
        fm_idx_stream.Release();
        amp_env_stream.Release();
        noise_env_stream.Release();
        AllowRestart();
    }

    void Controller() override {
        // --- CV application (bipolar via Proportion) ---
        float eff_pitch = constrain(
            (float)pitch_hz + Proportion(pitch_cv.In(), HEMISPHERE_MAX_INPUT_CV, 1000),
            10.f, 8000.f);
        float eff_dec = constrain(
            (float)dec + Proportion(dec_cv.In(), HEMISPHERE_MAX_INPUT_CV, 1000),
            1.f, 5000.f);
        float eff_swp = constrain(
            (float)swp + Proportion(swp_cv.In(), HEMISPHERE_MAX_INPUT_CV, 100),
            0.f, 200.f);
        float eff_rto = constrain(
            (float)rto + Proportion(rto_cv.In(), HEMISPHERE_MAX_INPUT_CV, 50),
            0.1f, 200.f);
        float eff_fmi = constrain(
            (float)fmi + Proportion(fmi_cv.In(), HEMISPHERE_MAX_INPUT_CV, 100),
            0.f, 200.f);
        float eff_fmd = constrain(
            (float)fmd + Proportion(fmd_cv.In(), HEMISPHERE_MAX_INPUT_CV, 500),
            1.f, 5000.f);
        float eff_noi = constrain(
            (float)noi + Proportion(noi_cv.In(), HEMISPHERE_MAX_INPUT_CV, 100),
            0.f, 200.f);
        float eff_ndc = constrain(
            (float)ndc + Proportion(ndc_cv.In(), HEMISPHERE_MAX_INPUT_CV, 500),
            1.f, 5000.f);
        float eff_mix = constrain(
            (float)mix + Proportion(mix_cv.In(), HEMISPHERE_MAX_INPUT_CV, 100),
            0.f, 100.f);

        // --- Per-block decay coefficients (recalculate only when effective value changes) ---
        // coeff = exp(-AUDIO_BLOCK_SAMPLES / (decay_s * AUDIO_SAMPLE_RATE_EXACT))
        // Uses fastexp (bit-manipulation) instead of stdlib expf to avoid math.h linkage.
        static const float kExpFactor = 128000.f / AUDIO_SAMPLE_RATE_EXACT;
        if (eff_dec != prev_eff_dec) { amp_coeff   = fastexp(-kExpFactor / eff_dec); prev_eff_dec = eff_dec; }
        if (eff_fmd != prev_eff_fmd) { fm_coeff    = fastexp(-kExpFactor / eff_fmd); prev_eff_fmd = eff_fmd; }
        if (eff_ndc != prev_eff_ndc) { noise_coeff = fastexp(-kExpFactor / eff_ndc); prev_eff_ndc = eff_ndc; }

        // --- Trigger detection ---
        if (trg.Clock()) {
            amp_env   = 1.0f;
            fm_env    = 1.0f;
            noise_env = 1.0f;
            modulator.phase(0.0f);  // phase reset for consistent transient
            trigger_flash = 8;
        }

        // --- Advance envelopes ---
        amp_env   *= amp_coeff;
        fm_env    *= fm_coeff;
        noise_env *= noise_coeff;
        if (trigger_flash > 0) --trigger_flash;

        // --- Oscillator frequencies ---
        // Pitch sweep: exponential ratio sweep — (1 + swp/100)^2 at peak, decays to pitch
        float sweep_ratio = 1.f + eff_swp * 0.01f;
        float carrier_hz  = eff_pitch * (1.f + (sweep_ratio * sweep_ratio - 1.f) * amp_env);
        carrier.frequency(constrain(carrier_hz, 1.f, 20000.f));
        modulator.frequency(constrain(carrier_hz * eff_rto * 0.1f, 1.f, 20000.f));

        // --- Push envelope values to audio streams ---
        fm_idx_stream.Push(
            float_to_q15(constrain(fm_env * eff_fmi * 0.01f, 0.f, 1.f)));
        amp_env_stream.Push(
            float_to_q15(constrain(amp_env, 0.f, 1.f)));
        noise_env_stream.Push(
            float_to_q15(constrain(noise_env, 0.f, 1.f)));

        // --- Mixer gains: drum scales with mix, passthrough inversely ---
        float mix_gain = eff_mix * 0.01f;
        output_mixer.gain(0, mix_gain);
        output_mixer.gain(1, constrain(eff_noi * mix_gain * 0.01f, 0.f, 2.f));
    }

    FLASHMEM void View() override {
        if (trigger_flash)
            gfxIcon(56, 2, ZAP_ICON);

        // Draw 6 visible rows from scroll_top (scroll_top is a row index)
        for (int i = 0; i < 6; ++i) {
            int row = scroll_top + i;
            if (row >= NUM_ROWS) break;
            DrawRow(row, 15 + i * 8);
        }

        // Scroll arrows
        if (scroll_top > 0)
            gfxIcon(57, 14, UP_ICON);
        if (scroll_top + 6 < NUM_ROWS)
            gfxIcon(57, 56, DOWN_ICON);

        gfxDisplayInputMapEditor();
    }

    FLASHMEM void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, NUM_CURSORS - 1);
            // Scroll to keep active row visible
            int row = cursorToRow(cursor);
            if (row < scroll_top)
                scroll_top = row;
            else if (row >= scroll_top + 6)
                scroll_top = row - 5;
            scroll_top = constrain(scroll_top, 0, NUM_ROWS - 6);
            return;
        }

        if (EditSelectedInputMap(direction)) return;

        switch (cursor) {
            case TRG:
                trg.ChangeSource(direction);
                break;
            case PRESET:
                if (preset_idx > NUM_PRESETS) preset_idx = (direction > 0) ? 0 : NUM_PRESETS;
                else preset_idx = (preset_idx + (NUM_PRESETS + 1) + direction) % (NUM_PRESETS + 1);
                LoadPreset(preset_idx);
                break;
            case PIT:    pitch_hz = constrain(pitch_hz + direction * 5, 10, 2000); break;
            case DCY:    dec      = constrain(dec + direction * (dec>>1), 10, 4000); break;
            case SWP:    swp      = constrain(swp + direction, 0, 100); break;
            case RTO:    rto      = constrain(rto + direction, 1, 100); break;
            case FMI:    fmi      = constrain(fmi + direction, 0, 100); break;
            case FMD:    fmd      = constrain(fmd + direction * 10, 10, 4000); break;
            case NOI:    noi      = constrain(noi + direction, 0, 100); break;
            case NDC:    ndc      = constrain(ndc + direction * 5, 5, 4000); break;
            case MIX:    mix      = constrain(mix + direction, 0, 100); break;
            case CV_PIT: pitch_cv.ChangeSource(direction); break;
            case CV_DCY: dec_cv.ChangeSource(direction);   break;
            case CV_SWP: swp_cv.ChangeSource(direction);   break;
            case CV_RTO: rto_cv.ChangeSource(direction);   break;
            case CV_FMI: fmi_cv.ChangeSource(direction);   break;
            case CV_FMD: fmd_cv.ChangeSource(direction);   break;
            case CV_NOI: noi_cv.ChangeSource(direction);   break;
            case CV_NDC: ndc_cv.ChangeSource(direction);   break;
            case CV_MIX: mix_cv.ChangeSource(direction);   break;
            default: break;
        }
    }

    FLASHMEM void OnButtonPress() override {
        if (CheckEditInputMapPress(cursor,
              IndexedInput(TRG,    trg),
              IndexedInput(CV_PIT, pitch_cv),
              IndexedInput(CV_DCY, dec_cv),
              IndexedInput(CV_SWP, swp_cv),
              IndexedInput(CV_RTO, rto_cv),
              IndexedInput(CV_FMI, fmi_cv),
              IndexedInput(CV_FMD, fmd_cv),
              IndexedInput(CV_NOI, noi_cv),
              IndexedInput(CV_NDC, ndc_cv),
              IndexedInput(CV_MIX, mix_cv)))
            return;
        CursorToggle();
    }

    FLASHMEM void AuxButton() override {
        preset_idx = (preset_idx >= NUM_PRESETS) ? 0 : preset_idx + 1;
        LoadPreset(preset_idx);
    }

    FLASHMEM void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(pitch_hz, pack<12>(dec), swp, rto, fmi, pack<12>(fmd));
        data[1] = PackPackables(noi, ndc, mix, trg, mix_cv);
        data[2] = PackPackables(pitch_cv, dec_cv, swp_cv, rto_cv);
        data[3] = PackPackables(fmi_cv, fmd_cv, noi_cv, ndc_cv);
    }

    FLASHMEM void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], pitch_hz, pack<12>(dec), swp, rto, fmi, pack<12>(fmd));
        UnpackPackables(data[1], noi, ndc, mix, trg, mix_cv);
        UnpackPackables(data[2], pitch_cv, dec_cv, swp_cv, rto_cv);
        UnpackPackables(data[3], fmi_cv, fmd_cv, noi_cv, ndc_cv);
        preset_idx = 9;  // sentinel "---": params from savestate, not a named preset
    }

protected:
    FLASHMEM void SetHelp() override {}

private:
    enum Cursor : int8_t {
        TRG,
        PRESET,
        PIT, CV_PIT,
        DCY, CV_DCY,
        SWP, CV_SWP,
        RTO, CV_RTO,
        FMI, CV_FMI,
        FMD, CV_FMD,
        NOI, CV_NOI,
        NDC, CV_NDC,
        MIX, CV_MIX,
        NUM_CURSORS  // 20
    };

    static const int NUM_ROWS = 11; // TRG, PRESET, + 9 param-pair rows

    int8_t cursorToRow(int8_t c) const {
        if (c <= PRESET) return c;
        return 2 + (c - PIT) / 2;
    }

    // --- Parameters ---
    int16_t pitch_hz = 100;  // 10..2000 Hz
    uint16_t dec      = 400;  // 10..4000 ms
    int8_t  swp      = 60;   // 0..100 %
    int8_t  rto      = 10;   // 1..100 (÷10 = 0.1..10.0×)
    int8_t  fmi      = 80;   // 0..100 %
    uint16_t fmd      = 200;  // 10..4000 ms
    int8_t  noi      = 20;   // 0..100 %
    int16_t ndc      = 80;   // 5..4000 ms
    int8_t  mix      = 25;  // 0..100 % (100 = drum only, 0 = full passthrough)

    DigitalInputMap trg;
    CVInputMap pitch_cv, dec_cv, swp_cv, rto_cv;
    CVInputMap fmi_cv, fmd_cv, noi_cv, ndc_cv, mix_cv;

    // --- Envelope state (not saved) ---
    float amp_env   = 0.0f;
    float fm_env    = 0.0f;
    float noise_env = 0.0f;

    // --- Cached decay coefficients (recalculated only when effective value changes) ---
    float amp_coeff   = 0.9999f;
    float fm_coeff    = 0.9999f;
    float noise_coeff = 0.9999f;
    float prev_eff_dec = -1.f;
    float prev_eff_fmd = -1.f;
    float prev_eff_ndc = -1.f;

    // --- UI state ---
    int8_t  cursor      = TRG;
    int8_t  scroll_top  = 0;
    uint8_t trigger_flash = 0;
    uint8_t preset_idx  = 9;  // 9 = sentinel "---": params from savestate, not a named preset (NUM_PRESETS=8, Rnd=8, None=9)

    // --- Audio objects ---
    AudioPassthrough<MONO>       input_stream;
    AudioSynthWaveform           modulator;
    InterpolatingStream<>        fm_idx_stream;
    AudioVCA                     mod_vca;
    AudioSynthWaveformModulated  carrier;
    InterpolatingStream<>        amp_env_stream;
    AudioVCA                     amp_vca;
    AudioSynthNoiseWhite         noise_gen;
    AudioFilterStateVariable2    noise_hpf;
    InterpolatingStream<>        noise_env_stream;
    AudioVCA                     noise_vca;
    AudioMixer<3>                output_mixer;

    // --- Preset system ---
    struct FMDrumPreset {
        int16_t pitch_hz;
        uint16_t dec;
        int8_t  swp;
        int8_t  rto;
        int8_t  fmi;
        uint16_t fmd;
        int8_t  noi;
        int16_t ndc;
        const char* name;
    };

    static const int NUM_PRESETS = 8;
    static constexpr FMDrumPreset PRESETS[NUM_PRESETS] = {
        //        hz   dec  swp  rto  fmi   fmd noi  ndc  name
        {  60,   500,  80,  10,  90,  200,   5,  30, "Kick"  },
        { 160,    80,  15,  14,  70,   40,  90, 320, "Snare" },
        {1000,    60,   0,  35,  25,   30, 100, 100, "HiHat" },
        {1000,   350,   0,  35,  25,   80, 100, 750, "O.Hat" },
        { 120,   350,  60,  12,  70,  150,  15,  60, "Tom"   },
        { 300,    80,   5,   8,  40,   50,  90,  80, "Clap"  },
        { 500,   100,   0,  37,  90,   80,  10,  80, "Metal" },
        { 562,   300,   0,  50,  80,  100,   5, 100, "Cowbl" },
    };

    FLASHMEM void LoadPreset(int idx) {
        if (idx < NUM_PRESETS) {
            const auto& p = PRESETS[idx];
            pitch_hz = p.pitch_hz;
            dec      = p.dec;
            swp      = p.swp;
            rto      = p.rto;
            fmi      = p.fmi;
            fmd      = p.fmd;
            noi      = p.noi;
            ndc      = p.ndc;
        } else {
            // Random
            pitch_hz = random(20, 800);
            dec      = random(30, 800);
            swp      = random(0, 100);
            rto      = random(1, 50);
            fmi      = random(20, 100);
            fmd      = random(20, 500);
            noi      = random(0, 80);
            ndc      = random(20, 300);
        }
    }

    // DrawRow renders a display row (by row index 0..NUM_ROWS-1).
    // Each param row shows value cursor then CV source cursor inline.
    FLASHMEM void DrawRow(int row, int y) {
        switch (row) {
            case 0: // TRG
                gfxPrint(1, y, "TRG:");
                gfxStartCursor(25, y);
                gfxPrint(trg);
                gfxEndCursor(cursor == TRG, true, trg.InputName());
                break;
            case 1: // PRESET
                gfxPrint(1, y, "Type:");
                gfxStartCursor(31, y);
                if      (preset_idx < NUM_PRESETS)       gfxPrint(PRESETS[preset_idx].name);
                else if (preset_idx == NUM_PRESETS)      gfxPrint("Rnd");
                else                                     gfxPrint("---");
                gfxEndCursor(cursor == PRESET);
                break;
            case 2: // Pit + CV
                gfxPrint(1, y, "Pit:");
                gfxStartCursor(25, y);
                graphics.printf("%4d", (int)pitch_hz);
                gfxEndCursor(cursor == PIT);
                gfxStartCursor();
                gfxPrint(pitch_cv);
                gfxEndCursor(cursor == CV_PIT, false, pitch_cv.InputName());
                break;
            case 3: // Dec + CV
                gfxPrint(1, y, "Dec:");
                gfxStartCursor(25, y);
                graphics.printf("%4d", (int)dec);
                gfxEndCursor(cursor == DCY);
                gfxStartCursor();
                gfxPrint(dec_cv);
                gfxEndCursor(cursor == CV_DCY, false, dec_cv.InputName());
                break;
            case 4: // Swp + CV
                gfxPrint(1, y, "Swp:");
                gfxStartCursor(25, y);
                graphics.printf("%3d%%", (int)swp);
                gfxEndCursor(cursor == SWP);
                gfxStartCursor();
                gfxPrint(swp_cv);
                gfxEndCursor(cursor == CV_SWP, false, swp_cv.InputName());
                break;
            case 5: // Rto + CV
                gfxPrint(1, y, "Rto:");
                gfxStartCursor(25, y);
                graphics.printf("%2d.%1d", rto / 10, rto % 10);
                gfxEndCursor(cursor == RTO);
                gfxStartCursor();
                gfxPrint(rto_cv);
                gfxEndCursor(cursor == CV_RTO, false, rto_cv.InputName());
                break;
            case 6: // FMi + CV
                gfxPrint(1, y, "FMi:");
                gfxStartCursor(25, y);
                graphics.printf("%3d%%", (int)fmi);
                gfxEndCursor(cursor == FMI);
                gfxStartCursor();
                gfxPrint(fmi_cv);
                gfxEndCursor(cursor == CV_FMI, false, fmi_cv.InputName());
                break;
            case 7: // FMd + CV
                gfxPrint(1, y, "FMd:");
                gfxStartCursor(25, y);
                graphics.printf("%4d", (int)fmd);
                gfxEndCursor(cursor == FMD);
                gfxStartCursor();
                gfxPrint(fmd_cv);
                gfxEndCursor(cursor == CV_FMD, false, fmd_cv.InputName());
                break;
            case 8: // Noi + CV
                gfxPrint(1, y, "Noi:");
                gfxStartCursor(25, y);
                graphics.printf("%3d%%", (int)noi);
                gfxEndCursor(cursor == NOI);
                gfxStartCursor();
                gfxPrint(noi_cv);
                gfxEndCursor(cursor == CV_NOI, false, noi_cv.InputName());
                break;
            case 9: // Ndc + CV
                gfxPrint(1, y, "Ndc:");
                gfxStartCursor(25, y);
                graphics.printf("%4d", (int)ndc);
                gfxEndCursor(cursor == NDC);
                gfxStartCursor();
                gfxPrint(ndc_cv);
                gfxEndCursor(cursor == CV_NDC, false, ndc_cv.InputName());
                break;
            case 10: // Mix + CV
                gfxPrint(1, y, "Lvl:");
                gfxStartCursor(25, y);
                graphics.printf("%3d%%", (int)mix);
                gfxEndCursor(cursor == MIX);
                gfxStartCursor();
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == CV_MIX, false, mix_cv.InputName());
                break;
            default:
                break;
        }
    }
};

constexpr FMDrumApplet::FMDrumPreset FMDrumApplet::PRESETS[FMDrumApplet::NUM_PRESETS];
