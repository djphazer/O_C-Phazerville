// Copyright (c) 2018, Jason Justian
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

class ScaleDuet : public HemisphereApplet {
public:

    const char* applet_name() {
        return "ScaleDuet";
    }
    const uint8_t* applet_icon() { return PhzIcons::scaleDuet; }

    void SetMask(uint16_t &mask) {
        quant.Configure(OC::Scales::GetScale(OC::Scales::SCALE_SEMI), mask);
    }
    void Start() {
        ForEachChannel(scale)
        {
            mask[scale] = 0xffff;
        }
        quant.Init();
        SetMask(mask[0]);
        last_scale = 0;
    }

    void Controller() {
        // unclock if >2V on CV2
        if (In(1) > 24*128) {
            continuous = 1;
        }

        // Prepare to read pitch and send gate in the near future; there's a slight
        // lag between when a gate is read and when the CV can be read.
        if (Clock(0)) {
            continuous = 0;
            StartADCLag();
        }

        if (continuous || EndOfADCLag()) {
            uint8_t scale = Gate(1);
            if (scale != last_scale) {
                SetMask(mask[scale]);
                last_scale = scale;
            }

            int new_pitch = quant.Process(In(0), 0, 0);
            if (q_pitch != new_pitch)
                ClockOut(1);
            q_pitch = new_pitch;

            Out(0, q_pitch);
        }
    }

    void View() {
        if (!continuous) gfxIcon(0, 13, CLOCK_ICON);
        DrawKeyboard();
        DrawMaskIndicators();
    }

    void OnButtonPress() {
        uint8_t scale = cursor > 11 ? 1 : 0;
        uint8_t bit = cursor - (scale * 12);

        // Toggle the mask bit at the cursor position
        mask[scale] ^= (0x01 << bit);
        if (scale == last_scale)
            SetMask(mask[scale]);
    }

    void OnEncoderMove(int direction) {
        if (cursor == 0 && direction == -1) cursor = 1;
        cursor = constrain(cursor + direction, 0, 23);
        ResetCursor();
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,12}, mask[0]);
        Pack(data, PackLocation {12,12}, mask[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        mask[0] = Unpack(data, PackLocation {0,12});
        mask[1] = Unpack(data, PackLocation {12,12});

        last_scale = 0;
        SetMask(mask[last_scale]);
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "S&H Clk";
    help[HELP_DIGITAL2] = "Scale2";
    help[HELP_CV1]      = "CV";
    help[HELP_CV2]      = "Unclock";
    help[HELP_OUT1]     = "Pitch";
    help[HELP_OUT2]     = "Trig";
    help[HELP_EXTRA1] = "Trig fires when";
    help[HELP_EXTRA2] = "pitch changes";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    uint16_t mask[2];
    uint8_t cursor; // 0-11=Scale 1; 12-23=Scale 2
    uint8_t last_scale; // The most-recently-used scale (used to set the mask when necessary)
    int q_pitch;
    bool continuous = 1;

    braids::Quantizer quant;

    void DrawKeyboard() {
        // Border
        gfxFrame(0, 27, 63, 32);

        // White keys
        for (uint8_t x = 0; x < 8; x++)
        {
            gfxLine(x * 8, 27, x * 8, 58);
        }

        // Black keys
        for (uint8_t i = 0; i < 6; i++)
        {
            if (i != 2) { // Skip the third position
                uint8_t x = (i * 8) + 6;
                gfxRect(x, 27, 5, 16);
            }
        }

        // Which scale
        gfxPrint(10, 15, "Scale ");
        gfxPrint(cursor < 12 ? 1 : 2);
    }

    void DrawMaskIndicators() {
        uint8_t scale = cursor < 12 ? 0 : 1;
        uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
        uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
        for (uint8_t i = 0; i < 12; i++)
        {
            if ((mask[scale] >> i) & 0x01) gfxInvert(x[i], (p[i] ? 37 : 51), 4 - p[i], 4 - p[i]);
            if (i == (cursor - (scale * 12))) {
                gfxCursor(x[i] - 1, p[i] ? 24 : 60, p[i] ? 5 : 6);
                gfxCursor(x[i] - 1, p[i] ? 25 : 61, p[i] ? 5 : 6);
            }
        }

        // If C is selcted, display a selector on the higher C, too
        if (mask[scale] & 0x01) gfxInvert(58, 51, 3, 4);
    }
};
