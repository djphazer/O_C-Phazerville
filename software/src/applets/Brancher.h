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

class Brancher : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Brancher";
    }

    void Start() {
        p = 50;
        choice = 0;
    }

    void Controller() {
        p_mod = p;
        Modulate(p_mod, 0, 0, 100);

        // handles physical and logical clock
        if (Clock(0)) {
            choice = (random(1, 100) <= p_mod) ? 0 : 1;

            // will be true only for logical clocks
            clocked = !Gate(0);

            if (clocked) ClockOut(choice);
        }

        // only pass thru physical gates
        if (!clocked) {
            GateOut(choice, Gate(0));
            GateOut(1 - choice, 0); // reset the other output
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        choice = 1 - choice;
    }

    /* Change the pability */
    void OnEncoderMove(int direction) {
        p = constrain(p + direction, 0, 100);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, p);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        p = Unpack(data, PackLocation {0,7});
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock/Gate";
        help[HEMISPHERE_HELP_CVS] = "1=p Mod";
        help[HEMISPHERE_HELP_OUTS] = "A,B=Clock/Gate";
        help[HEMISPHERE_HELP_ENCODER] = "Set p";
    }

private:
	int p, p_mod;
	int choice;
    bool clocked; // indicates a logical clock without a physical gate

	void DrawInterface() {
        // Show the probability in the middle
        gfxPrint(1, 15, "p=");
        gfxPrint(15 + pad(100, p_mod), 15, p_mod);
        gfxPrint(33, 15, hemisphere ? "%  C" : "%  A");
        gfxCursor(15, 23, 18);
        if (p != p_mod) gfxIcon(39, 12, CV_ICON);

        gfxPrint(12, 45, hemisphere ? "C" : "A");
        gfxPrint(44, 45, hemisphere ? "D" : "B");
        gfxFrame(9 + (32 * choice), 42, 13, 13);
	}
};
