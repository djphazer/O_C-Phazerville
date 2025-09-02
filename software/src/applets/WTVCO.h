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
        MENU_MOD_DEST,

        MENU_LAST = MENU_MOD_DEST
    };

    enum Wave_Cursor {
        WAVEFORM_OUT = 0,
        WAVEFORM_A,
        WAVEFORM_B,
        WAVEFORM_C,

        WAVEFORM_LAST = WAVEFORM_C
    };

    enum Param_Cursor {
        PARAM_NONE = 0,
        PARAM_PITCH_1,
        PARAM_PITCH_2,
        PARAM_WT_BLEND,
        PARAM_OSC2_DIRECTION,
        PARAM_ATTENUATION,
        PARAM_PULSE_DUTY,
        PARAM_SAMPLE_RATE_DIV,

        PARAM_LAST = PARAM_SAMPLE_RATE_DIV
    };
    static constexpr const char* const param_names[PARAM_LAST+1] = { // 7 char max
        "None", "Pitch", "", "Blend", "", "Volume", "SqDuty", "SR.Div"
    };

    enum ModDest_Cursor {
        MOD_CV1 = 1,
        MOD_CV2,

        MOD_LAST = MOD_CV2
    };

    const int CURSOR_LAST = WAVEFORM_LAST + PARAM_LAST + MOD_LAST;

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
    const uint8_t* applet_icon() { return PhzIcons::trending; }

    void Start() {
        waveform[A] = WAVE_SINE;
        waveform[B] = WAVE_TRIANGLE;
        waveform[C] = WAVE_PULSE;
        for (int w = A; w <= C; ++w) GenerateWaveTable(w);
    }

    void Controller() {
        ForEachChannel(ch) {
            if (Clock(0)) pitch[ch] = constrain(pitch[ch] - (128 * 12), MIN_PITCH, MAX_PITCH); // shift down 1 octave
            if (Clock(1)) pitch[ch] = constrain(pitch[ch] + (128 * 12), MIN_PITCH, MAX_PITCH); // shift up 1 octave
        }

        int16_t _pitch[2] = { pitch[0], pitch[1] };
        int _wt_blend = wt_blend;
        uint8_t _attenuation = attenuation;
        uint8_t _pulse_duty = pulse_duty;
        uint8_t _sample_rate_div = sample_rate_div;

        ForEachChannel(ch) {
            switch (cv_dest[ch]) {
                case PARAM_PITCH_1:
                case PARAM_PITCH_2:
                    if (pitch_cv_link) { // only weird thing here is if both are set to pitch, In(1) overrides In(0) when linked
                        _pitch[0] = pitch[0] + In(ch);
                        _pitch[1] = pitch[1] + In(ch);
                    } else _pitch[ch] = pitch[ch] + In(ch);
                    break;
                case PARAM_WT_BLEND:
                    _wt_blend = wt_blend + Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, WT_SIZE - 1);  // CV input < 0V or > 5V causes wavefolding
                    break;
                case PARAM_ATTENUATION:
                    _attenuation = constrain(attenuation + Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, 100), 0, 100);
                    break;
                case PARAM_PULSE_DUTY:
                    _pulse_duty = pulse_duty + Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, WT_SIZE - 1);
                    break;
                case PARAM_SAMPLE_RATE_DIV:
                    _sample_rate_div = constrain(sample_rate_div + Proportion(In(ch), 5 * HEMISPHERE_MAX_INPUT_CV / 6, SR_DIV_LIMIT), 0, SR_DIV_LIMIT);
                    break;
                default: break;
            }
        }

        if (++inc_count > _sample_rate_div) {
            ForEachChannel(ch) { phase[ch] += ComputePhaseIncrement(_pitch[ch]); }
            inc_count = 0;
        }

        uint8_t phase_acc_msb[2];
        ForEachChannel(ch) { phase_acc_msb[ch] = (uint8_t)(phase[ch] >> 24); }

        InterpolateSample(wavetable[OUT], _wt_blend, (wt_sample != phase_acc_msb[0]) ? wt_sample++ : ++wt_sample); // gurantee ui update even at low freq
        for (int w = A; w <= C; ++w) {
            if (waveform[w] == WAVE_PULSE) UpdatePulseDuty(wavetable[w], wt_sample, _pulse_duty);
            if (waveform[w] == WAVE_NOISE && !noise_freeze) UpdateNoiseSample(wavetable[w], wt_sample);
        }
        InterpolateSample(wavetable[OUT], _wt_blend, phase_acc_msb[0]);

#if defined(VOR)
        int16_t MAX_AMPLITUDE = HEMISPHERE_MAX_CV;
#else
        int16_t MAX_AMPLITUDE = -HEMISPHERE_MIN_CV; // prevents clipping and fixes amplitude asymmetery for non-VOR O_Cs
#endif

        Out(0, _attenuation * (wavetable[OUT][phase_acc_msb[0]] * MAX_AMPLITUDE / 127) / 100);
        Out(1, _attenuation * (wavetable[OUT][
            phase_acc_msb[1] * (1 - 2 * osc2_rev) + (255 * osc2_rev) // osc2 waveform can be reversed
        ] * MAX_AMPLITUDE / 127) / 100);
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
            case MENU_MOD_DEST:
                DrawCVDestMenu();
                DrawCVDestinations();
                break;
            default: break;
        }
        DrawSelector();
    }

    void AuxButton() {
        switch(menu_page) {
            case MENU_WAVETABLES: {
                if (page_cursor > WAVEFORM_OUT) {
                    const int idx = page_cursor - WAVEFORM_A;
                    if (waveform[idx] == WAVE_NOISE) noise_freeze = !noise_freeze; // toggle "realtime" or frozen noise wave buffer
                    else if (waveform[idx] == WAVE_RAND_STEPPED) GenerateWaveForm_RandStepped(wavetable[idx]); // re-roll random step wave
                }
                break;}
            case MENU_PARAMS:
                if (page_cursor == PARAM_PITCH_1 || page_cursor == PARAM_PITCH_2)
                    pitch_cv_link = !pitch_cv_link; // toggle pitch_cv_link
        }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            if (cursor > PARAM_LAST + WAVEFORM_LAST) {
                menu_page = MENU_MOD_DEST;
            } else if (cursor > WAVEFORM_LAST) {
                menu_page = MENU_PARAMS;
            } else {
                menu_page = MENU_WAVETABLES;
            }
            page_cursor = cursor - (menu_page > MENU_WAVETABLES) * WAVEFORM_LAST - (menu_page > MENU_PARAMS) * PARAM_LAST;
            return;
        }


        switch (menu_page) {
            case MENU_WAVETABLES:
                switch(page_cursor) {
                    case WAVEFORM_OUT:
                        wt_blend = constrain(wt_blend + direction, 0, (uint8_t)(WT_SIZE - 1));
                        break;
                    case WAVEFORM_A:
                    case WAVEFORM_B:
                    case WAVEFORM_C:
                    {
                        const int idx = page_cursor - WAVEFORM_A;
                        waveform[idx] = (WaveForms) constrain(((int)waveform[idx]) + direction, 0, WAVEFORM_COUNT - 1);
                        GenerateWaveTable(idx);
                        break;
                    }
                    default: break;
                }
                break;

            case MENU_PARAMS:
                switch(page_cursor) {
                    case PARAM_PITCH_1:
                        pitch[0] = constrain(pitch[0] + (direction * 128), MIN_PITCH, MAX_PITCH);
                        break;
                    case PARAM_PITCH_2:
                        pitch[1] = constrain(pitch[1] + (direction * 128), MIN_PITCH, MAX_PITCH);
                        break;
                    case PARAM_WT_BLEND:
                        wt_blend = constrain(wt_blend + direction, 0, (uint8_t)(WT_SIZE - 1));
                        break;
                    case PARAM_OSC2_DIRECTION:
                        osc2_rev = !osc2_rev;
                        break;
                    case PARAM_ATTENUATION:
                        attenuation = constrain(attenuation + direction, 0, 100);
                        break;
                    case PARAM_PULSE_DUTY:
                        pulse_duty = constrain(pulse_duty + direction, 0, (uint8_t)(WT_SIZE - 1));
                        break;
                    case PARAM_SAMPLE_RATE_DIV:
                        sample_rate_div = constrain(sample_rate_div + direction, 0, SR_DIV_LIMIT);
                        break;
                    default: break;
                }
                break;

            case MENU_MOD_DEST:
                int8_t c, d;
                switch(page_cursor) {
                    case MOD_CV1:
                    case MOD_CV2:
                        c = page_cursor - MOD_CV1;
                        d = cv_dest[c] + direction; // skip Pitch2 and Osc2Dir as mod destinations
                        cv_dest[c] = constrain(cv_dest[c] + direction * (1 + (d == PARAM_PITCH_2) + (d == PARAM_OSC2_DIRECTION)), 0, PARAM_LAST);
                        break;
                    default: break;
                }
                break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, (uint8_t)(pitch[0] / 128)); // only need to store semitone, can unpack to full pitch value
        Pack(data, PackLocation {8,8}, (uint8_t)(pitch[1] / 128)); // unsigned cast fixes packing of negative pitch values
        Pack(data, PackLocation {16,8}, (uint8_t)wt_blend);
        // Pack(data, PackLocation {24,7}, attenuation);
        // Pack(data, PackLocation {31,8}, pulse_duty);
        // Pack(data, PackLocation {39,6}, sample_rate_div);
        Pack(data, PackLocation {24,3}, cv_dest[0]);
        Pack(data, PackLocation {27,3}, cv_dest[1]);
        Pack(data, PackLocation {30,1}, noise_freeze); // noise_freeze
        Pack(data, PackLocation {31,1}, osc2_rev); // osc2_rev
        Pack(data, PackLocation {32,1}, pitch_cv_link); // pitch_cv_link
        // empty bits here
        for (size_t w = 0; w < 3; ++w) {
            Pack(data, PackLocation {46 + w*6, 6}, waveform[w]); // store up to 32 waves
        }
        return data;
    }//

    void OnDataReceive(uint64_t data) {
        pitch[0] = constrain(128 * (int8_t)Unpack(data, PackLocation {0,8}), MIN_PITCH, MAX_PITCH);
        pitch[1] = constrain(128 * (int8_t)Unpack(data, PackLocation {8,8}), MIN_PITCH, MAX_PITCH);
        wt_blend = constrain((int)Unpack(data, PackLocation{16,8}), 0, (int)(WT_SIZE - 1));
        // attenuation = constrain(Unpack(data, PackLocation {24,7}), 0, 100);
        // pulse_duty = constrain(Unpack(data, PackLocation {31,8}), 0, 255);
        // sample_rate_div = constrain(Unpack(data, PackLocation {39,6}), 0, SR_DIV_LIMIT);
        cv_dest[0] = constrain(Unpack(data, PackLocation {24,3}), 0, PARAM_LAST);
        cv_dest[1] = constrain(Unpack(data, PackLocation {27,3}), 0, PARAM_LAST);
        noise_freeze = (uint8_t)(constrain(Unpack(data, PackLocation {30,1}), 0, 1));
        osc2_rev = (uint8_t)(constrain(Unpack(data, PackLocation {31,1}), 0, 1));
        pitch_cv_link = (uint8_t)(constrain(Unpack(data, PackLocation {32,1}), 0, 1));
        // empty bits here
        for (size_t w = 0; w < 3; ++w) {
            waveform[w] = (WaveForms) constrain(Unpack(data, PackLocation {46 + w*6, 6}), 0, WAVEFORM_COUNT - 1); // store up to 32 waves
            GenerateWaveTable(w);
        }
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "OctDn";
        help[HELP_DIGITAL2] = "OctUp";
        help[HELP_CV1]      = param_names[cv_dest[0]];
        help[HELP_CV2]      = param_names[cv_dest[1]];
        help[HELP_OUT1]     = "Osc";
        help[HELP_OUT2]     = "Osc2";
        help[HELP_EXTRA1]   = "Encoder: Select/Edit";
        help[HELP_EXTRA2]   = "AuxBtn: Frz/Reroll";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor = 0; // WTVCO_Cursor
    int menu_page = 0;
    int page_cursor = 0;

    static constexpr int16_t MAX_PITCH = 16256;
    static constexpr int16_t MIN_PITCH = -16384;
    static constexpr size_t WT_SIZE = 256;
    static constexpr uint8_t SR_DIV_LIMIT = 24;

    int16_t pitch[2] = { 36*128, 48*128 }; // default base pitches of C2, and C3
    int wt_blend = 0;
    uint8_t attenuation = 100;
    uint8_t pulse_duty = 127;
    uint8_t sample_rate_div = 0;

    bool noise_freeze = false;
    bool osc2_rev = false;
    bool pitch_cv_link = false;

    uint8_t cv_dest[2] = { PARAM_PITCH_1, PARAM_WT_BLEND };

    uint32_t phase[2];

    WaveForms waveform[3];
    std::array<int8_t, WT_SIZE> wavetable[4]; // tried heap allocation but had intermittent hanging issues with T3.2 (fine on T4.1)

    uint8_t wt_sample = 0; // used to update gui even at low frequency
    uint8_t inc_count = 0; // count each time the phase increments to divide sample rate

// GRAPHIC STUFF:
    static constexpr uint8_t HEADER_HEIGHT = 11;
    static constexpr uint8_t X_DIV = 64 / 4;
    static constexpr uint8_t MENU_ROW = 14;
    static constexpr uint8_t Y_DIV = (64 - HEADER_HEIGHT) / 4;

    void gfxRenderWave(const int w) {
        for (size_t x = 0; x < WT_SIZE; x += 4) {
            uint8_t y = 44 - Proportion(wavetable[w][x], 127, 16);
            gfxPixel(x / 4, y);
        }
    }

    void DrawSelector() {
        uint8_t w = X_DIV;
        uint8_t x = 0;
        uint8_t y = HEADER_HEIGHT + Y_DIV;

        switch (menu_page) {
            case MENU_WAVETABLES:
                if(!EditMode()) x = page_cursor * X_DIV;
                else return;
                break;
            case MENU_PARAMS:
                if (page_cursor == 0) break;
                w = 19;
                x = 44 - (page_cursor % 2) * 32;
                if (page_cursor == PARAM_LAST) x += 32;
                y = MENU_ROW + 8 + Y_DIV * (1 + (page_cursor > PARAM_PITCH_2) + (page_cursor > PARAM_OSC2_DIRECTION));
                break;
            case MENU_MOD_DEST:
                if (page_cursor == 0) break;
                w = 39;
                x = 24;
                y = MENU_ROW + 8 + (page_cursor * Y_DIV);
                break;
            default: return;
        }
        gfxCursor(x, y, w);
    }

    void DrawBlendicator(int b) {
        const uint8_t y = HEADER_HEIGHT;
        const uint8_t h = 1;

        uint8_t x =  1 + X_DIV * (1 + (b / 128)) + ((b / 64) % 2) * Proportion(b - (64 * (b / 64)), 63, X_DIV);
        uint8_t w = -1 + X_DIV * (1 + ((b / 64) % 2)) + ((!((b / 64) % 2) * 2) - 1) * Proportion(b - (64 * (b / 64)), 63, X_DIV);

        gfxRect(x, y, w, h);
    }

    void DrawWaveMenu() {
        uint8_t x = 3;
        uint8_t y = MENU_ROW;

        if (!EditMode() || page_cursor == WAVEFORM_OUT) {
            gfxBitmap(x + 1, y, 8, WAVEFORM_ICON);
            char label[] = {'A', '\0'};
            for (int i = 0; i < 3; ++i) {
                x += X_DIV;
                gfxPrint(x + 2, y, label);
                ++label[0];
            }
            if (page_cursor == WAVEFORM_OUT) DrawBlendicator(wt_blend);
        } else {
            switch(page_cursor) {
                case WAVEFORM_A:
                case WAVEFORM_B:
                case WAVEFORM_C:
                {
                    const int idx = page_cursor - WAVEFORM_A;
                    char label[] = {char('A'+idx), ':', '\0'};
                    gfxPrint(3, MENU_ROW, label);
                    gfxPrint(waveform_names[waveform[idx]]);
                    break;
                }
                default: break;
            }
        }

        gfxLine(0, y + 11, 63, y + 11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawWaveForm() {
        switch(page_cursor) {
            case WAVEFORM_OUT:
                gfxRenderWave(OUT);
                break;
            case WAVEFORM_A:
            case WAVEFORM_B:
            case WAVEFORM_C:
                gfxRenderWave(page_cursor - WAVEFORM_A);
                break;
            default: break;
        }
    }

    void DrawParamMenu() {
        uint8_t x = 3;
        uint8_t y = MENU_ROW;

        gfxBitmap(x + 1, y, 8, EDIT_ICON);
        x += X_DIV;
        gfxPrint(x, y, "Params");

        gfxLine(0, y + 11, 63, y + 11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawParams() {
        uint8_t y = MENU_ROW + Y_DIV;

        // ForEachChannel(ch) {
        gfxIcon(0, y, NOTE_ICON); gfxBitmap(6, y + 6, 3, SUP_ONE); // Pitch 1
        gfxPos(6, y); graphics.printf("%4d", pitch[0] / 128);

        gfxIcon(32, y, pitch_cv_link ? LINK_ICON : NOTE_ICON); gfxBitmap(38, y + 2, 3, SUB_TWO); // Pitch 2
        gfxPos(38, y); graphics.printf("%4d", pitch[1] / 128);
        // }
        y += Y_DIV;

        gfxIcon(1, y, WAVEFORM_ICON); gfxBitmap(8, y - 1, 3, SUP_ONE); gfxBitmap(8, y + 2, 3, SUB_TWO); // Blend
        gfxPos(6, y); graphics.printf("%4d", wt_blend);

        gfxIcon(33, y, WAVEFORM_ICON); gfxBitmap(40, y + 2, 3, SUB_TWO); // Osc Waveform Reversal
        gfxIcon(50, y, osc2_rev ? ROTATE_L_ICON : ROTATE_R_ICON);
        y += Y_DIV;

        if (page_cursor < PARAM_LAST) {
            gfxIcon(1, y, PhzIcons::speaker); // Volume
            gfxPos(12, y); graphics.printf("%3d", attenuation);

            gfxIcon(33, y, GATE_ICON); // SqDuty
            gfxPos(44, y); graphics.printf("%3d", pulse_duty);
        } else {
            gfxIcon(1, y, GATE_ICON); // SqDuty
            gfxPos(12, y); graphics.printf("%3d", pulse_duty);

            gfxIcon(33, y, STAIRS_ICON); // SR.Div
            gfxPos(44, y); graphics.printf("%3d", sample_rate_div);
        }
    }

    void DrawCVDestMenu() {
        uint8_t x = 3;
        uint8_t y = MENU_ROW;

        gfxBitmap(x + 1, y, 8, ZAP_ICON);
        x += X_DIV;
        gfxPrint(x - 2, y, "CV Dest");

        gfxLine(0, y + 11, 63, y + 11);
        gfxLine(0, 63, 63, 63);
    }

    void DrawCVDestinations() {
        const uint8_t y = MENU_ROW + Y_DIV;
        ForEachChannel(ch) {
            gfxPrint(1, y + ch * Y_DIV, OC::Strings::cv_input_names_none[ch + io_offset + 1]);
            gfxPos(25, y + ch * Y_DIV); graphics.printf("%6s", param_names[cv_dest[ch]]);
        }
    }

// WAVETABLE STUFF:
    void InterpolateSample(std::array<int8_t, WT_SIZE>& wt, const int blend, const uint8_t sample) {
        wt[sample] = (int8_t) ((((blend <= 127) * ((127 - blend) * (int)wavetable[A][sample] + blend * (int)wavetable[B][sample]))
                               + ((blend > 127) * ((255 - blend) * (int)wavetable[B][sample] + (blend - 128) * (int)wavetable[C][sample]))) / 127);
    }

    void UpdatePulseDuty(std::array<int8_t, WT_SIZE>& wt, const uint8_t sample, const uint8_t duty) {
        wt[sample] = (sample < duty) ? 127 : -128;
    }

    void UpdateNoiseSample(std::array<int8_t, WT_SIZE>& wt, const uint8_t sample) {
        wt[sample] = static_cast<int8_t>(random(-128, 127));
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

// WAVEFORM GENERATORS:
    void GenerateWaveForm_Sine(std::array<int8_t, WT_SIZE>& waveform) {
        for (size_t i = 0; i < WT_SIZE; ++i) {
            q15_t t = static_cast<q15_t>(i * 32768 / WT_SIZE);
            waveform[i] = arm_sin_q15(t) >> 8;
        }
    }

    void GenerateWaveForm_Triangle(std::array<int8_t, WT_SIZE>& waveform) {
        int8_t sign = 1;
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

    // add more wave generators here, add a corresponding key in enum WaveForms{}, then call generator function in GenerateWaveTable(){}

};
