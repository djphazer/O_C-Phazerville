// Copyright (c) 2018, Jason Justian
//
// Chord library (c) 2018, Roel Das
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

#include "hem_arp_chord.h"
#include "HSMIDI.h"
#define HEM_CARPEGGIO_ANIMATION_SPEED 500

class Carpeggio : public HemisphereApplet {
public:

    enum CarpeggioCursor {
        CHORD,
        TRANSPOSE,
        SHUFFLE,
        NOTE_EDIT,
        LAST_SETTING = NOTE_EDIT
    };

    const char* applet_name() {
        return "Carpeggio";
    }

    void Start() {
        step = 0;
        replay = 0;
        transpose = 0;
        reset = true;
        ImprintChord(2);
        pitch_out_for_step();
    }

    void Controller() {
        if (Clock(1)) { // Reset
            step = 0; 
            reset = true;
            pitch_out_for_step();
        }

        if (Clock(0)) {
            // Are the X or Y position being set? If so, get step coordinates. Otherwise,
            // simply play current step and advance it. This way, the applet can be used as
            // a more conventional arpeggiator as well as a Cartesian one.
            if (DetentedIn(0) || DetentedIn(1)) {
                int x = ProportionCV(In(0), 4);
                int y = ProportionCV(In(1), 4);
                if (x > 3) x = 3;
                if (y > 3) y = 3;
                step = (y * 4) + x;
                pitch_out_for_step();
            } else {
                if (!reset) {
                    ++step;
                }
                reset = false;
                if (step > 15) step = 0;
                pitch_out_for_step();
            }
            replay = 0;
        } else if (replay) {
            pitch_out_for_step();
            replay = 0;
        }

        // Modulation output
        int xy = (In(0) * In(1)) / HEMISPHERE_MAX_INPUT_CV;
        Out(1, xy);

        // Handle imprint confirmation animation
        if (--confirm_animation_countdown < 0) {
            confirm_animation_position--;
            confirm_animation_countdown = HEM_CARPEGGIO_ANIMATION_SPEED;
        }
    }

    void View() {
        DrawSelector();
        DrawGrid();
    }

    void OnButtonPress() {
        if (cursor == SHUFFLE && !EditMode()) // special case toggle
            shuffle ? ImprintChord(sel_chord) : ShuffleChord();
        else // Advance or toggle cursor
            CursorAction(cursor, LAST_SETTING);

        // Set a chord imprint if a new chord is picked
        if (chord != sel_chord) {
            ImprintChord(chord);
            cursor = CHORD; // keep cursor on chord selection
        }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((CarpeggioCursor)cursor) {
        case NOTE_EDIT:
            sequence[step] = constrain(sequence[step] + direction, -24, 60);
            break;
        case CHORD:
            chord = constrain(chord + direction, 0, Nr_of_arp_chords - 1);
            break;
        case TRANSPOSE:
            transpose = constrain(transpose + direction, -24, 24);
            break;

        case SHUFFLE:
            if (shuffle && direction < 0) {
                ImprintChord(sel_chord);
            }
            if (direction > 0) {
                ShuffleChord();
            }
            break;
        }

        if (cursor != CHORD) replay = 1;
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, sel_chord);
        Pack(data, PackLocation {8,8}, transpose + 24);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        ImprintChord(Unpack(data, PackLocation {0,8}));
        transpose = Unpack(data, PackLocation {8,8}) - 24;
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=X 2=Y";
        help[HEMISPHERE_HELP_OUTS]     = "A=Quant B=(XY)/5V";
        help[HEMISPHERE_HELP_ENCODER]  = "Note/Chord/Trnspos";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // CarpeggioCursor

    // Sequencer state
    uint8_t step; // Current step number
    int16_t sequence[16];
    bool replay; // When the encoder is moved, re-quantize the output
    bool reset;

    // Settings
    int chord; // Selected chord
    int sel_chord; // Most recently-imprinted chord
    int transpose; // Transposition setting (-24 ~ +24)
    bool shuffle = false;

    // Variables to handle imprint confirmation animation
    int confirm_animation_countdown;
    int confirm_animation_position;

    void DrawSelector() {
        // Chord selector
        gfxPrint(0, 15, Arp_Chords[chord].chord_name);
        if (cursor == CHORD) {
            gfxCursor(1, 23, 53);
            gfxBitmap(55, 14, 8, chord == sel_chord ? CHECK_ON_ICON : CHECK_OFF_ICON);
        }

        // Transpose editor
        gfxPrint(32, 25, "Tr");
        gfxPrint(transpose < 0 ? "" : "+");
        gfxPrint(transpose);
        if (cursor == TRANSPOSE) gfxCursor(32, 33, 30);

        // Shuffle selector
        gfxBitmap(37, 36, 8, PLAY_ICON);
        gfxBitmap(49, 36, 8, LOOP_ICON);
        gfxInvert(36 + (shuffle ? 12 : 0), 35, 10, 10);
        if (cursor == SHUFFLE) gfxCursor(36, 46, 22, 11);

        // Note name editor
        uint8_t midi_note = constrain(sequence[step] + 36 + transpose, 0, 127);
        gfxPrint(38, 50, midi_note_numbers[midi_note]);
        if (cursor == NOTE_EDIT) gfxCursor(32, 58, 30);
    }

    void DrawGrid() {
        // Draw the Cartesian plane
        for (int s = 0; s < 16; s++) gfxFrame(1 + (8 * (s % 4)), 26 + (8 * (s / 4)), 5, 5);

        // Crosshairs for play position
        int cxy = step / 4;
        int cxx = step % 4;
        gfxDottedLine(3 + (8 * cxx), 26, 3 + (8 * cxx), 58, 2);
        gfxDottedLine(1, 28 + (8 * cxy), 32, 28 + (8 * cxy), 2);
        gfxRect(1 + (8 * cxx), 26 + (8 * cxy), 5, 5);

        // Draw confirmation animation, if necessary
        if (confirm_animation_position > -1) {
            int progress = 16 - confirm_animation_position;
            for (int s = 0; s < progress; s++)
            {
                gfxRect(1 + (8 * (s / 4)), 26 + (8 * (s % 4)), 7, 7);
            }
        }
    }

    void ImprintChord(int new_chord) {
        int num = Arp_Chords[new_chord].nr_notes;
        for (int s = 0; s < 16; s++)
        {
            int oct = (s / num) % 4; // Increase one octave each time this cycle repeats to a max of 4
            int tone = s % num;
            sequence[s] = Arp_Chords[new_chord].chord_tones[tone] + (12 * oct);
        }
        sel_chord = new_chord;
        chord = new_chord;
        confirm_animation_position = 16;
        confirm_animation_countdown = HEM_CARPEGGIO_ANIMATION_SPEED;
        shuffle = false;
    }

    void ShuffleChord() {
        int16_t old; // temp var for note being swapped
        int16_t rnd; // temp var for index of note swapping in
        for (int i = 0; i < 16; i++) {
            // set old to current step value
            old = sequence[i];
            rnd = random(0, 16);
            sequence[i] = sequence[rnd];
            sequence[rnd] = old;
        }
        confirm_animation_position = 16;
        confirm_animation_countdown = HEM_CARPEGGIO_ANIMATION_SPEED;
        shuffle = true;
    }

    void pitch_out_for_step() {
        int note = sequence[step];
        Out(0, MIDIQuantizer::CV(note + 36, transpose));
    }
};
