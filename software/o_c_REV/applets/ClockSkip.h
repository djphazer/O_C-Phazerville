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

class ClockSkip : public HemisphereApplet {
public:

    const char* applet_name() {
        return "ClockSkip";
    }

    void Start() {
        ForEachChannel(ch)
        {
            p[ch] = 100 - (25 * ch);
            trigger_countdown[ch] = 0;
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            if (Clock(ch)) {
                int prob = p[ch] + Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, 100);
                if (random(1, 100) <= prob) {
                    ClockOut(ch);
                    trigger_countdown[ch] = 1667;
                }
            }

            if (trigger_countdown[ch]) trigger_countdown[ch]--;
        }
    }

    void View() {
        DrawSelector();
        DrawIndicator();
    }

    void OnButtonPress() {
        CursorAction(cursor, 1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }
        p[cursor] = constrain(p[cursor] + direction, 0, 100);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, p[0]);
        Pack(data, PackLocation {7,7}, p[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        p[0] = Unpack(data, PackLocation {0,7});
        p[1] = Unpack(data, PackLocation {7,7});
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "Clock Ch1, Ch2";
        help[HEMISPHERE_HELP_CVS]      = "p Mod Ch1, Ch2";
        help[HEMISPHERE_HELP_OUTS]     = "Clock Ch1, Ch2";
        help[HEMISPHERE_HELP_ENCODER]  = "Set p";
    }
    
private:
    int16_t p[2];
    int trigger_countdown[2];
    int cursor;
    
    void DrawSelector()
    {
        ForEachChannel(ch)
        {
            gfxPrint(0 + (31 * ch), 15, p[ch]);
            gfxPrint("%");
            if (ch == cursor) gfxCursor(0 + (31 * ch), 23, 30);
        }
    }    
    
    void DrawIndicator()
    {
        ForEachChannel(ch)
        {
            gfxBitmap(12 + (32 * ch), 45, 8, CLOCK_ICON);
            if (trigger_countdown[ch] > 0) gfxFrame(9 + (32 * ch), 42, 13, 13);
        }
    }

};
