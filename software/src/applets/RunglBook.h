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
// Modified by Mace Ojala 2026

class RunglBook : public HemisphereApplet {
public:
  static constexpr int MIN_THRESH = ONE_OCTAVE;
  static constexpr int MAX_THRESH = ONE_OCTAVE * 5;

    const char* applet_name() {
        return "RunglBook";
    }
    const uint8_t* applet_icon() { return PhzIcons::runglBook; }

    void Start() {
        threshold = ONE_OCTAVE * 2;
    }

    void Controller() {
        threshold_mod = threshold;
        Modulate(threshold_mod, 1, MIN_THRESH, MAX_THRESH);

        if (Clock(0)) {
            if (Gate(1)) {
                // Digital 2 freezes the buffer, so just rotate left
                reg = (reg << 1) | ((reg >> 7) & 0x01);
            } else {
                uint8_t b0 = In(0) > threshold_mod ? 0x01 : 0x00;
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
        gfxPrintVoltage(threshold_mod);
        gfxSkyline();
    }

    void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        threshold = constrain(threshold + (direction * 128), MIN_THRESH, MAX_THRESH); // 1V - 5V
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
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Freeze";
    help[HELP_CV1]      = "Signal";
    help[HELP_CV2]      = "Thresh";
    help[HELP_OUT1]     = "Rungle";
    help[HELP_OUT2]     = "Alt";
    help[HELP_EXTRA1] = "Set: Threshold";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    uint8_t reg;
    uint16_t threshold;
    uint16_t threshold_mod;
};
