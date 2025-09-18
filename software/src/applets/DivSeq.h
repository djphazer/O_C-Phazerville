// Copyright (c) 2024, Nicholas Michalek
// Copyright (c) 2022, Benjamin Rosenbach
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

/* Used ProbDiv applet from benirose as a template */

#include "../util/clkdivmult.h"

class DivSeq : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_DIV = 63;
    static constexpr uint8_t NUM_STEPS = 5;

    enum DivSeqCursor {
        STEP1A, STEP2A, STEP3A, STEP4A, STEP5A,
        STEP1B, STEP2B, STEP3B, STEP4B, STEP5B,
        MUTE1A, MUTE2A, MUTE3A, MUTE4A, MUTE5A,
        MUTE1B, MUTE2B, MUTE3B, MUTE4B, MUTE5B,
        RE_ZAP,
        LAST_SETTING = RE_ZAP
    };

    DivSequence<NUM_STEPS> div_seq[2];

    const char* applet_name() {
        return "DivSeq";
    }
    const uint8_t* applet_icon() { return PhzIcons::divSeq; }

    void Start() {
      ForEachChannel(ch) {
        int total = 16 + ch*16;
        for (int i = 0; i < NUM_STEPS - 1; ++i) {
          int val = random(total);
          if (1 == val)
            div_seq[ch].Set(i, -random(7)-1);
          else
            div_seq[ch].Set(i, val);
          total -= val;
        }
        div_seq[ch].Set(NUM_STEPS - 1, total);
      }
      Reset();
    }

    void Reset() {
      ForEachChannel(ch) {
        div_seq[ch].Reset();
      }
    }
    void TrigOut(int ch) {
        ClockOut(ch);
        pulse_animation[ch] = HEMISPHERE_PULSE_ANIMATION_TIME;
    }

    void Controller() {
        // reset
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) {
          // sequence advance, get trigger bits
          bool trig_q[2] = { div_seq[0].Poke(true), div_seq[1].Poke(true) };

          ForEachChannel(ch) {
            // XOR with positive CV gate
            bool trig = (trig_q[ch] != (In(ch) > 6*128));

            // negative CV gate enables XOR with other channel
            if (In(ch) < -6*128)
                trig = (trig != trig_q[1-ch]);

            if (trig) TrigOut(ch);
          }
        } else {
          ForEachChannel(ch) {
            // gotta keep it ticking for the multipliers
            if (div_seq[ch].Poke(false)) TrigOut(ch);
          }
        }

        ForEachChannel(ch) {
          if (pulse_animation[ch] > 0) {
              pulse_animation[ch]--;
          }
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (RE_ZAP == cursor) {
          Start();
          cursor = 0;
          return;
        }

        if (cursor >= MUTE1A && !EditMode()) {
            const int ch = (cursor - MUTE1A) / NUM_STEPS;
            const int s = (cursor - MUTE1A) % NUM_STEPS;
            div_seq[ch].ToggleStep(s);
        } else
            CursorToggle();
    }
    void AuxButton() {
      const int ch = (cursor) / NUM_STEPS;
      const int s = (cursor) % NUM_STEPS;
      div_seq[ch].ToggleStep(s);
      CancelEdit();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        const int ch = cursor / NUM_STEPS;
        const int s = cursor % NUM_STEPS;
        if (ch > 1) // mutes
            div_seq[ch-2].ToggleStep(s);
        else {
            const int div = div_seq[ch].Get(s) + direction;
            div_seq[ch].Set(s, div);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        const size_t b = 6; // bitsize
        ForEachChannel(ch) {
          int offset = 0;
          for (size_t i = 0; i < NUM_STEPS; i++) {
            if (div_seq[ch].divmult[i].steps < 0) offset = 16;
          }
          for (size_t i = 0; i < NUM_STEPS; i++) {
            const uint8_t val = constrain(div_seq[ch].divmult[i].steps + offset, 0, 63);
            Pack(data, PackLocation {ch*NUM_STEPS*b + i*b, b}, val);
          }
          if (offset)
            Pack(data, PackLocation { size_t(60 + ch*2), 1 }, 1);

          if (!div_seq[ch].StepActive(0))
            Pack(data, PackLocation { size_t(61 + ch*2), 1 }, 1);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        const size_t b = 6; // bitsize
        ForEachChannel(ch) {
            int offset = Unpack(data, PackLocation { size_t(60 + ch*2), 1 }) ? 16 : 0;
            for (size_t i = 0; i < NUM_STEPS; i++) {
                div_seq[ch].divmult[i].steps = Unpack(data, PackLocation {ch*NUM_STEPS*b + i*b, b}) - offset;
                CONSTRAIN(div_seq[ch].divmult[i].steps, -MAX_DIV, MAX_DIV);
            }
            // step 1 cannot be zero
            if (div_seq[ch].divmult[0].steps == 0) ++div_seq[ch].divmult[0].steps;

            bool first_step_mute = Unpack(data, PackLocation{ size_t(61 + ch*2), 1});
            div_seq[ch].SetMute(0, first_step_mute);
        }
        Reset();
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Reset";
    help[HELP_CV1]      = "XOR 1";
    help[HELP_CV2]      = "XOR 2";
    help[HELP_OUT1]     = "Trig 1";
    help[HELP_OUT2]     = "Trig 2";
    help[HELP_EXTRA1] = "Negative CV engages";
    help[HELP_EXTRA2] = "cross-channel XOR";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor; // DivSeqCursor

    int pulse_animation[2] = {0,0};

    void DrawInterface() {
      if (RE_ZAP == cursor) {
        gfxIcon(28, 22, DOWN_ICON);
        gfxIcon(18, 32, RIGHT_ICON);
        gfxIcon(28, 32, ZAP_ICON);
        gfxIcon(38, 32, LEFT_ICON);
        gfxIcon(28, 42, UP_ICON);
        return;
      }

      // divisions
      ForEachChannel(ch) {
        const size_t x = 31*ch;

        for(int i = 0; i < NUM_STEPS; i++) {
          const size_t y = 15 + i*10;

          if (!div_seq[ch].StepActive(i) && cursor % MUTE1A != i + ch*NUM_STEPS) {
            gfxIcon(x, y, X_NOTE_ICON);
              continue;
          }

          const auto steps = div_seq[ch].divmult[i].steps;
          if (steps >= 0) {
            gfxPrint(1 + x, y, steps);
          } else {
            gfxPrint(1 + x, y, "x");
            gfxPrint( -steps );
          }

          if (cursor >= MUTE1A) {
            gfxIcon(16 + x, y, div_seq[ch].Muted(i) ? CHECK_OFF_ICON : CHECK_ON_ICON);
            if (cursor - MUTE1A == i+ch*NUM_STEPS) gfxFrame(15+x, y-1, 10, 10);
          } else if (steps >= 0) {
            DrawSlider(14 + x, y, 14, steps, MAX_DIV, cursor == i+ch*NUM_STEPS);
          } else {
            gfxIcon(22 + x, y, PULSES_ICON);
            if (cursor == i+ch*NUM_STEPS) gfxSpicyCursor(20 + x, y + 7, 10, 7);
          }

          if (div_seq[ch].step_index == i)
            gfxIcon(28 + x, y, LEFT_BTN_ICON);
        }
        // flash division when triggered
        if (pulse_animation[ch] > 0 && div_seq[ch].step_index >= 0) {
          gfxInvert(1 + x, 15 + (div_seq[ch].step_index*10), 12, 8);
        }
      }
    }

};
