// Copyright (c) 2024, Beau Sterling
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <arm_math.h>
#include "../tideslite.h"

class WTVCO : public HemisphereApplet {
public:

    enum MenuPages {
        MENU_WAVETABLES,
        MENU_PARAMS,
        MENU_MOD_SOURCES,

        MENU_LAST = MENU_MOD_SOURCES
    };

    enum Wave_Cursor {
        WAVEFORM_NEXT_PAGE = 0,
        WAVEFORM_OUT = WAVEFORM_NEXT_PAGE,
        WAVEFORM_A,
        WAVEFORM_B,
        WAVEFORM_C,

        WAVEFORM_LAST = WAVEFORM_C
    };

    enum Param_Cursor {
        PARAM_NEXT_PAGE = 0,
        PARAM_PITCH,
        PARAM_WT_BLEND,
        PARAM_ATTENUATION,
        PARAM_PULSE_DUTY,
        PARAM_SAMPLE_RATE_DIV,

        PARAM_LAST = PARAM_SAMPLE_RATE_DIV
    };
    static constexpr const char* const param_names[PARAM_LAST+1] = { // 7 char max
        "None", "Pitch", "Blend", "Volume", "SqDuty", "SR.Div"
    };

    enum ModSrc_Cursor {
        MOD_NEXT_PAGE = 0,
        MOD_CV1,
        MOD_CV2,

        MOD_LAST = MOD_CV2
    };

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
        WAVE_SHARKFIN,
        WAVE_PARABOLIC,
        WAVE_EXP_GROWTH,
        WAVE_EXP_DECAY,
        WAVE_SIGMOID,
        WAVE_GAUSSIAN,

        // add more waves here and generator functions at the bottom

        WAVEFORM_COUNT
    };
    static constexpr const char* const waveform_names[WAVEFORM_COUNT] = {
      "Sine", "Triangl", "Pulse", "Saw", "Ramp", "Stepped", "RandStp", "Noise",
      "ShrkFin", "Parabla", "ExpGrth", "ExpDcay", "Sigmoid", "Gauss"
    };

    const char* applet_name() {
        return "WTVCO";
    }

    void Start() {
        waveform[A] = WAVE_SINE;
        waveform[B] = WAVE_TRIANGLE;
        waveform[C] = WAVE_PULSE;
        for (int w = A; w <= C; ++w) GenerateWaveTable(w);
    }

    void Controller() {
        if (Clock(0)) pitch_range_shift = constrain(pitch_range_shift - 1, 0, 8);
        if (Clock(1)) pitch_range_shift = constrain(pitch_range_shift + 1, 0, 8);

        ForEachChannel(ch) {
            if (Changed(ch)) {
                switch (cv_dest[ch]) {
                    case PARAM_PITCH:
                        param.pitch = In(ch);
                        break;
                    case PARAM_WT_BLEND:
                        param.wt_blend = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 255);
                        break;
                    case PARAM_ATTENUATION:
                        param.attenuation = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 127);
                        break;
                    case PARAM_PULSE_DUTY:
                        param.pulse_duty = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 255);
                        break;
                    case PARAM_SAMPLE_RATE_DIV:
                        param.sample_rate_div = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 31);
                        break;
                    default: break;
                }
            }
        }

        phase_inc = ComputePhaseIncrement(param.pitch);
        if (++inc_count > param.sample_rate_div) {
            phase += phase_inc;
            inc_count = 0;
        }

        uint8_t phase_acc_msb = (uint8_t)(phase >> (24 - pitch_range_shift));

        InterpolateSample(wavetable[OUT], (wt_sample != phase_acc_msb) ? wt_sample++ : ++wt_sample); // gurantee ui update even at low freq
        for (int w = A; w <= C; ++w) {
            if (waveform[w] == WAVE_PULSE) UpdatePulseDuty(wavetable[w], wt_sample, param.pulse_duty);
            if (waveform[w] == WAVE_NOISE) UpdateNoiseSample(wavetable[w], wt_sample);
        }
        InterpolateSample(wavetable[OUT], phase_acc_msb);

        Out(0, param.attenuation * (wavetable[OUT][phase_acc_msb] * HEMISPHERE_MAX_CV / 127) / 127);
    }

    void View() {
        switch (menu_page) {
            case MENU_WAVETABLES:
                DrawWaveMenu();
                DrawWaveForm();
                break;
            case MENU_PARAMS:
                DrawParamMenu();
                DrawParams();
                break;
            case MENU_MOD_SOURCES:
                DrawModSourceMenu();
                DrawModSources();
                break;
            default: break;
        }
        DrawSelector();
    }

    void OnButtonPress() {
        if (cursor == 0) menu_page = (++menu_page > MENU_LAST) ? 0 : menu_page;
        else CursorToggle();
    }

    void OnEncoderMove(int direction) {
        switch (menu_page) {
            case MENU_WAVETABLES:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, WAVEFORM_LAST);
                    return;
                }
                switch((Wave_Cursor)cursor) {
                    case WAVEFORM_A:
                        waveform[A] = (WaveForms) constrain(((int)waveform[A]) + direction, 0, WAVEFORM_COUNT-1);
                        GenerateWaveTable(A);
                        break;
                    case WAVEFORM_B:
                        waveform[B] = (WaveForms) constrain(((int)waveform[B]) + direction, 0, WAVEFORM_COUNT-1);
                        GenerateWaveTable(B);
                        break;
                    case WAVEFORM_C:
                        waveform[C] = (WaveForms) constrain(((int)waveform[C]) + direction, 0, WAVEFORM_COUNT-1);
                        GenerateWaveTable(C);
                        break;
                    default: break;
                }
                break;

            case MENU_PARAMS:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, PARAM_LAST);
                    return;
                }
                switch((Param_Cursor)cursor) {
                    case PARAM_PITCH:
                        param.pitch = constrain(param.pitch + (direction * 72), 0, HEMISPHERE_MAX_INPUT_CV);
                        break;
                    case PARAM_WT_BLEND:
                        param.wt_blend = constrain(param.wt_blend + direction, 0, 255);
                        break;
                    case PARAM_ATTENUATION:
                        param.attenuation = constrain(param.attenuation + direction, 0, 127);
                        break;
                    case PARAM_PULSE_DUTY:
                        param.pulse_duty = constrain(param.pulse_duty + direction, 0, 255);
                        break;
                    case PARAM_SAMPLE_RATE_DIV:
                        param.sample_rate_div = constrain(param.sample_rate_div + direction, 0, 31);
                        break;

                    default: break;
                }
                break;

            case MENU_MOD_SOURCES:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, MOD_LAST);
                    return;
                }
                switch((ModSrc_Cursor)cursor) {
                    case MOD_CV1:
                        cv_dest[0] = constrain(cv_dest[0] + direction, 0, PARAM_LAST);
                        break;
                    case MOD_CV2:
                        cv_dest[1] = constrain(cv_dest[1] + direction, 0, PARAM_LAST);
                        break;
                    default: break;
                }
                break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0,8}, waveform[0]);
        Pack(data, PackLocation { 8,8}, waveform[1]);
        Pack(data, PackLocation {16,8}, waveform[2]);
        Pack(data, PackLocation {24,8}, cv_dest[0]);
        Pack(data, PackLocation {32,8}, cv_dest[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        waveform[0] = (WaveForms) Unpack(data, PackLocation { 0,8});
        waveform[1] = (WaveForms) Unpack(data, PackLocation { 8,8});
        waveform[2] = (WaveForms) Unpack(data, PackLocation {16,8});
        cv_dest[0] = Unpack(data, PackLocation {24,8});
        cv_dest[1] = Unpack(data, PackLocation {32,8});
        for (int w = 0; w < 3; ++w) GenerateWaveTable(w);
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "OctDn";
        help[HELP_DIGITAL2] = "OctUp";
        help[HELP_CV1]      = param_names[cv_dest[0]];
        help[HELP_CV2]      = param_names[cv_dest[1]];
        help[HELP_OUT1]     = "OscOut";
        help[HELP_OUT2]     = "";
        help[HELP_EXTRA1]   = "Encoder: Select/Edit";
        help[HELP_EXTRA2]   = "AuxBtn: N.A.";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor = 0; // WTVCO_Cursor
    int menu_page = 0;

    uint8_t cv_dest[2] = { PARAM_PITCH, PARAM_WT_BLEND };

    struct Param {
        int16_t pitch = 0;
        int wt_blend = 127;
        uint8_t attenuation = 127;
        int pulse_duty = 127;
        uint8_t sample_rate_div = 0;
    } param;

    uint32_t phase_inc = 0;
    uint32_t phase;

    static constexpr size_t WT_SIZE = 256;

    WaveForms waveform[3];
    std::array<int8_t, WT_SIZE> wavetable[4];

    uint8_t wt_sample = 0; // used to update gui even at low frequency
    uint8_t inc_count = 0; // count each time the phase increments to divide sample rate

    uint8_t pitch_range_shift = 3;

// GRAPHIC STUFF:
    static constexpr int HEADER_HEIGHT = 11;
    static constexpr int X_DIV = 64 / 4;
    static constexpr int MENU_ROW = 14;
    static constexpr int Y_DIV = (64 - HEADER_HEIGHT) / 4;

    void gfxRenderWave(int w) {
        for (size_t x = 0; x < WT_SIZE; x+=4) {
            int y = 44 - Proportion(wavetable[w][x], 127, 16);
            gfxPixel(x/4, y);
        }
    }

    void DrawSelector() {
        switch (menu_page) {
            case MENU_WAVETABLES:
                if(!EditMode()) gfxSpicyCursor((cursor * X_DIV), HEADER_HEIGHT + Y_DIV, X_DIV);
                break;
            case MENU_PARAMS:
                if (cursor == 0) gfxSpicyCursor(0, HEADER_HEIGHT + Y_DIV, X_DIV);
                else if (cursor < 3) gfxSpicyCursor(36, MENU_ROW + 8 + (cursor * Y_DIV), 27);
                else gfxSpicyCursor(42, MENU_ROW + 8 + ((cursor-2) * Y_DIV), 21);
                break;
            case MENU_MOD_SOURCES:
                if (cursor == 0) gfxSpicyCursor(0, HEADER_HEIGHT + Y_DIV, X_DIV);
                else gfxSpicyCursor(24, MENU_ROW + 8 + (cursor * Y_DIV), 39);
                break;
            default: break;
        }
    }

    void DrawWaveMenu() {
        int x = 3;
        int y = MENU_ROW;

        if (!EditMode()) {
            gfxBitmap(x+1, y, 8, WAVEFORM_ICON);
            x += X_DIV;
            gfxPrint(x+2, y, "A");
            x += X_DIV;
            gfxPrint(x+2, y, "B");
            x += X_DIV;
            gfxPrint(x+2, y, "C");
        } else {
            switch((Wave_Cursor)cursor) {
                case WAVEFORM_A:
                    gfxPrint(3, MENU_ROW, "A:");
                    gfxPrint(waveform_names[waveform[A]]);
                    break;
                case WAVEFORM_B:
                    gfxPrint(3, MENU_ROW, "B:");
                    gfxPrint(waveform_names[waveform[B]]);
                    break;
                case WAVEFORM_C:
                    gfxPrint(3, MENU_ROW, "C:");
                    gfxPrint(waveform_names[waveform[C]]);
                    break;
                default: break;
            }
        }

        gfxLine(0, y+11, 63, y+11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawWaveForm() {
        switch((Wave_Cursor)cursor) {
            case WAVEFORM_OUT:
                gfxRenderWave(OUT);
                break;
            case WAVEFORM_A:
                gfxRenderWave(A);
                break;
            case WAVEFORM_B:
                gfxRenderWave(B);
                break;
            case WAVEFORM_C:
                gfxRenderWave(C);
                break;
            default: break;
        }
    }

    void DrawParamMenu() {
        int x = 3;
        int y = MENU_ROW;

        gfxBitmap(x+1, y, 8, EDIT_ICON);
        x += X_DIV;
        gfxPrint(x, y, "Params");

        gfxLine(0, y+11, 63, y+11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawParams() {
        int y = MENU_ROW + Y_DIV;

        switch ((Param_Cursor)cursor) {
            case PARAM_NEXT_PAGE:
            case PARAM_PITCH:
            case PARAM_WT_BLEND:
                gfxPrint(1, y, param_names[PARAM_PITCH]); gfxPrint(":"); gfxPrint(param.pitch/72);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_WT_BLEND]); gfxPrint(":"); gfxPrint(param.wt_blend);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_ATTENUATION]); gfxPrint(":"); gfxPrint(param.attenuation);
                break;
            case PARAM_ATTENUATION:
            case PARAM_PULSE_DUTY:
            case PARAM_SAMPLE_RATE_DIV:
                gfxPrint(1, y, param_names[PARAM_ATTENUATION]); gfxPrint(":"); gfxPrint(param.attenuation);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_PULSE_DUTY]); gfxPrint(":"); gfxPrint(param.pulse_duty);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_SAMPLE_RATE_DIV]); gfxPrint(":"); gfxPrint(param.sample_rate_div);
                break;
            default: break;
        }
    }

    void DrawModSourceMenu() {
        int x = 3;
        int y = MENU_ROW;

        gfxBitmap(x+1, y, 8, ZAP_ICON);
        x += X_DIV;
        gfxPrint(x-2, y, "Mod Src");

        gfxLine(0, y+11, 63, y+11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawModSources() {
        int y = MENU_ROW + Y_DIV;

        gfxPrint(1, y, "CV1"); gfxPrint(":"); gfxPrint(param_names[cv_dest[0]]);
        y += Y_DIV;
        gfxPrint(1, y, "CV2"); gfxPrint(":"); gfxPrint(param_names[cv_dest[1]]);
    }

// WAVETABLE STUFF:
    void InterpolateSample(std::array<int8_t, WT_SIZE>& wt, uint8_t sample) {
        wt[sample] = (int8_t) ((((param.wt_blend <= 127) * ((127 - param.wt_blend) * (int)wavetable[A][sample] + param.wt_blend * (int)wavetable[B][sample]))
                                + ((param.wt_blend > 127) * ((255 - param.wt_blend) * (int)wavetable[B][sample] + (param.wt_blend - 128) * (int)wavetable[C][sample]))) / 127);
    }

    void UpdatePulseDuty(std::array<int8_t, WT_SIZE>& wt, uint8_t sample, uint8_t duty) {
        wt[sample] = (sample < duty) ? 127 : -128;
    }

    void UpdateNoiseSample(std::array<int8_t, WT_SIZE>& wt, uint8_t sample) {
        wt[sample] = static_cast<int8_t>(random(-128, 127));
    }

    void GenerateWaveTable(int w) {
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
            case WAVE_SHARKFIN:
                GenerateWaveForm_Sharkfin(wavetable[w]);
                break;
            case WAVE_PARABOLIC:
                GenerateWaveForm_Parabolic(wavetable[w]);
                break;
            case WAVE_EXP_GROWTH:
                GenerateWaveForm_ExponentialGrowth(wavetable[w]);
                break;
            case WAVE_EXP_DECAY:
                GenerateWaveForm_ExponentialDecay(wavetable[w]);
                break;
            case WAVE_SIGMOID:
                GenerateWaveForm_Sigmoid(wavetable[w]);
                break;
            case WAVE_GAUSSIAN:
                GenerateWaveForm_Gaussian(wavetable[w]);
                break;
            // add waves here
            default: break;
        }
    }


// standard waves
    void GenerateWaveForm_Sine(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            q15_t t = static_cast<q15_t>(i * 32768 / WT_SIZE);
            waveform[i] = arm_sin_q15(t) >> 8;
        }
    }

    void GenerateWaveForm_Triangle(std::array<int8_t, WT_SIZE>& waveform) {
        int sign = 1;
        int8_t value = 0;
        for (size_t i = 0; i < WT_SIZE; ++i) {
            waveform[i] = value * 255 / 128;
            if (i < (WT_SIZE / 4) || (i >= (3 * WT_SIZE / 4))) sign = 1;
            else sign = -1;
            value = value + sign;
        }
    }

    void GenerateWaveForm_Pulse(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            waveform[i] = (i < WT_SIZE / 2) ? 127 : -128;
        }
    }

    void GenerateWaveForm_Sawtooth(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int16_t value = ((WT_SIZE - i - 1) * 256) / WT_SIZE;
            waveform[i] = static_cast<int8_t>(value - 128);
        }
    }

    void GenerateWaveForm_Ramp(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int16_t value = (i * 256) / WT_SIZE;
            waveform[i] = static_cast<int8_t>(value - 128);
        }
    }

    void GenerateWaveForm_Stepped(std::array<int8_t, WT_SIZE>& waveform) {
        const size_t steps = 4;
        const size_t stepSize = WT_SIZE / 4;
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int stepValue = (i / stepSize) * 255 / (steps - 1);
            waveform[i] = static_cast<int8_t>(stepValue - 128);
        }
    }

    void GenerateWaveForm_RandStepped(std::array<int8_t, WT_SIZE>& waveform) {
        const size_t steps = 4;
        const size_t stepSize = WT_SIZE / steps;
        int step = 0;
        int value = random(-128, 127);
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int stepValue = (i / stepSize) * 255 / (steps - 1);
            if (step != stepValue) {
                step = stepValue;
                value = random(-128, 127);
            }
            waveform[i] = static_cast<int8_t>(value);
        }
    }

    void GenerateWaveForm_Noise(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            waveform[i] = static_cast<int8_t>(random(-128, 127));
        }
    }

    void GenerateWaveForm_Sharkfin(std::array<int8_t, WT_SIZE>& waveform) {
        q15_t t;
        int8_t value;
        for (size_t i = 0; i < WT_SIZE; ++i) {
            if (i < 128) {
                t = static_cast<q15_t>(i * 16384 / WT_SIZE);
                value = (arm_sin_q15(t) >> 7);
            } else {
                t = static_cast<q15_t>((i - 128) * 16384 / WT_SIZE);
                value = 255 - (arm_sin_q15(t) >> 7);
            }
            waveform[i] = (value - 128);
        }
    }

    void GenerateWaveForm_Parabolic(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            q15_t t = static_cast<q15_t>((i * 32768) / WT_SIZE);
            q15_t diff_squared = ((t - 16384) * (t - 16384)) >> 13;
            q15_t parabolic_value = ((diff_squared - 32767) * 255) >> 15;
            waveform[i] = (i > 0) ? static_cast<int8_t>(parabolic_value - 128) : 127;
        }
    }

    void GenerateWaveForm_ExponentialGrowth(std::array<int8_t, WT_SIZE>& waveform) { // too steppy
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int32_t exponent = (i * 9) / WT_SIZE;
            int32_t value = (1 << exponent);
            value = (value > 255) ? 255 : value;
            waveform[i] = static_cast<int8_t>(value - 128);
        }
    }

    void GenerateWaveForm_ExponentialDecay(std::array<int8_t, WT_SIZE>& waveform) { // too steppy
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int32_t exponent = (i * 12) / WT_SIZE;
            int32_t value = 256 / (1 << exponent);
            value = (value > 255) ? 255 : value;
            waveform[i] = static_cast<int8_t>(value - 128);
        }
    }

    void GenerateWaveForm_Sigmoid(std::array<int8_t, WT_SIZE>& waveform) { // git rid of float and std::
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / WT_SIZE;
            float scaled_t = (t - 0.5f) * 20.0f;
            float sigmoid_val = 1.0f / (1.0f + exp(-scaled_t));
            waveform[i] = static_cast<int8_t>((sigmoid_val * 255.0f) - 128);
        }
    }

    void GenerateWaveForm_Gaussian(std::array<int8_t, WT_SIZE>& waveform) { // git rid of float and std::
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / (WT_SIZE - 1.0f);
            float value = std::exp(-50.0f * (t - 0.5f) * (t - 0.5f));
            waveform[i] = static_cast<int8_t>(255 * value - 128);
        }
    }





// MOAR WAVEZ
// the wave generator functions below were created by ChatGPT
// and have not all been tested. its on the TODO list.

// seems like lots are redundant, or useless. still need to go through the rest.



// steps

    // void GenerateWaveForm_SinusoidalStep(std::array<int8_t, WT_SIZE>& waveform) {
    //     for (size_t i = 0; i < WT_SIZE; ++i) {
    //         waveform[i] = static_cast<int8_t>(127 * std::round(std::sin(TWO_PI * i / size)));
    //     }
    // }

    // void GenerateWaveForm_WavyStaircase(std::array<int8_t, WT_SIZE>& waveform) {
    //     size_t stepSize = WT_SIZE / 8;
    //     for (size_t i = 0; i < WT_SIZE; ++i) {
    //         size_t step = i / stepSize;
    //         waveform[i] = static_cast<int8_t>(127 * std::sin(TWO_PI * step / 8));
    //     }

    //         q15_t t = static_cast<q15_t>(i * 32767 / WT_SIZE);
    //         waveform[i] = arm_sin_q15(t) >> 8;
    // }



// noise

    // void GenerateWaveForm_GaussianNoise(std::array<int8_t, WT_SIZE>& waveform, const size_t size) {
    //     std::normal_distribution<> dist(0, 50); // Mean 0, std deviation 50
    //     for (size_t i = 0; i < size; ++i) {
    //         waveform[i] = static_cast<int8_t>(std::clamp<int>(dist(random()), -128, 127));
    //     }
    // }



// exponential



// modified sinusoids

//     void GenerateWaveForm_OffsetSine(std::array<int8_t, WT_SIZE>& waveform, double offset = 0.5) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             waveform[i] = static_cast<int8_t>(127 * (std::sin(TWO_PI * t) + offset));
//         }
//     }

//     void GenerateWaveForm_OffsetSine2(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double value = 0.5 + 0.5 * std::sin(2 * M_PI * t); // Sine wave offset to [0, 1]
//             waveform[i] = static_cast<int8_t>(127 * value - 64); // Offset to [-64, 63]
//         }
//     }

//     void GenerateWaveForm_Cosine(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             waveform[i] = static_cast<int8_t>(127 * std::cos(TWO_PI * t));
//         }
//     }

//     void GenerateWaveForm_SinusoidalAM(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double value = std::sin(2 * M_PI * t) * (0.5 + 0.5 * std::sin(4 * M_PI * t)); // Sine modulated by sine
//             waveform[i] = static_cast<int8_t>(127 * value);
//         }
//     }

//     void GenerateWaveForm_ClippedSine(std::array<int8_t, WT_SIZE>& waveform, double clip = 0.5) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double sineValue = std::sin(TWO_PI * t);
//             sineValue = std::clamp(sineValue, -clip, clip);
//             waveform[i] = static_cast<int8_t>(127 * sineValue);
//         }
//     }

//     void GenerateWaveForm_StretchedSine(std::array<int8_t, WT_SIZE>& waveform, double frequency = 2.0) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             waveform[i] = static_cast<int8_t>(127 * std::sin(TWO_PI * frequency * t));
//         }
//     }

//     void GenerateWaveForm_FullRectifiedSine(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             waveform[i] = static_cast<int8_t>(127 * std::fabs(std::sin(TWO_PI * t)));
//         }
//     }

//     void GenerateWaveForm_HalfRectifiedSine(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double sineValue = std::sin(TWO_PI * t);
//             waveform[i] = static_cast<int8_t>(127 * std::max((int)sineValue, 0));
//         }
//     }

//     void GenerateWaveForm_HalfSine(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double value = std::sin(M_PI * t); // Half sine wave
//             waveform[i] = static_cast<int8_t>(127 * value);
//         }
//     }



// // geometric



// // probably harsh

//     void GenerateWaveForm_Ripple(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             double t = static_cast<double>(i) / WT_SIZE;
//             double value = std::sin(2 * M_PI * t) + 0.3 * std::sin(10 * M_PI * t);
//             waveform[i] = static_cast<int8_t>(127 * value);
//         }
//     }

//     void GenerateWaveForm_Zigzag(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             waveform[i] = static_cast<int8_t>((i % 64) - 32);
//         }
//     }

//     void GenerateWaveForm_Spike(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; i += WT_SIZE / 8) {
//             waveform[i] = 127; // Set spikes periodically
//         }
//     }

//     void GenerateWaveForm_Alternating(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             waveform[i] = (i % 32 < 16) ? 127 : -128;
//         }
//     }

//     void GenerateWaveForm_SquarePulseTrain(std::array<int8_t, WT_SIZE>& waveform) {
//         for (size_t i = 0; i < WT_SIZE; ++i) {
//             waveform[i] = ((i / 16) % 2 == 0) ? 127 : -128;
//         }
//     }

    // add more wave generators here, and add to enum

};
