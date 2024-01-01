// Copyright (c) 2023, Nicholas J. Michalek
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

#define CAL8_PRECISION 10000

class Calibr8 : public HemisphereApplet {
public:
    enum CalCursor {
        SCALEFACTOR_A,
        TRANS_A,
        OFFSET_A,
        SCALEFACTOR_B,
        TRANS_B,
        OFFSET_B,

        MAX_CURSOR = OFFSET_B
    };

    const char* applet_name() {
        return "Calibr8";
    }

    void Start() {
        clocked_mode = false;
        AllowRestart();
    }

    void Controller() {
        bool clocked = Clock(0);
        if (clocked) clocked_mode = true;

        ForEachChannel(ch) {
            uint8_t input_note = MIDIQuantizer::NoteNumber(In(ch), 0);

            // clocked transpose
            if (!clocked_mode || clocked)
                transpose_active[ch] = transpose[ch];

            input_note += transpose_active[ch];

            int output_cv = MIDIQuantizer::CV(input_note) * (CAL8_PRECISION + scale_factor[ch]) / CAL8_PRECISION;
            output_cv += offset[ch];

            Out(ch, output_cv);
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, MAX_CURSOR);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, MAX_CURSOR);
            return;
        }

        bool ch = (cursor > OFFSET_A);
        switch (cursor) {
        case OFFSET_A:
        case OFFSET_B:
            offset[ch] = constrain(offset[ch] + direction, -100, 100);
            break;

        case SCALEFACTOR_A:
        case SCALEFACTOR_B:
            scale_factor[ch] = constrain(scale_factor[ch] + direction, -500, 500);
            break;

        case TRANS_A:
        case TRANS_B:
            transpose[ch] = constrain(transpose[ch] + direction, -36, 60);
            break;

        default: break;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0,10}, scale_factor[0] + 500); 
        Pack(data, PackLocation {10,10}, scale_factor[1] + 500); 
        Pack(data, PackLocation {20, 8}, offset[0] + 100); 
        Pack(data, PackLocation {28, 8}, offset[1] + 100); 
        Pack(data, PackLocation {36, 7}, transpose[0] + 36); 
        Pack(data, PackLocation {43, 7}, transpose[1] + 36); 
        return data;
    }

    void OnDataReceive(uint64_t data) {
        scale_factor[0] = Unpack(data, PackLocation { 0,10}) - 500;
        scale_factor[1] = Unpack(data, PackLocation {10,10}) - 500;
        offset[0]       = Unpack(data, PackLocation {20, 8}) - 100;
        offset[1]       = Unpack(data, PackLocation {28, 8}) - 100;
        transpose[0]    = Unpack(data, PackLocation {36, 7}) - 36;
        transpose[1]    = Unpack(data, PackLocation {43, 7}) - 36;
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock";
        help[HEMISPHERE_HELP_CVS]      = "1,2=Pitch inputs";
        help[HEMISPHERE_HELP_OUTS]     = "A,B=Pitch outputs";
        help[HEMISPHERE_HELP_ENCODER]  = "Scale/Offset/Trans";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor;
    bool clocked_mode = false;
    int scale_factor[2] = {0,0}; // precision of 0.01% as an offset from 100%
    int offset[2] = {0,0}; // fine-tuning offset
    int transpose[2] = {0,0}; // in semitones
    int transpose_active[2] = {0,0}; // held value while waiting for trigger
    
    void DrawInterface() {
        ForEachChannel(ch) {
            int y = 14 + ch*21;
            gfxPrint(0, y, ch?"B:":"A:");

            int whole = (scale_factor[ch] + CAL8_PRECISION) / 100;
            int decimal = (scale_factor[ch] + CAL8_PRECISION) % 100;
            gfxPrint(12 + pad(100, whole), y, whole);
            gfxPrint(".");
            if (decimal < 10) gfxPrint("0");
            gfxPrint(decimal);
            gfxPrint("%");

            // second line
            y += 10;
            gfxIcon(0, y, BEND_ICON);
            gfxPrint(8, y, transpose[ch]);
            gfxIcon(32, y, UP_DOWN_ICON);
            gfxPrint(40, y, offset[ch]);
        }
        gfxLine(0, 33, 63, 33); // gotta keep em separated

        bool ch = (cursor > OFFSET_A);
        int param = (cursor % 3);
        if (param == 0) // Scaling
            gfxCursor(12, 22 + ch*21, 40);
        else // Transpose or Fine Tune
            gfxCursor(8 + (param-1)*32, 32 + ch*21, 20);

        gfxSkyline();
    }
};
