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

class VectorMorph : public HemisphereApplet {
public:

    const char* applet_name() {
        return "VectMorph";
    }

    void Start() {
        ForEachChannel(ch)
        {
        		phase[ch] = (ch * 180) + (hemisphere * 90);
        		last_phase[ch] = 0;
            SwitchWaveform(ch, HS::Morph1);
            Out(ch, 0);
        }
    }

    void Controller() {
    		int cv_phase = 0;
    		if (DetentedIn(1)) linked = 0; // Turn off linking if CV 2 gets input
    		
        ForEachChannel(ch)
        {
        		if (!linked || ch == 0) {
        		    cv_phase = Proportion(In(ch), HEMISPHERE_MAX_INPUT_CV, 3599);
        		    	cv_phase = constrain(cv_phase, -3599, 3599);
        		}
        		last_phase[ch] = (phase[ch] * 10) + cv_phase;
			Out(ch, osc[ch].Phase(last_phase[ch]));
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
            linked = 1; // Restore link when waveform is changed
        }
        if (c == 0) { // Phase
        		phase[ch] = constrain(phase[ch] + (direction * 5), 0, 355);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, waveform_number[0]);
        Pack(data, PackLocation {6,6}, waveform_number[1]);
        Pack(data, PackLocation {12,9}, phase[0]);
        Pack(data, PackLocation {21,9}, phase[1]);
        Pack(data, PackLocation {30,1}, linked);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        phase[0] = Unpack(data, PackLocation {12,9});
        phase[1] = Unpack(data, PackLocation {21,9});
        SwitchWaveform(0, Unpack(data, PackLocation {0,6}));
        SwitchWaveform(1, Unpack(data, PackLocation {6,6}));
        linked = Unpack(data, PackLocation {30,1});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "";
        help[HEMISPHERE_HELP_CVS]      = "1,2=Phase";
        help[HEMISPHERE_HELP_OUTS]     = "A,B=Out";
        help[HEMISPHERE_HELP_ENCODER]  = "Phase/Waveform";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // 0=Phase A; 1=Waveform A; 2=Phase B; 3=Waveform B
    VectorOscillator osc[2];
    int last_phase[2]; // For display

    // Settings
    int waveform_number[2];
    int phase[2];
    bool linked = 1;
    
    void DrawInterface() {
        byte c = cursor;
        byte ch = cursor < 2 ? 0 : 1;
        if (ch) c -= 2;

        // Show channel output
        gfxPos(1, 15);
        if (hemisphere == 0) gfxPrint(ch ? "B" : "A");
        else gfxPrint(ch ? "D" : "C");
        gfxInvert(1, 14, 7, 9);

        gfxPrint(10, 15, phase[ch]);
        gfxPrint("`"); // Grave accent has been converted to the degree symbol in gfx_font6x8.h
        DrawWaveform(ch);
        
        // Cursors
        if (c == 0) gfxCursor(8, 23, 55);
        if (c == 1 && (EditMode() || CursorBlink()) ) gfxFrame(0, 24, 63, 40);
        
        // Link icon
        if (linked) gfxIcon(54, 15, LINK_ICON);
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
        
        // Phase transport location
        byte transport_x = Proportion(abs(last_phase[ch]) % 3600, 3600, 63);
        gfxDottedLine(transport_x, 24, transport_x, 63, 3);
    }

    void SwitchWaveform(byte ch, int waveform) {
        osc[ch] = WaveformManager::VectorOscillatorFromWaveform(waveform);
        waveform_number[ch] = waveform;
        osc[ch].SetScale(HEMISPHERE_MAX_CV);
    }
};
