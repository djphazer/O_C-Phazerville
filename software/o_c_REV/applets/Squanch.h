// Copyright (c) 2018, Jason Justian
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

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

class Squanch : public HemisphereApplet {
public:
    enum SquanchCursor {
        SHIFT1, SHIFT2,
        SCALE, ROOT_NOTE,

        LAST_SETTING = ROOT_NOTE
    };

    const char* applet_name() {
        return "Squanch";
    }

    void Start() {
        scale = 5;
        ForEachChannel(ch) {
            QuantizerConfigure(ch, scale);
        }
    }

    void Controller() {
        if (Clock(0)) {
            continuous = 0; // Turn off continuous mode if there's a clock
            StartADCLag(0);
        }

        if (continuous || EndOfADCLag(0)) {
            int32_t pitch = In(0);

            ForEachChannel(ch)
            {
                // For the B/D output, CV 2 is used to shift the output; for the A/C
                // output, the output is raised by one octave when Digital 2 is gated.
                int32_t shift_alt = (ch == 1) ? DetentedIn(1) : Gate(1) * (12 << 7);

                int32_t quantized = Quantize(ch, pitch + shift_alt, root << 7, shift[ch]);
                Out(ch, quantized);
                last_note[ch] = quantized;
            }
        }

    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch (cursor) {
        case SHIFT1:
        case SHIFT2:
            shift[cursor] = constrain(shift[cursor] + direction, -48, 48);
            break;

        case SCALE:
            scale += direction;
            if (scale >= OC::Scales::NUM_SCALES) scale = 0;
            if (scale < 0) scale = OC::Scales::NUM_SCALES - 1;
            ForEachChannel(ch)
                QuantizerConfigure(ch, scale);
            continuous = 1; // Re-enable continuous mode when scale is changed
            break;

        case ROOT_NOTE:
            root = constrain(root + direction, 0, 11);
            break;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, scale);
        Pack(data, PackLocation {8,8}, shift[0] + 48);
        Pack(data, PackLocation {16,8}, shift[1] + 48);
        Pack(data, PackLocation {24,4}, root);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        scale = Unpack(data, PackLocation {0,8});
        shift[0] = Unpack(data, PackLocation {8,8}) - 48;
        shift[1] = Unpack(data, PackLocation {16,8}) - 48;
        root = Unpack(data, PackLocation {24,4});
        root = constrain(root, 0, 11);
        ForEachChannel(ch)
            QuantizerConfigure(ch, scale);
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=+1 Oct A";
        help[HEMISPHERE_HELP_CVS]      = "1=Signal 2=Shift B";
        help[HEMISPHERE_HELP_OUTS]     = "A,B=Quantized";
        help[HEMISPHERE_HELP_ENCODER]  = "Shift/Scale";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // 0=A shift, 1=B shift, 2=Scale
    bool continuous = 1;
    int last_note[2]; // Last quantized note

    // Settings
    int scale;
    uint8_t root;
    int16_t shift[2];

    void DrawInterface() {
        const uint8_t * notes[2] = {NOTE_ICON, NOTE2_ICON};

        // Shift for A/C
        ForEachChannel(ch) {
            gfxIcon(1 + ch*32, 14, notes[ch]);
            gfxPrint(10 + pad(10, shift[ch]) + ch*32, 15, shift[ch] > -1 ? "+" : "");
            gfxPrint(shift[ch]);
        }

        // Scale & Root Note
        gfxIcon(1, 24, SCALE_ICON);
        gfxPrint(10, 25, OC::scale_names_short[scale]);
        gfxPrint(40, 25, OC::Strings::note_names_unpadded[root]);

        // Display icon if clocked
        if (!continuous) gfxIcon(56, 25, CLOCK_ICON);

        // Cursors
        switch (cursor) {
        case SHIFT1:
        case SHIFT2:
            gfxCursor(10 + (cursor - SHIFT1)*32, 23, 19);
            break;
        case SCALE:
            gfxCursor(10, 33, 25);
            break;
        case ROOT_NOTE:
            gfxCursor(40, 33, 13);
            break;
        }

        // Little note display

        // This is for relative visual purposes only, so I don't really care if this isn't a semitone
        // scale, or even if it has 12 notes in it:
        ForEachChannel(ch)
        {
            int semitone = (last_note[ch] / 128) % 12;
            while (semitone < 0) semitone += 12;
            int note_x = semitone * 3; // pixels per semitone
            gfxPrint(0, 41 + 10*ch, midi_note_numbers[MIDIQuantizer::NoteNumber(last_note[ch])] );
            gfxIcon(19 + note_x, 41 + (10 * ch), notes[ch]);
        }

    }
};
