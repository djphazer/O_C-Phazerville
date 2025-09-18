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


class Voltage : public HemisphereApplet {
public:

  static constexpr int VOLTAGE_INCREMENTS = 128;
  #define VOLTAGE_MIN (HEMISPHERE_MIN_CV / VOLTAGE_INCREMENTS)
  #define VOLTAGE_MAX (HEMISPHERE_MAX_CV / VOLTAGE_INCREMENTS)

    const char* applet_name() {
        return "Voltage";
    }
    const uint8_t* applet_icon() { return PhzIcons::voltage; }

    void Start() {
        voltage[0] = VOLTAGE_MAX;
        voltage[1] = VOLTAGE_MIN;
        gate[0] = 0;
        gate[1] = 0;
    }

    void Controller() {
        int cv;
        ForEachChannel(ch)
        {
            if (gate[ch]) { // Normally off
                if (Gate(ch)) cv = voltage[ch] * VOLTAGE_INCREMENTS;
                else cv = 0;
            } else {
                if (Gate(ch)) cv = 0;
                else cv = voltage[ch] * VOLTAGE_INCREMENTS;
            }
            view[ch] = cv ? 1 : 0;
            Out(ch, cv);
        }
    }

    void View() {
        DrawInterface();
    }

    //void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        uint8_t ch = cursor / 2;
        if (cursor == 0 || cursor == 2) {
            // Change voltage
            voltage[ch] = constrain(voltage[ch] + direction, VOLTAGE_MIN, VOLTAGE_MAX);
        } else {
            gate[ch] = 1 - gate[ch];
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,9}, voltage[0] + 256);
        Pack(data, PackLocation {10,9}, voltage[1] + 256);
        Pack(data, PackLocation {19,1}, gate[0]);
        Pack(data, PackLocation {20,1}, gate[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        voltage[0] = Unpack(data, PackLocation {0,9}) - 256;
        voltage[1] = Unpack(data, PackLocation {10,9}) - 256;
        gate[0] = Unpack(data, PackLocation {19,1});
        gate[1] = Unpack(data, PackLocation {20,1});

        CONSTRAIN(voltage[0], VOLTAGE_MIN, VOLTAGE_MAX);
        CONSTRAIN(voltage[1], VOLTAGE_MIN, VOLTAGE_MAX);
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Gate 1";
        help[HELP_DIGITAL2] = "Gate 2";
        help[HELP_CV1]      = "";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "Volt 1";
        help[HELP_OUT2]     = "Volt 2";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int cursor;
    bool view[2];

    // Settings
    int voltage[2];
    bool gate[2]; // 0 = Normally on, gate turns off, 1= Normally off, gate turns on

    void DrawInterface() {
        ForEachChannel(ch)
        {
            gfxPrint(0, 15 + (ch * 20), OutputLabel(ch));
            gfxPrint(" ");
            int cv = voltage[ch] * VOLTAGE_INCREMENTS;
            gfxPrintVoltage(cv);
            gfxPrint(0, 25 + (ch * 20), gate[ch] ? "  G-On" : "  G-Off");

            if (view[ch]) gfxInvert(0, 14 + (ch * 20), 7, 9);
        }

        gfxCursor(12, 23 + cursor * 10, 37);
    }

};
