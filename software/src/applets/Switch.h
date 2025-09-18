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

class Switch : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Switch";
    }
    const uint8_t* applet_icon() { return PhzIcons::switchApp; }

    void Start() {
        active[0] = 1;
        active[1] = 1;
    }

    void Controller() {
        // Channel 1 is the sequential switch - When clocked, step between channel 1 and 2
        if (Clock(0)) {
            step = 1 - step;
            active[0] = step + 1;
        }
        Out(0, In(step));

        // Channel 2 is the gated switch - When gate is high, use channel 2, otherwise channel 1
        if (Gate(1)) {
            active[1] = 2;
            Out(1, In(1));
        } else {
            active[1] = 1;
            Out(1, In(0));
        }
    }

    void View() {
        DrawCaptions();
        DrawIndicator();
        gfxSkyline();
    }

    void OnButtonPress() {
    }

    void OnEncoderMove(int direction) {
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        return data;
    }

    void OnDataReceive(uint64_t data) {
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Flip";
        help[HELP_DIGITAL2] = "Gate";
        help[HELP_CV1]      = "Input 1";
        help[HELP_CV2]      = "Input 2";
        help[HELP_OUT1]     = "Toggled";
        help[HELP_OUT2]     = "Gated";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int step = 0;
    int active[2];

    void DrawIndicator()
    {
        ForEachChannel(ch)
        {
            // Selected input indicator
            gfxPrint(5 + (46 * ch), 40, active[ch]);
        }
    }

    void DrawCaptions()
    {
        gfxPrint(1, 15, "Seq");
        gfxPrint(36, 15, "Gate");
    }
};
