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

#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"
#include "vector_osc/HSVectorOscillator.h"
#include "vector_osc/WaveformManager.h"

static constexpr int pow10_lut[] = { 1, 10, 100, 1000 };

class VectorLFO : public HemisphereApplet {
public:

    const char* applet_name() {
        return "VectorLFO";
    }

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
        int freq_mod = Proportion(DetentedIn(0), HEMISPHERE_3V_CV, 3000);
        freq_mod = constrain(freq[0] + freq_mod, min_freq, max_freq);
        osc[0].SetFrequency(freq_mod);

        // Input 2 determines signal 1's level on the B/D output mix
        int mix_level = DetentedIn(1);
        mix_level = constrain(mix_level, -HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV);

        int signal = 0; // Declared here because the first channel's output is used in the second channel; see below
        ForEachChannel(ch)
        {
            if (Clock(ch)) {
                uint32_t ticks = ClockCycleTicks(ch);
                int new_freq = 1666666 / ticks;
                freq[ch] = constrain(new_freq, min_freq, max_freq);
                osc[ch].SetFrequency(freq[ch]);
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

    void OnButtonPress() {
        CursorAction(cursor, 3);
    }

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
            int sign = (direction > 0) ? 1 : -1;

            // if the encoder moved more than once, ensure we use the appropriate increment
            //  when crossing boundaries
            for (int i = 0; i < abs(direction); ++i) {
              int cur_direction = sign;
              if (freq[ch] + sign > 10000) cur_direction *= 1000;
              else if (freq[ch] + sign > 1000) cur_direction *= 100;
              else if (freq[ch] + sign > 250) cur_direction *= 10;  
              freq[ch] = constrain(freq[ch] + cur_direction, min_freq, max_freq);
            }
            osc[ch].SetFrequency(freq[ch]);  
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);

        for (int i = 0; i < 2; ++i) {
          int exponent = 0;
          if (freq[i] > 250) exponent++;
          if (freq[i] > 1000) exponent++;
          if (freq[i] > 10000) exponent++;
          Pack(data, PackLocation {uint8_t(12 + i * 10), 2}, exponent);

          int mantissa = freq[i] / pow10_lut[exponent];
          Pack(data, PackLocation {uint8_t(12 + i * 10 + 2), 8}, mantissa);
        }
        
        return data;
    }
    
    void OnDataReceive(uint64_t data) {
        for (int i = 0; i < 2; ++i) {
          int exponent = Unpack(data, PackLocation {uint8_t(12 + i * 10), 2});
          int mantissa = Unpack(data, PackLocation {uint8_t(12 + i * 10 + 2), 8});
          
          freq[i] = mantissa * pow10_lut[exponent];
        }
        SwitchWaveform(0, Unpack(data, PackLocation {0,6}));
        SwitchWaveform(1, Unpack(data, PackLocation {6,6}));
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1,2=Sync";
        help[HEMISPHERE_HELP_CVS]      = "1=Freq1  2=Mix 1@B";
        help[HEMISPHERE_HELP_OUTS]     = "Out A=1, B=2+1";
        help[HEMISPHERE_HELP_ENCODER]  = "Freq./Waveform";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // 0=Freq A; 1=Waveform A; 2=Freq B; 3=Waveform B
    VectorOscillator osc[2];

    // Settings
    int waveform_number[2];
    int freq[2]; // in centihertz
    
    void DrawInterface() {
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        // Show channel output
        gfxPos(1, 15);
        if (hemisphere == 0) gfxPrint(ch ? "B" : "A");
        else gfxPrint(ch ? "D" : "C");
        gfxInvert(1, 14, 7, 9);

        gfxPrint(10, 15, ones(freq[ch]));
        if (freq[ch] < 1000) {
          gfxPrint(".");
          int h = hundredths(freq[ch]);
          if (h < 10) gfxPrint("0");
          gfxPrint(h);  
        }
        
        gfxPrint(" Hz");
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
        osc[ch].SetFrequency(freq[ch]);
#ifdef BUCHLA_4U
        osc[ch].Offset((12 << 7) * 4);
        osc[ch].SetScale((12 << 7) * 4);
#else
        osc[ch].SetScale((12 << 7) * 3);
#endif
    }

    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}
};
