// Copyright (c) 2024, Jakob Zerbian
//
// Inspired by Nibbler from Schlappi Engineering
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

#define ACC_MIN_B 0
#define ACC_MAX_B 15

class Cumulus : public HemisphereApplet {
public:

    enum CumuCursor {
        OPERATION,
        OUTMODE_A,
        OUTMODE_B,
        CONSTANT_B,
        LAST_CURSOR,
    };

    enum AccOperator {
        ADD,
        SUB,
        MULADD1,
        XOR_ROTL, //xor with rotl
        SUB_ROTR,
        OP_LAST
    };

    const char* applet_name() {
        return "Cumulus";
    }

    void Start() {
        cursor = 0;
        accoperator = ADD;
        acc_register = 0;
        b_constant = 0;
    }



    void Controller() {
        b_constant_mod = b_constant;
        Modulate(b_constant_mod, 1, 0, ACC_MAX_B);

        a_mod = outmode[0];
        Modulate(a_mod, 0, 0, 7);

        // randomize accumulator register
        if (Clock(1)) {
            acc_register = random(0, 1 << 8);
        }

        if (Clock(0)) {
            switch ((AccOperator)accoperator) {
            case ADD:   acc_register += b_constant_mod; break;
            case SUB:   acc_register -= b_constant_mod; break;
            case MULADD1:   acc_register = acc_register * b_constant_mod + 1; break;
            case XOR_ROTL:
                acc_register ^= b_constant_mod;
                acc_register = (acc_register << 1) | (acc_register >> 7);
                break;
            case SUB_ROTR:
                acc_register -= b_constant_mod;
                acc_register = (acc_register >> 2) | (acc_register << 6);
                break;
            default:
                break;
            }

            GateOut(0, (acc_register >> a_mod) & 1);
            GateOut(1, (acc_register >> outmode[1]) & 1);

        }

    }

    void View() {
        DrawIndicator();
        DrawSelector();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_CURSOR);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_CURSOR - 1);
            return;
        }

        switch ((CumuCursor)cursor) {
        case OPERATION:
            accoperator = (AccOperator) constrain(accoperator + direction, 0, OP_LAST - 1);
            break;
        case CONSTANT_B:
            b_constant = constrain(b_constant + direction, ACC_MIN_B, ACC_MAX_B);
            break;
        case OUTMODE_A:
            outmode[0] = constrain(outmode[0] + direction, 0, 7);
            break;
        case OUTMODE_B:
            outmode[1] = constrain(outmode[1] + direction, 0, 7);
        default:
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 3}, accoperator);
        Pack(data, PackLocation { 3, 4}, b_constant);
        Pack(data, PackLocation { 7, 4}, outmode[0]);
        Pack(data, PackLocation {13, 4}, outmode[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        accoperator = (AccOperator) Unpack(data, PackLocation { 0, 3});
        b_constant = Unpack(data, PackLocation { 3, 4});
        outmode[0] = Unpack(data, PackLocation { 7, 4});
        outmode[1] = Unpack(data, PackLocation {13, 4});
    }

protected:
    void SetHelp() {
    //                                    "------------------" <-- Size Guide      
        help[HEMISPHERE_HELP_DIGITALS] =  "1=Clock 2=Rand Z";
        help[HEMISPHERE_HELP_CVS] =       "1=a mod 2=k mod";
        help[HEMISPHERE_HELP_OUTS] =      "Assignable";
        help[HEMISPHERE_HELP_ENCODER] =   "Select/Push 2 Edit";
    //                                    "------------------" <-- Size Guide       
    }

private:
    int cursor;
    AccOperator accoperator;
    uint8_t outmode[2] = {1, 0};
    uint8_t a_mod;
    uint8_t a_display;

    uint8_t b_constant;
    uint8_t b_constant_mod;
    uint8_t b_display;

    uint8_t acc_register;

    const char* OP_NAMES[OP_LAST] = {"z+k", "z-k", "z*k+1", "(z^k)<<1", "(z-k)>>2"};


    void DrawSelector() {
        a_display = isEditing ? outmode[0] : a_mod;
        gfxBitmap(1, 15, 8, BEAKER_ICON);
        gfxPrint(12, 15, OP_NAMES[accoperator]);

        gfxPrint(1, 26, "A:");
        gfxPrint(15, 26, a_display);

        gfxPrint(32, 26, "B:");
        gfxPrint(47, 26, outmode[1]);

        gfxLine(0, 36, 63, 36);

        switch ((CumuCursor)cursor) {
        case OPERATION:     gfxCursor(11, 23, 50); break;
        case OUTMODE_A:     gfxCursor(14, 34, 16); break;
        case OUTMODE_B:     gfxCursor(46, 34, 16); break;
        case CONSTANT_B:    gfxCursor(36, 48, 25); break;
        default:
            break;
        }
    }

    void DrawIndicator() {
        gfxPrint(1, 40, "k");
        gfxPrint(1, 52, "Z");

        // when editing modulating parameters show original value
        b_display = isEditing ? b_constant : b_constant_mod;

        for (int i = 0; i < 8; i++) {
            gfxPrint(12 + (i * 6), 52, (acc_register >> (7 - i)) & 1);
            
            if (i > 3) gfxPrint(12 + (i * 6), 40, (b_display >> (7 - i)) & 1);
        }

        gfxLine((7 - a_mod) * 6 + 13, 50, (7 - a_mod) * 6 + 17, 50);
        gfxLine((7 - outmode[1]) * 6 + 13, 60, (7 - outmode[1]) * 6 + 17, 60);
    }
};