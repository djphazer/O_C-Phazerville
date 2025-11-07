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

class Squanch : public HemisphereApplet {
public:
    enum SquanchCursor {
        SHIFT1, SHIFT2,
        NOTE_WRAP1, NOTE_WRAP2,
        SCALE, ROOT_NOTE,

        LAST_SETTING = ROOT_NOTE
    };

    const char* applet_name() {
        return "Squanch";
    }
    const uint8_t* applet_icon() { return PhzIcons::squanch; }

    void Start() {
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
                int32_t pitch_shifted = pitch + shift_alt;
                if (note_wrap[ch]) {
                  int offset = GetRootNote(ch) << 7;
                  pitch_shifted = ((pitch_shifted - offset) % (QuantizerLookup(ch, 64 + note_wrap[ch]) - offset)) + offset;
                }

                int32_t quantized = Quantize(ch, pitch_shifted, 0, shift[ch]);
                Out(ch, quantized);
                last_note[ch] = quantized;
            }
        }

    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

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

        case NOTE_WRAP1:
        case NOTE_WRAP2:
            note_wrap[cursor - NOTE_WRAP1] = constrain(note_wrap[cursor - NOTE_WRAP1] + direction, 0, 60);
            break;

        case SCALE:
            NudgeScale(0, direction);
            SetScale(1, GetScale(0)); // set both to the same
            continuous = 1; // Re-enable continuous mode when scale is changed
            break;

        case ROOT_NOTE:
          {
            int root = GetRootNote(0) + direction;
            ForEachChannel(ch)
              SetRootNote(ch, root);
            break;
          }
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, GetScale(0));
        Pack(data, PackLocation {8,8}, shift[0] + 48);
        Pack(data, PackLocation {16,8}, shift[1] + 48);
        Pack(data, PackLocation {24,4}, GetRootNote(0));
        Pack(data, PackLocation {28,6}, note_wrap[0]);
        Pack(data, PackLocation {34,6}, note_wrap[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        int scale = Unpack(data, PackLocation {0,8});
        shift[0] = Unpack(data, PackLocation {8,8}) - 48;
        shift[1] = Unpack(data, PackLocation {16,8}) - 48;
        int root = Unpack(data, PackLocation {24,4});
        note_wrap[0] = Unpack(data, PackLocation {28,6});
        note_wrap[1] = Unpack(data, PackLocation {34,6});
        ForEachChannel(ch) {
          SetScale(ch, scale);
          SetRootNote(ch, root);
        }
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "+OctCh1";
        help[HELP_CV1]      = "Signal";
        help[HELP_CV2]      = "Trn Ch2";
        help[HELP_OUT1]     = "Qnt Ch1";
        help[HELP_OUT2]     = "Qnt Ch2";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                  "---------------------" <-- Extra text size guide
    }
    
private:
    int cursor; // 0=A shift, 1=B shift, 2=Scale
    bool continuous = 1;
    int last_note[2]; // Last quantized note
    uint8_t note_wrap[2] = {0};

    // Settings
    int16_t shift[2];

    void DrawInterface() {
        const uint8_t * notes[2] = {NOTE_ICON, NOTE2_ICON};

        // Shift for A/C
        ForEachChannel(ch) {
            gfxIcon(1 + ch*32, 14, notes[ch]);
            gfxPrint(10 + pad(10, shift[ch]) + ch*32, 15, shift[ch] > -1 ? "+" : "");
            gfxPrint(shift[ch]);

            if (note_wrap[ch] || (NOTE_WRAP1 + ch) == cursor) {
              gfxIcon(7 + ch*32, 25, UP_DOWN_ICON);
              gfxPrint(16 + pad(10, note_wrap[ch]) + ch*32, 25, note_wrap[ch]);
            }
        }

        // Scale & Root Note
        gfxIcon(1, 34, SCALE_ICON);
        gfxPrint(10, 35, OC::scale_names_short[GetScale(0)]);
        gfxPrint(40, 35, OC::Strings::note_names_unpadded[GetRootNote(0)]);

        // Display icon if clocked
        if (!continuous) gfxIcon(55, 35, CLOCK_ICON);

        // Cursors
        switch (cursor) {
        case SHIFT1:
        case SHIFT2:
            gfxCursor(10 + (cursor - SHIFT1)*32, 23, 19);
            break;
        case NOTE_WRAP1:
        case NOTE_WRAP2:
            gfxCursor(16 + (cursor - NOTE_WRAP1)*32, 33, 13);
            break;
        case SCALE:
            gfxCursor(10, 43, 25);
            break;
        case ROOT_NOTE:
            gfxCursor(40, 43, 13);
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
            gfxPrint(0, 44 + 10*ch, midi_note_numbers[MIDIQuantizer::NoteNumber(last_note[ch])] );
            gfxIcon(19 + note_x, 44 + (10 * ch), notes[ch]);
        }

    }
};
