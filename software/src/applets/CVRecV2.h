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
#include "SegmentDisplay.h"
#define CVREC_MAX_STEP 384

const char* const CVRecV2_MODES[4] = {
    "Play", "Rec 1", "Rec 2", "Rec 1+2"
};

class CVRecV2 : public HemisphereApplet {
public:

    const char* applet_name() {
        return "CVRec";
    }

    void Start() {
        segment.Init(SegmentSize::BIG_SEGMENTS);
    }

    void Controller() {
        if (Clock(1)) reset = true;
        if (reset) {
            step = start;
            if (punch_out) punch_out = end - start + 1; // inclusive
        }

        // check for deferred recording
        if (EndOfADCLag() && punch_out && mode) {
            ForEachChannel(ch)
            {
                if (mode & (0x01 << ch)) { // Record this channel
                    cv[ch][step] = In(ch);
                }
            }
            if (!reset) {
                if (--punch_out == 0) mode = 0;
            }
        }

        if (Clock(0) ) { // sequence advance
            if (!reset) step++;
            if (step > end || step < start) step = start;
            ForEachChannel(ch)
            {
                signal[ch] = int2simfloat(cv[ch][step]);
                byte next_step = step + 1;
                if (next_step > end) next_step = start;
                if (smooth) rise[ch] = (int2simfloat(cv[ch][next_step]) - int2simfloat(cv[ch][step])) / ClockCycleTicks(0);
                else rise[ch] = 0;
            }

            // defer recording
            StartADCLag();
            reset = false;
        }

        ForEachChannel(ch)
        {
            if (!(mode & (0x01 << ch))) { // If not recording this channel, play it
                Out(ch, simfloat2int(signal[ch]));
                signal[ch] += rise[ch];
            } else {
                Out(ch, In(ch));
            }
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (cursor == 2 && !EditMode()) { // special case to toggle smoothing
            smooth = 1 - smooth;
            ResetCursor();
            return;
        }

        if (cursor == 3 && EditMode()) { // activate recording if selected
            punch_out = (mode > 0) ? end - start + 1 : 0;
        }

        CursorAction(cursor, 3);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) { //not editing, move cursor
            MoveCursor(cursor, direction, 3);
            return;
        }
        
        switch (cursor) {
        case 0: {
            int16_t fs = start; // Former start value
            start = constrain(start + direction, 0, end - 1);
            if (fs != start && punch_out) punch_out -= direction;
            break;
        }
        case 1: {
            int16_t fe = end; // Former end value
            end = constrain(end + direction, start + 1, CVREC_MAX_STEP - 1);
            if (fe != end && punch_out) punch_out += direction;
            break;
        }
        case 2:
            smooth = 1 - smooth;
            ResetCursor();
            break;
        case 3:
            mode = constrain(mode + direction, 0, 3);
            break;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,9}, start);
        Pack(data, PackLocation {9,9}, end);
        Pack(data, PackLocation {18,1}, smooth);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        start = Unpack(data, PackLocation {0,9});
        end = Unpack(data, PackLocation {9,9});
        smooth = Unpack(data, PackLocation {18,1});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "Rec: 1=Tr1 2=Tr2";
        help[HEMISPHERE_HELP_OUTS]     = "Play: A=Tr1 B=Tr2";
        help[HEMISPHERE_HELP_ENCODER]  = "Range/Smooth/Rec";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // 0=Start 1=End 2=Smooth 3=Record Mode
    SegmentDisplay segment;

    int16_t cv[2][CVREC_MAX_STEP];
    simfloat rise[2];
    simfloat signal[2];
    bool smooth;
    bool reset = true;

    // Transport
    int mode = 0; // 0=Playback, 1=Rec Track 1, 2=Rec Track 2, 3= Rec Tracks 1 & 2
    int16_t start = 0; // Start step number
    int16_t end = 63; // End step number
    int16_t step = 0; // Current step
    int16_t punch_out = 0;
    
    void DrawInterface() {
        // Range
        gfxIcon(1, 15, LOOP_ICON);
        gfxPrint(18 + pad(100, start + 1), 15, start + 1);
        gfxPrint("-");
        gfxPrint(pad(100, end + 1), end + 1);

        // Smooth
        gfxPrint(1, 25, "Smooth");
        if (cursor != 2 || CursorBlink()) gfxIcon(54, 25, smooth ? CHECK_ON_ICON : CHECK_OFF_ICON);

        // Record Mode
        gfxPrint(1, 35, CVRecV2_MODES[mode]);

        // Status icon
        if (mode > 0 && punch_out > 0) {
            if (!CursorBlink()) gfxIcon(54, 35, RECORD_ICON);
        }
        else gfxIcon(54, 35, PLAY_ICON);

        // Record time indicator
        if (punch_out > 0) gfxInvert(0, 34, punch_out / 6, 9);

        // Cursor
        switch(cursor){
            case 0: gfxCursor(19, 23, 18); break;
            case 1: gfxCursor(43, 23, 18); break;
            case 3: gfxCursor(1, 43, 63); break;
        }

        // Step indicator
        segment.PrintWhole(hemisphere * 64, 50, step + 1, 100);

        // CV Indicators
        ForEachChannel(ch)
        {
            int w = Proportion(ViewOut(ch), HEMISPHERE_MAX_CV, 32);
            w = constrain(w, -32, 32);
            if (w > 0) gfxRect(32, (ch * 6) + 50, w, 4);
            if (w < 0) gfxFrame(32, (ch * 6) + 50, -w, 4);
        }
    }
};
