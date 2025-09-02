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

class VectorMod : public HemisphereApplet {
public:

    const char* applet_name() {
        return "VectorMod";
    }
    const uint8_t* applet_icon() { return PhzIcons::vectorMod; }

    void Start() {
        ForEachChannel(ch)
        {
            freq[ch] = 50;
            SwitchWaveform(ch, 0);
            Out(ch, 0);
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            if (Clock(ch)) osc[ch].Start();

            bool cycle = (In(ch) > HEMISPHERE_3V_CV);
            osc[ch].Cycle(cycle);

            Out(ch, osc[ch].Next());
        }
    }

    void View() {
        DrawInterface();
    }

    //void OnButtonPress() { }

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
        }
        if (c == 0) { // Frequency
            if (freq[ch] > 50) direction *= 10;
            freq[ch] = constrain(freq[ch] + direction, 10, 500);
            osc[ch].SetFrequency(freq[ch]);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);
        Pack(data, PackLocation {12,10}, freq[0] & 0x03ff);
        Pack(data, PackLocation {22,10}, freq[1] & 0x03ff);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        freq[0] = Unpack(data, PackLocation {12,10});
        freq[1] = Unpack(data, PackLocation {22,10});
        SwitchWaveform(0, Unpack(data, PackLocation {0,6}));
        SwitchWaveform(1, Unpack(data, PackLocation {6,6}));
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Trig 1";
        help[HELP_DIGITAL2] = "Trig 2";
        help[HELP_CV1]      = "Cycle 1";
        help[HELP_CV2]      = "Cycle 2";
        help[HELP_OUT1]     = "Ch1 Mod";
        help[HELP_OUT2]     = "Ch2 Mod";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "Enc: Freq, Waveform";
       //                   "---------------------" <-- Extra text size guide
    }
    
private:
    int cursor; // 0=Freq A; 1=Waveform A; 2=Freq B; 3=Waveform B
    VectorOscillator osc[2];

    // Settings
    int waveform_number[2];
    int freq[2];
    
    void DrawInterface() {
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        // Show channel output
        gfxPos(1, 15);
        gfxPrint(OutputLabel(ch));
        gfxInvert(1, 14, 7, 9);

        gfxPrint(10, 15, ones(freq[ch]));
        gfxPrint(".");
        int h = hundredths(freq[ch]);
        if (h < 10) gfxPrint("0");
        gfxPrint(h);
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
#ifdef NORTHERNLIGHT
        osc[ch].Offset((12 << 7) * 4);
        osc[ch].SetScale((12 << 7) * 4);
#else
        osc[ch].SetScale((12 << 7) * 3);
#endif
        osc[ch].Cycle(0); // Non cycling
        osc[ch].Reset();
    }

    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}
};
