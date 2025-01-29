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
#include "../extern/fastapprox/fastexp.h"

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
        PARAM_NOISE_FREEZE,

        PARAM_LAST = PARAM_NOISE_FREEZE
    };
    static constexpr const char* const param_names[PARAM_LAST+1] = { // 7 char max
        "None", "Pitch", "Blend", "Volume", "SqDuty", "SR.Div", "Noise"
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

        int atn;
        ForEachChannel(ch) {
            switch (cv_dest[ch]) {
                case PARAM_PITCH:
                    pitch = In(ch);
                    break;
                case PARAM_WT_BLEND:
                    wt_blend = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 255);
                    break;
                case PARAM_ATTENUATION:
                    atn = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 100);
                    attenuation = constrain(atn, 0, 100);
                    break;
                case PARAM_PULSE_DUTY:
                    pulse_duty = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 255);
                    break;
                case PARAM_SAMPLE_RATE_DIV:
                    sample_rate_div = Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 31);
                    break;
                default: break;
            }
        }

        phase_inc = ComputePhaseIncrement(pitch);
        if (++inc_count > sample_rate_div) {
            phase += phase_inc;
            inc_count = 0;
        }

        uint8_t phase_acc_msb = (uint8_t)(phase >> (24 - pitch_range_shift));

        InterpolateSample(wavetable[OUT], (wt_sample != phase_acc_msb) ? wt_sample++ : ++wt_sample); // gurantee ui update even at low freq
        for (int w = A; w <= C; ++w) {
            if (waveform[w] == WAVE_PULSE) UpdatePulseDuty(wavetable[w], wt_sample, pulse_duty);
            if (waveform[w] == WAVE_NOISE && !noise_freeze) UpdateNoiseSample(wavetable[w], wt_sample);
        }
        InterpolateSample(wavetable[OUT], phase_acc_msb);

        Out(0, attenuation * (wavetable[OUT][phase_acc_msb] * HEMISPHERE_MAX_CV / 127) / 100);
        Out(1, attenuation * (wavetable[OUT][255-phase_acc_msb] * HEMISPHERE_MAX_CV / 127) / 100); // backwards wave
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

    void AuxButton() {
        if (menu_page == MENU_WAVETABLES) {
            switch ((Wave_Cursor)cursor) {
                case WAVEFORM_A:
                case WAVEFORM_B:
                case WAVEFORM_C:
                  {
                    const int idx = cursor - WAVEFORM_A;
                    if (waveform[idx] == WAVE_NOISE) noise_freeze = !noise_freeze;
                    else if (waveform[idx] == WAVE_RAND_STEPPED) GenerateWaveForm_RandStepped(wavetable[idx]);
                    break;
                  }
                default: break;
            }
        }
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
                    case WAVEFORM_B:
                    case WAVEFORM_C:
                      {
                        const int idx = cursor - WAVEFORM_A;
                        waveform[idx] = (WaveForms) constrain(((int)waveform[idx]) + direction, 0, WAVEFORM_COUNT-1);
                        GenerateWaveTable(idx);
                        break;
                      }
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
                        pitch = constrain(pitch + (direction * 72), 0, HEMISPHERE_MAX_INPUT_CV);
                        break;
                    case PARAM_WT_BLEND:
                        wt_blend = constrain(wt_blend + direction, 0, 255);
                        break;
                    case PARAM_ATTENUATION:
                        attenuation = constrain(attenuation + direction, 0, 100);
                        break;
                    case PARAM_PULSE_DUTY:
                        pulse_duty = constrain(pulse_duty + direction, 0, 255);
                        break;
                    case PARAM_SAMPLE_RATE_DIV:
                        sample_rate_div = constrain(sample_rate_div + direction, 0, 31);
                        break;
                    case PARAM_NOISE_FREEZE:
                        noise_freeze = constrain(noise_freeze + direction, 0, 1);
                        break;
                    default: break;
                }
                break;

            case MENU_MOD_SOURCES:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, MOD_LAST);
                    return;
                }
                switch((ModSrc_Cursor)cursor) { // PARAM_LAST-1 excludes noise latch, update if more params added
                    case MOD_CV1:
                    case MOD_CV2:
                        cv_dest[cursor - MOD_CV1] = constrain(cv_dest[cursor - MOD_CV1] + direction, 0, PARAM_LAST-1);
                        break;
                    default: break;
                }
                break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t w = 0; w < 3; ++w) {
          Pack(data, PackLocation {0 + w*8,8}, waveform[w]);
        }
        Pack(data, PackLocation {24,8}, cv_dest[0]);
        Pack(data, PackLocation {32,8}, cv_dest[1]);
        Pack(data, PackLocation {40,1}, noise_freeze);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t w = 0; w < 3; ++w) {
          waveform[w] = (WaveForms) constrain(Unpack(data, PackLocation {0 + w*8,8}), 0, WAVEFORM_COUNT);
          GenerateWaveTable(w);
        }
        cv_dest[0] = constrain(Unpack(data, PackLocation {24,8}), 0, PARAM_LAST);
        cv_dest[1] = constrain(Unpack(data, PackLocation {32,8}), 0, PARAM_LAST);
        noise_freeze = constrain(Unpack(data, PackLocation {40,1}), 0, 1);
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
        help[HELP_EXTRA2]   = "AuxBtn: Frz/Reroll";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor = 0; // WTVCO_Cursor
    int menu_page = 0;

    uint8_t cv_dest[2] = { PARAM_PITCH, PARAM_WT_BLEND };

    int16_t pitch = 0;
    int wt_blend = 127;
    uint8_t attenuation = 100;
    int pulse_duty = 127;
    bool noise_freeze = false;
    uint8_t sample_rate_div = 0;
    uint8_t pitch_range_shift = 3;

    uint32_t phase_inc = 0;
    uint32_t phase;

    static constexpr size_t WT_SIZE = 256;

    WaveForms waveform[3];
    std::array<int8_t, WT_SIZE> wavetable[4];

    uint8_t wt_sample = 0; // used to update gui even at low frequency
    uint8_t inc_count = 0; // count each time the phase increments to divide sample rate

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
        int x = 0;
        int y = HEADER_HEIGHT + Y_DIV;
        int w = X_DIV;

        switch (menu_page) {
            case MENU_WAVETABLES:
                if(!EditMode()) x = cursor * X_DIV;
                else return;
                break;
            case MENU_PARAMS:
                if (cursor == 0) break;
                if (cursor < 3) {
                  x = 36;
                  y = MENU_ROW + 8 + (cursor * Y_DIV);
                  w = 27;
                } else if (cursor < 6) {
                  x = 42;
                  y = MENU_ROW + 8 + ((cursor-2) * Y_DIV);
                  w = 21;
                } else {
                  x = 36;
                  y = MENU_ROW + 8 + ((cursor-5) * Y_DIV);
                  w = 21;
                }
                break;
            case MENU_MOD_SOURCES:
                if (cursor == 0) break;
                x = 24;
                y = MENU_ROW + 8 + (cursor * Y_DIV);
                w = 39;
                break;
            default: return;
        }
        gfxSpicyCursor(x, y, w);
    }

    void DrawWaveMenu() {
        int x = 3;
        int y = MENU_ROW;

        if (!EditMode()) {
            gfxBitmap(x+1, y, 8, WAVEFORM_ICON);
            char label[] = {'A', '\0'};
            for (int i = 0; i<3; ++i) {
              x += X_DIV;
              gfxPrint(x+2, y, label);
              ++label[0];
            }
        } else {
            switch((Wave_Cursor)cursor) {
                case WAVEFORM_A:
                case WAVEFORM_B:
                case WAVEFORM_C:
                  {
                    const int idx = cursor - WAVEFORM_A;
                    char label[] = {char('A'+idx), ':', '\0'};
                    gfxPrint(3, MENU_ROW, label);
                    gfxPrint(waveform_names[waveform[idx]]);
                    break;
                  }
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
            case WAVEFORM_B:
            case WAVEFORM_C:
                gfxRenderWave(cursor - WAVEFORM_A);
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
                gfxPrint(1, y, param_names[PARAM_PITCH]); gfxPrint(":"); gfxPrint(pitch/72);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_WT_BLEND]); gfxPrint(":"); gfxPrint(wt_blend);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_ATTENUATION]); gfxPrint(":"); gfxPrint(attenuation);
                break;
            case PARAM_ATTENUATION:
            case PARAM_PULSE_DUTY:
            case PARAM_SAMPLE_RATE_DIV:
                gfxPrint(1, y, param_names[PARAM_ATTENUATION]); gfxPrint(":"); gfxPrint(attenuation);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_PULSE_DUTY]); gfxPrint(":"); gfxPrint(pulse_duty);
                y += Y_DIV;
                gfxPrint(1, y, param_names[PARAM_SAMPLE_RATE_DIV]); gfxPrint(":"); gfxPrint(sample_rate_div);
                break;
            case PARAM_NOISE_FREEZE:
                gfxPrint(1, y, param_names[PARAM_NOISE_FREEZE]); gfxPrint(":"); gfxPrint((noise_freeze) ? "Frz" : "RT");
                y += Y_DIV;
                gfxPrint(1, y, "FrqShft:"); gfxPrint(pitch_range_shift);
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
        const int y = MENU_ROW + Y_DIV;
        ForEachChannel(ch) {
          gfxPrint(1, y + ch*Y_DIV, OC::Strings::cv_input_names[ch + io_offset]);
          gfxPrint(":");
          gfxPrint(param_names[cv_dest[0]]);
        }
    }

// WAVETABLE STUFF:
    void InterpolateSample(std::array<int8_t, WT_SIZE>& wt, uint8_t sample) {
        wt[sample] = (int8_t) ((((wt_blend <= 127) * ((127 - wt_blend) * (int)wavetable[A][sample] + wt_blend * (int)wavetable[B][sample]))
                                + ((wt_blend > 127) * ((255 - wt_blend) * (int)wavetable[B][sample] + (wt_blend - 128) * (int)wavetable[C][sample]))) / 127);
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
        const size_t steps = 5;
        const size_t stepSize = WT_SIZE / steps;
        for (size_t i = 0; i < WT_SIZE; ++i) {
            int value = (i / stepSize) * 255 / (steps - 1);
            waveform[i] = static_cast<int8_t>(value - 128);
        }
    }

    void GenerateWaveForm_RandStepped(std::array<int8_t, WT_SIZE>& waveform) {
        const size_t steps = 5;
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
            q15_t value = ((diff_squared - 32767) * 255) >> 15;
            waveform[i] = (i > 0) ? static_cast<int8_t>(value - 128) : 127;
        }
    }

    void GenerateWaveForm_ExponentialGrowth(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / (WT_SIZE - 1);
            float value = (fastexp(4.0f * t) - 1) / (fastexp(4.0f) - 1);
            waveform[i] = static_cast<int8_t>((value * 255.0f) - 128);
        }
    }

    void GenerateWaveForm_ExponentialDecay(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / (WT_SIZE - 1);
            float value = (fastexp(4.0f * (1.0f - t)) - 1) / (fastexp(4.0f) - 1);
            waveform[i] = static_cast<int8_t>((value * 255.0f) - 128);
        }
    }

    void GenerateWaveForm_Sigmoid(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / WT_SIZE;
            float scaled_t = (t - 0.5f) * 20.0f;
            float value = 1.0f / (1.0f + fastexp(-scaled_t));
            waveform[i] = static_cast<int8_t>((value * 255.0f) - 128);
        }
    }

    void GenerateWaveForm_Gaussian(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            float t = static_cast<float>(i) / (WT_SIZE - 1);
            float value = fastexp(-50.0f * (t - 0.5f) * (t - 0.5f));
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
