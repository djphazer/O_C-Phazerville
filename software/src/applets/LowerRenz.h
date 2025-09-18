// Copyright (c) 2018, Jason Justian
//
// Port of subset of Low Rents Copyright (c) 2016 Patrick Dowling,
// Copyright (c) 2014 Émilie Gillet, Copyright (c) 2016 Tim Churches
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

#include "../streams_lorenz_generator.h"
#include "../util/util_math.h"
#include "../HSLorenzGeneratorManager.h" // Singleton Lorenz manager

class LowerRenz : public HemisphereApplet {
public:

    const char* applet_name() {
        return "LowerRenz";
    }
    const uint8_t* applet_icon() { return PhzIcons::lowerenz; }

    void Start() {
        freq = 128;
        rho = 64;
    }

    void Controller() {
        if (!Gate(1)) { // Freeze if gated
            int freq_cv = Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 63);
            int rho_cv = Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 31);

            int32_t freq_h = SCALE8_16(constrain(freq + freq_cv, 0, 255));
            freq_h = USAT16(freq_h);
            lorenz_m->SetFreq(hemisphere, freq_h);

            int32_t rho_h = SCALE8_16(constrain(rho + rho_cv, 4, 127));
            lorenz_m->SetRho(hemisphere, USAT16(rho_h));

            if (Clock(0, true)) lorenz_m->Reset(hemisphere);
            lorenz_m->Process();

            // The scaling here is based on observation of the value range
            int x = Proportion(lorenz_m->GetOut(0 + (hemisphere * 2)) - 17000, 25000, HEMISPHERE_MAX_CV);
            int y = Proportion(lorenz_m->GetOut(1 + (hemisphere * 2)) - 17000, 25000, HEMISPHERE_MAX_CV);

            Out(0, x);
            Out(1, y);
        }
    }

    void View() {
        DrawEditor();
        DrawOutput();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }

        if (cursor == 0) freq = constrain(freq + direction, 0, 255);
        if (cursor == 1) rho = constrain(rho + direction, 4, 127);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, (uint8_t)freq);
        Pack(data, PackLocation {8,8}, (uint8_t)rho);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        freq = Unpack(data, PackLocation {0,8});
        rho = Unpack(data, PackLocation {8,8});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Reset";
    help[HELP_DIGITAL2] = "Freeze";
    help[HELP_CV1]      = "Freq";
    help[HELP_CV2]      = "Rho";
    help[HELP_OUT1]     = "X";
    help[HELP_OUT2]     = "Y";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    LorenzGeneratorManager *lorenz_m = lorenz_m->get();
    int freq;
    int rho;
    int cursor; // 0 = Frequency, 1 = Rho

    void DrawEditor() {
        gfxPrint(1, 15, "Freq");
        gfxPrint(1, 24, freq);
        if (cursor == 0) gfxCursor(1, 32, 30);

        gfxPrint(45, 15, "Rho");
        gfxPrint(45 + (rho > 99 ? 0 : 6), 24, rho);
        if (cursor == 1) gfxCursor(32, 32, 31);
    }

    void DrawOutput() {
        gfxPrint(1, 38, "x");
        gfxPrint(1, 50, "y");
        ForEachChannel(ch)
        {
            int w = ProportionCV(ViewOut(ch), 62);
            gfxInvert(1, 38 + (12 * ch), w, 10);
        }
    }

};
