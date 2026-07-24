// Copyright (c) 2018, Jason Justian
// Panner variant copyright (C) 2026, Eric Gao
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

#define PANNER_MAX_VALUE 255
#define PANNER_CV_RANGE (5 * ONE_OCTAVE)  // 0..5V control range

// A CV-controllable panner derived from Xfader.
// One signal (CV1) is routed between OUT1 (left) and OUT2 (right).
// The encoder sets a base position; CV2 (0..5V, unipolar) adds a rightward offset.
class Panner : public HemisphereApplet {
  APPLET_INTERFACE(Panner, "Panner", PhzIcons::mixerBal);

private:
    uint8_t base_position;
    int pos = 128;  // live position (base + CV2)

    void DrawInterface() {
        const int bar_x = 14;
        const int bar_w = 48;

        // Output gain for each side (complementary)
        int left_gain = PANNER_MAX_VALUE - pos;  // OUT1 / left
        int right_gain = pos;                     // OUT2 / right

        // Left output level bar
        gfxPrint(1, 20, "L");
        gfxFrame(bar_x, 20, bar_w, 7);
        gfxRect(bar_x, 20, Proportion(left_gain, PANNER_MAX_VALUE, bar_w), 7);

        // Right output level bar
        gfxPrint(1, 34, "R");
        gfxFrame(bar_x, 34, bar_w, 7);
        gfxRect(bar_x, 34, Proportion(right_gain, PANNER_MAX_VALUE, bar_w), 7);

        // Position readout: tick = encoder base, caret = live pos
        gfxFrame(bar_x, 48, bar_w, 5);
        int bx = bar_x + Proportion(base_position, PANNER_MAX_VALUE, bar_w - 1);
        gfxLine(bx, 46, bx, 54);
        int px = bar_x + Proportion(pos, PANNER_MAX_VALUE, bar_w - 1);
        gfxRect(px - 1, 49, 3, 3);
    }
};

FLASHMEM void Panner::Start() {
    base_position = 128;  // center
}

void Panner::Controller() {
    int signal = In(0);
    int cv2 = In(1);

    int offset = Proportion(constrain(cv2, 0, PANNER_CV_RANGE),
                            PANNER_CV_RANGE, PANNER_MAX_VALUE);  // 0..5V -> rightward

    pos = constrain((int)base_position + offset, 0, PANNER_MAX_VALUE);

    Out(0, Proportion(PANNER_MAX_VALUE - pos, PANNER_MAX_VALUE, signal));  // OUT1 = left
    Out(1, Proportion(pos, PANNER_MAX_VALUE, signal));                     // OUT2 = right
}

FLASHMEM void Panner::View() {
    DrawInterface();
}

FLASHMEM void Panner::OnEncoderMove(int direction) {
    base_position = constrain((int)base_position + direction, 0, PANNER_MAX_VALUE);
}

uint64_t Panner::OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation {0,8}, base_position);
    return data;
}

void Panner::OnDataReceive(uint64_t data) {
    base_position = Unpack(data, PackLocation {0,8});
}

void Panner::SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "Signal";
    help[HELP_CV2]      = "Pan CV";
    help[HELP_OUT1]     = "Left";
    help[HELP_OUT2]     = "Right";
    help[HELP_EXTRA1]   = "Enc: Position";
    help[HELP_EXTRA2]   = "";
    //                    "---------------------" <-- Extra text size guide
}
