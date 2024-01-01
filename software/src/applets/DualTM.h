// Copyright (c) 2018, Jason Justian
// Copyright (c) 2022, Nicholas J. Michalek
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

/*
 * Turing Machine based on https://thonk.co.uk/documents/random%20sequencer%20documentation%20v2-1%20THONK%20KIT_LargeImages.pdf
 *
 * Thanks to Tom Whitwell for creating the concept, and for clarifying some things
 * Thanks to Jon Wheeler for the CV length and probability updates
 *
 * Heavily adapted as DualTM from ShiftReg/TM by djphazer (Nicholas J. Michalek)
 */

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define TM2_MAX_SCALE OC::Scales::NUM_SCALES

#define TM2_MIN_LENGTH 2
#define TM2_MAX_LENGTH 32

class DualTM : public HemisphereApplet {
public:
    
    enum TM2Cursor {
        LENGTH,
        PROB,
        SCALE,
        ROOT_NOTE,
        RANGE,
        SLEW,
        CVMODE1,
        CVMODE2,
        OUT_A,
        OUT_B,
        LAST_SETTING = OUT_B
    };

    enum OutputMode {
        PITCH_BLEND,
        PITCH1,
        PITCH2,
        MOD1,
        MOD2,
        TRIG1,
        TRIG2,
        GATE1,
        GATE2,
        GATE_SUM,
        OUTMODE_LAST
    };

    enum InputMode {
        SLEW_MOD,
        LENGTH_MOD,
        P_MOD,
        RANGE_MOD,
        TRANSPOSE1,
        TRANSPOSE2,
        BLEND_XFADE, // actually crossfade blend of both pitches
        INMODE_LAST
    };

    const char* applet_name() {
        return "DualTM";
    }

    void Start() {
        reg[0] = random(0, 65535);
        reg[1] = ~reg[0];
    }

    void Controller() {
        bool clk = Clock(0);
        if (clk) StartADCLag(0);
        bool update_cv = EndOfADCLag(0);

        int cv_data[2];
        cv_data[0] = DetentedIn(0);
        cv_data[1] = DetentedIn(1);

        // default to no mod
        p_mod = p;
        len_mod = length;
        range_mod = range;
        smooth_mod = smoothing;
        int trans_mod[3] = {0, 0, 0}; // default transpose

        // process CV inputs
        ForEachChannel(ch) {
            switch (cvmode[ch]) {
            case SLEW_MOD:
                Modulate(smooth_mod, ch, 0, 127);
                break;
            case LENGTH_MOD:
                Modulate(len_mod, ch, TM2_MIN_LENGTH, TM2_MAX_LENGTH);
                break;

            case P_MOD:
                Modulate(p_mod, ch, 0, 100);
                break;

            case RANGE_MOD:
                Modulate(range_mod, ch, 1, 32);
                break;

            // bi-polar transpose before quantize
            case TRANSPOSE1:
            case TRANSPOSE2:
            case BLEND_XFADE:
                if (update_cv) // S&H style transpose
                    trans_mod[cvmode[ch] - TRANSPOSE1] = MIDIQuantizer::NoteNumber(cv_data[ch], 0) - 60; // constrain to range_mod?
                break;

            default: break;
            }
        }

        if (update_cv) {
            // Update transpose values
            for (int i = 0; i < 3; ++i) { note_trans[i] = trans_mod[i]; }
        }

        // Advance the register on clock, flipping bits as necessary
        if (clk) {
            // If the cursor is not on the p value, and Digital 2 is not gated, the sequence remains the same
            int prob = (cursor == PROB || Gate(1)) ? p_mod : 0;

            for (int i = 0; i < 2; ++i) {
                // Grab the bit that's about to be shifted away
                int last = (reg[i] >> (len_mod - 1)) & 0x01;

                // Does it change?
                if (random(0, 99) < prob) last = 1 - last;

                // Shift left, then potentially add the bit from the other side
                reg[i] = (reg[i] << 1) + last;
            }
        }
 
        // Send 8-bit scaled and quantized CV
        int32_t note = Proportion(reg[0] & 0xff, 0xff, range_mod) + 64;
        int32_t note2 = Proportion(reg[1] & 0xff, 0xff, range_mod) + 64;

        ForEachChannel(ch) {
            switch (outmode[ch]) {
            case PITCH_BLEND: {
              // this is the unique case where input CV crossfades between the two melodies
              int x = constrain(note_trans[2], -range_mod, range_mod);
              int y = range_mod;
              int n = (note * (y + x) + note2 * (y - x)) / (2*y);
              slew(Output[ch], QuantizerLookup(0, n));
              break;
              }
            case PITCH1:
              slew(Output[ch], QuantizerLookup(0, note + note_trans[0]));
              break;
            case PITCH2:
              slew(Output[ch], QuantizerLookup(0, note2 + note_trans[1]));
              break;
            case MOD1: // 8-bit bi-polar proportioned CV
              slew(Output[ch], Proportion( int(reg[0] & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case MOD2:
              slew(Output[ch], Proportion( int(reg[1] & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case TRIG1:
            case TRIG2:
              if (clk && (reg[outmode[ch]-TRIG1] & 0x01) == 1) // trigger if 1st bit is high
              {
                Output[ch] = HEMISPHERE_MAX_CV; //ClockOut(ch);
                trigpulse[ch] = HEMISPHERE_CLOCK_TICKS * trig_length;
              }
              else // decay
              {
                // hold until it's time to pull it down
                if (--trigpulse[ch] < 0)
                  slew(Output[ch]);
              }
              break;
            case GATE1:
            case GATE2:
              slew(Output[ch], (reg[outmode[ch] - GATE1] & 0x01)*HEMISPHERE_MAX_CV );
              break;

            case GATE_SUM:
              slew(Output[ch], ((reg[0] & 0x01)+(reg[1] & 0x01))*HEMISPHERE_MAX_CV/2 );
              break;

            default: break;
            }

            Out(ch, Output[ch]);
        }
    }

    void View() {
        DrawSelector();
        DrawIndicator();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((TM2Cursor)cursor) {
        case LENGTH:
            length = constrain(length + direction, TM2_MIN_LENGTH, TM2_MAX_LENGTH);
            break;
        case PROB:
            p = constrain(p + direction, 0, 100);
            break;
        case SCALE:
            NudgeScale(0, direction);
            NudgeScale(1, direction);
            break;
        case ROOT_NOTE:
            root_note = GetRootNote(0);
            root_note = SetRootNote(0, constrain(root_note + direction, 0, 11));
            break;
        case RANGE:
            range = constrain(range + direction, 1, 32);
            break;
        case OUT_A:
            outmode[0] = (OutputMode) constrain(outmode[0] + direction, 0, OUTMODE_LAST-1);
            break;
        case OUT_B:
            outmode[1] = (OutputMode) constrain(outmode[1] + direction, 0, OUTMODE_LAST-1);
            break;
        case CVMODE1:
            cvmode[0] = (InputMode) constrain(cvmode[0] + direction, 0, INMODE_LAST-1);
            break;
        case CVMODE2:
            cvmode[1] = (InputMode) constrain(cvmode[1] + direction, 0, INMODE_LAST-1);
            break;
        case SLEW:
            smoothing = constrain(smoothing + direction, 0, 127);
            break;

        default: break;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, p);
        Pack(data, PackLocation {7,5}, length - 1);
        Pack(data, PackLocation {12,5}, range - 1);
        Pack(data, PackLocation {17,4}, outmode[0]);
        Pack(data, PackLocation {21,4}, outmode[1]);
        Pack(data, PackLocation {25,8}, constrain(GetScale(0), 0, 255));
        Pack(data, PackLocation {33,4}, cvmode[0]);
        Pack(data, PackLocation {37,4}, cvmode[1]);
        Pack(data, PackLocation {41,6}, smoothing);
        Pack(data, PackLocation {47,4}, root_note);

        // TODO: utilize enigma's global turing machine storage for the registers

        return data;
    }

    void OnDataReceive(uint64_t data) {
        p = Unpack(data, PackLocation {0,7});
        length = Unpack(data, PackLocation {7,5}) + 1;
        range = Unpack(data, PackLocation{12,5}) + 1;
        outmode[0] = (OutputMode) Unpack(data, PackLocation {17,4});
        outmode[1] = (OutputMode) Unpack(data, PackLocation {21,4});
        int scale = Unpack(data, PackLocation {25,8});
        cvmode[0] = (InputMode) Unpack(data, PackLocation {33,4});
        cvmode[1] = (InputMode) Unpack(data, PackLocation {37,4});
        smoothing = Unpack(data, PackLocation {41,6});
        smoothing = constrain(smoothing, 0, 127);
        root_note = Unpack(data, PackLocation {47,4});

        QuantizerConfigure(0, scale);
        SetRootNote(0, root_note);
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=p Gate";
        help[HEMISPHERE_HELP_CVS]      = "Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "Assignable";
        help[HEMISPHERE_HELP_ENCODER]  = "Select/Push 2 Edit";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // TM2Cursor

    int root_note = 0;

    // TODO: consider using the TuringMachine class or whatev
    uint32_t reg[2]; // 32-bit sequence registers

    // most recent output values
    int Output[2] = {0, 0};
    int trigpulse[2] = {0, 0}; // tick timer for Trig output modes

    // Settings and modulated copies
    int length = 16; // Sequence length
    int len_mod; // actual length after CV mod
    int p = 0; // Probability of bit flipping on each cycle
    int p_mod;
    int range = 24;
    int range_mod;
    int smoothing = 0;
    int smooth_mod;
    int note_trans[3] = {0, 0, 0}; // transpose from CV input

    OutputMode outmode[2] = {PITCH1, TRIG2};
    InputMode cvmode[2] = {LENGTH_MOD, RANGE_MOD};

    void slew(int &old_val, const int new_val = 0) {
        const int s = 1 + smooth_mod;
        // more smoothing causes more ticks to be skipped
        if (OC::CORE::ticks % s) return;

        old_val = (old_val * (s - 1) + new_val) / s;
    }

    void DrawOutputMode(int ch) {
        const int y = 35;
        const int x = 34*ch;

        gfxPrint(x+1, y+1, ch ? (hemisphere ? "D" : "B") : (hemisphere ? "C" : "A") );
        gfxPrint(":");

        switch (outmode[ch]) {
        case PITCH_BLEND: gfxBitmap(24+x, y, 3, SUP_ONE);
        case PITCH1:
        case PITCH2:
            gfxBitmap(15 + x, y, 8, NOTE_ICON);
            break;
        case MOD1:
        case MOD2:
            gfxBitmap(15 + x, y, 8, WAVEFORM_ICON);
            break;
        case TRIG1:
        case TRIG2:
            gfxBitmap(15 + x, y, 8, CLOCK_ICON);
            break;
        case GATE_SUM: gfxBitmap(24+x, y, 3, SUB_TWO);
        case GATE1:
        case GATE2:
            gfxBitmap(15 + x, y, 8, GATE_ICON);
            break;

        default: break;
        }

        // indicator for reg1 or reg2
        gfxBitmap(24+x, y, 3, (outmode[ch] % 2) ? SUP_ONE : SUB_TWO );
    }

    void DrawCVMode(int ch) {
        const int y = 25;
        const int x = 34*ch;

        gfxIcon(1 + x, y, CV_ICON);
        gfxBitmap(9 + x, y, 3, ch ? SUB_TWO : SUP_ONE);

        switch (cvmode[ch]) {
        case SLEW_MOD:
            gfxIcon(15 + x, y, SLEW_ICON);
            break;
        case LENGTH_MOD:
            gfxIcon(15 + x, y, LOOP_ICON);
            break;
        case P_MOD:
            gfxIcon(15 + x, y, TOSS_ICON);
            break;
        case RANGE_MOD:
            gfxIcon(15 + x, y, UP_DOWN_ICON);
            break;
        case TRANSPOSE1:
            gfxIcon(15 + x, y, BEND_ICON);
            gfxBitmap(24+x, y, 3, SUP_ONE);
            break;
        case BLEND_XFADE:
            gfxBitmap(24+x, y, 3, SUP_ONE);
        case TRANSPOSE2:
            gfxIcon(15 + x, y, BEND_ICON);
            gfxBitmap(24+x, y, 3, SUB_TWO);
            break;

        default: break;
        }

    }

    void DrawSelector() {
        gfxBitmap(1, 14, 8, LOOP_ICON);
        gfxPrint(12 + pad(10, len_mod), 15, len_mod);
        gfxPrint(35 + pad(100, p_mod), 15, p_mod);
        if (cursor == PROB || Gate(1)) { // p unlocked
            gfxBitmap(55, 15, 8, TOSS_ICON);
        } else { // p is disabled
            gfxBitmap(55, 15, 8, LOCK_ICON);
        }

        // two separate pages of params
        switch ((TM2Cursor)cursor){
        default:
        case SLEW:
            gfxBitmap(1, 25, 8, SCALE_ICON);
            gfxPrint(9, 25, OC::scale_names_short[GetScale(0)]);
            gfxPrint(39, 25, OC::Strings::note_names_unpadded[GetRootNote(0)]);

            gfxBitmap(1, 35, 8, UP_DOWN_ICON);
            gfxPrint(10, 35, range_mod);

            gfxIcon(35, 35, SLEW_ICON);
            gfxPrint(44, 35, smooth_mod);

            break;
        case CVMODE1:
        case CVMODE2:
        case OUT_A:
        case OUT_B:
            ForEachChannel(ch) {
                DrawCVMode(ch);
                DrawOutputMode(ch);
            }

            break;
        }

        // TODO: generalize this as a cursor LUT for all applets
        switch ((TM2Cursor)cursor) {
            case LENGTH: gfxCursor(12, 23, 13); break;
            case PROB:   gfxCursor(35, 23, 19); break;
            case SCALE:  gfxCursor( 9, 33, 25); break;
            case ROOT_NOTE:  gfxCursor( 39, 33, 13); break;
            case RANGE:  gfxCursor(10, 43, 13); break;
            case SLEW:   gfxCursor(44, 43, 19); break;

            case CVMODE1:
            case CVMODE2:
                gfxCursor(14 + 34*(cursor-CVMODE1), 33, 10);
                break;

            case OUT_A:  gfxCursor(14, 43, 10); break;
            case OUT_B:  gfxCursor(48, 43, 10); break;

            default: break;
        }
    }

    void DrawIndicator() {
        gfxLine(0, 45, 63, 45);
        gfxLine(0, 62, 63, 62);
        for (int b = 0; b < 16; b++)
        {
            int v = (reg[0] >> b) & 0x01;
            int v2 = (reg[1] >> b) & 0x01;
            if (v) gfxRect(60 - (4 * b), 47, 3, 7);
            if (v2) gfxRect(60 - (4 * b), 54, 3, 7);
        }
    }

};

