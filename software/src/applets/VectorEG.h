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

#ifndef _HEM_VECTOR_EG_H_
#define _HEM_VECTOR_EG_H_

#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"

class VectorEG : public HemisphereApplet {
public:
  static constexpr int MIN_FREQ = 1; // centihertz
  static constexpr int MAX_FREQ = 2000;

    const char* applet_name() {
        return "VectorEG";
    }
    const uint8_t* applet_icon() { return PhzIcons::vectorEG; }

    void Start() {
        ForEachChannel(ch) {
            freq[ch] = 50;
            wave_mod[ch] = waveform_number[ch] = HS::EG1 + ch;
            SwitchWaveform(ch, waveform_number[ch]);
            gated[ch] = 0;
            Out(ch, 0);
        }
    }

    void Controller() {
        ForEachChannel(ch) {
            // modulate shape or frequency
            if (modshape) {
              int wave = WaveformManager::GetNextWaveform(waveform_number[ch], SemitoneIn(ch));
              if (wave_mod[ch] != wave) {
                wave_mod[ch] = wave;
                SwitchWaveform(ch, wave_mod[ch]);
              }
            } else {
              // Input is frequency modulation for channel
              //osc[ch].SetPhaseIncrement(ComputePhaseIncrement(pitch[ch] + In(ch)));
              freq_mod[ch] = freq[ch];
              Modulate(freq_mod[ch], ch, MIN_FREQ, MAX_FREQ);
              osc[ch].SetFrequency(freq_mod[ch]);
            }

            if (Gate(ch)) {
                if (!gated[ch]) { // Gate wasn't on last time, so start the waveform
                    osc[ch].Start();
                }
                gated[ch] = 1;
            } else {
                if (gated[ch]) { // Gate isn't on now, but was on last time, so release
                    osc[ch].Release();
                }
                gated[ch] = 0;
            }
            Out(ch, osc[ch].Next());
        }
    }

    void View() {
        DrawInterface();
    }

    void AuxButton() {
      modshape = cursor & 1;
      if (!modshape) {
        ForEachChannel(ch) {
          wave_mod[ch] = waveform_number[ch];
          SwitchWaveform(ch, waveform_number[ch]);
        }
      }
      CancelEdit();
    }
    //void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        const uint8_t c = cursor & 1;
        const uint8_t ch = cursor >> 1;

        if (c == 1) { // Waveform
            waveform_number[ch] = WaveformManager::GetNextWaveform(waveform_number[ch], direction);
            SwitchWaveform(ch, waveform_number[ch]);
        }
        if (c == 0) { // Frequency
            if (freq[ch] > 50) direction *= 10;
            freq_mod[ch] = freq[ch] = constrain(freq[ch] + direction, MIN_FREQ, MAX_FREQ);
            osc[ch].SetFrequency(freq[ch]);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);
        Pack(data, PackLocation {12,10}, freq[0] & 0x03ff);
        Pack(data, PackLocation {22,10}, freq[1] & 0x03ff);
        Pack(data, PackLocation {32, 1}, modshape);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        freq[0] = Unpack(data, PackLocation {12,10});
        freq[1] = Unpack(data, PackLocation {22,10});
        waveform_number[0] = Unpack(data, PackLocation {0,6});
        waveform_number[1] = Unpack(data, PackLocation {6,6});
        SwitchWaveform(0, waveform_number[0]);
        SwitchWaveform(1, waveform_number[1]);
        modshape = Unpack(data, PackLocation {32, 1});
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Gate 1";
        help[HELP_DIGITAL2] = "Gate 2";
        help[HELP_CV1]      = modshape? "Shape" : "Freq";
        help[HELP_CV2]      = modshape? "Shape" : "Freq";
        help[HELP_OUT1]     = "Ch1 Env";
        help[HELP_OUT2]     = "Ch2 Env";
        help[HELP_EXTRA1] = "Enc: Freq, Shape";
        help[HELP_EXTRA2] = "AuxBtn: CV mode";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int cursor; // 0=Freq A; 1=Waveform A; 2=Freq B; 3=Waveform B
    VectorOscillator osc[2];
    bool gated[2];

    // Settings
    bool modshape = false;
    uint8_t waveform_number[2];
    uint8_t wave_mod[2];
    int16_t freq[2];
    int16_t freq_mod[2];

    void DrawInterface() {
        uint8_t c = cursor & 1;
        uint8_t ch = cursor >> 1;

        // Show channel output
        gfxPos(1, 15);
        gfxPrint(OutputLabel(ch));
        gfxInvert(1, 14, 7, 9);

        gfxPrint(10, 15, ones(freq_mod[ch]));
        gfxPrint(".");
        int h = hundredths(freq_mod[ch]);
        if (h < 10) gfxPrint("0");
        gfxPrint(h);
        gfxPrint(" Hz");

        if (freq[ch] != freq_mod[ch]) gfxIcon(56, 10, CV_ICON);

        gfxIcon(56, 15, modshape? DOWN_ICON : LEFT_ICON);

        DrawWaveform(ch);

        if (c == 0) gfxSpicyCursor(9, 23, 27);
        if (c == 1 && (EditMode() || CursorBlink()) ) gfxFrame(0, 24, 63, 40, true);
    }

    void DrawWaveform(uint8_t ch) {
        uint16_t total_time = osc[ch].TotalTime();
        VOSegment seg = osc[ch].GetSegment(osc[ch].SegmentCount() - 1);
        uint8_t prev_x = 0; // Starting coordinates
        uint8_t prev_y = 63 - Proportion(seg.level, 255, 38);
        for (uint8_t i = 0; i < osc[ch].SegmentCount(); i++)
        {
            seg = osc[ch].GetSegment(i);
            uint8_t y = 63 - Proportion(seg.level, 255, 38);
            uint8_t seg_x = Proportion(seg.time, total_time, 62);
            uint8_t x = prev_x + seg_x;
            x = constrain(x, 0, 62);
            y = constrain(y, 25, 62);
            gfxLine(prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }

        // Zero line
        gfxDottedLine(0, 44, 63, 44, 8);

        // current position
        const int pos = osc[ch].GetPhase() >> 26;
        gfxLine(pos, 25, pos, 63);
    }

    void SwitchWaveform(uint8_t ch, uint8_t waveform) {
        osc[ch] = WaveformManager::VectorOscillatorFromWaveform(waveform);
        osc[ch].SetFrequency(freq[ch]);

        // unipolar
        osc[ch].SetScale(HEMISPHERE_MAX_CV/2);
        osc[ch].Offset(HEMISPHERE_MAX_CV/2);

        //if (OC::DAC::kOctaveZero == 0) {
        //} else {
        // TODO: bipolar
        //osc[ch].Offset(0);
        //osc[ch].SetScale(HEMISPHERE_MAX_CV);
        //}

        osc[ch].Sustain(); // EG
        osc[ch].Cycle(0); // Non cycling
    }

    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}
};
#endif
