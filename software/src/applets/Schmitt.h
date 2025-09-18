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

#define SCHMITT_FLASH_SPEED 4000

class Schmitt : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "SchmittTr";
    }
    const uint8_t* applet_icon() { return PhzIcons::schmitt; }

    void Start() {
        low = 3200; // ~2.1V
        high = 3968; // ~2.6V
        cursor = 0;
        gate_countdown = 0;
    }

    void Controller() {
        ForEachChannel(ch)
        {
            if (!state[ch] && In(ch) > high) state[ch] = 1;
            if (state[ch] && In(ch) < low) state[ch] = 0;
            GateOut(ch, state[ch]);
        }

        if (--gate_countdown < -SCHMITT_FLASH_SPEED) gate_countdown = SCHMITT_FLASH_SPEED;
    }

    void View() {
        DrawInterface();
    }

    // No controls
    void OnButtonPress() {
        if (++cursor == 3) cursor = 0;
    }

    void OnEncoderMove(int direction) {
        if (cursor == 1) {
            low += (64 * direction);
            low = constrain(low, 64, high - 64);
        }
        if (cursor == 2) {
            high += (64 * direction);
            high = constrain(high, low + 64, HEMISPHERE_MAX_CV);
        }
        ResetCursor();
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,16}, low);
        Pack(data, PackLocation {16,16}, high);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        low = Unpack(data, PackLocation {0,16});
        high = Unpack(data, PackLocation {16,16});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "CV 1";
    help[HELP_CV2]      = "CV 2";
    help[HELP_OUT1]     = "Gate 1";
    help[HELP_OUT2]     = "Gate 2";
    help[HELP_EXTRA1] = "Set: High/Low Thresh";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor; // 0 = locked 1 = high threshold, 2 = low threshold

    // Housekeeping
    bool state[2];
    int gate_countdown;

    // Settings
    uint16_t low;
    uint16_t high;

    void DrawInterface() {
        // Draw two Schmitt Trigger symbols and inputs
        ForEachChannel(ch)
        {
            uint8_t x = 3 + (14 * ch);
            uint8_t y = 26 + (16 * ch);
            DrawSchmittTriggerAtPositionWithState(x, y, state[ch]);

            // Input monitor
            gfxLine(3, 22 + (38 * ch), ProportionCV(In(ch), 58) + 3, 22 + (38 * ch));

            // Line to input
            if (ch == 0) gfxDottedLine(3, 32, 3, 23, 2);
            else {
                gfxDottedLine(15, 49, 3, 49, 2);
                gfxDottedLine(3, 49, 3, 59, 2);
            }
        }

        // Draw the threshold line
        int lx = ProportionCV(low, 58) + 3;
        int hx = ProportionCV(high, 58) + 3;
        gfxLine(lx, 15, hx, 15);
        if (cursor != 1 || CursorBlink()) gfxLine(lx, 15, lx, 18);
        if (cursor != 2 || CursorBlink()) gfxLine(hx, 15, hx, 18);

    }

    void DrawSchmittTriggerAtPositionWithState(uint8_t x, uint8_t y, bool state) {
        gfxCircle(x, y + 7, 2); // Input point
        gfxLine(x + 2, y + 7, x + 10, y + 7); // Input line
        gfxLine(x + 10, y, x + 10, y + 14); // Base of triangle
        gfxLine(x + 10, y, x + 32, y + 7); // Top angle
        gfxLine(x + 10, y + 14, x + 32, y + 7); // Bottom angle
        gfxLine(x + 32, y + 7, x + 40, y + 7); // Output line
        gfxCircle(x + 42, y + 7, 2); // Output point

        gfxLine(x + 15, y + 5, x + 21, y + 5); // Schmitt symbol, top
        gfxLine(x + 12, y + 9, x + 18, y + 9); // Schmitt symbol, bottom
        gfxLine(x + 15, y + 5, x + 15, y + 9); // Scmitt symbol, left
        gfxLine(x + 18, y + 5, x + 18, y + 9); // Schmitt symbol, right

        if (state) {
            if (gate_countdown > 0) gfxCircle(x + 42, y + 7, 1);
            else gfxCircle(x + 42, y + 7, 4);
        }
    }
};
