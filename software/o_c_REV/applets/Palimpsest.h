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

#define HEM_PALIMPSEST_MAX_VALUE 100

class Palimpsest : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Palimpsest";
    }

    void Start() {
        brush = 0;
        length = 16;
        step = 0;
        for (int s = 0; s < length; s++) accent[s] = 0;
    }

    void Controller() {
        // Handle CV modulation of compose and decompose
        effective_decompose = decompose;
        if (DetentedIn(0)) {
            int mod = Proportion(In(0), HEMISPHERE_3V_CV, HEM_PALIMPSEST_MAX_VALUE / 2);
            mod = constrain(mod, -(HEM_PALIMPSEST_MAX_VALUE / 2), HEM_PALIMPSEST_MAX_VALUE / 2);
            effective_decompose = constrain(decompose + mod, 0, HEM_PALIMPSEST_MAX_VALUE);
        }

        effective_compose = compose;
        if (DetentedIn(1)) {
            int mod = Proportion(In(1), HEMISPHERE_3V_CV, HEM_PALIMPSEST_MAX_VALUE / 2);
            mod = constrain(mod, -(HEM_PALIMPSEST_MAX_VALUE / 2), HEM_PALIMPSEST_MAX_VALUE / 2);
            effective_compose = constrain(compose + mod, 0, HEM_PALIMPSEST_MAX_VALUE);
        }

        if (Gate(1)) {
            // If this is the first time the brush was received during this step, compose the step,
            // unless the cursor is on the length parameter
            if (!brush && cursor != 2) {
                accent[step] += (HEMISPHERE_MAX_CV / HEM_PALIMPSEST_MAX_VALUE) * effective_compose;
                accent[step] = constrain(accent[step], 0, HEMISPHERE_MAX_CV);
                brush = 1;
            }
        }

        if (Clock(0)) {
            // If the brush wasn't received during this step, decompose the step, unless
            // the cursor is on the length parameter
            if (!brush && cursor != 2) {
                accent[step] -= (HEMISPHERE_MAX_CV / HEM_PALIMPSEST_MAX_VALUE) * effective_decompose;
                accent[step] = constrain(accent[step], 0, HEMISPHERE_MAX_CV);
            }

            brush = 0;
            Out(0, accent[step]);

            if (accent[step] > HEMISPHERE_3V_CV) ClockOut(1);

            if (++step >= length) step = 0;
        } else {
            Out(0, accent[step]);
        }
    }

    void View() {
        DrawControls();
        DrawSequence();
    }

    void OnButtonPress() {
        CursorAction(cursor, 2);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 2);
            return;
        }

        switch (cursor) {
        case 0:
            compose = constrain(compose + direction, 0, HEM_PALIMPSEST_MAX_VALUE);
            break;
        case 1:
            decompose = constrain(decompose - direction, 0, HEM_PALIMPSEST_MAX_VALUE);
            break;
        case 2:
            length = constrain(length + direction, 2, 16);
            break;
        }
        ResetCursor();
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, compose);
        Pack(data, PackLocation {7,7}, decompose);
        Pack(data, PackLocation {14,4}, length - 1);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        compose = Unpack(data, PackLocation {0,7});
        decompose = Unpack(data, PackLocation {7,7});
        length = Unpack(data, PackLocation {14,4}) + 1;
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Brush";
        help[HEMISPHERE_HELP_CVS]      = "Mod 1=Compose 2=De";
        help[HEMISPHERE_HELP_OUTS]     = "A=Output B=Trigger";
        help[HEMISPHERE_HELP_ENCODER]  = "Comp/Decomp/Length";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int accent[16]; // Accent data
    int step;
    int effective_compose;
    int effective_decompose;
    int cursor;
    bool brush;

    // Settings
    int length;
    int compose;
    int decompose;

    void DrawControls() {
        int comp_w = Proportion(effective_compose, HEM_PALIMPSEST_MAX_VALUE, 30);
        int decomp_w = Proportion(effective_decompose, HEM_PALIMPSEST_MAX_VALUE, 30);

        gfxFrame(30 - decomp_w, 15, decomp_w, 7);
        gfxRect(32, 15, comp_w, 7);

        if (cursor == 0) gfxCursor(32, 23, 30);
        if (cursor == 1) gfxCursor(1, 23, 30);
        if (cursor == 2) {
            gfxBitmap(28, 26, 8, LOCK_ICON);
            if (CursorBlink()) {
                gfxLine(0, 24, (length - 1) * 4 + 1, 24);
                gfxLine((length - 1) * 4 + 1, 24, (length - 1) * 4 + 1, 28);
                gfxLine(0, 24, 0, 28);
            }
        }
    }

    void DrawSequence() {
        for (int s = 0; s < length; s++)
        {
            int index = s + step;
            if (index > length) index -= length;
            int h = ProportionCV(accent[index], 33);
            if (s == 0 && brush) gfxRect(s * 4 + 0, BottomAlign(h), 3, h);
            else gfxLine(s * 4 + 1, BottomAlign(h), s * 4 + 1, 62);
        }
    }
};
