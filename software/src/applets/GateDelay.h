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

class GateDelay : public HemisphereApplet {
public:

    const char* applet_name() {
        return "GateDelay";
    }
    const uint8_t* applet_icon() { return PhzIcons::gateDelay; }

    void Start() {
        ForEachChannel(ch)
        {
            for (int i = 0; i < 64; i++) tape[ch][i] = 0x0000;
            time[ch] = 1000;
            location[ch] = 0;
            last_gate[ch] = 0;
        }
        cursor = 0;
        ms_countdown = 0;
    }

    void Controller() {
        if (--ms_countdown < 0) {
            ForEachChannel(ch)
            {
                record(ch, Gate(ch));
                int mod_time = Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, 1000) + time[ch];
                mod_time = constrain(mod_time, 0, 2000);

                bool p = play(ch, mod_time);
                if (p) last_gate[ch] = OC::CORE::ticks;
                GateOut(ch, p);

                if (++location[ch] > 2047) location[ch] = 0;
            }
            ms_countdown = 16;
        }
    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }

        if (time[cursor] > 100) direction *= 2;
        if (time[cursor] > 500) direction *= 2;
        if (time[cursor] > 1000) direction *= 2;
        time[cursor] = constrain(time[cursor] + direction, 0, 2000);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,11}, time[0]);
        Pack(data, PackLocation {11,11}, time[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        time[0] = Unpack(data, PackLocation {0,11});
        time[1] = Unpack(data, PackLocation {11,11});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Gate 1";
    help[HELP_DIGITAL2] = "Gate 2";
    help[HELP_CV1]      = "Time 1";
    help[HELP_CV2]      = "Time 2";
    help[HELP_OUT1]     = "Delay 1";
    help[HELP_OUT2]     = "Delay 2";
    help[HELP_EXTRA1] = "Set: Time,";
    help[HELP_EXTRA2] = "     per channel";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    uint32_t tape[2][64]; // 63 x 32 = 2016 bits per channel, or 2s at 1ms resolution
    int time[2]; // Length of each channel (in ms)
    uint16_t location[2]; // Location of record head (playback head = location + time)
    uint32_t last_gate[2]; // Time of last gate, for display of icon
    int cursor;
    int16_t ms_countdown; // Countdown for 1 ms

    void DrawInterface() {
        ForEachChannel(ch)
        {
            int y = 15 + (ch * 25);

            gfxPrint(1, y, time[ch]);
            gfxPrint("ms");

            if (OC::CORE::ticks - last_gate[ch] < 1667) gfxBitmap(54, y, 8, CLOCK_ICON);
        }
        gfxCursor(0, 23 + (cursor * 25), 63);
    }

    /* Write the gate state into the tape at the tape head */
    void record(int ch, bool gate) {
        uint16_t word = location[ch] / 32;
        uint8_t bit = location[ch] % 32;
        if (gate) {
            // Set the current bit
            tape[ch][word] |= (0x01 << bit);
        } else {
            // Clear the current bit
            tape[ch][word] &= (0xffff ^ (0x01 << bit));
        }
    }

    /* Get the status of the tape at the current play head location */
    bool play(int ch, int mod_time) {
        int play_location = location[ch] - mod_time;
        if (play_location < 0) play_location += 2048;
        uint16_t word = play_location / 32;
        uint8_t bit = play_location % 32;
        return ((tape[ch][word] >> bit) & 0x01);
    }

};
