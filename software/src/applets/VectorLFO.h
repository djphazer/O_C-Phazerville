// Copyright (c) 2018, Jason Justian
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

#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"

static constexpr int pow10_lut[] = { 1, 10, 100, 1000 };

class VectorLFO : public HemisphereApplet {
public:

    const char* applet_name() {
        return "VectorLFO";
    }
    const uint8_t* applet_icon() { return PhzIcons::vectorLFO; }

    static constexpr int min_freq = 8;
    static constexpr int max_freq = 100000;

    void Start() {
        ForEachChannel(ch)
        {
            freq[ch] = 200;
            waveform_number[ch] = 0;
            SwitchWaveform(ch, 0);
            Out(ch, 0);
        }
    }

    void Controller() {
        // Input 1 is frequency modulation for channel 1
        float f = constrain(scale_freq(freq[0], In(0)), 1, 40000000);
        osc[0].SetFrequency_4dec(f);

        // Input 2 determines signal 1's level on the B/D output mix
        int mix_level = DetentedIn(1);
        mix_level = constrain(mix_level, -HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV);

        int signal = 0; // Declared here because the first channel's output is used in the second channel; see below
        ForEachChannel(ch)
        {
            if (Clock(ch)) {
                uint32_t ticks = ClockCycleTicks(ch);
                int new_freq = 1666666 / ticks;
                new_freq = constrain(new_freq, 3, 99900);
                osc[ch].SetFrequency(new_freq);
                freq[ch] = new_freq * 10000;
                osc[ch].Reset();
            }

            if (ch == 0) {
                // Out A is always just the first oscillator at full amplitude
                signal = osc[ch].Next();
            } else {
                // Out B can have channel 1 blended into it, depending on the value of mix_level.
                // At a value of 6V, Out B is a 50/50 mix of channels 1 and 2.
                // At a value of 0V, channel 1 is absent from Out B.
                signal = Proportion(mix_level, HEMISPHERE_MAX_CV, signal); // signal from channel 1's iteration
                signal += osc[ch].Next();

                // Proportionally blend the signal, depending on mix.
                // If mix_level is at (+ or -) max, then this effectively divides the signal by 2.
                signal = Proportion(HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV + abs(mix_level), signal);
            }
            Out(ch, signal);
        }
    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        if (c == 1) { // Waveform
            waveform_number[ch] = WaveformManager::GetNextWaveform(waveform_number[ch], direction);
            SwitchWaveform(ch, waveform_number[ch]);
            // Reset both waveform to provide a sync mechanism
            ForEachChannel(ch) osc[ch].Reset();
        }
        if (c == 0) { // Frequency
            freq_ix[ch] += direction;
            freq_ix[ch] = constrain(freq_ix[ch], 0, 1023);
            freq[ch] = ix_to_4dec(freq_ix[ch]);
            osc[ch].SetFrequency_4dec(freq[ch]);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);
        Pack(data, PackLocation {12,10}, freq_ix[0] & 0x03ff);
        Pack(data, PackLocation {22,10}, freq_ix[1] & 0x03ff);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        freq_ix[0] = Unpack(data, PackLocation {12,10});
        freq_ix[1] = Unpack(data, PackLocation {22,10});
        freq[0] = ix_to_4dec(freq_ix[0]);
        freq[1] = ix_to_4dec(freq_ix[1]);
        SwitchWaveform(0, Unpack(data, PackLocation {0,6}));
        SwitchWaveform(1, Unpack(data, PackLocation {6,6}));
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Sync 1";
        help[HELP_DIGITAL2] = "Sync 2";
        help[HELP_CV1]      = "FM 1";
        help[HELP_CV2]      = "1->2Mix";
        help[HELP_OUT1]     = "Ch1 LFO";
        help[HELP_OUT2]     = "Ch2 LFO";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "Enc: Freq, Waveform";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int cursor; // 0=Freq A; 1=Waveform A; 2=Freq B; 3=Waveform B
    VectorOscillator osc[2];

    // Settings
    int waveform_number[2];
    int freq_ix[2];
    int freq[2];

    void DrawInterface() {
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        // Show channel output
        gfxPos(1, 15);
        gfxPrint(OutputLabel(ch));
        gfxInvert(1, 14, 7, 9);

        int f = freq[ch];

        if (f < 10000) {
            print4Dec(10, 15, 100000000 / f);
            gfxPrint(" S");
        } else {
            print4Dec(10, 15, f);
            gfxPrint(" Hz");
        }

        DrawWaveform(ch);

        if (c == 0) gfxCursor(8, 23, 55);
        if (c == 1 && (EditMode() || CursorBlink()) ) gfxFrame(0, 24, 63, 40);
    }

    void DrawWaveform(byte ch) {
        uint16_t total_time = osc[ch].TotalTime();
        VOSegment seg = osc[ch].GetSegment(osc[ch].SegmentCount() - 1);
        byte prev_x = 0; // Starting coordinates
        byte prev_y = 63 - Proportion(seg.level, 255, 38);
        for (byte i = 0; i < osc[ch].SegmentCount(); i++)
        {
            seg = osc[ch].GetSegment(i);
            byte y = 63 - Proportion(seg.level, 255, 38);
            byte seg_x = Proportion(seg.time, total_time, 62);
            byte x = prev_x + seg_x;
            x = constrain(x, 0, 62);
            y = constrain(y, 25, 62);
            gfxLine(prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }

        // Zero line
        gfxDottedLine(0, 44, 63, 44, 8);
    }

    void SwitchWaveform(byte ch, int waveform) {
        osc[ch] = WaveformManager::VectorOscillatorFromWaveform(waveform);
        waveform_number[ch] = waveform;
        osc[ch].SetFrequency_4dec(freq[ch]);
#ifdef NORTHERNLIGHT
        osc[ch].Offset((12 << 7) * 4);
        osc[ch].SetScale((12 << 7) * 4);
#else
        osc[ch].SetScale((12 << 7) * 3);
#endif
    }

    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}

    void print4Dec(int x, int y, int n) {
        gfxPos(x, y);
        int digits_printed = 0;
        int first_digit = 0;
        for (int i = 8; i > 0; i--) {
            if (i == 4) {
                gfxPrint(".");
            }
            int digit = (n / 10000000);
            n         = (n % 10000000) * 10;
            if (digit != 0 && i > first_digit) {
                first_digit = i;
            }
            if (digit != 0 || digits_printed > 0 || i <= 5) {
                gfxPrint(digit);
                if (first_digit > 0) {
                    digits_printed++;
                }
            }
            if (digits_printed > 2 && i <= 5) {
                return;
            }
        }
    }
};

int ix_to_4dec(int ix) {
    const int twos_steps = 60;
    const int fives_steps = 50;
    const int tens_steps = 50;
    const int ones_steps = 900 - (2 * twos_steps + 5 * fives_steps + 10 * tens_steps);
    const int steps = ones_steps + twos_steps + fives_steps + tens_steps;

    const int twos_max = ones_steps + twos_steps;
    const int fives_max = twos_max + fives_steps;


    // Start with step size of 100, which is the smallest unit that matters.
    ix += ones_steps + twos_steps + fives_steps;
    int oom = ix / steps;
    int result = 100;
    int fine = ix % steps;
    if (fine < ones_steps) {
        result += fine * 1;
    } else if (fine < twos_max) {
        result += ones_steps + (fine - ones_steps) * 2;
    } else if (fine < fives_max) {
        result += ones_steps + 2 * twos_steps + (fine - twos_max) * 5;
    } else {
        result += ones_steps + 2 * twos_steps + 5 * fives_steps + (fine - fives_max) * 10;
    }
    return result * pow10(oom) / 10;
}

int pow10(int n) {
    static int pow10_lut[] = {
        1,
        10,
        100,
        1000,
        10000,
        100000,
        1000000,
        10000000,
        100000000,
        1000000000
    };
    return pow10_lut[n];
}

int pitch_to_freq_scalar(int pitch) {
    // avoid overflow in voltage range
    return exp2_s15_16(pitch * 512 / 12) * 100 / 64 * 100 / 1024;
}

int scale_freq(int freq_4dec, int pitch) {
    // fuck it I'm lazy
    return int32_t(int64_t(freq_4dec) * pitch_to_freq_scalar(pitch) / 10000);
}

// From https://stackoverflow.com/questions/36550388/power-of-2-approximation-in-fixed-point
int32_t exp2_s15_16 (int32_t a) {
    int32_t i, f, r, s;
    /* split a = i + f, such that f in [-0.5, 0.5] */
    i = (a + 0x8000) & ~0xffff; // 0.5
    f = a - i;
    s = ((15 << 16) - i) >> 16;
    /* minimax approximation for exp2(f) on [-0.5, 0.5] */
    r = 0x00000e20;                 // 5.5171669058037949e-2
    r = (r * f + 0x3e1cc333) >> 17; // 2.4261112219321804e-1
    r = (r * f + 0x58bd46a6) >> 16; // 6.9326098546062365e-1
    r = r * f + 0x7ffde4a3;         // 9.9992807353939517e-1
    return (uint32_t)r >> s;
}
