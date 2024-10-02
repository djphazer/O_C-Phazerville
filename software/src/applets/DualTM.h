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

class DualTM : public HemisphereApplet {
public:
    
    static constexpr int MAX_SCALE = OC::Scales::NUM_SCALES;
    static constexpr int MIN_LENGTH = 2;
    static constexpr int MAX_LENGTH = 32;

    enum TM2Cursor {
        LENGTH,
        PROB,
        QUANT_A,
        QUANT_B,
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
        TRIGPITCH1,
        TRIGPITCH2,
        TRIG1,
        TRIG2,
        GATE1,
        GATE2,
        GATE_SUM,
        OUTMODE_COUNT
    };
    static constexpr const char* const outmode_names[OUTMODE_COUNT] = {
      "Blend", "Pitch1", "Pitch2", "Mod 1", "Mod 2",
      "TrPtch1", "TrPtch2", "Trig 1", "Trig 2",
      "Gate 1", "Gate 2", "Gate1+2"
    };

    enum InputMode {
        SLEW_MOD,
        LENGTH_MOD,
        P_MOD,
        Q_MOD,
        RANGE_MOD,
        TRANSPOSE1,
        TRANSPOSE2,
        BLEND_XFADE, // actually crossfade blend of both pitches
        INMODE_COUNT
    };
    static constexpr const char* const cvmode_names[INMODE_COUNT] = {
      // 7 char max each
      "Slew", "Length", "p mod", "Q mod", "Range", "Trans1", "Trans2", "Xfade"
    };

    const char* applet_name() {
        return "DualTM";
    }

    void Start() {
        reg[0] = random(0xFFFFFFFF);
        reg[1] = random(0xFFFFFFFF);
        qselect[0] = io_offset;
        qselect[1] = io_offset + 1;
    }

    void Reset() {
      if (reset_active) {
        ForEachChannel(ch) reg[ch] = reg_snap[ch];
      }
    }

    void Controller() {
        bool clk = Clock(0);
        if (clk) StartADCLag(0);
        bool update_cv = EndOfADCLag(0);

        int cv_data[2];
        cv_data[0] = DetentedIn(0);
        cv_data[1] = DetentedIn(1);

        // Reset
        if (Clock(1)) { Reset(); }

        // default to no mod
        p_mod = p;
        qselect_mod[0] = qselect[0];
        qselect_mod[1] = qselect[1];
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
                Modulate(len_mod, ch, MIN_LENGTH, MAX_LENGTH);
                break;

            case P_MOD:
                Modulate(p_mod, ch, 0, 100);
                break;

            case Q_MOD: {
                // select Quantizer with 3-semitone steps at the input
                const int cv = SemitoneIn(ch) / 3;
                qselect_mod[ch] = constrain(qselect_mod[ch] + cv, 0, QUANT_CHANNEL_COUNT - 1);
                break;
            }

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
          int prob = (cursor == PROB || (!reset_active && Gate(1))) ? p_mod : 0;

          if (rotate_right)
            ShiftRight(prob);
          else
            ShiftLeft(prob);
        }
 
        // Send 8-bit scaled and quantized CV
        const int32_t note[2] = {
          Proportion(reg[0] & 0xff, 0xff, range_mod) + 64,
          Proportion(reg[1] & 0xff, 0xff, range_mod) + 64
        };

        ForEachChannel(ch) {
            switch (outmode[ch]) {
            case PITCH_BLEND: {
              // this is the unique case where input CV crossfades between the two melodies
              int x = constrain(note_trans[2], -range_mod, range_mod);
              int y = range_mod;
              int n = (note[0] * (y + x) + note[1] * (y - x)) / (2*y);
              slew(Output[ch], HS::QuantizerLookup(qselect_mod[ch], n));
              break;
            }
            case PITCH1:
              slew(Output[ch], HS::QuantizerLookup(qselect_mod[ch], note[0] + note_trans[0]));
              break;
            case PITCH2:
              slew(Output[ch], HS::QuantizerLookup(qselect_mod[ch], note[1] + note_trans[1]));
              break;
            case MOD1: // 8-bit bi-polar proportioned CV
              slew(Output[ch], Proportion( int(reg[0] & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case MOD2:
              slew(Output[ch], Proportion( int(reg[1] & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case TRIGPITCH1:
            case TRIGPITCH2: {
              const int rnum = outmode[ch] - TRIGPITCH1;
              if (clk && (reg[outmode[ch]-TRIGPITCH1] & 0x01) == 1) // trigger if 1st bit is high
              {
                Output[ch] = HEMISPHERE_MAX_CV;
                trigpulse[ch] = HEMISPHERE_CLOCK_TICKS * trig_length;
              }
              else // decay to pitch
              {
                // hold until it's time to pull it down
                if (--trigpulse[ch] < 0)
                  slew(Output[ch], HS::QuantizerLookup(qselect_mod[ch], note[rnum] + note_trans[rnum]));
              }
              break;
            }
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
                  slew(Output[ch], 0);
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

    void DrawFullScreen() {
      HemisphereApplet::DrawFullScreen();
      if (cursor >= CVMODE1 && cursor <= OUT_B) {
        // this is an ugly hack, but it'll work lol
        gfxCursor(19 - gfx_offset + 64*((cursor-CVMODE1)%2), 32 + 10*((cursor-CVMODE1)/2), 44);
      }
    }
    // void OnButtonPress() { }

    void AuxButton() {
      switch (cursor) {
      case QUANT_A:
      case QUANT_B:
        HS::QuantizerEdit(qselect[cursor - QUANT_A]);
      default:
        isEditing = false;
        break;

      case PROB:
        reset_active = !reset_active;
        if (reset_active) {
          // grab snapshots of the registers
          ForEachChannel(ch) reg_snap[ch] = reg[ch];
        }
        break;

      case LENGTH:
        rotate_right = !rotate_right;
        break;
      }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((TM2Cursor)cursor) {
        case LENGTH:
            length = constrain(length + direction, MIN_LENGTH, MAX_LENGTH);
            break;
        case PROB:
            p = constrain(p + direction, 0, 100);
            break;
        case QUANT_A:
        case QUANT_B:
            HS::qview = qselect[cursor - QUANT_A] =
              constrain(qselect[cursor - QUANT_A] + direction, 0, QUANT_CHANNEL_COUNT - 1);
            HS::PokePopup(QUANTIZER_POPUP);
            break;
        case RANGE:
            range = constrain(range + direction, 1, 32);
            break;
        case OUT_A:
            outmode[0] = (OutputMode) constrain(outmode[0] + direction, 0, OUTMODE_COUNT-1);
            break;
        case OUT_B:
            outmode[1] = (OutputMode) constrain(outmode[1] + direction, 0, OUTMODE_COUNT-1);
            break;
        case CVMODE1:
            cvmode[0] = (InputMode) constrain(cvmode[0] + direction, 0, INMODE_COUNT-1);
            break;
        case CVMODE2:
            cvmode[1] = (InputMode) constrain(cvmode[1] + direction, 0, INMODE_COUNT-1);
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
        //Pack(data, PackLocation {25,8}, constrain(GetScale(0), 0, 255));
        Pack(data, PackLocation {33,4}, cvmode[0]);
        Pack(data, PackLocation {37,4}, cvmode[1]);
        Pack(data, PackLocation {41,6}, smoothing);

        Pack(data, PackLocation {48,4}, qselect[0]);
        Pack(data, PackLocation {52,4}, qselect[1]);

        Pack(data, PackLocation {56,1}, rotate_right);

        // TODO: utilize enigma's global turing machine storage for the registers

        return data;
    }

    void OnDataReceive(uint64_t data) {
        p = Unpack(data, PackLocation {0,7});
        length = Unpack(data, PackLocation {7,5}) + 1;
        range = Unpack(data, PackLocation{12,5}) + 1;
        outmode[0] = (OutputMode) Unpack(data, PackLocation {17,4});
        outmode[1] = (OutputMode) Unpack(data, PackLocation {21,4});
        //int scale = Unpack(data, PackLocation {25,8});
        cvmode[0] = (InputMode) Unpack(data, PackLocation {33,4});
        cvmode[1] = (InputMode) Unpack(data, PackLocation {37,4});
        smoothing = Unpack(data, PackLocation {41,6});
        smoothing = constrain(smoothing, 0, 127);

        qselect[0] = Unpack(data, PackLocation {48,4});
        qselect[1] = Unpack(data, PackLocation {52,4});
        CONSTRAIN(qselect[0], 0, DAC_CHANNEL_LAST - 1);
        CONSTRAIN(qselect[1], 0, DAC_CHANNEL_LAST - 1);

        rotate_right = Unpack(data, PackLocation {56,1});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "p Gate";
    help[HELP_CV1]      = cvmode_names[cvmode[0]];
    help[HELP_CV2]      = cvmode_names[cvmode[1]];
    help[HELP_OUT1]     = outmode_names[outmode[0]];
    help[HELP_OUT2]     = outmode_names[outmode[1]];
    help[HELP_EXTRA1]  = "Encoder: Select/Edit";
    help[HELP_EXTRA2]  = "AuxBtn: Reverse/Lock";
    //                   "---------------------" <-- Extra text size guide
  }
    
private:
    int cursor; // TM2Cursor

    int root_note = 0;

    // TODO: consider using the TuringMachine class or whatev
    uint32_t reg[2]; // 32-bit sequence registers
    uint32_t reg_snap[2]; // for resetting
    bool reset_active = false;
    bool rotate_right = true;

    // most recent output values
    int Output[2] = {0, 0};
    int trigpulse[2] = {0, 0}; // tick timer for Trig output modes

    // Settings and modulated copies
    int qselect[2];
    int qselect_mod[2];
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

    void ShiftLeft(int prob) {
      ForEachChannel(i) {
        // Grab the bit that's about to be shifted away
        uint32_t last = (reg[i] >> (len_mod - 1)) & 0x01;

        // Does it change?
        if (random(0, 99) < prob) last = 1 - last;

        // Shift left, then potentially add the bit from the other side
        reg[i] = (reg[i] << 1) + last;
      }
    }
    void ShiftRight(int prob) {
      ForEachChannel(i) {
        // Grab the bit that's about to be shifted away
        uint32_t last = reg[i] & 0x01;

        // Does it change?
        if (random(0, 99) < prob) last = 1 - last;
        last = last << (len_mod - 1);

        // Shift right, then potentially add the bit from the other side
        reg[i] = ((reg[i] >> 1) & ~(1 << (len_mod - 1))) | last ;
      }
    }

    void DrawOutputMode(int ch) {
        const int y = 35;
        const int x = 34*ch;

        gfxPrint(x+1, y+1, OutputLabel(ch));
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
        case TRIGPITCH1:
        case TRIGPITCH2:
            gfxBitmap(15 + x, y, 8, SINGING_PIGEON_ICON);
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
        case Q_MOD:
            gfxPrint(15 + x, y, "Q");
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
        gfxPrint(11 + pad(10, len_mod), 15, len_mod);
        gfxIcon(25, 15, rotate_right ? ROTATE_R_ICON : ROTATE_L_ICON);

        gfxPrint(35 + pad(100, p_mod), 15, p_mod);
        if (cursor == PROB || (!reset_active && Gate(1))) { // p unlocked
            gfxBitmap(55, 15, 8, TOSS_ICON);
        } else { // p is disabled
            gfxBitmap(55, 15, 8, LOCK_ICON);
        }
        if (reset_active) gfxInvert(54, 14, 9, 9);

        // two separate pages of params
        switch ((TM2Cursor)cursor){
        default:
        case SLEW:
            gfxBitmap(1, 25, 8, SCALE_ICON);
            gfxPrint(12, 25, "Q"); gfxPrint(qselect_mod[0] + 1);
            gfxPrint(39, 25, "Q"); gfxPrint(qselect_mod[1] + 1);

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
            case LENGTH: gfxSpicyCursor(11, 23, 13); break;
            case PROB:   gfxSpicyCursor(35, 23, 19); break;
            case QUANT_A:  gfxSpicyCursor(12, 33, 13); break;
            case QUANT_B:  gfxSpicyCursor(39, 33, 13); break;
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
        for (int b = 0; b < 32; ++b)
        {
            int v = (reg[0] >> b) & 0x01;
            int v2 = (reg[1] >> b) & 0x01;
            if (v) gfxRect(62 - (2 * b), 47, 1, 7);
            if (v2) gfxRect(62 - (2 * b), 54, 1, 7);
        }
    }

};

