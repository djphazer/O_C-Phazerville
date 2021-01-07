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

class GatedVCA : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "Gated VCA";
    }

    void Start() {
        amp_offset_pct = 0;
        amp_offset_cv = 0;
    }

    void Controller() {
        int signal = In(0);
        int amplitude = In(1);
        int output = ProportionCV(amplitude, signal);
        output += amp_offset_cv;
        output = constrain(output, -HEMISPHERE_MAX_CV, HEMISPHERE_MAX_CV);

        if (Gate(0)) Out(0, output); // Normally-off gated VCA output on A
        else Out(0, 0);

        if (Gate(1)) Out(1, 0); // Normally-on ungated VCA output on B
        else Out(1, output);
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
    }

    void OnEncoderMove(int direction) {
        amp_offset_pct = constrain(amp_offset_pct + direction, 0, 100);
        amp_offset_cv = Proportion(amp_offset_pct, 100, HEMISPHERE_MAX_CV);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        return data;
    }

    void OnDataReceive(uint64_t data) {
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "1=A Gate 2=B Revrs";
        help[HEMISPHERE_HELP_CVS] = "1=CV signal 2=Amp";
        help[HEMISPHERE_HELP_OUTS] = "A=Norm off B=N. on";
        help[HEMISPHERE_HELP_ENCODER] = "T=Amp CV Offset";
    }
    
private:
    int amp_offset_pct; // Offset as percentage of max cv
    int amp_offset_cv; // Raw CV offset; calculated on encoder move

    void DrawInterface() {
        gfxPrint(0, 15, "Offset:");
        gfxPrint(pad(100, amp_offset_pct), amp_offset_pct);
        gfxSkyline();
    }
};
