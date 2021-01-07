// Copyright (c) 2018, Jason Justian
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

#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"

class ClockDivider : public HemisphereApplet {
public:

  enum ClockDivCursor {
    CHAN_A1, CHAN_A2,
    CHAN_B1, CHAN_B2,

    LAST_SETTING = CHAN_B2
  };

    static constexpr int CLOCKDIV_MAX = 32;

    struct ClkDivMult {
      int8_t steps = 1; // positive for division, negative for multiplication
      uint8_t clock_count = 0; // Number of clocks since last output (for clock divide)
      uint32_t next_clock = 0; // Tick number for the next output (for clock multiply)
      uint32_t last_clock = 0;
      int cycle_time = 0; // Cycle time between the last two clock inputs

      void Set(int s) {
        steps = constrain(s, -CLOCKDIV_MAX, CLOCKDIV_MAX);
      }
      bool Tick(bool clocked = 0) {
        if (steps == 0) return false;
        bool trigout = 0;
        const uint32_t this_tick = OC::CORE::ticks;

        if (clocked) {
          cycle_time = this_tick - last_clock;
          last_clock = this_tick;

          if (steps > 0) { // Positive value indicates clock division
              clock_count++;
              if (clock_count == 1) trigout = 1; // fire on first step
              if (clock_count >= steps) clock_count = 0; // Reset on last step
          }
          if (steps < 0) {
              // Calculate next clock for multiplication on each clock
              int tick_interval = (cycle_time / -steps);
              next_clock = this_tick + tick_interval;
              clock_count = 0;
              trigout = 1;
          }
        }

        // Handle clock multiplication
        if (steps < 0) {
            if ( this_tick >= next_clock && clock_count+1 < -steps) {
                int tick_interval = (cycle_time / -steps);
                next_clock += tick_interval;
                ++clock_count;
                trigout = 1;
            }
        }
        return trigout;
      }
      void Reset() {
        clock_count = 0;
        next_clock = 0;
      }
    } divmult[4];

    const char* applet_name() {
        return "Clock Div";
    }

    void Start() {
      divmult[0].steps = 2;
      divmult[2].steps = 4;
    }

    void Controller() {
        // Modulate setting via CV
        ForEachChannel(ch)
        {
          int div_m = div[ch];
          Modulate(div_m, ch, -CLOCKDIV_MAX, CLOCKDIV_MAX);
          divmult[ch*2].steps = div_m;
        }

        if (Clock(1)) { // Reset
          ForEachChannel(ch) {
            ForEachChannel(d) divmult[ch*2 + d].Reset();
          }
        }

        ForEachChannel(ch)
        {
            bool trig = divmult[ch*2 + 0].Tick( Clock(0) );
            trig = divmult[ch*2 + 1].Tick( trig );
            if (trig) ClockOut(ch);
        }
    }

    void View() {
        DrawSelector();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        if (cursor & 0x1)
          divmult[cursor].Set( divmult[cursor].steps + direction );
        else
          div[cursor / 2] = constrain( div[cursor / 2] + direction, -CLOCKDIV_MAX, CLOCKDIV_MAX);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < 2; ++i) {
          Pack(data, PackLocation {0 + i*8,8}, div[i] + 32);
          Pack(data, PackLocation {16 + i*8,8}, divmult[1+i*2].steps + 32);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < 2; ++i) {
          div[i] = Unpack(data, PackLocation {0 + i*8,8}) - 32;
          divmult[1+i*2].Set( Unpack(data, PackLocation {16 + i*8,8}) - 32 );
        }
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS] = "Div/Mult Ch1,Ch2";
        help[HEMISPHERE_HELP_OUTS] = "Clk A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER] = "Div,Mult";
    }

private:
    int div[2] = {1, 2}; // Division setting before modulation.
                         // Positive for divisions, negative for multipliers.
                         // Zero is mute.
    int cursor = 0; // Which output is currently being edited

    void DrawSelector() {
        static const char * chan_name[] = { "A", "B", "C", "D" };
        ForEachChannel(ch)
        {
            const int y = 15 + (ch * 24);

            gfxPrint(1, y, chan_name[ch+hemisphere*2]);
            ForEachChannel(d) {
              const int s = divmult[ch*2 + d].steps;

              gfxPos(13, y + d*12);
              if (s > 0) {
                  gfxPrint("/");
                  gfxPrint(s);
                  gfxPrint(" Div");
              }
              if (s < 0) {
                  gfxPrint("x");
                  gfxPrint(-s);
                  gfxPrint(" Mult");
              }
              if (s == 0) {
                  gfxPrint("(off)");
              }
            }

            if (divmult[ch*2].steps != div[ch]) gfxIcon(18, y+8, CV_ICON);
        }
        gfxCursor(12, 23 + (cursor * 12), 50);
        gfxDottedLine(1, 36, 63, 36); // to separate the two channels
    }
};
