#pragma once

// F32-native: the wavetable oscillator, blend interpolation, VCA, and output
// mix run on float32 blocks (see synth_waveform_F32.h). The wavetables store
// float, so blended/held slices never truncate to int16 and the arbitrary-
// waveform readout interpolates in full float precision. The chain still sees
// int16 via the HemisphereAudioAppletF32 edge adapters. Params, ranges, and
// UI are unchanged.

#include "../extern/f32/AudioMixer_F32.h"
#include "../Audio/synth_waveform_F32.h"
#include "../Audio/InterpolatingStreamF32.h"
#include "../Audio/AudioVCA_F32.h"

class WTVCOApplet : public HemisphereAudioAppletF32 {
public:
    const char* applet_name() {
        return "WTVCO";
    }

    void Start() override {
        sd_ready = CheckSD();

        waveform[A] = WAVE_SINE;
        waveform[B] = WAVE_TRIANGLE;
        waveform[C] = WAVE_PULSE;
        for (int w = A; w <= C; ++w) GenerateWaveTable(w);

        synth.arbitraryWaveform(wavetable[OUT], AUDIO_SAMPLE_RATE_EXACT / 2);
        synth.amplitude(1.0f);
        synth.begin(WAVEFORM_ARBITRARY);

        vca_cv.Acquire();
        vca_cv.Method(INTERPOLATION_LINEAR);
        vca.rectify(true);

        PatchCableF32(InputF32(), 0, mixer, 0);
        PatchCableF32(vca_cv, 0, vca, 1);
        PatchCableF32(synth, 0, vca, 0);
        PatchCableF32(vca, 0, mixer, 1);
        PatchCableF32(mixer, 0, OutputF32(), 0);

        mixer.gain(0, 1.0f);
        mixer.gain(1, 1.0f);
    }

    void Unload() override {
        AllowRestart();
    }

    void Controller() override {
        float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
        synth.frequency(freq);
        int _wt_blend = wt_blend + wt_blend_cv.InRescaled(WT_SIZE - 1);
        for (int w = A; w <= C; ++w) {
            if (waveform[w] == WAVE_PULSE) UpdatePulseDuty(wavetable[w], wt_sample, pulse_duty + pulse_duty_cv.In());
            if (waveform[w] == WAVE_NOISE && !noise_freeze) UpdateNoiseSample(wavetable[w], wt_sample);
        }
        InterpolateSample(wavetable[OUT], _wt_blend, wt_sample++);  // this does not necessarilly interpolate the same sample of the waveform that is played from the output during the same program cycle

        // stolen from OscApplet
        float gain = dbToScalar(level);
        if (level_cv.enabled()) {
            vca.bias(0.0f);
            vca.level(gain);
            float cv = level_cv.InF();
            vca_cv.Push(cv * cv);
        } else {
            vca.bias(gain);
            vca.level(0.0f);
        }
        // There's a good chance of phase correlation if the incoming signal is internal, so use equal amplitude
        float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);
        mixer.gain(1, m);
        mixer.gain(0, 1.0f - m);
    }

    void View() override {
        if (cursor > WAVEFORM_LAST) {
            DrawParams();
        } else {
            DrawWaveMenu();
        }
        DrawScope();
        DrawSelector();
        gfxDisplayInputMapEditor();
    }

    void OnButtonPress() override {
        userwave_select = false;
        if (cursor == PARAM_OSC_DIRECTION) {
            osc_rev = !osc_rev;
            return;
        }
        if (CheckEditInputMapPress( cursor,
            IndexedInput(PARAM_PITCH_CV, pitch_cv),
            IndexedInput(PARAM_BLEND_CV, wt_blend_cv),
            IndexedInput(PARAM_PULSE_DUTY_CV, pulse_duty_cv),
            IndexedInput(PARAM_LEVEL_CV, level_cv),
            IndexedInput(PARAM_MIX_CV, mix_cv)
        )) return;
        CursorToggle();
    }

    void AuxButton() {
        if (cursor > WAVEFORM_OUT && cursor <= WAVEFORM_LAST) {
            const int idx = cursor - WAVEFORM_A;
            if (waveform[idx] == WAVE_NOISE) noise_freeze = !noise_freeze;  // toggle "realtime" or frozen noise wave buffer
            else if (waveform[idx] == WAVE_RAND_STEPPED) GenerateWaveForm_RandStepped(wavetable[idx]);  // re-roll random step wave
            else if (waveform[idx] == WAVE_USER) userwave_select = true;
        }
    }

    void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }
        if (EditSelectedInputMap(direction)) return;

        if (cursor > WAVEFORM_LAST) {
            switch(cursor) {
                case PARAM_OCTAVE:
                    pitch = constrain(pitch + direction * 1 * 128, MIN_PITCH, MAX_PITCH);
                    break;
                case PARAM_PITCH:
                    pitch = constrain(pitch + direction * 4, MIN_PITCH, MAX_PITCH);
                    break;
                case PARAM_PITCH_CV:
                    pitch_cv.ChangeSource(direction);
                    break;
                case PARAM_BLEND:
                    wt_blend = constrain(wt_blend + direction, 0, (uint8_t)(WT_SIZE - 1));
                    break;
                case PARAM_BLEND_CV:
                    wt_blend_cv.ChangeSource(direction);
                    break;
                case PARAM_PULSE_DUTY:
                    pulse_duty = constrain(pulse_duty + direction, 0, (uint8_t)(WT_SIZE - 1));
                    break;
                case PARAM_PULSE_DUTY_CV:
                    pulse_duty_cv.ChangeSource(direction);
                    break;
                case PARAM_LEVEL:
                    level = constrain(level + direction, LVL_MIN_DB, LVL_MAX_DB);
                    break;
                case PARAM_LEVEL_CV:
                    level_cv.ChangeSource(direction);
                    break;
                case PARAM_MIX:
                    mix = constrain(mix + direction, 0, 100);
                    break;
                case PARAM_MIX_CV:
                    mix_cv.ChangeSource(direction);
                    break;
                default: break;
            }
        } else {
            switch(cursor) {
                case WAVEFORM_OUT:
                    wt_blend = constrain(wt_blend + direction, 0, (uint8_t)(WT_SIZE - 1));
                    break;
                case WAVEFORM_A:
                case WAVEFORM_B:
                case WAVEFORM_C: {
                    const int idx = cursor - WAVEFORM_A;
                    if (userwave_select) userwave[idx] = raw_wave_count > 0 ? constrain(userwave[idx] + direction, 0, raw_wave_count - 1) : 0;
                    else waveform[idx] = (WaveForms)constrain(((int)waveform[idx]) + direction, 0, WAVEFORM_COUNT - 1);
                    GenerateWaveTable(idx);
                    break;
                }
                default: break;
            }
        }
    }

    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(pitch, wt_blend, pulse_duty, level, mix);
        data[1] = PackPackables(pitch_cv, wt_blend_cv, pulse_duty_cv, level_cv);
        data[2] = PackPackables(mix_cv, waveform[A], waveform[B], waveform[C], userwave[A], userwave[B], userwave[C]);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], pitch, wt_blend, pulse_duty, level, mix);
        UnpackPackables(data[1], pitch_cv, wt_blend_cv, pulse_duty_cv, level_cv);
        UnpackPackables(data[2], mix_cv, waveform[A], waveform[B], waveform[C], userwave[A], userwave[B], userwave[C]);

        CONSTRAIN(level, LVL_MIN_DB, LVL_MAX_DB);

        for (int w = A; w <= C; ++w) GenerateWaveTable(w);
    }

protected:
    void SetHelp() override {}

private:
    enum Cursor {
        WAVEFORM_OUT = 0,
        WAVEFORM_A,
        WAVEFORM_B,
        WAVEFORM_C, WAVEFORM_LAST = WAVEFORM_C,

        PARAM_OCTAVE,
        PARAM_PITCH,
        PARAM_PITCH_CV,
        PARAM_OSC_DIRECTION,
        PARAM_BLEND,
        PARAM_BLEND_CV,
        PARAM_PULSE_DUTY,
        PARAM_PULSE_DUTY_CV,
        PARAM_LEVEL,
        PARAM_LEVEL_CV,
        PARAM_MIX,
        PARAM_MIX_CV, PARAM_LAST = PARAM_MIX_CV
    };
    const int CURSOR_LAST = PARAM_LAST;

    enum WaveTables { A, B, C, OUT };

    enum WaveForms : uint8_t {
        WAVE_SINE,
        WAVE_TRIANGLE,
        WAVE_PULSE,
        WAVE_SAW,
        WAVE_RAMP,
        WAVE_STEPPED,
        WAVE_RAND_STEPPED,
        WAVE_NOISE,
        WAVE_SILENCE,
        // add more waves here and generator functions at the bottom
        WAVE_USER,

        WAVEFORM_COUNT
    };
    static constexpr const char* const waveform_names[WAVEFORM_COUNT] = {
        "Sine", "Triangl", "Pulse", "Saw", "Ramp", "Stepped", "RandStp", "Noise", "Silence", "User "
    };

    CVInputMap pitch_cv;
    CVInputMap wt_blend_cv;
    CVInputMap pulse_duty_cv;
    CVInputMap level_cv;
    CVInputMap mix_cv;

    AudioSynthWaveformF32 synth;
    InterpolatingStreamF32<> vca_cv;
    AudioVCA_F32 vca;
    AudioMixer4_F32 mixer;

    int cursor = 0;  // WTVCO_Cursor

    static constexpr int16_t WT_SIZE = 256;
    static constexpr int16_t MAX_PITCH = 7 * 12 * 128;
    static constexpr int16_t MIN_PITCH = -3 * 12 * 128;
    static constexpr uint8_t MAX_RAW_WAVES = 255;

    int16_t pitch = 1 * 12 * 128;  // C4
    uint8_t wt_blend = 0;
    uint8_t pulse_duty = 127;
    int8_t level = 0;  // dB
    int8_t mix = 100;
    uint8_t wt_sample = 0;  // used to interpolate between waveforms
    bool osc_rev = false;
    bool noise_freeze = false;  // push aux button while Noise wave is selected to freeze the buffer

    bool userwave_select = false;  // push aux button while User wave is selected to choose from SD card.
    bool sd_ready = false;
    int raw_wave_count = 0;

    WaveForms waveform[3];  // selected waveform name
    float wavetable[4][WT_SIZE];  // audio data, float32 (full precision through blend/hold)
    uint8_t userwave[3] = {0, 0, 0};  // for custom waves in an SD card

// GRAPHIC STUFF:
    static constexpr uint8_t HEADER_HEIGHT = 11;
    static constexpr uint8_t MENU_ROW = 14;
    static constexpr uint8_t X_DIV = 64 / 4;
    static constexpr uint8_t Y_DIV = (64 - HEADER_HEIGHT) / 4;

    void gfxRenderWave(const int w) {
        for (int x = 0; x < WT_SIZE; x += 4) {
            uint8_t y = 44 - static_cast<int>(wavetable[w][x] * 16.0f);
            gfxPixel(x / 4, y);
        }
    }

    void DrawSelector() {
        uint8_t x = 0;
        uint8_t y = HEADER_HEIGHT + 1;
        uint8_t w = X_DIV;
        uint8_t h = HEADER_HEIGHT + 1;
        if (!EditMode() && cursor <= WAVEFORM_LAST) {
            x = cursor * X_DIV;
            gfxInvert(x, y, w, h);
        }
        else return;
    }

    void DrawBlendicator(int b) {
        const uint8_t y = 2 * HEADER_HEIGHT;
        const uint8_t h = 2;
        uint8_t x =  1 + X_DIV * (1 + (b / 128)) + ((b / 64) % 2) * Proportion(b - (64 * (b / 64)), 63, X_DIV);
        uint8_t w = -1 + X_DIV * (1 + ((b / 64) % 2)) + ((!((b / 64) % 2) * 2) - 1) * Proportion(b - (64 * (b / 64)), 63, X_DIV);
        gfxRect(x, y, w, h);
    }

    void DrawWaveMenu() {
        uint8_t x = 3;
        uint8_t y = MENU_ROW;
        if (!EditMode() || cursor == WAVEFORM_OUT) {
            gfxBitmap(x + 1, y - 1, 8, WAVEFORM_ICON);
            char label[] = {'A', '\0'};
            for (int i = 0; i < 3; ++i) {
                x += X_DIV;
                gfxPrint(x + 2, y, label);
                ++label[0];
            }
            if (cursor == WAVEFORM_OUT) DrawBlendicator(wt_blend);
        } else {
            switch(cursor) {
                case WAVEFORM_A:
                case WAVEFORM_B:
                case WAVEFORM_C: {
                    const int idx = cursor - WAVEFORM_A;
                    char label[] = {char('A'+idx), ':', '\0'};
                    gfxPrint(3, MENU_ROW, label);
                    gfxPrint(waveform_names[waveform[idx]]);
                    if (waveform[idx] == WAVE_USER) gfxPrint(userwave[idx]);
                    break;
                }
                default: break;
            }
        }
    }

    void DrawScope() {
        switch(cursor) {
            case WAVEFORM_A:
            case WAVEFORM_B:
            case WAVEFORM_C:
                gfxRenderWave(cursor - WAVEFORM_A);
                break;
            case WAVEFORM_OUT:
            default:
                gfxRenderWave(OUT);
                break;
        }
        gfxDottedLine(0, MENU_ROW + 11, 63, MENU_ROW + 11, 4U);
        gfxDottedLine(0, 63, 63, 63, 4U);
    }

    void DrawParams() {
        switch(cursor) {
            case PARAM_OCTAVE:
            case PARAM_PITCH:
            case PARAM_PITCH_CV: {
                gfxStartCursor(1, 14);
                gfxPrintTuningIndicator(pitch);
                gfxEndCursor(cursor == PARAM_OCTAVE);

                gfxStartCursor(11, 14);
                gfxPrintPitchHz(pitch);
                gfxEndCursor(cursor == PARAM_PITCH);

                gfxStartCursor();
                gfxPrint(pitch_cv);
                gfxEndCursor(cursor == PARAM_PITCH_CV, false, pitch_cv.InputName());
                break;
            }
            case PARAM_OSC_DIRECTION:
            case PARAM_BLEND:
            case PARAM_BLEND_CV: {
                gfxStartCursor(1, 14);
                gfxPrint(1, 14, "Blnd:");
                gfxEndCursor(cursor == PARAM_OSC_DIRECTION);
                if (osc_rev) gfxInvert(1, 14, 7 * 4, 8);

                gfxStartCursor();
                graphics.printf("%4d", wt_blend);
                gfxEndCursor(cursor == PARAM_BLEND);

                gfxStartCursor();
                gfxPrint(wt_blend_cv);
                gfxEndCursor(cursor == PARAM_BLEND_CV, false, wt_blend_cv.InputName());
                break;
            }
            case PARAM_PULSE_DUTY:
            case PARAM_PULSE_DUTY_CV: {
                gfxPrint(1, 14, "Duty:");
                gfxStartCursor();
                graphics.printf("%4d", pulse_duty);
                gfxEndCursor(cursor == PARAM_PULSE_DUTY);

                gfxStartCursor();
                gfxPrint(pulse_duty_cv);
                gfxEndCursor(cursor == PARAM_PULSE_DUTY_CV, false, pulse_duty_cv.InputName());
                break;
            }
            case PARAM_LEVEL:
            case PARAM_LEVEL_CV: {
                gfxPrint(1, 14, "Lvl:");
                gfxStartCursor();
                graphics.printf("%3ddB", level);
                gfxEndCursor(cursor == PARAM_LEVEL);

                gfxStartCursor();
                gfxPrint(level_cv);
                gfxEndCursor(cursor == PARAM_LEVEL_CV, false, level_cv.InputName());
                break;
            }
            case PARAM_MIX:
            case PARAM_MIX_CV: {
                gfxPrint(1, 14, "Mix: ");
                gfxStartCursor();
                graphics.printf("%3d%%", mix);
                gfxEndCursor(cursor == PARAM_MIX);

                gfxStartCursor();
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == PARAM_MIX_CV, false, mix_cv.InputName());
                break;
            }
            default: break;
        }
    }

// WAVETABLE STUFF:
    void InterpolateSample(float* wt, int blend, uint8_t sample) {
        uint8_t s = sample * (1 - 2 * osc_rev) + (255 * osc_rev);
        // same A/B/C crossfade coefficients as the int16 original, in float;
        // CV-pushed blend clamps instead of wrapping through int16
        CONSTRAIN(blend, 0, 255);
        if (blend <= 127) {
            wt[s] = (static_cast<float>(127 - blend) * wavetable[A][sample]
                   + static_cast<float>(blend) * wavetable[B][sample]) * (1.0f / 127.0f);
        } else {
            wt[s] = (static_cast<float>(255 - blend) * wavetable[B][sample]
                   + static_cast<float>(blend - 128) * wavetable[C][sample]) * (1.0f / 127.0f);
        }
    }

    void UpdatePulseDuty(float* wt, const uint8_t sample, const uint8_t duty) {
        wt[sample] = (sample < duty) ? 1.0f : -1.0f;
    }

    void UpdateNoiseSample(float* wt, const uint8_t sample) {  // noise sounds funny, i think this is getting called less frequently than the audio output
        wt[sample] = static_cast<float>(random(-32768, 32768)) * (1.0f / 32768.0f);
    }

    void GenerateWaveTable(const int w) {
        switch(waveform[w]) {
            case WAVE_SINE:
                GenerateWaveForm_Sine(wavetable[w]);
                break;
            case WAVE_TRIANGLE:
                GenerateWaveForm_Triangle(wavetable[w]);
                break;
            case WAVE_PULSE:
                GenerateWaveForm_Pulse(wavetable[w]);
                break;
            case WAVE_SAW:
                GenerateWaveForm_Sawtooth(wavetable[w]);
                break;
        // probably going to get rid of these:
            case WAVE_RAMP:
                GenerateWaveForm_Ramp(wavetable[w]);
                break;
            case WAVE_STEPPED:
                GenerateWaveForm_Stepped(wavetable[w]);
                break;
            case WAVE_RAND_STEPPED:
                GenerateWaveForm_RandStepped(wavetable[w]);
                break;
        //
            case WAVE_NOISE:
                GenerateWaveForm_Noise(wavetable[w]);
                break;
            case WAVE_SILENCE:
                GenerateWaveForm_Silence(wavetable[w]);
                break;
            case WAVE_USER:
                GenerateWaveForm_User(wavetable[w], w);
                break;
            default: break;
        }
    }

// WAVEFORM GENERATORS:
    void GenerateWaveForm_Sine(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = sinf(static_cast<float>(i) * (6.283185307179586f / WT_SIZE));
        }
    }

    void GenerateWaveForm_Triangle(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {  // theres probably a cleaner way to do this but i want a full wave period starting at 0
            float value;
            if (i < (WT_SIZE >> 2))
                value = static_cast<float>(i) / (WT_SIZE >> 2);
            else if (i < 3 * (WT_SIZE >> 2))
                value = 1.0f - static_cast<float>(i - (WT_SIZE >> 2)) * 2.0f / (WT_SIZE >> 1);
            else
                value = -1.0f + static_cast<float>(i - 3 * (WT_SIZE >> 2)) / (WT_SIZE >> 2);

            waveform[i] = value;
        }
    }

    void GenerateWaveForm_Pulse(float* waveform) {
        int half = WT_SIZE / 2;
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = (i < half) ? 1.0f : -1.0f;
        }
    }

    void GenerateWaveForm_Sawtooth(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = static_cast<float>(WT_SIZE - i - 1) * (2.0f / WT_SIZE) - 1.0f;
        }
    }

    void GenerateWaveForm_Ramp(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = static_cast<float>(i) * (2.0f / WT_SIZE) - 1.0f;
        }
    }

    void GenerateWaveForm_Stepped(float* waveform) {
        const int steps = 5;
        const int stepSize = WT_SIZE / steps;
        for (int i = 0; i < WT_SIZE; ++i) {
            int step = i / stepSize;
            if (step > steps - 1) step = steps - 1;  // int16 version wrapped on the final sample
            waveform[i] = static_cast<float>(step) * 2.0f / (steps - 1) - 1.0f;
        }
    }

    void GenerateWaveForm_RandStepped(float* waveform) {
        const int steps = 5;
        const int stepSize = WT_SIZE / steps;
        int currentStep = -1;
        float value = 0.0f;
        for (int i = 0; i < WT_SIZE; ++i) {
            int step = i / stepSize;
            if (step != currentStep) {
                currentStep = step;
                value = static_cast<float>(random(-32768, 32768)) * (1.0f / 32768.0f);
            }
            waveform[i] = value;
        }
    }

    void GenerateWaveForm_Noise(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = static_cast<float>(random(-32768, 32768)) * (1.0f / 32768.0f);
        }
    }

    void GenerateWaveForm_Silence(float* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = 0.0f;
        }
    }

    void GenerateWaveForm_User(float* waveform, const int idx) {
        if (!ReadWaveFromSD(waveform, idx)) GenerateWaveForm_Silence(waveform);
    }

    bool ReadWaveFromSD(float* waveform, const int idx = 0) {
        if(!sd_ready) return false;

        File dir = SD.open("/WTVCO");
        File file;
        for (int i = 0; i <= userwave[idx]; ++i) {
            file = dir.openNextFile();
        }

        bool read = false;
        if (!file.isDirectory() && isRawFile(file.name())) {
            int16_t raw[WT_SIZE];  // .raw files stay 16-bit on disk
            file.read(raw, 2 * WT_SIZE);
            for (int i = 0; i < WT_SIZE; ++i) {
                waveform[i] = static_cast<float>(raw[i]) * (1.0f / 32768.0f);
            }
            read = true;
        }
        file.close();
        dir.close();
        return read;
    }

    bool CheckSD() {
        File dir = SD.open("/WTVCO");
        if (!dir || !dir.isDirectory()) {
            sd_ready = false;
            return sd_ready;
        }

        while (true) {
            File file = dir.openNextFile();
            if (!file) break;

            if (!file.isDirectory() && isRawFile(file.name())) {
                if (raw_wave_count < MAX_RAW_WAVES) {
                    raw_wave_count++;
                }
            }
            file.close();
        }
        dir.close();

        if (raw_wave_count > 0) {
            sd_ready = true;
        }
        return sd_ready;
    }

    bool isRawFile(const char* name) {
        const char* ext = strrchr(name, '.');
        return ext && strcasecmp(ext, ".raw") == 0;
    }
};
