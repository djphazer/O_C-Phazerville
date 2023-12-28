// Copyright (c) 2021, Benjamin Rosenbach
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

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define HEM_SHREDDER_ANIMATION_SPEED 500
#define HEM_SHREDDER_DOUBLE_CLICK_DELAY 5000
#define HEM_SHREDDER_POS_5V 7680 // 5 * (12 << 7)
#define HEM_SHREDDER_NEG_3V 4608 // 3 * (12 << 7)

class Shredder : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Shredder";
    }

    void Start() {
        step = 0;
        replay = 0;
        reset = true;
        quant_channels = 0;
        scale = OC::Scales::SCALE_NONE;
        QuantizerConfigure(0, scale);
        ForEachChannel(ch) {
            Shred(ch);
        }
        VolageOut();
    }

    void Controller() {
        if (Clock(1)) {
            step = 0; // Reset
            reset = true;
            VolageOut();
        }

        if (Clock(0)) {
            // Are the X or Y position being set? If so, get step coordinates. Otherwise,
            // simply play current step and advance it. This way, the applet can be used as
            // a more conventional arpeggiator as well as a Cartesian one.
            if (DetentedIn(0) || DetentedIn(1)) {
                int x = ProportionCV(In(0), 4);
                int y = ProportionCV(In(1), 4);
                if (x > 3) x = 3;
                if (y > 3) y = 3;
                step = (y * 4) + x;
                VolageOut();
            } else {
                if (!reset) {
                    ++step;
                }
                reset = false;
                if (step > 15) step = 0;
                VolageOut();
            }
            replay = 0;
        } else if (replay) {
            VolageOut();
            replay = 0;
        }

        // Handle imprint confirmation animation
        if (--confirm_animation_countdown < 0) {
            confirm_animation_position--;
            confirm_animation_countdown = HEM_SHREDDER_ANIMATION_SPEED;
        }

        // Handle double click delay
        if (double_click_delay > 0) {
            // decrement delay and if it's 0, move the cursor
            if (--double_click_delay < 1) {
                // if we hit zero before being reset (aka no double click), move the cursor
                CursorButton();
            }
        }
    }

    void View() {
        DrawParams();
        DrawMeters();
        DrawGrid();
    }

    void CursorButton() {
        CursorAction(cursor, 3);
    }

    void OnButtonPress() {
        if (cursor < 2) {
            // first two cursor params support double-click to shred voltages
            if (double_click_delay == 0) {
                // first click
                double_click_delay = HEM_SHREDDER_DOUBLE_CLICK_DELAY;    
            } else {
                // second click
                double_click_delay = 0; // kill the delay
                Shred(cursor);
            }
        } else {
            CursorButton();
        }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        if (cursor < 2) {
            range[cursor] += direction;
            if (bipolar[cursor]) {
                if (range[cursor] > 3) {
                    range[cursor] = 0;
                    bipolar[cursor] = false;
                } else if (range[cursor] < 1) {
                    range[cursor] = 5;
                    bipolar[cursor] = false;
                }
            } else {
                if (range[cursor] > 5) {
                    range[cursor] = 1;
                    bipolar[cursor] = true;
                } else if (range[cursor] < 0) {
                    range[cursor] = 3;
                    bipolar[cursor] = true;
                }
            }
        }
        if (cursor == 2) quant_channels = constrain(quant_channels + direction, 0, 2);
        if (cursor == 3) {
          scale += direction;
          if (scale >= OC::Scales::NUM_SCALES) scale = 0;
          if (scale < 0) scale = OC::Scales::NUM_SCALES - 1;
          QuantizerConfigure(0, scale);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // Not enough room to save the sequences, so we'll just have to save settings
        Pack(data, PackLocation {0,4}, range[0]); // range will never be more than 4 bits
        Pack(data, PackLocation {4,1}, int(bipolar[0]));
        Pack(data, PackLocation {8,4}, range[1]);
        Pack(data, PackLocation {12,1}, int(bipolar[1]));
        Pack(data, PackLocation {16,8}, quant_channels);
        Pack(data, PackLocation {24,8}, scale);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        range[0] = Unpack(data, PackLocation {0,4}); // only 4 bits used for range
        bipolar[0] = Unpack(data, PackLocation {4,1}); 
        range[1] = Unpack(data, PackLocation {8,4});
        bipolar[1] = Unpack(data, PackLocation {12,1}); 
        quant_channels = Unpack(data, PackLocation {16,8});
        scale = Unpack(data, PackLocation {24,8});
        QuantizerConfigure(0, scale);
        ForEachChannel(ch) {
            Shred(ch);
        }
        VolageOut();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=X     2=Y";
        help[HEMISPHERE_HELP_OUTS]     = "A=Ch 1  B=Ch 2";
        help[HEMISPHERE_HELP_ENCODER]  = "DblClk to Shred";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor;

    // Sequencer state
    uint8_t step; // Current step number
    int sequence[2][16];
    int current[2];
    bool replay; // When the encoder is moved, re-quantize the output
    bool reset;

    // settings
    int range[2] = {1,0};
    bool bipolar[2] = {false, false};
    int8_t quant_channels;
    int scale;

    // Variables to handle imprint confirmation animation
    int confirm_animation_countdown;
    int confirm_animation_position;
    // Variable for double-clicking to shred voltage
    int double_click_delay;

    void DrawParams() {
        // Channel 1 voltage
        gfxPrint(1, 15, "1:+");
        gfxPrint(19, 15, (char) (range[0]));
        if (bipolar[0]) {
          gfxPrint(13, 18, "-");
        }
        if (cursor == 0) gfxCursor(13, 23, 12);

        // Channel 2 voltage
        gfxPrint(32, 15, "2:+");
        gfxPrint(50, 15, (char) (range[1]));
        if (bipolar[1]) {
          gfxPrint(44, 18, "-");
        }
        if (cursor == 1) gfxCursor(44, 23, 12);

        // quantize channel selection
        gfxIcon(32, 25, SCALE_ICON);
        if (quant_channels == 0) {
          gfxPrint(42, 25, "1+2");
        } else {
          gfxPrint(48, 25, (char) quant_channels);
        }
        if (cursor == 2) gfxCursor(42, 33, 20);

        // quantize scale selection
        gfxPrint(32, 35, OC::scale_names_short[scale]);
        if (cursor == 3) gfxCursor(32, 43, 30);

    }

    void DrawMeters() {
      ForEachChannel(ch) {
        int o = ch * 10; // offset
        gfxLine(34, 47+o, 62, 47+o); // top line
        gfxLine(34, 50+o, 62, 50+o); // bottom line
        gfxLine(44, 45+o, 44, 52+o); // zero line
        // 10 pixels for neg, 18 pixels for pos
        if (current[ch] > 0) {
          int w = Proportion(current[ch], HEM_SHREDDER_POS_5V, 18);
          gfxRect(45, 48+o, w, 2);
        } else {
          int w = Proportion(-current[ch], HEM_SHREDDER_NEG_3V, 10);
          gfxRect(44-w, 48+o, w, 2);
        }
      }
    }
    
    void DrawGrid() {
        // Draw the Cartesian plane
        for (int s = 0; s < 16; s++) gfxFrame(1 + (8 * (s % 4)), 26 + (8 * (s / 4)), 5, 5);

        // Crosshairs for play position
        int cxy = step / 4;
        int cxx = step % 4;
        gfxDottedLine(3 + (8 * cxx), 26, 3 + (8 * cxx), 58, 2);
        gfxDottedLine(1, 28 + (8 * cxy), 32, 28 + (8 * cxy), 2);
        gfxRect(1 + (8 * cxx), 26 + (8 * cxy), 5, 5);

        // Draw imprint animation, if necessary
        if (confirm_animation_position > -1) {
            int progress = 16 - confirm_animation_position;
            for (int s = 0; s < progress; s++)
            {
                gfxRect(1 + (8 * (s / 4)), 26 + (8 * (s % 4)), 7, 7);
            }
        }
    }

    void Shred(int ch) {
        int max;
        int min;
        for (int i = 0; i < 16; i++) {
            if (range[ch] == 0) {
                sequence[ch][i] = 0;
            } else {
                max = range[ch] * (12 << 7);
                min = bipolar[ch] ? -max : 0;
                sequence[ch][i] = random(min, max);
            }
        }

        // start imprint animation
        confirm_animation_position = 16;
        confirm_animation_countdown = HEM_SHREDDER_ANIMATION_SPEED;
    }

    void VolageOut() {
        ForEachChannel(ch) {
            current[ch] = sequence[ch][step];
            int8_t qc = quant_channels - 1; 
            if (qc < 0 || qc == ch) current[ch] = Quantize(0, current[ch], 0, 0);
            Out(ch, current[ch]);
        }
    }
};
