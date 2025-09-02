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

class SeqPlay7 : public HemisphereApplet {
public:

    static constexpr int GLIDE_FACTOR = 12;

    enum SeqPlay7Cursor {
      PATTERN1, PATTERN2, PATTERN3, PATTERN4, PATTERN5, PATTERN6, PATTERN7,
      PLAYER_COUNT,
      MAX_CURSOR = PATTERN7
    };
    enum SeqPlay7AuxCursor {
      P_SELECT, Q_SELECT, REPEATS,
      AUX_CURSOR_MAX
    };

    struct MiniSeqPlayer {
      MiniSeq seq;

      int pattern_number = 0; // 0 to 7 -- 3 bits
      int qselect = 0; // 0 to 7 -- 3 bits
      int repeats = 0; // 0 to 7 (0 is disabled) -- 3 bits
      // TODO: MUTE player slot?

      int repeat_count = -1; // internal counter

      void SetPattern(int p_) {
        pattern_number = p_;
        seq.SetPattern(pattern_number);
      }
      void NudgePattern(int dir) {
        pattern_number += dir;
        seq.SetPattern(pattern_number);
      }
      void Reset() {
        seq.Reset();
        repeat_count = -1;
      }
      // advance sequencer; return false at the end of all repeats
      bool Poke() {
        seq.Advance();
        if (seq.step == 0) ++repeat_count;
        return repeat_count < repeats;
      }
      int GetNote() {
        return seq.GetNote();
      }
      bool Muted() {
        return seq.muted(seq.step);
      }
      bool Accent() {
        return seq.accent(seq.step);
      }
    } seq_player[PLAYER_COUNT];

    const char* applet_name() { // Maximum 10 characters
        return "SeqPlay7";
    }
    const uint8_t* applet_icon() { return PhzIcons::seqPlay7; }

    void Start() {
      seq_player[0].repeats = 1;
    }

    void Reset() {
      for (int i = 0; i < PLAYER_COUNT; ++i) {
        seq_player[i].Reset();
      }
      player_index = 0;
    }

    void Controller()
    {
      if (Clock(1)) { // reset
        Reset();
      }

      if (Clock(0)) // clock
      {
        // Advance pattern & step as needed
        while (!seq_player[player_index].Poke()) {
          ++player_index %= PLAYER_COUNT;
          seq_player[player_index].Reset();
          // TODO: watch out for the infinite loop!
        }

        if (seq_player[player_index].Muted()) {
          GateOut(1, false);
        } else {
          play_accent_ = seq_player[player_index].Accent();
          if (play_accent_) {
            // - tied note
            GateOut(1, true);
          } else {
            // regular trigger
            ClockOut(1);
          }

          int play_note = seq_player[player_index].GetNote() + 64;
          CONSTRAIN(play_note, 0, 127);

          // set CV output
          play_cv_ = HS::QuantizerLookup(seq_player[player_index].qselect, play_note);
        }
      }

      int transpose_cv = DetentedIn(0);
      int play_cv = HS::Quantize(seq_player[player_index].qselect, play_cv_ + transpose_cv);

      // continuously output pitch for glides and transpose (aka input quantizer with sequenced shifting and scale selection, etc.)
      if (play_accent_) {
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
      CursorToggle();

      if ( OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME ) {
        // double-click toggles something?
      } else {
        click_tick = OC::CORE::ticks;
      }
    }
    void AuxButton() {
      ++aux_cursor %= AUX_CURSOR_MAX;
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR);
        return;
      }

      switch (aux_cursor) {
        case P_SELECT:
          seq_player[cursor].NudgePattern(direction);
          break;
        case Q_SELECT:
          seq_player[cursor].qselect = 
            constrain(seq_player[cursor].qselect + direction, 0, QUANT_CHANNEL_COUNT - 1);
          break;
        case REPEATS:
          // first player cannot be 0
          seq_player[cursor].repeats =
            constrain(seq_player[cursor].repeats + direction, cursor ? 0 : 1, 7);
          break;
      }
      edit_ticker = 5000;
    }

    uint64_t OnDataRequest() {
      uint64_t data = 0;
      for (size_t i = 0; i < PLAYER_COUNT; ++i) {
        // 7 slots, 9 bits each == 63 bits
        Pack(data, PackLocation {0 + i*9, 3}, seq_player[i].pattern_number);
        Pack(data, PackLocation {3 + i*9, 3}, seq_player[i].qselect);
        Pack(data, PackLocation {6 + i*9, 3}, seq_player[i].repeats);
      }
      return data;
    }

    void OnDataReceive(uint64_t data) {
      for (size_t i = 0; i < PLAYER_COUNT; ++i) {
        seq_player[i].SetPattern( Unpack(data, PackLocation {0 + i*9, 3}) );
        seq_player[i].qselect =   Unpack(data, PackLocation {3 + i*9, 3});
        seq_player[i].repeats =   Unpack(data, PackLocation {6 + i*9, 3});
        seq_player[i].Reset();
      }
      player_index = 0;
      // first slot cannot be muted
      if (!seq_player[0].repeats) ++seq_player[0].repeats;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Transp";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Gate";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "AuxButton: Edit Step";
       //                   "---------------------" <-- Extra text size guide
    }
    
private:
    int cursor = PATTERN1; // which slot
    int aux_cursor = P_SELECT; // which setting per slot
    int player_index = 0;
    int play_cv_ = 0; // cached CV, only updated for unmuted steps
    bool play_accent_ = 0;

    uint32_t click_tick = 0;

    int flash_ticker = 0;
    int edit_ticker = 0;

    void DrawPanel() {

      // param editing on top
      if (EditMode()) {
        MiniSeqPlayer &pedit = seq_player[cursor];
        gfxPrint(1, 12, "#");
        gfxPrint(pedit.pattern_number + 1);
        gfxPrint(" Q");
        gfxPrint(pedit.qselect + 1);

        gfxIcon(35, 12, LOOP_ICON);
        gfxPrint(43, 12, pedit.repeats);

        gfxDottedLine(0, 21, 63, 21, 3);

        gfxIcon( 8 + aux_cursor * 16, 22, UP_BTN_ICON);
      }


      // a row of squares in the middle
      for (size_t i = 0; i < PLAYER_COUNT; ++i) {
        gfxIcon(2 + i*8, 30, seq_player[i].repeats > 0 ? STOP_ICON : PULSES_ICON);
      }

      // cursor is a down arrow above the row
      gfxIcon(2 + cursor*8, 22, EditMode() ? DOWN_BTN_ICON : DOWN_ICON);

      // play cursor under the row
      gfxIcon(2 + player_index*8, 38, UP_BTN_ICON);

      MiniSeqPlayer &player = seq_player[player_index];
      // current slot status at the bottom
      gfxPrint(1, 44, "#");
      gfxPrint(player.pattern_number + 1);
      gfxPrint(" Q");
      gfxPrint(player.qselect + 1);
      gfxPrint(" x");
      gfxPrint(player.repeat_count + 1);
      gfxPrint("/");
      gfxPrint(player.repeats);

      gfxPrint(1, 54, player.seq.step + 1);
      gfxPrint("/");
      gfxPrint(player.seq.GetLength());
    }

};
