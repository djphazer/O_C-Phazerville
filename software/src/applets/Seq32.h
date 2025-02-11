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
      GATE_25,
      GATE_50,
      GATE_75,
      GATE_100,

      ACCENT_MODE_LAST = GATE_100
    };

    enum Seq32Cursor {
      // ---
      PATTERN,
      LENGTH,
      TRANSPOSE,
      // ---
      QUANT_SCALE,
      ACCENT_MODE,
      // ---
      WRITE_MODE,
      NOTES,

      MAX_CURSOR = WRITE_MODE + MiniSeq::MAX_STEPS
    };

   MiniSeq seq;

    const char* applet_name() { // Maximum 10 characters
        return "Seq32";
    }
    const uint8_t* applet_icon() { return PhzIcons::seq32; }

    void Start() {
    }

    void Reset() {
      seq.Reset();
    }

    void Controller()
    {
      if (Clock(1)) {
        if (write_mode) {
          // insert rest
          seq.Mute(seq.step);
          StartADCLag();
        } else
          Reset();
      }
      if (Clock(0)) { // clock

        if (!write_mode) {
          // CV modulation of pattern and transposition
          pattern_mod = pattern_index;
          Modulate(pattern_mod, 1, 0, MiniSeq::SEQUENCE_COUNT - 1);
          seq.SetPattern(pattern_mod);

          trans_mod = transpose;
          Modulate(trans_mod, 0, -MAX_TRANS, MAX_TRANS);

          seq.Advance();
        } else {
          seq.Unmute(seq.step);
          StartADCLag();
        }

        if (seq.muted(seq.step)) {
          GateOut(1, false);
        } else {
          current_note = seq.GetNote();

          if (seq.accent(seq.step)) {
            switch (seqmode) {
              case GATE_25:
              case GATE_50:
              case GATE_75:
                // - variable length gate
                ClockOut(1, ClockCycleTicks(0) * (1 + seqmode - GATE_25)/4);
                break;

              case GATE_100:
              default:
                // - tied note
                GateOut(1, true);
                break;
            }

          } else {
            // regular trigger
            ClockOut(1);
          }
        }
      }

      if (EndOfADCLag() && write_mode) {
        // sample and record note number from CV1, accent from CV2, and then advance
        Quantize(0, In(0));
        current_note = GetLatestNoteNumber(0) - 64;
        seq.SetNote(current_note, seq.step);
        seq.SetAccent(seq.step, In(1) > (24 << 7)); // cv2 > 2V qualifies as accent
        seq.Advance();
      }

      // continuously compute CV with transpose
      int play_note = current_note + 64 + trans_mod;
      CONSTRAIN(play_note, 0, 127);
      // set CV output
      int play_cv = QuantizerLookup(0, play_note);
      if (seq.accent(seq.step) && glide_on) {
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
      if (cursor == QUANT_SCALE)
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
      if (cursor == WRITE_MODE) {
        seq.ToggleMute(seq.step);
        return;
      }
      if (cursor >= NOTES) {
        seq.ToggleMute(cursor - NOTES);
        return;
      }
      if (cursor == ACCENT_MODE) {
        glide_on = !glide_on;
        return;
      }

      if (cursor == LENGTH)
        seq.Randomize(true);
      else if (cursor == TRANSPOSE)
        seq.SowPitches(abs(transpose));
      else if (cursor == PATTERN)
        seq.Clear();

      flash_ticker = 1000;
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

        case TRANSPOSE:
          transpose = constrain(transpose + direction, -MAX_TRANS, MAX_TRANS);
          break;

        case ACCENT_MODE:
          seqmode = (AccentMode)constrain(seqmode + direction, 0, ACCENT_MODE_LAST);
          break;

        case WRITE_MODE:
          seq.Advance(direction < 0);
          if (!seq.muted())
            current_note = seq.GetNote();
          break;
      }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: save Global Settings data to store modified sequences
        Pack(data, PackLocation {0, 4}, pattern_index);
        Pack(data, PackLocation {4, 4}, seqmode);
        Pack(data, PackLocation {8, 1}, glide_on);
        // 5 empty bits here
        Pack(data, PackLocation {14, 6}, transpose + MAX_TRANS);

        //Pack(data, PackLocation {20, 8}, (uint8_t)GetScale(0));
        //Pack(data, PackLocation {28, 4}, (uint8_t)GetRootNote(0));

        return data;
    }

    void OnDataReceive(uint64_t data) {
      pattern_mod = pattern_index = Unpack(data, PackLocation {0, 4});
      seqmode = (AccentMode)Unpack(data, PackLocation {4, 4});
      glide_on = Unpack(data, PackLocation {8, 1});
      trans_mod = transpose = Unpack(data, PackLocation {14, 6}) - MAX_TRANS;

      /* let's leave global quantizer settings as it were
      const uint8_t scale = Unpack(data, PackLocation {20,8});
      const uint8_t root_note = Unpack(data, PackLocation {28, 4});
      SetScale(0, scale);
      SetRootNote(0, root_note);
      */

      seq.SetPattern(pattern_index);
      seq.Reset();
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = write_mode? "Record":"Clock";
        help[HELP_DIGITAL2] = write_mode? "Rest":"Reset";
        help[HELP_CV1]      = write_mode? "Pitch" :"Transp";
        help[HELP_CV2]      = write_mode? "Accent":"Seq Sel";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Gate";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                  "---------------------" <-- Extra text size guide
    }

private:
    int cursor = 0;
    int pattern_index;
    AccentMode seqmode = GATE_100;
    int current_note = 0;
    int transpose = 0;
    int trans_mod, pattern_mod;
    bool write_mode = 0;
    bool glide_on = true;
    uint32_t click_tick = 0;

    int flash_ticker = 0;
    int edit_ticker = 0;

    void DrawSequenceNumber() {
        gfxPrint(0, 13, "#");
        gfxPrint(edit_ticker? pattern_index + 1 : pattern_mod + 1);
        if (edit_ticker == 0 && pattern_mod != pattern_index) gfxIcon(11, 10, CV_ICON);
    }
    void DrawPanel() {
      gfxDottedLine(0, 27, 63, 27, 3);

      switch (cursor) {
        case QUANT_SCALE:
        case ACCENT_MODE:
        {
          gfxPrint(1, 13, OC::scale_names_short[GetScale(0)]);
          gfxPrint(31, 13, OC::Strings::note_names_unpadded[GetRootNote(0)]);

          int gatelen = (1 + seqmode - GATE_25)*4;
          gfxIcon(47, 14, glide_on ? SLEW_ICON : GATE_ICON);
          gfxIcon(55, 14, glide_on ? SLEW_ICON : GATE_ICON);
          gfxFrame(47, 12, gatelen, 4);

          if (QUANT_SCALE == cursor)
            gfxCursor(1, 21, 25);
          else
            gfxSpicyCursor(46, 22, 18, 11);

          break;
        }

        case WRITE_MODE:
          gfxFrame(49, 12, 10, 10);
        default: // cursor >= NOTES
        {
          DrawSequenceNumber();

          gfxIcon(50, 13, RECORD_ICON);
          if (write_mode)
            gfxInvert(49, 12, 10, 10);

          int notenum = seq.GetNote((WRITE_MODE==cursor)? seq.step : cursor - NOTES);
          notenum = MIDIQuantizer::NoteNumber( QuantizerLookup(0, notenum + 64) );
          gfxPrint(29, 13, midi_note_numbers[notenum]);
          break;
        }

        case PATTERN:
        case LENGTH:
        case TRANSPOSE:
        {
          DrawSequenceNumber();

          const int t = edit_ticker? transpose : trans_mod;
          gfxPrint(45, 13, t >= 0 ? "+" : "-");
          gfxPrint(abs(t));
          if (edit_ticker == 0 && trans_mod != transpose) gfxIcon(44, 8, CV_ICON);

          gfxIcon(22, 13, LOOP_ICON);
          gfxPrint(31, 13, seq.GetLength());

          if (cursor == TRANSPOSE)
            gfxSpicyCursor(45, 21, 19);
          else
            gfxSpicyCursor(6 + (cursor-PATTERN)*25, 21, 13);

          break;
        }
      }

      // Draw steps
      for (int s = 0; s < seq.GetLength(); s++)
      {
        const int x = 2 + (s % 8)*8;
        const int y = 34 + (s / 8)*8;
        if (!seq.muted(s))
        {
          if (seq.accent(s))
            gfxRect(x-1, y-1, 5, 4);
          else
            gfxFrame(x-1, y-1, 5, 4);
        }

        if (seq.step == s)
          gfxIcon(x-2, y-8, DOWN_BTN_ICON);
        // TODO: visualize note value?

        if (cursor - NOTES == s) {
          gfxFrame(x-2, y-2, 8, 7);
          if (EditMode()) gfxInvert(x-1, y-1, 6, 5);
        }
      }

      if (flash_ticker) gfxInvert(0, 22, 63, 41);
    }

};
