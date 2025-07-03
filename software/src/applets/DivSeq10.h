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

class DivSeq10 : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_DIV = 63;
    static constexpr uint8_t NUM_STEPS = 10;

    enum DivSeqCursor {
        STEP1A, STEP2A, STEP3A, STEP4A, STEP5A,
        STEP1B, STEP2B, STEP3B, STEP4B, STEP5B,
        MUTE1A, MUTE2A, MUTE3A, MUTE4A, MUTE5A,
        MUTE1B, MUTE2B, MUTE3B, MUTE4B, MUTE5B,
        RE_ZAP,
        LAST_SETTING = RE_ZAP
    };

    struct DivSequence {
      int step_index = -1;
      int clock_count = 0;
      ClkDivMult divmult[NUM_STEPS]; // separate DividerMultiplier for each step
      uint16_t muted = 0x0; // bitmask
      uint32_t last_clock = 0;

      int Get(int s) {
        return divmult[s].steps;
      }
      void Set(int s, int div) {
        divmult[s].Set(div);
      }
      void ToggleStep(int idx) {
        muted ^= (0x01 << idx);
      }
      void SetMute(int idx, bool set = true) {
        muted = (muted & ~(0x01 << idx)) | ((set & 0x01) << idx);
      }
      bool Muted(int idx) {
        return (muted >> idx) & 0x01;
      }
      bool StepActive(int idx) {
        return divmult[idx].steps != 0 && !Muted(idx);
      }
      bool Poke(bool clocked = 0) {
        if (step_index < 0) {
          // reset case
          if (clocked) {
              step_index = 0;
              divmult[step_index].last_clock = last_clock;
              last_clock = OC::CORE::ticks;
              return divmult[step_index].Tick(true) && StepActive(step_index);
          }
          return false; // reset and not ready
        }

        bool trigout = divmult[step_index].Tick(clocked);

        if (clocked)
        {
          if (divmult[step_index].steps < 0 || ++clock_count >= divmult[step_index].steps)
          {
            // special case to proceed to next step
            clock_count = 0;

            int i = 0;
            do {
                ++step_index %= NUM_STEPS;
                ++i;
            } while (!StepActive(step_index) && i < NUM_STEPS);

            if (StepActive(step_index)) {
              divmult[step_index].Reset();
              divmult[step_index].last_clock = last_clock;
              trigout = divmult[step_index].Tick(true);
            }
          }

          last_clock = OC::CORE::ticks;
        }


        return trigout;
      }
      void Reset() {
        step_index = -1;
        clock_count = 0;
        for (auto &d : divmult) d.Reset();
      }

    } div_seq;

    const char* applet_name() {
        return "DivSeq10";
    }
    const uint8_t* applet_icon() { return PhzIcons::divSeq; }

    void Start() {
      int total = 64;

      for (int i = 0; i < NUM_STEPS - 1; ++i) {
        int val = random(total/2+1);
        if (1 == val)
          div_seq.Set(i, -random(7)-1);
        else
          div_seq.Set(i, val);
        total -= val;
      }
      div_seq.Set(NUM_STEPS - 1, total);
      Reset();
    }

    void Reset() {
      div_seq.Reset();
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
          bool trig_q = div_seq.Poke(true);

          // XOR with positive gate at CV1
          bool trig = (trig_q != (In(0) > 6*128));

          // TODO:
          // - negative CV gate?
          // - CV2 does things?
          // - assignable to steps?

          if (trig) TrigOut(0);
          else TrigOut(1);

        } else {
          // gotta keep it ticking for the multipliers
          // a gate at CV2 sends ratchets to 2nd output
          if (div_seq.Poke(false)) TrigOut(In(1) > 6*128);
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
            const int s = (cursor - MUTE1A) % NUM_STEPS;
            div_seq.ToggleStep(s);
        } else
            CursorToggle();
    }
    void AuxButton() {
      const int s = (cursor) % NUM_STEPS;
      div_seq.ToggleStep(s);
      CancelEdit();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        const int s = cursor % NUM_STEPS;
        if (cursor >= MUTE1A) // mutes
            div_seq.ToggleStep(s);
        else {
            const int div = div_seq.Get(s) + direction;
            div_seq.Set(s, div);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        const size_t b = 6; // bitsize
        int offset = 0;
        for (size_t i = 0; i < NUM_STEPS; i++) {
          if (div_seq.divmult[i].steps < 0) offset = 16;
        }
        for (size_t i = 0; i < NUM_STEPS; i++) {
          const uint8_t val = constrain(div_seq.divmult[i].steps + offset, 0, 63);
          Pack(data, PackLocation {i*b, b}, val);
        }
        if (offset)
          Pack(data, PackLocation { size_t(60), 1 }, 1);

        if (!div_seq.StepActive(0))
          Pack(data, PackLocation { size_t(61), 1 }, 1);

        return data;
    }

    void OnDataReceive(uint64_t data) {
        const size_t b = 6; // bitsize
        int offset = Unpack(data, PackLocation { size_t(60), 1 }) ? 16 : 0;
        for (size_t i = 0; i < NUM_STEPS; i++) {
            div_seq.divmult[i].steps = Unpack(data, PackLocation {i*b, b}) - offset;
            CONSTRAIN(div_seq.divmult[i].steps, -MAX_DIV, MAX_DIV);
        }
        // step 1 cannot be zero
        if (div_seq.divmult[0].steps == 0) ++div_seq.divmult[0].steps;

        bool first_step_mute = Unpack(data, PackLocation{ size_t(61), 1});
        div_seq.SetMute(0, first_step_mute);
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
        int x_ = random(5)-2;
        int y_ = random(5)-2;
        gfxIcon(28+x_, 22+y_, DOWN_ICON);
        gfxIcon(18+x_, 32+y_, RIGHT_ICON);
        gfxIcon(28+x_, 32+y_, ZAP_ICON);
        gfxIcon(38+x_, 32+y_, LEFT_ICON);
        gfxIcon(28+x_, 42+y_, UP_ICON);
        return;
      }

      // divisions
      for(int i = 0; i < NUM_STEPS; i++) {
        const size_t x = 31*(i>4);
        const size_t y = 15 + i%5*10;

        if (!div_seq.StepActive(i) && cursor % MUTE1A != i) {
          gfxIcon(x, y, X_NOTE_ICON);
          continue;
        }

        const int steps = div_seq.divmult[i].steps;
        if (steps >= 0) {
          gfxPrint(1 + x, y, steps);
        } else {
          gfxPrint(1 + x, y, "x");
          gfxPrint( -steps );
        }

        if (cursor >= MUTE1A) {
          gfxIcon(16 + x, y, div_seq.Muted(i) ? CHECK_OFF_ICON : CHECK_ON_ICON);
          if (cursor - MUTE1A == i) gfxFrame(15+x, y-1, 10, 10);
        } else if (steps >= 0) {
          DrawSlider(14 + x, y, 14, steps, MAX_DIV, cursor == i);
        } else {
          gfxIcon(22 + x, y, PULSES_ICON);
          if (cursor == i) gfxSpicyCursor(20 + x, y + 7, 10, 7);
        }

        if (div_seq.step_index == i)
          gfxIcon(28 + x, y, LEFT_BTN_ICON);
      }
      // flash division when triggered
      if (pulse_animation[0] > 0 && div_seq.step_index >= 0) {
        gfxInvert(1 + 31*(div_seq.step_index>4), 15 + (div_seq.step_index%5*10), 12, 8);
      }
    }

};
