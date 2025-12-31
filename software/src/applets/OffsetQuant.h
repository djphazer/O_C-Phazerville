// Copyright (c) 2018, Jason Justian, Nick Beirne
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

// #define OFFSET_QUANT_MAX_CV_INPUT (9216 + NorthernLightModular*(4*12<<7)) // 6V or 10V

// just 20/24 simplifies to 5/6, but we want _just_ over 5v. 
#define OFFSET_QUANT_MAX_CV_INPUT (HEMISPHERE_MAX_INPUT_CV * 40 / 48)

enum RangeMode : uint8_t {
    RANGE_FULL,   // Full passthrough
    RANGE_0_2,    // 0V to 2V
    RANGE_1_3,    // 1V to 3V
    RANGE_2_4,    // 2V to 4V
    RANGE_3_5,    // 3V to 5V
    RANGE_LAST
};

const char* const range_mode_names[] = {
    "FULL", "0V-2V", "1V-3V", "2V-4V", "3V-5V"
};

class OffsetQuant : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "OffQuant";
    }
    const uint8_t* applet_icon() { return PhzIcons::dualQuantizer; }

    void Start() {
        cursor = 0;
        ForEachChannel(ch)
        {
            last_note[ch] = 0;
            continuous[ch] = 1;
            range_mode[ch] = RANGE_0_2;
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            // Use ADC lag to sample CV after a clock, or run continuously
            if (Clock(ch)) {
                continuous[ch] = 0; // Turn off continuous mode if there's a clock
                StartADCLag(ch);
            }

            if (continuous[ch] || EndOfADCLag(ch)) {
                int32_t pitch = In(ch);
                int32_t offset = 0;

                switch (range_mode[ch]) {
                    case RANGE_FULL:
                        offset = pitch;
                        break;
                    case RANGE_0_2:
                        offset = Proportion(pitch, OFFSET_QUANT_MAX_CV_INPUT, 2 * ONE_OCTAVE);
                        break;
                    case RANGE_1_3:
                        offset = Proportion(pitch, OFFSET_QUANT_MAX_CV_INPUT, 2 * ONE_OCTAVE) + (1 * ONE_OCTAVE);
                        break;
                    case RANGE_2_4:
                        offset = Proportion(pitch, OFFSET_QUANT_MAX_CV_INPUT, 2 * ONE_OCTAVE) + (2 * ONE_OCTAVE);
                        break;
                    case RANGE_3_5:
                        offset = Proportion(pitch, OFFSET_QUANT_MAX_CV_INPUT, 2 * ONE_OCTAVE) + (3 * ONE_OCTAVE);
                        break;
                    case RANGE_LAST: // Should not happen
                        break;
                }

                int32_t quantized = Quantize(ch, offset);
                Out(ch, quantized);

                last_note[ch] = quantized;
            }
        }
    }

    void View() {
        DrawSelector();
    }

    void OnButtonPress() {
        if (EditMode()) { // Exiting edit mode
            // Apply the temporary setting
            range_mode[cursor] = temp_range_mode;
        } else { // Entering edit mode
            // Store the current setting
            temp_range_mode = range_mode[cursor];
        }
        CursorToggle();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            cursor = (cursor + direction) % 2;
            ResetCursor();
            return;
        }

        temp_range_mode = (RangeMode)constrain(temp_range_mode + direction, 0, RANGE_LAST - 1);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 3}, range_mode[0]);
        Pack(data, PackLocation {3, 3}, range_mode[1]);
        Pack(data, PackLocation {6, 8}, GetScale(0));
        Pack(data, PackLocation {14, 8}, GetScale(1));
        Pack(data, PackLocation {22, 4}, GetRootNote(0));
        Pack(data, PackLocation {26, 4}, GetRootNote(1));
        return data;
    }

    void OnDataReceive(uint64_t data) {
        range_mode[0] = (RangeMode)Unpack(data, PackLocation {0, 3});
        range_mode[1] = (RangeMode)Unpack(data, PackLocation {3, 3});
        SetScale(0, Unpack(data, PackLocation {6, 8}));
        SetScale(1, Unpack(data, PackLocation {14, 8}));
        SetRootNote(0, Unpack(data, PackLocation {22, 4}));
        SetRootNote(1, Unpack(data, PackLocation {26, 4}));
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Pitch 2";
    help[HELP_EXTRA1] = "Set: Range";
    help[HELP_EXTRA2] = "     per chan";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor;
    int last_note[2]; // Last quantized note
    bool continuous[2]; // Each channel starts as continuous and becomes clocked when a clock is received
    RangeMode range_mode[2]; // The active range mode for each channel
    RangeMode temp_range_mode; // Temporary storage for editing

    void DrawSelector()
    {
        ForEachChannel(ch)
        {
            int x = 32 * ch;
            gfxPrint(x, 15, range_mode_names[ (ch == cursor && EditMode()) ? temp_range_mode : range_mode[ch] ]);

            // Draw cursor
            if (ch == cursor) {
                gfxCursor(x, 23, 30);
            }

            // Little note display
            if (!continuous[ch]) gfxBitmap(1, 41 + (10 * ch),  8, CLOCK_ICON); // Display icon if clocked
            int semitone = (last_note[ch] / 128) % 12;
            int note_x = semitone * 4; // 4 pixels per semitone
            if (note_x < 0) note_x = 0;
            gfxIcon(10 + note_x, 41 + (10 * ch), ch ? NOTE2_ICON : NOTE_ICON);
        }
    }
};
