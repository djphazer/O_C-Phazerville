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
#include "../tideslite.h"

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
            pitch[ch] = 0;
            waveform_number[ch] = 0;
            SwitchWaveform(ch, 0);
            Out(ch, 0);
        }
    }

    void Controller() {
        // Input 1 is frequency modulation for channel 1
        osc[0].SetPhaseIncrement(ComputePhaseIncrement(pitch[0] + In(0)));

        // Input 2 determines signal 1's level on the B/D output mix
        int mix_level = DetentedIn(1);
        mix_level = constrain(mix_level, -HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV);

        int signal = 0; // Declared here because the first channel's output is used in the second channel; see below
        ForEachChannel(ch)
        {
            if (Clock(ch)) {
                uint32_t ticks = ClockCycleTicks(ch);
                uint32_t phase_inc = 0xffffffff / ticks;
                osc[ch].SetPhaseIncrement(phase_inc);
                pitch[ch] = ComputePitch(phase_inc);
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
            // macro needs to act on an int and this reads better than a cast.
            int new_accel = knob_accel + direction - direction * (millis_since_turn / 20);
            knob_accel = constrain(new_accel, -120, 120);
            if (direction * knob_accel <= 0) knob_accel = direction;
            pitch[ch] = constrain(
                pitch[ch] + knob_accel,
                -32768 - HEMISPHERE_MIN_CV,
                ComputePitch(0x7fffffff) // nyquist
            );
            osc[ch].SetPhaseIncrement(ComputePhaseIncrement(pitch[ch]));
        }
        millis_since_turn = 0;
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);
        Pack(data, PackLocation {12,16}, pitch[0]);
        Pack(data, PackLocation {28,16}, pitch[1]);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        pitch[0] = Unpack(data, PackLocation {12,16});
        pitch[1] = Unpack(data, PackLocation {28,16});
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
    int16_t pitch[2];

    int8_t knob_accel = 0;
    elapsedMillis millis_since_turn;
    

    void DrawInterface() {
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        // Show channel output
        gfxPos(1, 15);
        gfxPrint(OutputLabel(ch));
        gfxInvert(1, 14, 7, 9);

        gfxPos(10, 15);
        gfxPrintFreqFromPitch(pitch[ch]);

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
        osc[ch].SetPhaseIncrement(ComputePhaseIncrement(pitch[ch]));
        if (OC::DAC::kOctaveZero == 0) {
          osc[ch].Offset(HEMISPHERE_MAX_CV/2);
          osc[ch].SetScale(HEMISPHERE_MAX_CV/2);
        } else {
          osc[ch].Offset(0);
          osc[ch].SetScale(-HEMISPHERE_MIN_CV);
        }
    }
};
