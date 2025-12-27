// Copyright (c) 2024, Parker Jones
// Alessio Degani, 2025: Added octave selector, quantized Pitch and Chord output
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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
    const uint8_t* applet_icon() { return PhzIcons::scaleDuet; }

    void Start() {
        cursor = 0;
        scale = 6; // Ionian
        QuantizerConfigure(0, scale);
        last_note = 0;
        continuous = 1;
        code_maj = voltageToCode(voltage_maj);
        code_min = voltageToCode(voltage_min);
        code_dim = voltageToCode(voltage_dim);
        code_no_match = voltageToCode(voltage_no_match);
    }

    int voltageToCode(int voltageIn) {
        return (int)(((float)voltageIn / 2.0 - 0.25) * ONE_OCTAVE);
    }

    int determineInterval(int scale, int rootNote, int currentNote) {
      
        // Calculate the number of semitones between root and current note
        int interval = (currentNote - rootNote + 12) % 12;
        int voltageToUse = code_no_match;
        // Determine the interval based on the scale
        switch (scale) {
            case 5: // Semi
                chord_quality = 0;
                voltageToUse = code_maj;
            case 6:  // Ionian (major)
                if (interval == 0 || interval == 5 || interval == 7) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 2 || interval == 4 || interval == 9) // Minor interval
                    {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 11) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                }
                break;
            case 7: // Dorian
                if (interval == 3 || interval == 5 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 0 || interval == 2 || interval == 7) // Minor interval
                    {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 9) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                    }
                break;
            case 8: // Phrygian
                if (interval == 1 || interval == 3 || interval == 8) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 0 || interval == 5 || interval == 10) // Minor interval
                   {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 7) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                    }
                break;
            case 9: // Lydian
                if (interval == 0 || interval == 2 || interval == 7) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 4 || interval == 9 || interval == 11) // Minor interval
                    {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 6) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                    }
                break;
            case 10: // Mixolydian
                if (interval == 0 || interval == 5 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 2 || interval == 7 || interval == 9) // Minor interval
                    {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 4) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                    }
                break;
            case 11: // Aeolian (minor)
                if (interval == 3 || interval == 8 || interval == 10) // Major interval
                    {
                    chord_quality = 0;
                    voltageToUse = code_maj;
                    }
                else if (interval == 0 || interval == 5 || interval == 7) // Minor interval
                    {
                    chord_quality = 1;
                    voltageToUse = code_min;
                    }
                else if (interval == 2) // Diminished interval
                    {
                    chord_quality = 2;
                    voltageToUse = code_dim;
                    }
                else {
                    chord_quality = 3;
                    voltageToUse = code_no_match;
                    }
                break;
        }
        // Default case
        return voltageToUse;
    }

    void Controller() {
        if (Clock(0)) {
            continuous = 0; // Turn off continuous mode if there's a clock
            StartADCLag(0);
        }
        
        if (continuous || EndOfADCLag(0)) {
            octaveCv = Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 7);
            int32_t pitch = In(0);
            int32_t quantized = Quantize(0, pitch, root << 7, 0);
            int semitone = (quantized / 128) % 12;
            int output_voltage = determineInterval(scale, root, semitone);
            
            Out(0, quantized + (ONE_OCTAVE*(octave+octaveCv)));
            Out(1, output_voltage);
            last_note = quantized;
        }
    }

    void View() {
        DrawSelector();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 6);
            return;
        }

        if (cursor == 0) {
             // Root selection
            root = constrain(root + direction, 0, 11);
        } else if (cursor == 1) {
           // Scale selection
            scale += direction;
            // only allow Semitone, and Ionian to Aeolian scales
            scale = constrain(scale, 5, 11);
            // scale = constrain(scale, 0, OC::Scales::NUM_SCALES - 1);
            QuantizerConfigure(0, scale);
            continuous = 1; // Re-enable continuous mode when scale is changed
        } else if (cursor == 2) {
            // change octave
            octave += direction;
            octave = constrain(octave, -5, 5);
        } else if (cursor == 3) {
            // change voltage_maj value
            voltage_maj += direction;
            voltage_maj = constrain(voltage_maj, 1, 10);
            code_maj = voltageToCode(voltage_maj);
        } else if (cursor == 4) {
            voltage_min += direction;
            voltage_min = constrain(voltage_min, 1, 10);
            code_min = voltageToCode(voltage_min);
        } else if (cursor == 5) {
            voltage_dim += direction;
            voltage_dim = constrain(voltage_dim, 1, 10);
            code_dim = voltageToCode(voltage_dim);
        } else if (cursor == 6) {
            voltage_no_match += direction;
            voltage_no_match = constrain(voltage_no_match, 1, 10);
            code_no_match = voltageToCode(voltage_no_match);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, scale);
        Pack(data, PackLocation {8,4}, octave+5);
        Pack(data, PackLocation {12,4}, voltage_maj);
        Pack(data, PackLocation {16,4}, voltage_min);
        Pack(data, PackLocation {20,4}, voltage_dim);
        Pack(data, PackLocation {24,4}, voltage_no_match);
        Pack(data, PackLocation {28,4}, root);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        scale = Unpack(data, PackLocation {0,8});
        octave = Unpack(data, PackLocation {8,4})-5;
        voltage_maj = Unpack(data, PackLocation {12,4});
        voltage_min = Unpack(data, PackLocation {16,4});
        voltage_dim = Unpack(data, PackLocation {20,4});
        voltage_no_match = Unpack(data, PackLocation {24,4});
        root = Unpack(data, PackLocation {28,4});

        root = constrain(root, 0, 11);
        QuantizerConfigure(0, scale);
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "-";
    help[HELP_CV1]      = "Pitch";
    help[HELP_CV2]      = "Octave";
    help[HELP_OUT1]     = "Note";
    help[HELP_OUT2]     = "Scale";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int last_note; // Last quantized note
    bool continuous; // Each channel starts as continuous and becomes clocked when a clock is received
    int cursor;

    // Settings
    int scale; // Scale 
    uint8_t root; // Root

    int voltage_maj = 3;
    int voltage_min = 4;
    int voltage_dim = 5;
    int voltage_no_match = 6;

    int code_maj = 0;
    int code_min = 0;
    int code_dim = 0;
    int code_no_match = 0;

    int octave = 0;
    int octaveCv = 0;

    int chord_quality = 0; // 0 = Maj, 1 = Min, 2 = Dim, 3 = No Match

    void DrawSelector()
    {
        const uint8_t * notes[2] = {NOTE_ICON, NOTE2_ICON};


        // Draw Key Information
        gfxPrint(27, 15, OC::scale_names_short[scale]);
        gfxBitmap(0, 15, 8, notes[0]);
        gfxPrint(10, 15, OC::Strings::note_names_unpadded[root]);

        // Draw Resulting Note Information
        int semitone = ((last_note / 128) % 12 + 12) % 12;
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

        if (octave+octaveCv < 0) {
            gfxPrint(49, 27, "-");
        } else {
            gfxPrint(49, 27, "+");
        }

        gfxPrint(56, 27, abs(octave+octaveCv));

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
            gfxCursor(49, 35, 28);
        } else if (cursor == 3) {
           gfxCursor(14, 48, 14);
        } else if (cursor == 4) {
            gfxCursor(44, 48, 14);
        } else if (cursor == 5) {
            gfxCursor(14, 60, 14);
        } else if (cursor == 6) {
            gfxCursor(44, 60, 14);
        } 
    }
};
