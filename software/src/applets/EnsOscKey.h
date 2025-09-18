// // Copyright (c) 2018, Jason Justian
// //
// // Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
// //
// // Permission is hereby granted, free of charge, to any person obtaining a copy
// // of this software and associated documentation files (the "Software"), to deal
// // in the Software without restriction, including without limitation the rights
// // to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// // copies of the Software, and to permit persons to whom the Software is
// // furnished to do so, subject to the following conditions:
// //
// // The above copyright notice and this permission notice shall be included in all
// // copies or substantial portions of the Software.
// //
// // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// // SOFTWARE.


// scale: quality(interval)
// ionian (major): I(0), ii(2), iii(4), IV(5), V(7), vi(9), vii(dim)(11)
// dorian: i(0), ii(2), III(3), IV(5), v(7), vi(dim)(9), VII(10)
// phrygian: i(0), II(1), III(3), iv(5), v(dim)(7), VI(8), vii(10)
// lydian: I(0), II(2), iii(4), iv(dim)(6), V(7), vi(9), vii(11)
// mixolydian: I(0), ii(2), iii(dim)(4), IV(5), v(7), vi(9), VII(10)
// aeolian (minor):  i(0), ii(dim)(2), III(3), iv(5), v(7), VI(8), VII(10)


class EnsOscKey : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "EnsOscKey";
    }

    void Start() {
        cursor = 0;
        scale[0] = 6; // Ionian
        QuantizerConfigure(0, scale[0]);
        last_note[0] = 0;
        continuous[0] = 1;
    }

    int determineInterval(int scale, int rootNote, int currentNote) {
      // TODO: Teensy 3.2 chokes on all these floats when loaded on both sides!

        int one_volt = HSAPPLICATION_5V / 5;
        double maj_temp = (voltage_maj / 2.0 - 0.25) * one_volt;
        int maj_out = static_cast<int>(maj_temp);
        double min_temp = (voltage_min / 2.0 - 0.25) * one_volt;
        int min_out = static_cast<int>(min_temp);
        double dim_temp = (voltage_dim / 2.0 - 0.25) * one_volt;
        int dim_out = static_cast<int>(dim_temp);
        double no_match_temp = (voltage_no_match / 2.0 - 0.25) * one_volt;
        int no_match_out = static_cast<int>(no_match_temp);
    // Calculate the number of semitones between root and current note
        int interval = (currentNote - rootNote + 12) % 12;

    // Determine the interval based on the scale
        switch (scale) {
            case 6:  // Ionian (major)
                if (interval == 0 || interval == 5 || interval == 7) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 2 || interval == 4 || interval == 9) // Minor interval
                    {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 11) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                }
                break;
            case 7: // Dorian
                if (interval == 3 || interval == 5 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 0 || interval == 2 || interval == 7) // Minor interval
                    {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 9) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                    }
                break;
            case 8: // Phrygian
                if (interval == 1 || interval == 3 || interval == 8) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 0 || interval == 5 || interval == 10) // Minor interval
                   {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 7) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                    }
                break;
            case 9: // Lydian
                if (interval == 0 || interval == 2 || interval == 7) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 4 || interval == 9 || interval == 11) // Minor interval
                    {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 6) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                    }
                break;
            case 10: // Mixolydian
                if (interval == 0 || interval == 5 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 2 || interval == 7 || interval == 9) // Minor interval
                    {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 4) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                    }
                break;
            case 11: // Aeolian (minor)
                if (interval == 3 || interval == 8 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    return maj_out;
                    }
                else if (interval == 0 || interval == 5 || interval == 7) // Minor interval
                    {
                    chord_quality = 1;
                    return min_out;
                    }
                else if (interval == 2) // Diminished interval
                    {
                    chord_quality = 2;
                    return dim_out;
                    }
                else {
                    chord_quality = 3;
                    return no_match_out;
                    }
                break;
        }
        // Default case
        return no_match_out;
    }

    void Controller() {
        if (Clock(0)) {
            continuous[0] = 0; // Turn off continuous mode if there's a clock
            StartADCLag(0);
        }
        
        if (continuous[0] || EndOfADCLag(0)) {
            int32_t pitch = In(0);
            int32_t quantized = Quantize(0, pitch, root[0] << 7, 0);
            int semitone = (quantized / 128) % 12;
            int output_voltage = determineInterval(scale[0], root[0], semitone);
            
            Out(0, output_voltage);
            last_note[0] = quantized;
        }

        // on channel 2, for each clock pulse, move the cursor to the next setting, or if the cursor is !Edit mode, do CursorToggle to set it into Edit mode. Let's make this only cycle through cursor 2-5
        if (Clock(1)) {
            if (EditMode()) {
                if (cursor < 2) {
                    cursor = 2;
                } else if (cursor < 5) {
                    cursor++;
                } else {
                    cursor = 2;
                }
            } else {
                CursorToggle();
                cursor = 2;
            }
        }

        // change the voltage_{chord_quality} of the selected setting with the voltage input in channel 2
        int32_t voltage = In(1);
        // scale voltage to 0-10
        int voltage_setting = voltage / 409;
        if (EditMode() && voltage_setting > 0 && voltage_setting < 11) {
            if (cursor == 2) {
                voltage_maj = voltage_setting;
                    } else if (cursor == 3) {
                voltage_min = voltage_setting;
            } else if (cursor == 4) {
                voltage_dim = voltage_setting;
            } else if (cursor == 5) {
                voltage_no_match = voltage_setting;
            }
        }
    }

    void View() {
        DrawSelector();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 5);
            return;
        }

        if (cursor == 0) {
             // Root selection
            root[0] = constrain(root[0] + direction, 0, 11);
        } else if (cursor == 1) {
           // Scale selection
            scale[0] += direction;
            // only allow Ionian and Aeolian scales
            scale[0] = constrain(scale[0], 6, 11);
            QuantizerConfigure(0, scale[0]);
            continuous[0] = 1; // Re-enable continuous mode when scale is changed
        } else if (cursor == 2) {
            // change voltage_maj value
            voltage_maj += direction;
            voltage_maj = constrain(voltage_maj, 1, 10);
        } else if (cursor == 3) {
            voltage_min += direction;
            voltage_min = constrain(voltage_min, 1, 10);
        } else if (cursor == 4) {
            voltage_dim += direction;
            voltage_dim = constrain(voltage_dim, 1, 10);
        } else if (cursor == 5) {
            voltage_no_match += direction;
            voltage_no_match = constrain(voltage_no_match, 1, 10);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, scale[0]);
        Pack(data, PackLocation {8,8}, scale[1]);
        Pack(data, PackLocation {16,4}, root[0]);
        Pack(data, PackLocation {20,4}, root[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        scale[0] = Unpack(data, PackLocation {0,8});
        scale[1] = Unpack(data, PackLocation {8,8});
        root[0] = Unpack(data, PackLocation {16,4});
        root[1] = Unpack(data, PackLocation {20,4});

        root[0] = constrain(root[0], 0, 11);
        QuantizerConfigure(0, scale[0]);
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock 1";
    help[HELP_DIGITAL2] = "Clock 2";
    help[HELP_CV1]      = "CV Ch1";
    help[HELP_CV2]      = "CV Ch2";
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Pitch 2";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int last_note[2]; // Last quantized note
    bool continuous[2]; // Each channel starts as continuous and becomes clocked when a clock is received
    int cursor;

    // Settings
    int scale[2]; // Scale per channel
    uint8_t root[2]; // Root per channel

    int voltage_maj = 3;
    int voltage_min = 4;
    int voltage_dim = 5;
    int voltage_no_match = 6;

    int chord_quality = 0; // 0 = Maj, 1 = Min, 2 = Dim, 3 = No Match

    void DrawSelector()
    {
        const uint8_t * notes[2] = {NOTE_ICON, NOTE2_ICON};


        // Draw Key Information
        gfxPrint(27, 15, OC::scale_names_short[scale[0]]);
        gfxBitmap(0, 15, 8, notes[0]);
        gfxPrint(10, 15, OC::Strings::note_names_unpadded[root[0]]);

        // Draw Resulting Note Information
        int semitone = ((last_note[0] / 128) % 12 + 12) % 12;
        gfxPrint(10, 27, OC::Strings::note_names_unpadded[semitone]);
        gfxPrint(0, 27, "=");

        // Draw Chord Quality
        if (chord_quality == 0) {
            gfxPrint(27, 27, "Maj");
        } else if (chord_quality == 1) {
            gfxPrint(27, 27, "Min");
        } else if (chord_quality == 2) {
            gfxPrint(27, 27, "Dim");
        } else {
            gfxPrint(27, 27, "N/A");
        }

        // Draw Voltage Selection Values
        gfxPrint(0, 40, "I:");
        gfxPrint(14, 40, voltage_maj);

        gfxPrint(30, 40, "i:");
        gfxPrint(44, 40, voltage_min);

        gfxPrint(0, 52, "o:");
        gfxPrint(14, 52, voltage_dim);

        gfxPrint(30, 52, "x:");
        gfxPrint(44, 52, voltage_no_match);

        // Draw cursor
        if (cursor == 0) {
            gfxCursor(10, 23, 14);
        } else if (cursor == 1) {
            gfxCursor(27, 23, 27);
        } else if (cursor == 2) {
           gfxCursor(14, 48, 14);
        } else if (cursor == 3) {
            gfxCursor(44, 48, 14);
        } else if (cursor == 4) {
            gfxCursor(14, 60, 14);
        } else if (cursor == 5) {
            gfxCursor(44, 60, 14);
        } 
    }
};
