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

#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"

#define SCOPE_CURRENT_SETTING_TIMEOUT 50001
const uint8_t HEM_PPQN_VALUES[] = {1, 2, 4, 8, 16, 24};

class Scope : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Scope";
    }

    void Start() {
        last_bpm_tick = OC::CORE::ticks;
        bpm = 0;
        sample_ticks = 320;
        freeze = 0;
        last_scope_tick = 0;
        current_setting = 0;
        current_display = 0;
    }

    void Controller() {
        if (Clock(0)) {
            int this_tick = OC::CORE::ticks;
            int time = this_tick - last_bpm_tick;
            last_bpm_tick = this_tick;
            bpm = 1000000 / time;
            if (bpm > 9999) bpm = 9999;
        }

        if (Clock(1)) {
            if (last_scope_tick) {
                int cycle_ticks = OC::CORE::ticks - last_scope_tick;
                sample_ticks = cycle_ticks / 64;
                sample_ticks = constrain(sample_ticks, 2, 64000);
            }
            last_scope_tick = OC::CORE::ticks;
        }

        if (!freeze) {
            last_cv = In((current_display & 1) == 1);

            if (--sample_countdown < 1) {
                sample_countdown = sample_ticks;
                ++sample_num %= 128;

                for (int n = 0; n < 2; n++) {
                  int sample = Proportion(In(n) + HEMISPHERE_MAX_INPUT_CV, 2*HEMISPHERE_MAX_INPUT_CV, 255);
                  sample = constrain(sample, 0, 255);
                  snapshot[n][sample_num] = (uint8_t)sample;
                }
            }

            ForEachChannel(ch) Out(ch, In(ch));
        }
    }

    void DrawFullScreen() {
        int thing = (current_display == 4) ? -1 : (current_display & 0x2);
        if (hemisphere == LEFT_HEMISPHERE) {
            DrawInput(thing, 127, 63);
        } else {
            DrawInput(thing);
        }
    }

    void View() {
        if(current_display == 4) {
            DrawInput(-1);
        } else {
            DrawBPM();
            DrawInput((current_display & 2) == 2);
            PrintInput();
        }
        
        DrawCurrentSetting();
        if (freeze) {
            gfxInvert(0, 24, 64, 40);
        }
    }

    void OnButtonPress() {
        if (current_setting == 2 && !EditMode()) // FREEZE button
            freeze = !freeze;
        else if (OC::CORE::ticks - last_encoder_move < SCOPE_CURRENT_SETTING_TIMEOUT) // params visible? toggle edit
            CursorAction(current_setting, 2);
        else // show params
            last_encoder_move = OC::CORE::ticks;
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) { // switch setting
            MoveCursor(current_setting, direction, 2);
        } else { // edit
            if(current_setting == 0) {
                if (sample_ticks < 32) sample_ticks += direction;
                else sample_ticks += direction * 10;
                sample_ticks = constrain(sample_ticks, 2, 64000);
            } else if(current_setting == 1) {
                current_display = constrain(current_display + direction, 0, 4);
            }
        }
        last_encoder_move = OC::CORE::ticks;
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // example: pack property_name at bit 0, with size of 8 bits
        // Pack(data, PackLocation {0,8}, property_name); 
        return data;
    }

    void OnDataReceive(uint64_t data) {
        // example: unpack value at bit 0 with size of 8 bits to property_name
        // property_name = Unpack(data, PackLocation {0,8}); 
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "Clk 1=BPM 2=Cycle1";
        help[HEMISPHERE_HELP_CVS]      = "1=CV1 2=CV2";
        help[HEMISPHERE_HELP_OUTS]     = "A=CV1 B=CV2";
        help[HEMISPHERE_HELP_ENCODER]  = "T=Value P=Setting";
        //                               "------------------" <-- Size Guide
    }
    
private:
    // BPM Calcultion
    int last_bpm_tick;
    int bpm;

    // CV monitor
    int last_cv;
    bool freeze;

    // Scope
    int current_display;
    int current_setting;
    uint8_t snapshot[2][128];
    int sample_ticks; // Ticks between samples
    int sample_countdown; // Last time a sample was taken
    int sample_num; // Current sample number at the start
    int last_encoder_move; // The last the the sample_ticks value was changed
    int last_scope_tick; // Used to auto-calculate sample countdown

    void DrawBPM() {
        gfxPrint(9, 15, "BPM ");
        gfxPrint(bpm / 4);
        gfxLine(0, 24, 63, 24);

        if (OC::CORE::ticks - last_bpm_tick < 1666) gfxBitmap(1, 15, 8, CLOCK_ICON);
    }

    void DrawCurrentSetting() {
        if (OC::CORE::ticks - last_encoder_move < SCOPE_CURRENT_SETTING_TIMEOUT) {
            if(current_setting == 0) {
                gfxPrint(1, 26, "Rate");
                gfxPrint(32, 26, sample_ticks);
            } else if(current_setting == 1) {
                gfxPrint(1, 26, "Mode ");
                if(current_display == 4) {
                    gfxPrint("1,2");
                } else {
                    gfxPrint((current_display & 2) == 2 ? 2 : 1);
                    gfxPrint("+");
                    gfxPrint((current_display & 1) == 1 ? 2 : 1);
                }
            } else if(current_setting == 2) {
                gfxPrint(1, 26, "Freeze ");
                gfxPrint(freeze ? "ON" : "OFF");
            }

            if (EditMode()) gfxInvert(1, 25, 31, 9);
        }
    }

    void PrintInput() {
        gfxLine(0, 53, 63, 53);
        gfxBitmap(1, 55, 8, CV_ICON);
        gfxPos(12, 55);
        gfxPrintVoltage(last_cv);
    }

    void DrawInput(const int input, const int width = 63, int height = 28) {
        if (input < 0) height = 54;
        for (int s = 0; s <= width; s++)
        {
            int n = (sample_num + s) % 128;
            int px, py;
            if (input < 0) { // X-Y mode
                px = Proportion(snapshot[0][n], 255, width);
                py = Proportion(snapshot[1][n], 255, height);
                py = constrain((height - py) + 10, 0, 63);
            } else {
                px = n % width;
                py = Proportion(snapshot[input][n], 255, height);
                py = constrain((height - py) + (63 - height), 0, 63);
            }
            gfxPixel(px, py);
        }
    }
};
