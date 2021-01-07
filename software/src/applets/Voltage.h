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

#define VOLTAGE_INCREMENTS 128

class Voltage : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Voltage";
    }

    void Start() {
        voltage[0] = (5 * (12 << 7)) / VOLTAGE_INCREMENTS; // 5V
        voltage[1] = (-3 * (12 << 7)) / VOLTAGE_INCREMENTS; // -3V
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

    void OnButtonPress() {
        CursorAction(cursor, 3);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        uint8_t ch = cursor / 2;
        if (cursor == 0 || cursor == 2) {
            // Change voltage
            int min = -HEMISPHERE_MAX_CV / VOLTAGE_INCREMENTS;
            int max = HEMISPHERE_MAX_CV / VOLTAGE_INCREMENTS;
            voltage[ch] = constrain(voltage[ch] + direction, min, max);
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
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "Gate 1,2";
        help[HEMISPHERE_HELP_CVS]      = "";
        help[HEMISPHERE_HELP_OUTS]     = "Voltage A,B";
        help[HEMISPHERE_HELP_ENCODER]  = "Voltage/Gating";
        //                               "------------------" <-- Size Guide
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
            gfxPrint(0, 15 + (ch * 20), (hemisphere ? (ch ? "D " : "C ") : (ch ? "B " : "A ")));
            int cv = voltage[ch] * VOLTAGE_INCREMENTS;
            gfxPrintVoltage(cv);
            gfxPrint(0, 25 + (ch * 20), gate[ch] ? "  G-On" : "  G-Off");

            if (view[ch]) gfxInvert(0, 14 + (ch * 20), 7, 9);
        }

        gfxCursor(12, 23 + cursor * 10, 37);
    }

};
