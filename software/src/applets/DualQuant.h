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

class DualQuant : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "DualQuant";
    }
    const uint8_t* applet_icon() { return PhzIcons::dualQuantizer; }

    void Start() {
        cursor = 0;
        ForEachChannel(ch)
        {
            last_note[ch] = 0;
            continuous[ch] = 1;
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            if (Clock(ch)) {
                continuous[ch] = 0; // Turn off continuous mode if there's a clock
                StartADCLag(ch);
            }

            if (continuous[ch] || EndOfADCLag(ch)) {
                int32_t pitch = In(ch);
                int32_t quantized = Quantize(ch, pitch);
                Out(ch, quantized);
                last_note[ch] = quantized;
            }
        }
    }

    void View() {
        DrawSelector();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        uint8_t ch = cursor / 2;
        if (cursor == 0 || cursor == 2) {
          // Scale selection
          NudgeScale(ch, direction);
          continuous[ch] = 1; // Re-enable continuous mode when scale is changed
        } else {
          // Root selection
          SetRootNote(ch, GetRootNote(ch) + direction);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, GetScale(0));
        Pack(data, PackLocation {8,8}, GetScale(1));
        Pack(data, PackLocation {16,4}, GetRootNote(0));
        Pack(data, PackLocation {20,4}, GetRootNote(1));
        return data;
    }

    void OnDataReceive(uint64_t data) {
        SetScale(0, Unpack(data, PackLocation {0,8}));
        SetScale(1, Unpack(data, PackLocation {8,8}));
        SetRootNote(0, Unpack(data, PackLocation {16,4}));
        SetRootNote(1, Unpack(data, PackLocation {20,4}));
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
    help[HELP_EXTRA1] = "Set: Scale / Root";
    help[HELP_EXTRA2] = "     per channel";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int last_note[2]; // Last quantized note
    bool continuous[2]; // Each channel starts as continuous and becomes clocked when a clock is received
    int cursor;

    void DrawSelector()
    {
        const uint8_t * notes[2] = {NOTE_ICON, NOTE2_ICON};

        ForEachChannel(ch)
        {
            // Draw settings
            gfxPrint((31 * ch), 15, OC::scale_names_short[GetScale(ch)]);
            gfxBitmap(0 + (31 * ch), 25, 8, notes[ch]);
            gfxPrint(10 + (31 * ch), 25, OC::Strings::note_names_unpadded[GetRootNote(ch)]);

            // Draw cursor
            int y = cursor % 2; // 0=top line, 1=bottom
            if (ch == (cursor / 2)) {
                gfxCursor(y*10 + ch*31, 23 + y*10, 12+(1-y)*18);
            }

            // Little note display
            if (!continuous[ch]) gfxBitmap(1, 41 + (10 * ch),  8, CLOCK_ICON); // Display icon if clocked
            // This is for relative visual purposes only, so I don't really care if this isn't a semitone
            // scale, or even if it has 12 notes in it:
            int semitone = (last_note[ch] / 128) % 12;
            int note_x = semitone * 4; // 4 pixels per semitone
            if (note_x < 0) note_x = 0;
            gfxBitmap(10 + note_x, 41 + (10 * ch), 8, notes[ch]);
        }
    }
};
