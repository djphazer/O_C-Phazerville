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

/* original 8-step mod by Logarhythm, adapted by djphazer
 */

#include "HSMIDI.h"

class SequenceX : public HemisphereApplet {
public:

    // DON'T GO PAST 8!
    static constexpr int SEQX_STEPS = 8;
    static constexpr int SEQX_MIN_VALUE = -30;
    static constexpr int SEQX_MAX_VALUE = 30;

    const char* applet_name() { // Maximum 10 characters
        return "SequenceX";
    }

    void Start() {
        Randomize();
    }

    void Randomize() {
        for (int s = 0; s < SEQX_STEPS; s++) note[s] = random(SEQX_MIN_VALUE, SEQX_MAX_VALUE);
    }

    void Controller() {
        if (In(1) > (24 << 7) ) // 24 semitones == 2V
        {
            // new random sequence if CV2 goes high
            if (!cv2_gate) {
                cv2_gate = 1;
                Randomize();
            }
        }
        else cv2_gate = 0;

        if (Clock(1)) { // reset
            Reset();
        }
        if (Clock(0)) { // clock

            if (!reset) Advance(step);
            reset = false;
            // send trigger on first step
            if (step == 0) ClockOut(1);
        }
        
        // continuously compute the note
        int transpose = DetentedIn(0) / 128; // 128 ADC steps per semitone
        int play_note = note[step] + 60 + transpose;
        play_note = constrain(play_note, 0, 127);
        // set CV output
        Out(0, MIDIQuantizer::CV(play_note));

    }

    void View() {
        DrawPanel();
    }

    void OnButtonPress() {
        if (cursor >= SEQX_STEPS && !EditMode()) // toggle mute
            muted ^= (0x01 << (cursor - SEQX_STEPS));
        else
            CursorAction(cursor, SEQX_STEPS*2-1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, SEQX_STEPS*2-1);
            return;
        }

        if (cursor >= SEQX_STEPS) // toggle mute
            muted ^= (0x01 << (cursor - SEQX_STEPS));
        else {
            note[cursor] = constrain(note[cursor] + direction, SEQX_MIN_VALUE, SEQX_MAX_VALUE);
            muted &= ~(0x01 << cursor); // unmute
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        const int b = 6; // bits per step
        for (size_t s = 0; s < SEQX_STEPS; s++)
        {
            Pack(data, PackLocation {s * b, b}, note[s] - SEQX_MIN_VALUE);
        }
        Pack(data, PackLocation{SEQX_STEPS * b, SEQX_STEPS}, muted);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        const int b = 6; // bits per step
        for (size_t s = 0; s < SEQX_STEPS; s++)
        {
            note[s] = Unpack(data, PackLocation {s * b, b}) + SEQX_MIN_VALUE;
        }
        muted = Unpack(data, PackLocation {SEQX_STEPS * b, SEQX_STEPS});
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=Trans 2=RndSeq";
        help[HEMISPHERE_HELP_OUTS]     = "A=CV B=Clk Step 1";
        help[HEMISPHERE_HELP_ENCODER]  = "Edit Step / Mutes";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor = 0;
    uint8_t muted = 0xF0; // Bitfield for muted steps; ((muted >> step) & 1) means muted
    int8_t note[SEQX_STEPS]; // Sequence value (-30 to 30)
    int step = 0; // Current sequencer step
    bool reset = true;
    bool cv2_gate = false;

    void Advance(int starting_point) {
        if (++step == SEQX_STEPS) step = 0;
        // If all the steps have been muted, stay where we were
        if (step_is_muted(step) && step != starting_point) Advance(starting_point);
    }

    void Reset() {
        step = 0;
        reset = true;
    }

    void DrawPanel() {
        // dotted middle line
        gfxDottedLine(0, 40, 63, 40, 3);

        // Sliders
        for (int s = 0; s < SEQX_STEPS; s++)
        {
            const int x = 3 + (8 * s);
            const int y = 20;
            const int h = 60 - y;
            
            if (!step_is_muted(s)) {
                gfxLine(x, y, x, 60, (s != cursor) ); // dotted line for unselected steps

                // scaled pixel position on slider
                const int pos = (note[s] + 30) * (h-2) / 60;

                // When cursor, there's a solid slider
                if (s == cursor) {
                    gfxRect(x - 2, 58 - pos, 5, 3);
                } else {
                    gfxFrame(x - 2, 58 - pos, 5, 3);
                }
                
                // Arrow indicator for current step
                if (s == step) {
                    gfxIcon(x - 3, 60, UP_BTN_ICON);
                }
            } else {
                if (s == cursor)
                    gfxLine(x, y, x, 60);
            }

            if (s == cursor - SEQX_STEPS)
              gfxIcon(x - 3, y - 8, step_is_muted(s) ? CHECK_OFF_ICON : CHECK_ON_ICON);
            if (s == cursor && EditMode()) {
                gfxInvert(x - 2, y, 5, h + 1);
                int w_ = 18 - pad(100, note[s]);
                int x_ = constrain(x - 9 + pad(10, note[s]), 0, 63 - w_);

                gfxLine(x_, y, x_ + w_, y);
                gfxPrint(x_, y - 8, note[s]);
            }
        }
    }

    bool step_is_muted(int step) {
        return (muted & (0x01 << step));
    }
};
