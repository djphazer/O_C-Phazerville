// Copyright (c) 2024, Nicholas J. Michalek
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

#include "MiniSeq.h"

class Seq32 : public HemisphereApplet {
public:

    static constexpr int MAX_TRANS = 32;
    static constexpr int GLIDE_FACTOR = 12;

    // sets the function of the accent bit
    enum AccentMode {
      VELOCITY,
      TIE_NOTE,
      OCTAVE_UP,
      DOUBLE_OCTAVE_UP,
    };

    enum Seq32Cursor {
      // ---
      PATTERN,
      LENGTH,
      WRITE_MODE,
      // ---
      QUANT_SCALE,
      QUANT_ROOT,
      TRANSPOSE,
      // ---
      NOTES,
      MAX_CURSOR = TRANSPOSE + MiniSeq::MAX_STEPS
    };

   MiniSeq seq;

    const char* applet_name() { // Maximum 10 characters
        return "Seq32";
    }

    void Start() {
    }

    void Reset() {
      seq.Reset();
    }

    void Controller()
    {
      if (Clock(1)) { // reset
        Reset();
      }
      if (Clock(0)) { // clock

        if (!write_mode) {
          // CV modulation of pattern and transposition
          pattern_mod = pattern_index;
          Modulate(pattern_mod, 0, 0, 7);
          seq.SetPattern(pattern_mod);

          trans_mod = transpose;
          Modulate(trans_mod, 1, -MAX_TRANS, MAX_TRANS);
        }

        seq.Advance();

        if (seq.muted(seq.step)) {
          GateOut(1, false);
        } else {
          current_note = seq.GetNote();

          if (seq.accent(seq.step)) {
            // - tied note
            //GateOut(1, true);

            // - 50% duty cycle gate
            ClockOut(1, ClockCycleTicks(0)/2);

          } else {
            // regular trigger
            ClockOut(1);
          }
        }
        StartADCLag();
      }

      if (EndOfADCLag() && write_mode) {
        // sample and record note number from cv1
        Quantize(0, In(0));
        current_note = GetLatestNoteNumber(0) - 64;
        seq.SetNote(current_note, seq.step);

        if (In(1) > (6 << 7)) // cv2 > 0.5V determines mute state
          seq.Unmute(seq.step);
        else
          seq.Mute(seq.step);

        seq.SetAccent(seq.step, In(1) > (24 << 7)); // cv2 > 2V qualifies as accent
      }
      
      // continuously compute CV with transpose
      int play_note = current_note + 64 + trans_mod;
      CONSTRAIN(play_note, 0, 127);
      // set CV output
      int play_cv = QuantizerLookup(0, play_note);
      if (seq.accent(seq.step)) {
        // glide
        SmoothedOut(0, play_cv, GLIDE_FACTOR);
      } else {
        Out(0, play_cv);
      }

      if (flash_ticker) --flash_ticker;
      if (edit_ticker) --edit_ticker;
    }

    void View() {
      DrawPanel();
    }

    void OnButtonPress() {
      if (cursor == QUANT_SCALE || cursor == QUANT_ROOT)
        HS::QuantizerEdit(io_offset);
      else
        CursorToggle();

      if (cursor == WRITE_MODE) // toggle
        write_mode = EditMode();

      // double-click toggles accent
      if (cursor >= NOTES) {
        if ( OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME ) {
          seq.ToggleAccent(cursor - NOTES);
        } else {
          click_tick = OC::CORE::ticks;
        }
      }
    }
    void AuxButton() {
      if (cursor >= NOTES)
        seq.ToggleMute(cursor - NOTES);
      else if (cursor == LENGTH)
        seq.Randomize(true);
      else if (cursor == TRANSPOSE)
        seq.SowPitches(abs(transpose));
      else if (cursor == PATTERN)
        seq.Clear();
      
      if (cursor < NOTES) flash_ticker = 1000;

      isEditing = false;
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR - (MiniSeq::MAX_STEPS - seq.GetLength()));
        return;
      }

      edit_ticker = 5000;
      switch (cursor) {
        default: // (cursor >= NOTES)
          seq.SetNote(seq.GetNote(cursor-NOTES) + direction, cursor-NOTES);
          if ((cursor-NOTES) == seq.step)
            current_note = seq.GetNote();
          break;
        case PATTERN:
          // TODO: queued pattern changes
          pattern_index += direction;
          seq.SetPattern(pattern_index);
          pattern_mod = pattern_index;
          break;

        case LENGTH:
          seq.SetLength(constrain(seq.GetLength() + direction, 1, MiniSeq::MAX_STEPS));
          break;

        /* handled in popup editor
        case QUANT_SCALE:
          NudgeScale(0, direction);
          break;

        case QUANT_ROOT:
          SetRootNote(0, GetRootNote(0) + direction);
          break;
        */

        case TRANSPOSE:
          transpose = constrain(transpose + direction, -MAX_TRANS, MAX_TRANS);
          break;
      }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: save Global Settings data to store modified sequences
        Pack(data, PackLocation {0, 4}, pattern_index);
        Pack(data, PackLocation {4, 4}, seqmode);
        // there are 6 bits here now that length is global.
        Pack(data, PackLocation {14, 6}, transpose + MAX_TRANS);

        Pack(data, PackLocation {20, 8}, (uint8_t)GetScale(0));
        Pack(data, PackLocation {28, 4}, (uint8_t)GetRootNote(0));

        return data;
    }

    void OnDataReceive(uint64_t data) {
      pattern_mod = pattern_index = Unpack(data, PackLocation {0, 4});
      seqmode = (AccentMode)Unpack(data, PackLocation {4, 4});
      trans_mod = transpose = Unpack(data, PackLocation {14, 6}) - MAX_TRANS;

      const uint8_t scale = Unpack(data, PackLocation {20,8});
      const uint8_t root_note = Unpack(data, PackLocation {28, 4});

      seq.SetPattern(pattern_index);
      SetScale(0, scale);
      SetRootNote(0, root_note);
      seq.Reset();
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Seq Sel";
        help[HELP_CV2]      = "Transp";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Gate";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                  "---------------------" <-- Extra text size guide
    }
      
private:
    int cursor = 0;
    int pattern_index;
    AccentMode seqmode;
    int current_note = 0;
    int transpose = 0;
    int trans_mod, pattern_mod;
    bool write_mode = 0;
    uint32_t click_tick = 0;

    int flash_ticker = 0;
    int edit_ticker = 0;

    void DrawSequenceNumber() {
        gfxPrint(0, 13, "#");
        gfxPrint(edit_ticker? pattern_index + 1 : pattern_mod + 1);
        if (edit_ticker == 0 && pattern_mod != pattern_index) gfxIcon(11, 10, CV_ICON);
    }
    void DrawPanel() {
      gfxDottedLine(0, 22, 63, 22, 3);

      switch (cursor) {
        case QUANT_SCALE:
        case QUANT_ROOT:
        case TRANSPOSE:
        {
          gfxPrint(1, 13, OC::scale_names_short[GetScale(0)]);
          gfxPrint(31, 13, OC::Strings::note_names_unpadded[GetRootNote(0)]);

          const int t = edit_ticker? transpose : trans_mod;
          gfxPrint(45, 13, t >= 0 ? "+" : "-");
          gfxPrint(abs(t));
          if (edit_ticker == 0 && trans_mod != transpose) gfxIcon(44, 8, CV_ICON);

          if (cursor == TRANSPOSE)
            gfxSpicyCursor(45, 21, 19);
          else
            gfxCursor(1 + (cursor-QUANT_SCALE)*30, 21, (1-(cursor-QUANT_SCALE))*12 + 13);
          break;
        }

        default: // cursor >= NOTES
        {
          DrawSequenceNumber();

          // XXX: probably not the most efficient approach...
          int notenum = seq.GetNote(cursor - NOTES);
          notenum = MIDIQuantizer::NoteNumber( QuantizerLookup(0, notenum + 64) );
          gfxPrint(33, 13, midi_note_numbers[notenum]);
          break;
        }

        case PATTERN:
        case LENGTH:
        case WRITE_MODE:
          DrawSequenceNumber();

          gfxIcon(24, 13, LOOP_ICON);
          gfxPrint(33, 13, seq.GetLength());
          gfxIcon(49, 13, RECORD_ICON);
          if (cursor == WRITE_MODE)
            gfxFrame(48, 12, 10, 10);
          else
            gfxSpicyCursor(6 + (cursor-PATTERN)*27, 21, 13);

          if (write_mode)
            gfxInvert(48, 12, 10, 10);
          break;
      }

      // Draw steps
      for (int s = 0; s < seq.GetLength(); s++)
      {
        const int x = 2 + (s % 8)*8;
        const int y = 26 + (s / 8)*10;
        if (!seq.muted(s))
        {
          if (seq.accent(s))
            gfxRect(x-1, y-1, 5, 5);
          else
            gfxFrame(x-1, y-1, 5, 5);
        }

        if (seq.step == s)
          gfxIcon(x-2, y-7, DOWN_BTN_ICON);
        // TODO: visualize note value

        if (cursor - NOTES == s) {
          gfxFrame(x-2, y-2, 8, 10);
          if (EditMode()) gfxInvert(x-1, y-1, 6, 8);
        }
      }

      if (flash_ticker) gfxInvert(0, 22, 63, 41);
    }

};
