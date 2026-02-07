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

// Arithmetic functions and typedef to function pointer
int hem_MIN(int v1, int v2) {return (v1 < v2) ? v1 : v2;}
int hem_MAX(int v1, int v2) {return (v1 > v2) ? v1 : v2;}
int hem_SUM(int v1, int v2) {return constrain(v1 + v2, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);}
int hem_DIFF(int v1, int v2) {return hem_MAX(v1, v2) - hem_MIN(v1, v2);}
int hem_MEAN(int v1, int v2) {return (v1 + v2) / 2;}
typedef int(*CalcFunction)(int, int);

class Calculate : public HemisphereApplet {
public:
    enum CalcFunctions : uint8_t {
      MIN_FN,
      MAX_FN,
      SUM_FN,
      DIFF_FN,
      MEAN_FN,
      S_AND_H_FN,
      RND_UNI_FN,
      RND_BI_FN,
      CALC_FN_COUNT
    };

    const char* applet_name() {
        return "Calculate";
    }
    const uint8_t* applet_icon() { return PhzIcons::calculate; }

    void Start() {
        selected = 0;
        ForEachChannel(ch)
        {
            operation[ch] = ch;
            rand_clocked[ch] = 0;
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            const uint8_t &idx = operation[ch];

            if (idx == 5) { // S&H
                if (Clock(ch)) StartADCLag(ch);
                if (EndOfADCLag(ch)) Out(ch, In(ch));
            } else if (idx >= 6) { // Rand
                // The first time a clock comes in, Rand becomes clocked.
                // Otherwise, Rand is unclocked, and outputs a random value with each tick.

                bool clk = Clock(ch);
                bool recalc = clk || !rand_clocked[ch];
                if (clk)
                    rand_clocked[ch] = 1;
                else if (ch == 1 && rand_clocked[0] && !rand_clocked[1]) // normalled clock input from TR1 for channel 2
                    recalc = Clock(0);

                if (recalc) {
                  if (idx == 6) // unipolar
                    Out(ch, random(0, HEMISPHERE_MAX_CV));
                  else // bipolar
                    Out(ch, random(0, HEMISPHERE_MAX_CV - HEMISPHERE_MIN_CV) + HEMISPHERE_MIN_CV);
                }
            } else if (idx < 5) {
                int result = calc_fn[idx](In(0), In(1));
                Out(ch, result);
            }
        }
    }

    void View() {
        DrawSelector();
        gfxSkyline();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(selected, direction, 1);
            return;
        }
        operation[selected] = constrain(operation[selected] + direction, 0, CALC_FN_COUNT - 1);
        rand_clocked[selected] = 0;
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 8}, operation[0]);
        Pack(data, PackLocation {8, 8}, operation[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        operation[0] = constrain(Unpack(data, PackLocation {0, 8}), 0, CALC_FN_COUNT - 1);
        operation[1] = constrain(Unpack(data, PackLocation {8, 8}), 0, CALC_FN_COUNT - 1);
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "HoldCV1";
    help[HELP_DIGITAL2] = "HoldCV2";
    help[HELP_CV1]      = "CV1";
    help[HELP_CV2]      = "CV2";
    help[HELP_OUT1]     = op_name[operation[0]];
    help[HELP_OUT2]     = op_name[operation[1]];
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    const char* const op_name[CALC_FN_COUNT] = {"Min", "Max", "Sum", "Diff", "Mean", "S&H", "Rnd+", "-Rnd+"};
    const CalcFunction calc_fn[5] = {hem_MIN, hem_MAX, hem_SUM, hem_DIFF, hem_MEAN};
    uint8_t operation[2];
    int selected;
    bool rand_clocked[2];
    
    void DrawSelector()
    {
        ForEachChannel(ch)
        {
            gfxPrint(31 * ch, 15, op_name[operation[ch]]);
            // Show the icon if this random calculator is clocked
            if (operation[ch] >= 6 && rand_clocked[ch]) gfxIcon(8 + 31 * ch, 25, CLOCK_ICON);

            if (ch == selected) gfxCursor(0 + (31 * ch), 23, 30);
        }
    }
};
