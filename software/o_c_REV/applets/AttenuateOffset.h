// Copyright (c) 2019, Jason Justian
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

#define ATTENOFF_INCREMENTS 128
#define ATTENOFF_MAX_LEVEL 63

class AttenuateOffset : public HemisphereApplet {
public:

    const char* applet_name() {
        return "AttenOff";
    }

    void Start() {
        ForEachChannel(ch) level[ch] = ATTENOFF_MAX_LEVEL;
    }

    void Controller() {
        mix_final = mix || Gate(1);
        int prevSignal = 0;
        
        ForEachChannel(ch)
        {
            int signal = Proportion(level[ch], ATTENOFF_MAX_LEVEL, In(ch)) + (offset[ch] * ATTENOFF_INCREMENTS);
            if (ch == 1 && mix_final) {
                signal = signal + prevSignal;
            }

            // use the unconstrained signal for mixing
            prevSignal = signal;

            signal = constrain(signal, -HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV);
            Out(ch, signal);
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (cursor == 4 && !EditMode()) // special case when modal editing
            mix = !mix;
        else
            CursorAction(cursor, 4);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 4);
            return;
        }
        if (cursor == 4) { // non-modal editing special case toggle
            mix = !mix;
            return;
        }

        uint8_t ch = cursor / 2;
        if (cursor == 0 || cursor == 2) {
            // Change offset voltage
            int min = -HEMISPHERE_MAX_CV / ATTENOFF_INCREMENTS;
            int max = HEMISPHERE_MAX_CV / ATTENOFF_INCREMENTS;
            offset[ch] = constrain(offset[ch] + direction, min, max);
        } else {
            // Change level percentage (+/-200%)
            level[ch] = constrain(level[ch] + direction, -ATTENOFF_MAX_LEVEL*2, ATTENOFF_MAX_LEVEL*2);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,9}, offset[0] + 256);
        Pack(data, PackLocation {10,9}, offset[1] + 256);
        Pack(data, PackLocation {19,8}, level[0] + ATTENOFF_MAX_LEVEL*2);
        Pack(data, PackLocation {27,8}, level[1] + ATTENOFF_MAX_LEVEL*2);
        Pack(data, PackLocation {35,1}, mix);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        offset[0] = Unpack(data, PackLocation {0,9}) - 256;
        offset[1] = Unpack(data, PackLocation {10,9}) - 256;
        level[0] = Unpack(data, PackLocation {19,8}) - ATTENOFF_MAX_LEVEL*2;
        level[1] = Unpack(data, PackLocation {27,8}) - ATTENOFF_MAX_LEVEL*2;
        mix = Unpack(data, PackLocation {35,1});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "2=Mix A&B";
        help[HEMISPHERE_HELP_CVS]      = "CV Inputs 1,2";
        help[HEMISPHERE_HELP_OUTS]     = "Outputs A,B";
        help[HEMISPHERE_HELP_ENCODER]  = "Offset V / Level %";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor;
    int level[2];
    int offset[2];
    bool mix = false;
    bool mix_final = false;
    
    void DrawInterface() {
        ForEachChannel(ch)
        {
            gfxPrint(0, 15 + (ch * 20), (hemisphere ? (ch ? "D " : "C ") : (ch ? "B " : "A ")));
            int cv = offset[ch] * ATTENOFF_INCREMENTS;
            gfxPrintVoltage(cv);
            gfxPrint(16, 25 + (ch * 20), Proportion(level[ch], 63, 100));
            gfxPrint("%");
        }

        if (mix_final) {
            gfxIcon(1, 25, DOWN_ICON);
        }

        if (cursor == 4) {
            if (CursorBlink()) {
                gfxFrame(0, 24, 9, 10);
            }
        } else {
            gfxCursor(12, 23 + cursor * 10, 37);
        }    
    }

};
