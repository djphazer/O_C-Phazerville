// Copyright (c) 2025, Nicholas J. Michalek
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

class ClkToGate : public HemisphereApplet {
public:

  enum ClkToGateCursor {
    WIDTH1, RANGE1, SKIP1,
    WIDTH2, RANGE2, SKIP2,

    PARAM_COUNT,
    PARAM_PER_CHAN = 3,
    LAST_SETTING = SKIP2
  };

    const char* applet_name() {
        return "Clk2Gate";
    }
    const uint8_t* applet_icon() { return GATE_ICON; }

    void Start() {
      ForEachChannel(ch) {
        width_mod[ch] = width[ch] = (ch+1)*25;
        range[ch] = ch*25;
        skip[ch] = 0;
      }
    }

    void Reset() {
    }

    void Controller() {
      ForEachChannel(ch) {
        if (Clock(ch)) {
          if (random(100) < skip[ch]) continue;

          width_mod[ch] = width[ch];
          Modulate(width_mod[ch], ch, 1, 100);
          if (range[ch] > 0)
            width_mod[ch] += random(range[ch] + 1);
          if (range[ch] < 0)
            width_mod[ch] -= random(-range[ch] + 1);

          CONSTRAIN(width_mod[ch], 1, 100);

          if (width_mod[ch] == 100)
            GateOut(ch, 1); // tied notes don't turn off
          else
            ClockOut(ch, ClockCycleTicks(ch) * width_mod[ch] / 100);
        }
      }
    }

    void View() {
        DrawSelector();
    }

    //void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        int ch = 0;
        switch (cursor) {
          case WIDTH2:
            ch = 1;
          case WIDTH1:
            width_mod[ch] = width[ch] = constrain( width[ch] + direction, 1, 100 );
            break;

          case RANGE2:
            ch = 1;
          case RANGE1:
            range[ch] = constrain( range[ch] + direction, -99, 99);
            break;

          case SKIP2:
            ch = 1;
          case SKIP1:
            skip[ch] = constrain( skip[ch] + direction, 0, 100);
            break;

          default: break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < 2; ++i) {
          Pack(data, PackLocation {0 + i*32,7}, width[i]);
          Pack(data, PackLocation {8 + i*32,7}, abs(range[i])); // amount
          Pack(data, PackLocation {15 + i*32,1}, (range[i] < 0)); // sign
          Pack(data, PackLocation {16 + i*32,7}, skip[i]);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < 2; ++i) {
          width[i] = Unpack(data, PackLocation {0 + i*32,7});
          range[i] = Unpack(data, PackLocation {8 + i*32,7});
          if (Unpack(data, PackLocation{15 + i*32, 1}))
            range[i] = -range[i];
          skip[i] = Unpack(data, PackLocation {16 + i*32,7});

          CONSTRAIN(width[i], 1, 100);
          CONSTRAIN(range[i], -99, 99);
          CONSTRAIN(skip[i], 0, 100);
        }
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clk1";
    help[HELP_DIGITAL2] = "Clk2";
    help[HELP_CV1]      = "PW1";
    help[HELP_CV2]      = "PW2";
    help[HELP_OUT1]     = "Gate";
    help[HELP_OUT2]     = "Gate";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int width[2] = {25, 50}; // pulsewidth of gates in % of clock cycle
    int width_mod[2]; // from CV
    // randomization parameters
    int8_t range[2];
    uint8_t skip[2];

    int cursor = 0;

    void DrawSelector() {
        ForEachChannel(ch)
        {
            const int y = 16 + (ch * 26);

            // visual gate width meter
            gfxRect(0, y-3, width_mod[ch] * 64 / 100, 2);

            gfxPrint(1, y, OutputLabel(ch));
            gfxPos(13, y);
            gfxPrint(width_mod[ch]);
            gfxPrint("%");
            if (width[ch] != width_mod[ch]) gfxIcon(38, y, MOD_ICON);
            if (DetentedIn(ch)) gfxIcon(46, y, CV_ICON);

            const bool plus = (range[ch] >= 0);
            gfxPrint(6, y+10, plus? ">" : "<");
            gfxPrint(abs(range[ch]));
            gfxIcon(35, y+10, RANDOM_ICON);
            gfxPrint(44, y+10, skip[ch]);
            gfxPrint("%");
        }
        gfxDottedLine(1, 37, 63, 37); // to separate the two channels

        int param = cursor % PARAM_PER_CHAN;
        int cur_x = param ? (12 + (param-1) * 31) : 12;
        int cur_y = 24 + cursor / PARAM_PER_CHAN * 26 + (param?10:0);
        gfxCursor(cur_x, cur_y, 19);
    }
};
