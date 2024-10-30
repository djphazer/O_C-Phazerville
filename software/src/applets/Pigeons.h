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
#include "../HSProbLoopLinker.h"

class Pigeons : public HemisphereApplet {
public:
    ProbLoopLinker *loop_linker = loop_linker->get();

    enum PigeonCursor {
        CHAN1_V1, CHAN1_V2,
        CHAN1_MOD,
        QUANT_A,

        CHAN2_V1, CHAN2_V2,
        CHAN2_MOD,
        QUANT_B,
        // TODO: octave?

        CURSOR_LAST = QUANT_B
    };

    const char* applet_name() {
        return "Pigeons";
    }

    void Start() {
        ForEachChannel(ch) {
            pigeons[ch].mod = 7 + ch*3;
            qselect[ch] = io_offset + ch;
        }
    }

    void Controller() {
        loop_linker->RegisterMelo(hemisphere);

        ForEachChannel(ch)
        {
            // CV modulation of modulo value
            pigeons[ch].mod_v = pigeons[ch].mod;
            Modulate(pigeons[ch].mod_v, ch, 2, 64);

            if (loop_linker->TrigPop(ch) || Clock(ch)) {
                int signal = HS::QuantizerLookup(qselect[ch], pigeons[ch].Bump() + 64);
                Out(ch, signal);

                pigeons[ch].pulse = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            }
            if (pigeons[ch].pulse) --pigeons[ch].pulse;
        }
    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }

        // param LUT
        const struct { uint8_t &p; int min, max; } params[] = {
            { pigeons[0].val[0], 0, 63 }, // CHAN1_V1
            { pigeons[0].val[1], 0, 63 }, // CHAN1_V2
            { pigeons[0].mod, 2, 64 }, // CHAN1_MOD
            { qselect[0], 0, QUANT_CHANNEL_COUNT - 1 }, // QUANT_A
            { pigeons[1].val[0], 0, 63 }, // CHAN2_V1
            { pigeons[1].val[1], 0, 63 }, // CHAN2_V2
            { pigeons[1].mod, 2, 64 }, // CHAN2_MOD
            { qselect[1], 0, QUANT_CHANNEL_COUNT - 1 }, // QUANT_B
        };

        params[cursor].p = constrain(params[cursor].p + direction, params[cursor].min, params[cursor].max);

        if (cursor == QUANT_A || cursor == QUANT_B) {
          HS::qview = params[cursor].p;
          HS::PokePopup(QUANTIZER_POPUP);
        }
    }

    void AuxButton() {
      if (cursor == QUANT_A || cursor == QUANT_B) {
        HS::QuantizerEdit( qselect[(cursor == QUANT_B)] );
      }
      isEditing = false;
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, pigeons[0].val[0]);
        Pack(data, PackLocation {6,6}, pigeons[0].val[1]);
        Pack(data, PackLocation {12,6}, pigeons[0].mod - 1);
        Pack(data, PackLocation {18,6}, pigeons[1].val[0]);
        Pack(data, PackLocation {24,6}, pigeons[1].val[1]);
        Pack(data, PackLocation {30,6}, pigeons[1].mod - 1);
        Pack(data, PackLocation {36,4}, qselect[0]);
        Pack(data, PackLocation {44,4}, qselect[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        pigeons[0].val[0] = Unpack(data, PackLocation {0,6});
        pigeons[0].val[1] = Unpack(data, PackLocation {6,6});
        pigeons[0].mod =    Unpack(data, PackLocation {12,6}) + 1;
        pigeons[1].val[0] = Unpack(data, PackLocation {18,6});
        pigeons[1].val[1] = Unpack(data, PackLocation {24,6});
        pigeons[1].mod =    Unpack(data, PackLocation {30,6}) + 1;
        qselect[0]     =    Unpack(data, PackLocation {36,4});
        qselect[1]     =    Unpack(data, PackLocation {44,4});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock 1";
    help[HELP_DIGITAL2] = "Clock 2";
    help[HELP_CV1]      = "Modulo1";
    help[HELP_CV2]      = "Modulo2";
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Pitch 2";
    help[HELP_EXTRA1] = "Set: Notes / Modulus";
    help[HELP_EXTRA2] = " Quantizer (Aux=Edit)";
    //                  "---------------------" <-- Extra text size guide
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
            if (0 == (val[0] + val[1])) ++val[index]; // revival
            val[index] = (val[0] + val[1]) % mod_v;
            index = !index;
            return val[index];
        }
    } pigeons[2];

    uint8_t qselect[2] = { 0, 1 };

    void DrawInterface() {
        // cursor LUT
        const struct { uint8_t x, y, w; } cur[CURSOR_LAST+1] = {
            { 1, 22, 13 }, // val1
            { 25, 22, 13 }, // val2
            { 49, 22, 13 }, // mod
            { 48, 33, 13 }, // quantizer for A

            { 1, 47, 13 }, // val1
            { 25, 47, 13 }, // val2
            { 49, 47, 13 }, // mod
            { 48, 58, 13 }, // quantizer for B
        };

        ForEachChannel(ch) {
            int y = 14+ch*25;
            gfxPrint(1 + pad(10, pigeons[ch].val[0]), y, pigeons[ch].val[0]);
            gfxPrint(16, y, "+");
            gfxPrint(25 + pad(10, pigeons[ch].val[1]), y, pigeons[ch].val[1]);
            gfxPrint(40, y, "%");
            gfxPrint(49, y, pigeons[ch].mod_v);

            y += 11;
            gfxIcon( 5+(pigeons[ch].index ? 24 : 0), y, NOTE_ICON);
            gfxIcon( 16, y+3, pigeons[ch].pulse ? SINGING_PIGEON_ICON : SILENT_PIGEON_ICON );
            gfxPrint(48, y, "Q");
            gfxPrint(qselect[ch] + 1);

            y += 11;
            gfxLine(4, y, 60, y); // ---------------------------------- //
        }

        if (QUANT_A == cursor || QUANT_B == cursor)
          gfxSpicyCursor( cur[cursor].x, cur[cursor].y, cur[cursor].w );
        else
          gfxCursor( cur[cursor].x, cur[cursor].y, cur[cursor].w );
    }

};
