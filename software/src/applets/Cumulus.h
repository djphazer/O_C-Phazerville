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

#include "../SegmentDisplay.h"

#define ACC_MIN_B 0
#define ACC_MAX_B 15

class Cumulus : public HemisphereApplet {
public:

    enum CumuCursor {
        OPERATION,
        CONSTANT_B,
        OUTMODE,
        LAST_CURSOR,
    };

    enum AccOperator {
        ADD,
        SUB,
        WXOR_LSHIFT, // wide xor with rotl
        OP_LAST
    };

    enum OutputMode {
        SINGLE,
        OUTMODE_LAST
    };

    const char* applet_name() {
        return "Cumulus";
    }

    void Start() {
        cursor = 0;
        accoperator = ADD;
        acc_register = 0;
        b_constant = 0;

        segment.Init(SegmentSize::TINY_SEGMENTS);
    }



    void Controller() {

        if (DetentedIn(0) > 0) {
            b_constant = ProportionCV(In(0), ACC_MAX_B + 1);
            b_constant = constrain(b_constant, ACC_MIN_B, ACC_MAX_B);   
        }

        if (Clock(0)) {
            switch ((AccOperator)accoperator) {
            case ADD:
                acc_register += b_constant;
                break;
            case SUB:
                acc_register -= b_constant;
                break; 
            case WXOR_LSHIFT:
                mask = (b_constant << 4) | b_constant;
                acc_register ^= mask;
                acc_register = (acc_register << 1) | (acc_register >> 7);
            default:
                break;
            }

            switch ((OutputMode)outmode) {
            case SINGLE:
                GateOut(0, acc_register & 1);
                GateOut(1, (acc_register >> 1) & 1);
                break;
            default:
                break;
            }
        }

        if (Clock(1)) {
            acc_register = 0;
        }

    }

    void View() {
        DrawSelector();
        DrawIndicator();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_CURSOR);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_CURSOR);
            return;
        }

        switch ((CumuCursor)cursor) {
        case OPERATION:
            accoperator = (AccOperator) constrain( + direction, 0, OP_LAST - 1);
            break;
        case CONSTANT_B:
            b_constant = constrain(b_constant + direction, ACC_MIN_B, ACC_MAX_B);
            break;
        case OUTMODE:
            break;
        default:
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // Pack(data, PackLocation {start, end+1}, xxx);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        // xxx = Unpack(data, PackLocation {start, bits});
    }

protected:
    void SetHelp() {
    //                                    "------------------" <-- Size Guide      
        help[HEMISPHERE_HELP_DIGITALS] =  "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS] =       "1=CONSTANT ";
        help[HEMISPHERE_HELP_OUTS] =      "A= B=";
        help[HEMISPHERE_HELP_ENCODER] =   "Mode: Outs / Ops";
    //                                    "------------------" <-- Size Guide       
    }

private:
    int cursor;
    AccOperator accoperator;
    OutputMode outmode;

    uint8_t b_constant;
    uint8_t b_mod;

    uint8_t acc_register;
    uint8_t mask;

    SegmentDisplay segment;


    void DrawSelector() {}

    void DrawIndicator() {

    }
};