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

// Modified by Samuel Burt 2023

#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"

class RunglBook : public HemisphereApplet {
public:

    const char* applet_name() {
        return "RunglBook";
    }

    void Start() {
        threshold = (12 << 7) * 2;
    }

    void Controller() {
        if (Clock(0)) {
            if (Gate(1)) {
                // Digital 2 freezes the buffer, so just rotate left
                reg = (reg << 1) | ((reg >> 7) & 0x01);
            } else {
                byte b0 = In(0) > threshold ? 0x01 : 0x00;
                reg = (reg << 1) | b0;
            }

            int rungle = Proportion(reg & 0x07, 0x07, HEMISPHERE_MAX_CV);
            int rungle_tap = Proportion((reg >> 5) & 0x07, 0x07, HEMISPHERE_MAX_CV);

            Out(0, rungle);
            Out(1, rungle_tap);
        }
    }

    void View() {
        gfxPrint(1, 15, "Thr:");
        gfxPrintVoltage(threshold);
        gfxSkyline();
    }

    void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        threshold += (direction * 128);
        threshold = constrain(threshold, (12 << 7), (12 << 7) * 5); // 1V - 5V
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,16}, threshold);
        return data;
    }
    void OnDataReceive(uint64_t data) {
        threshold = Unpack(data, PackLocation {0,16});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Freeze";
        help[HEMISPHERE_HELP_CVS]      = "1=Signal";
        help[HEMISPHERE_HELP_OUTS]     = "A=Rungle B=Alt";
        help[HEMISPHERE_HELP_ENCODER]  = "Threshold";
        //                               "------------------" <-- Size Guide
    }
    
private:
    byte reg;
    uint16_t threshold;
};
