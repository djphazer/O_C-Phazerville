#pragma once

class WTVCOApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override {
        return "WTVCO";
    }

    void Start() override {
        bool sd_ready = CheckSD();

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

        PatchCable(input_stream, 0, mixer, 0);
        PatchCable(synth, 0, vca, 0);
        PatchCable(vca, 0, mixer, 1);
        PatchCable(mixer, 0, output_stream, 0);

        mixer.gain(0, 1.0f);
        mixer.gain(1, 1.0f);
    }

    void Unload() override {
        AllowRestart();
    }

    void Controller() override {
        float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
        synth.frequency(freq);
        int _wt_blend = wt_blend + Proportion(wt_blend_cv.In(), HEMISPHERE_MAX_INPUT_CV, WT_SIZE - 1);

        InterpolateSample(wavetable[OUT], _wt_blend, wt_sample++);
        for (int w = A; w <= C; ++w) {
            if (waveform[w] == WAVE_PULSE) UpdatePulseDuty(wavetable[w], wt_sample, pulse_duty + pulse_duty_cv.In());
            if (waveform[w] == WAVE_NOISE && !noise_freeze) UpdateNoiseSample(wavetable[w], wt_sample);
        }

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
        if(EditSelectedInputMap(direction)) return;

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
                    if (userwave_select) {
                        userwave[idx] = constrain(userwave[idx] + direction, 0, 255);
                    } else {
                        waveform[idx] = (WaveForms)constrain(((int)waveform[idx]) + direction, 0, WAVEFORM_COUNT - 1);
                    }
                        GenerateWaveTable(idx);
                    break;
                }
                default: break;
            }
        }
    }

    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(pitch, wt_blend, pulse_duty);
        data[1] = PackPackables(level, mix);
        data[2] = PackPackables(pitch_cv, wt_blend_cv, pulse_duty_cv);
        data[3] = PackPackables(level_cv, mix_cv);
        data[4] = PackPackables(waveform[A], waveform[B], waveform[C]);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], pitch, wt_blend, pulse_duty);
        UnpackPackables(data[1], level, mix);
        UnpackPackables(data[2], pitch_cv, wt_blend_cv, pulse_duty_cv);
        UnpackPackables(data[3], level_cv, mix_cv);
        UnpackPackables(data[4], waveform[A], waveform[B], waveform[C]);

        CONSTRAIN(level, LVL_MIN_DB, LVL_MAX_DB);

        waveform[A] = WAVE_SINE;
        waveform[B] = WAVE_TRIANGLE;
        waveform[C] = WAVE_PULSE;
        for (int w = A; w <= C; ++w) GenerateWaveTable(w);
    }

    AudioStream* InputStream() override {
        return &input_stream;
    }
    AudioStream* OutputStream() override {
        return &output_stream;
    };

protected:
    void SetHelp() override {}

private:
    enum WTVCO_Cursor {
        WAVEFORM_OUT = 0,
        WAVEFORM_A,
        WAVEFORM_B,
        WAVEFORM_C,
        WAVEFORM_LAST = WAVEFORM_C,

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
        PARAM_MIX_CV,
        PARAM_LAST = PARAM_MIX_CV
    };
    const int CURSOR_LAST = PARAM_LAST;

    enum WaveTables { A, B, C, OUT };

    enum WaveForms {
        WAVE_SINE,
        WAVE_TRIANGLE,
        WAVE_PULSE,
        WAVE_SAW,
        WAVE_RAMP,
        WAVE_STEPPED,
        WAVE_RAND_STEPPED,
        WAVE_NOISE,
        WAVE_USER,
        // add more waves here and generator functions at the bottom

        WAVEFORM_COUNT
    };
    static constexpr const char* const waveform_names[WAVEFORM_COUNT] = {
        "Sine", "Triangl", "Pulse", "Saw", "Ramp", "Stepped", "RandStp", "Noise", "User"
    };

    CVInputMap pitch_cv;
    CVInputMap wt_blend_cv;
    CVInputMap pulse_duty_cv;
    CVInputMap level_cv;
    CVInputMap mix_cv;

    AudioPassthrough<MONO> input_stream;
    AudioSynthWaveform synth;
    InterpolatingStream<> vca_cv;
    AudioVCA vca;
    AudioMixer<2> mixer;
    AudioPassthrough<MONO> output_stream;

    int cursor = 0;  // WTVCO_Cursor

    static constexpr int16_t WT_SIZE = 256;
    static constexpr int16_t MAX_PITCH = 7 * 12 * 128;
    static constexpr int16_t MIN_PITCH = -3 * 12 * 128;

    int16_t pitch = 1 * 12 * 128;  // C4
    int wt_blend = 0;
    uint8_t pulse_duty = 127;
    int8_t level = 0;  // dB
    int8_t mix = 100;
    uint8_t wt_sample = 0;  // used to interpolate between waveforms
    bool osc_rev = false;
    bool noise_freeze = false;  // push aux button while Noise wave is selected to freeze the buffer

    bool userwave_select = false;  // push aux button while User wave is selected to choose from SD card.
    int raw_wave_count = 0;

    WaveForms waveform[3];  // selected waveform name
    int16_t wavetable[4][WT_SIZE];  // audio data. had to upsize to 16 bit from 8 to use the AudioSynthWaveform().arbitraryWaveform
    uint8_t userwave[3] = {0, 0, 0};  // for custom waves in an SD card

// GRAPHIC STUFF:
    static constexpr uint8_t HEADER_HEIGHT = 11;
    static constexpr uint8_t MENU_ROW = 14;
    static constexpr uint8_t X_DIV = 64 / 4;
    static constexpr uint8_t Y_DIV = (64 - HEADER_HEIGHT) / 4;

    void gfxRenderWave(const int w) {
        for (int x = 0; x < WT_SIZE; x += 4) {
            uint8_t y = 44 - Proportion(wavetable[w][x], 32767, 16);
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
            case PARAM_PITCH_CV:
                // pitch
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

            case PARAM_OSC_DIRECTION:
            case PARAM_BLEND:
            case PARAM_BLEND_CV:
                // blend
                // gfxStartCursor(1,14);
                // gfxIcon(1, 14, WAVEFORM_ICON);
                // if (osc_rev) gfxIcon(9, 14, LEFT_ICON);
                gfxPrint(1, 14, "Blnd:");
                if (osc_rev) gfxInvert(1, 14, 7*4, 8);
                gfxStartCursor();
                gfxEndCursor(cursor == PARAM_OSC_DIRECTION);

                gfxStartCursor(11, 14);
                graphics.printf("%4d", wt_blend);
                gfxEndCursor(cursor == PARAM_BLEND);

                gfxStartCursor();
                gfxPrint(wt_blend_cv);
                gfxEndCursor(cursor == PARAM_BLEND_CV, false, wt_blend_cv.InputName());
                break;

            case PARAM_PULSE_DUTY:
            case PARAM_PULSE_DUTY_CV:
                gfxPrint(1, 14, "Duty:");
                gfxStartCursor();
                graphics.printf("%4d", pulse_duty);
                gfxEndCursor(cursor == PARAM_PULSE_DUTY);

                gfxStartCursor();
                gfxPrint(pulse_duty_cv);
                gfxEndCursor(cursor == PARAM_PULSE_DUTY_CV, false, pulse_duty_cv.InputName());
                break;

            case PARAM_LEVEL:
            case PARAM_LEVEL_CV:
                gfxPrint(1, 14, "Lvl:");
                gfxStartCursor();
                graphics.printf("%3ddB", level);
                gfxEndCursor(cursor == PARAM_LEVEL);

                gfxStartCursor();
                gfxPrint(level_cv);
                gfxEndCursor(cursor == PARAM_LEVEL_CV, false, level_cv.InputName());
                break;

            case PARAM_MIX:
            case PARAM_MIX_CV:
                gfxPrint(1, 14, "Mix: ");
                gfxStartCursor();
                graphics.printf("%3d%%", mix);
                gfxEndCursor(cursor == PARAM_MIX);

                gfxStartCursor();
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == PARAM_MIX_CV, false, mix_cv.InputName());
                break;

            default: break;
        }

        // gfxPrint(1, 35, "Lvl:");
        // gfxStartCursor();
        // graphics.printf("%3ddB", level);
        // gfxEndCursor(cursor == LEVEL);

        // gfxStartCursor();
        // gfxPrint(level_cv);
        // gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

        // gfxPrint(1, 45, "Mix: ");
        // gfxStartCursor();
        // graphics.printf("%3d%%", mix);
        // gfxEndCursor(cursor == MIX);

        // gfxStartCursor();
        // gfxPrint(mix_cv);
        // gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

    //     uint8_t y = MENU_ROW + Y_DIV;

    //     gfxIcon(0, y, NOTE_ICON); gfxBitmap(6, y + 6, 3, SUP_ONE); // Pitch 1
    //     gfxPos(6, y); graphics.printf("%4d", pitch / 128);
    //     y += Y_DIV;

    //     gfxIcon(1, y, WAVEFORM_ICON); gfxBitmap(8, y - 1, 3, SUP_ONE); gfxBitmap(8, y + 2, 3, SUB_TWO); // Blend
    //     gfxPos(6, y); graphics.printf("%4d", wt_blend);
    //     y += Y_DIV;

    //     if (page_cursor < PARAM_LAST) {
    //         gfxIcon(33, y, GATE_ICON); // SqDuty
    //         gfxPos(44, y); graphics.printf("%3d", pulse_duty);
    //     } else {
    //         gfxIcon(1, y, GATE_ICON); // SqDuty
    //         gfxPos(12, y); graphics.printf("%3d", pulse_duty);

    //         gfxIcon(33, y, STAIRS_ICON); // SR.Div
    //         gfxPos(44, y); graphics.printf("%3d", sample_rate_div);
    //     }
    }

// WAVETABLE STUFF:
    void InterpolateSample(int16_t* wt, const int blend, uint8_t sample) {
        uint8_t s = sample * (1 - 2 * osc_rev) + (255 * osc_rev);
        wt[s] = (((blend <= 127) * ((127 - blend) * (int)wavetable[A][sample] + blend * (int)wavetable[B][sample]))
                      + ((blend > 127) * ((255 - blend) * (int)wavetable[B][sample] + (blend - 128) * (int)wavetable[C][sample]))) / 127;
    }

    void UpdatePulseDuty(int16_t* wt, const uint8_t sample, const uint8_t duty) {
        wt[sample] = (sample < duty) ? 32767 : -32768;
    }

    void UpdateNoiseSample(int16_t* wt, const uint8_t sample) {
        wt[sample] = static_cast<int16_t>(random(-32768, 32768));
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
            case WAVE_RAMP:
                GenerateWaveForm_Ramp(wavetable[w]);
                break;
            case WAVE_STEPPED:
                GenerateWaveForm_Stepped(wavetable[w]);
                break;
            case WAVE_RAND_STEPPED:
                GenerateWaveForm_RandStepped(wavetable[w]);
                break;
            case WAVE_NOISE:
                GenerateWaveForm_Noise(wavetable[w]);
                break;
            case WAVE_USER:
                GenerateWaveForm_User(wavetable[w]);
                break;
            // add waves here
            default: break;
        }
    }

// WAVEFORM GENERATORS:
    void GenerateWaveForm_Sine(int16_t* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            q15_t phase = static_cast<q15_t>(i * 32768 / WT_SIZE);
            waveform[i] = arm_sin_q15(phase);
        }
    }

    void GenerateWaveForm_Triangle(int16_t* waveform) {
        int value = 0;
        for (int i = 0; i < WT_SIZE; ++i) {  // theres probably a cleaner way to do this but i want a full wave period starting at 0
            if (i < (WT_SIZE >> 2))
                value = i * 32767 / (WT_SIZE >> 2);
            else if (i < 3 * (WT_SIZE >> 2))
                value = 32767 - (i - (WT_SIZE >> 2)) * 65535 / (WT_SIZE >> 1);
            else
                value = -32768 + (i - 3 * (WT_SIZE >> 2)) * 32768 / (WT_SIZE >> 2);

            waveform[i] = static_cast<int16_t>(value);
        }
    }

    void GenerateWaveForm_Pulse(int16_t* waveform) {
        int half = WT_SIZE / 2;
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = (i < half) ? 32767 : -32768;
        }
    }

    void GenerateWaveForm_Sawtooth(int16_t* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            int value = ((WT_SIZE - i - 1) * 65536) / WT_SIZE;
            waveform[i] = static_cast<int16_t>(value - 32768);
        }
    }

    void GenerateWaveForm_Ramp(int16_t* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            int value = (i * 65536) / WT_SIZE;
            waveform[i] = static_cast<int16_t>(value - 32768);
        }
    }

    void GenerateWaveForm_Stepped(int16_t* waveform) {
        const int steps = 5;
        const int stepSize = WT_SIZE / steps;
        for (int i = 0; i < WT_SIZE; ++i) {
            int value = (i / stepSize) * 65535 / (steps - 1);
            waveform[i] = static_cast<int16_t>(value - 32768);
        }
    }

    void GenerateWaveForm_RandStepped(int16_t* waveform) {
        const int steps = 5;
        const int stepSize = WT_SIZE / steps;
        int currentStep = -1;
        int16_t value = 0;
        for (int i = 0; i < WT_SIZE; ++i) {
            int step = i / stepSize;
            if (step != currentStep) {
                currentStep = step;
                value = static_cast<int16_t>(random(-32768, 32768));
            }
            waveform[i] = value;
        }
    }

    void GenerateWaveForm_Noise(int16_t* waveform) {
        for (int i = 0; i < WT_SIZE; ++i) {
            waveform[i] = static_cast<int16_t>(random(-32768, 32768));
        }
    }

    void GenerateWaveForm_User(int16_t* waveform) {
        GenerateWaveForm_Sine(waveform);
        // if(!SD.begin(BUILTIN_SDCARD)) GenerateWaveForm_Sine(waveform);
        // else GenerateWaveForm_Sawtooth(waveform);  // SelectUserWaveform()
    }

    void SelectUserWaveForm(int* waveform, int dir) {

    }

    bool CheckSD() {
        int fileCount = 0;
        bool wtvcoReady = false;

        File dir = SD.open("/WTVCO");

        if (!dir || !dir.isDirectory()) {
            // Directory does not exist
            return wtvcoReady;
        }

        while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;

            if (!entry.isDirectory() && isRawFile(entry.name())) {
                // if (fileCount < 256) {
                //     strncpy(filenames[fileCount], entry.name(), 7);
                //     filenames[fileCount][7 - 1] = '\0';
                //     fileCount++;
                // }
            }

            entry.close();
        }

        dir.close();

        if (fileCount > 0) {
            wtvcoReady = true;
        }
        return wtvcoReady;
    }

    bool isRawFile(const char* name) {
        const char* ext = strrchr(name, '.');
        return ext && strcasecmp(ext, ".raw") == 0;
    }

    // bool loadWavetableByIndex(int index) {
    //     if (!wtvcoReady || index < 0 || index >= fileCount) return false;

    //     char path[64];
    //     snprintf(path, sizeof(path), "/WTVCO/%s", filenames[index]);

    //     File file = SD.open(path);
    //     if (!file) return false;

    //     if (file.size() < WT_SIZE) {
    //         file.close();
    //         return false;
    //     }

    //     size_t readBytes = file.read((uint8_t*)wavetable, WT_SIZE);
    //     file.close();

    //     if (readBytes != TABLE_SIZE) return false;

    //     // updateShortName(filenames[index]);

    //     return true;
    // }

    // void changeWavetable(int delta) {
    //     if (!wtvcoReady) return;

    //     int newIndex = currentIndex + delta;

    //     // wrap
    //     if (newIndex < 0) newIndex = fileCount - 1;
    //     if (newIndex >= fileCount) newIndex = 0;

    //     if (newIndex != currentIndex) {
    //         currentIndex = newIndex;
    //         loadWavetableByIndex(currentIndex);
    //     }
    // }
};
