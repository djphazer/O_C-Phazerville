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

// Hijacking ProbDiv triggers, if available, by pretending to be ProbMeloD ;)
#include "HSProbLoopLinker.h"

class Pigeons : public HemisphereApplet {
public:
    ProbLoopLinker *loop_linker = loop_linker->get();

    enum PigeonCursor {
        CHAN1_V1, CHAN1_V2,
        CHAN1_MOD,
        CHAN2_V1, CHAN2_V2,
        CHAN2_MOD,
        SCALE, ROOT_NOTE,

        CURSOR_LAST = ROOT_NOTE
    };

    const char* applet_name() {
        return "Pigeons";
    }

    void Start() {
        ForEachChannel(ch) {
            pigeons[ch].mod = 7 + ch*3;
            QuantizerConfigure(ch, scale);
        }
    }

    void Controller() {
        loop_linker->RegisterMelo(hemisphere);

        ForEachChannel(ch)
        {
            // CV modulation of modulo value
            pigeons[ch].mod_v = pigeons[ch].mod;
            Modulate(pigeons[ch].mod_v, ch, 1, 64);

            if (loop_linker->TrigPop(ch) || Clock(ch)) {
                int signal = QuantizerLookup(ch, pigeons[ch].Bump() + 64) + (root_note << 7);
                Out(ch, signal);

                pigeons[ch].pulse = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            }
            if (pigeons[ch].pulse) --pigeons[ch].pulse;
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, CURSOR_LAST);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }

        // param LUT
        const struct { uint8_t &p; int min, max; } params[] = {
            { pigeons[0].val[0], 0, 63 }, // CHAN1_V1
            { pigeons[0].val[1], 0, 63 }, // CHAN1_V2
            { pigeons[0].mod, 1, 64 }, // CHAN1_MOD
            { pigeons[1].val[0], 0, 63 }, // CHAN2_V1
            { pigeons[1].val[1], 0, 63 }, // CHAN2_V2
            { pigeons[1].mod, 1, 64 }, // CHAN2_MOD
            { root_note, 0, 11 }, // DUMMY?
            { root_note, 0, 11 }, // ROOT NOTE
        };

        if (cursor == SCALE) {
            scale += direction;
            if (scale >= OC::Scales::NUM_SCALES) scale = 0;
            if (scale < 0) scale = OC::Scales::NUM_SCALES - 1;
            QuantizerConfigure(0, scale);
            QuantizerConfigure(1, scale);

            return;
        }

        params[cursor].p = constrain(params[cursor].p + direction, params[cursor].min, params[cursor].max);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, pigeons[0].val[0]);
        Pack(data, PackLocation {6,6}, pigeons[0].val[1]);
        Pack(data, PackLocation {12,6}, pigeons[0].mod - 1);
        Pack(data, PackLocation {18,6}, pigeons[1].val[0]);
        Pack(data, PackLocation {24,6}, pigeons[1].val[1]);
        Pack(data, PackLocation {30,6}, pigeons[1].mod - 1);
        Pack(data, PackLocation {36,8}, scale);
        Pack(data, PackLocation {44,4}, root_note);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        pigeons[0].val[0] = Unpack(data, PackLocation {0,6});
        pigeons[0].val[1] = Unpack(data, PackLocation {6,6});
        pigeons[0].mod =    Unpack(data, PackLocation {12,6}) + 1;
        pigeons[1].val[0] = Unpack(data, PackLocation {18,6});
        pigeons[1].val[1] = Unpack(data, PackLocation {24,6});
        pigeons[1].mod =    Unpack(data, PackLocation {30,6}) + 1;
        scale     =    Unpack(data, PackLocation {36,8});
        root_note =    Unpack(data, PackLocation {44,4});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "Clock  1,2";
        help[HEMISPHERE_HELP_CVS]      = "Modulo 1,2";
        help[HEMISPHERE_HELP_OUTS]     = "Pitch  1,2";
        help[HEMISPHERE_HELP_ENCODER]  = "Params";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor;
    struct Pigeon {
        bool index;
        uint8_t val[2] = {1, 2};
        uint8_t mod = 10;
        uint8_t mod_v = 10;
        uint32_t pulse;

        uint8_t Get() { return val[index]; }
        uint8_t Bump() {
            val[index] = (val[0] + val[1]) % mod_v;
            index = !index;
            return val[index];
        }
    } pigeons[2];

    int scale = 16; // Pentatonic minor 
    uint8_t root_note = 4; // key of E

    void DrawInterface() {
        // cursor LUT
        const struct { uint8_t x, y, w; } cur[CURSOR_LAST+1] = {
            { 1, 22, 13 }, // val1
            { 25, 22, 13 }, // val2
            { 49, 22, 13 }, // mod

            { 1, 43, 13 }, // val1
            { 25, 43, 13 }, // val2
            { 49, 43, 13 }, // mod

            { 10, 63, 25 }, // scale
            { 40, 63, 13 }, // root note
        };

        ForEachChannel(ch) {
            int y = 14+ch*21;
            gfxPrint(1 + pad(10, pigeons[ch].val[0]), y, pigeons[ch].val[0]);
            gfxPrint(16, y, "+");
            gfxPrint(25 + pad(10, pigeons[ch].val[1]), y, pigeons[ch].val[1]);
            gfxPrint(40, y, "%");
            gfxPrint(49, y, pigeons[ch].mod_v);

            y += 10;
            gfxIcon( 5+(pigeons[ch].index ? 24 : 0), y, NOTE_ICON);
            gfxIcon( 16, y, pigeons[ch].pulse ? SINGING_PIGEON_ICON : SILENT_PIGEON_ICON );

            y += 8;
            gfxLine(4, y, 54, y); // ---------------------------------- //
        }

        gfxPrint(10, 55, OC::scale_names_short[scale]);
        gfxPrint(40, 55, OC::Strings::note_names_unpadded[root_note]);

        // i'm proud of this one:
        gfxCursor( cur[cursor].x, cur[cursor].y, cur[cursor].w );
    }

};
