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

class TrigSeq : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "Trig8x2";
    }
    const uint8_t* applet_icon() { return PhzIcons::trigseq; }

    void Start() {
        ForEachChannel(ch)
        {
            pattern[ch] = random(1, 255);
            end_step[ch] = 7;
            step[ch] = 0;
        }
        cursor = 0;
        reset = true;
    }

    void Reset() {
        ForEachChannel(ch) step[ch] = -1;
    }

    void Controller() {
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) { // clock advance
            bool swap = In(0) >= HEMISPHERE_3V_CV;
            ForEachChannel(ch)
            {
                if (step[ch] >= end_step[ch]) step[ch] = -1;
                step[ch]++;
                active_step[ch] = Step(ch);
                if ((pattern[ch] >> active_step[ch]) & 0x01) ClockOut(swap ? (1 - ch) : ch);
            }
        }
    }

    void View() {
        DrawDisplay();
    }

    void OnButtonPress() {
        cursor++;
        if (cursor > 5) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        int ch = cursor > 2 ? 1 : 0;
        int this_cursor = cursor - (3 * ch);

        // Update end_step
        if (this_cursor == 2) {
            end_step[ch] = constrain(end_step[ch] + direction, 0, 7);
        } else {
            // Get the current pattern
            int curr_patt = pattern[ch];

            // Shift right based on which nybble is selcted
            uint8_t nybble = (curr_patt >> (this_cursor * 4)) & 0x0f;
            nybble += direction;
            nybble &= 0x0f;

            // Put the updated nybble back where it belongs
            if (this_cursor == 0) pattern[ch] = (curr_patt & 0xf0) + nybble;
            else pattern[ch] = (curr_patt & 0x0f) + (nybble << 4);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, pattern[0]);
        Pack(data, PackLocation {8,8}, pattern[1]);
        Pack(data, PackLocation {16,3}, end_step[0]);
        Pack(data, PackLocation {19,3}, end_step[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        pattern[0] = Unpack(data, PackLocation {0,8});
        pattern[1] = Unpack(data, PackLocation {8,8});
        end_step[0] = Unpack(data, PackLocation {16,3});
        end_step[1] = Unpack(data, PackLocation {19,3});

        ForEachChannel(ch) step[ch] = -1;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Swap Ch";
        help[HELP_CV2]      = "Offset";
        help[HELP_OUT1]     = "Trg Ch1";
        help[HELP_OUT2]     = "Trg Ch2";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int step[2]; // Current step of each channel
    int active_step[2];
    uint8_t pattern[2];
    int end_step[2];
    int cursor; // 0=ch1 low, 1=ch1 hi, 2=c1 end_step,  3=ch2 low, 4=ch3 hi, 5=ch2 end_step
    bool reset;

    inline int Length(int ch) const {
        return end_step[ch] + 1;
    }

    int Step(int ch) {
        int s = step[ch] + Offset(ch);
        s %= Length(ch);
        return s;
    }

    int Offset(int ch) {
        int offset = Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, end_step[ch]);
        if (offset < 0) offset += Length(ch);
        offset %= Length(ch);
        return offset;
    }
    
    void DrawDisplay() {
        ForEachChannel(ch)
        {
            int this_cursor = cursor - (3 * ch);
            int x = 10 + (31 * ch);
            int offset = Offset(ch);

            // Draw the steps for this channel
            bool stop = 0; // Stop displaying when end_step is reached
            for (int s = 0; s < 8; s++)
            {
                if (!stop) {
                    int y = 18 + (6 * s);
                    gfxCircle(x, y, 3);
                    int value = (pattern[ch] >> s) & 0x01;
                    if (value) {
                        for (int r = 1; r < 3; r++) {
                            gfxCircle(x, y, r);
                            gfxCircle(x + 1, y, r);
                            gfxCircle(x + 2, y, r);
                        }
                    }

                    if (s == active_step[ch]) {
                        gfxLine(x + 4, y, x + 10, y);
                    }

                    if (s == offset) {
                        gfxFrame(x - 4, y - 4, 9, 9);
                    }

                    // Draw the end_step cursor
                    if (s == end_step[ch]) {
                        if (this_cursor == 2 && CursorBlink()) {
                            gfxLine(x - 8, y + 3, x + 5, y + 3);
                        }
                        stop = 1;
                    }
                }
            }

            // Draw the nybble cursor
            if ((this_cursor == 0 || this_cursor == 1) && CursorBlink()) {
                int y = 15 + (this_cursor * 24);
                gfxLine(x - 5, y, x - 5, y + 24);
            }
        }
    }
};
