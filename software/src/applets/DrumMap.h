// Copyright (c) 2021, Benjamin Rosenbach
//
// Based on Grids pattern generator, Copyright 2011 Émilie Gillet.
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

#ifdef DRUMMAP_GRIDS2
#include "grids2_resources.h"
#else
#include "grids_resources.h"
#endif

#define HEM_DRUMMAP_PULSE_ANIMATION_TICKS 1000
#define HEM_DRUMMAP_VALUE_ANIMATION_TICKS 16000
#define HEM_DRUMMAP_AUTO_RESET_TICKS 30000

class DrumMap : public HemisphereApplet {
public:

    static constexpr int MAX_VAL = 255;

    const char* applet_name() {
        return "DrumMap";
    }

    void Start() {
        step = 0;
        last_clock = OC::CORE::ticks;
    }

    void Controller() {
        _fill[0] = fill[0];
        _fill[1] = fill[1];
        _x = x;
        _y = y;
        _chaos = chaos;

        switch (cv_mode) {
        case 0:
          Modulate(_fill[0], 0, 0, MAX_VAL);
          Modulate(_fill[1], 1, 0, MAX_VAL);
          break;

        case 1:
          Modulate(_x, 0, 0, MAX_VAL);
          Modulate(_y, 1, 0, MAX_VAL);
          break;

        case 2:
          Modulate(_fill[0], 0, 0, MAX_VAL);
          Modulate(_chaos, 1, 0, MAX_VAL);
          break;
        }

        if (Clock(1)) Reset();

        if (Clock(0)) {
            // generate randomness for each drum type on first step of the pattern
            if (step == 0) {
                for (int i = 0; i < 3; i++) {
                    randomness[i] = random(0, _chaos >> 2);
                }
            }

            ForEachChannel(ch) {
                // accent on ch 1 will be for whatever part ch 0 is set to
                uint8_t part = (ch == 1 && mode[ch] == 3) ? mode[0] : mode[ch];
                int level = ReadDrumMap(step, part, _x, _y);
                level = constrain(level + randomness[part], 0, MAX_VAL);
                // use ch 0 fill if ch 1 is in accent mode
                uint8_t threshold = (ch == 1 && mode[ch] == 3) ? ~_fill[0] : ~_fill[ch];
                if (level > threshold) {
                    if (mode[ch] < 3) {
                        // normal part
                        ClockOut(ch);
                        pulse_animation[ch] = HEM_DRUMMAP_PULSE_ANIMATION_TICKS;
                    } else if (level > 192) {
                        // accent
                        ClockOut(ch);
                        pulse_animation[ch] = HEM_DRUMMAP_PULSE_ANIMATION_TICKS;
                    }
                }
            }

            // keep track of last clock for auto-reset
            last_clock = OC::CORE::ticks;
            // loop back to first step
            if (++step > 31) step = 0;
        }

        // animate pulses
        ForEachChannel(ch) {
            if (pulse_animation[ch] > 0) {
                pulse_animation[ch]--;
            }
        }

        // animate value changes
        if (value_animation > 0) {
          value_animation--;
        }

        // decrease knob acceleration
        if (knob_accel > 256) {
          knob_accel--;
        }

        // auto-reset after ~2 seconds of no clock
        if (OC::CORE::ticks - last_clock > HEM_DRUMMAP_AUTO_RESET_TICKS && step != 0) {
            Reset();
        }
        
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, 7);
        if (mode[1] > 2 && cursor == 3) ++cursor;
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            do {
                MoveCursor(cursor, direction, 7);
            } while (mode[1] > 2 && cursor == 3);

            ResetCursor();
            return;
        }

        int accel = knob_accel >> 8;
        // modes
        switch (cursor) {
        case 0:
            mode[0] += direction;
            if (mode[0] > 2) mode[0] = 0;
            if (mode[0] < 0) mode[0] = 2;
            break;
        case 1:
            mode[1] += direction;
            if (mode[1] > 3) mode[1] = 0;
            if (mode[1] < 0) mode[1] = 3;
            break;
        // fill
        case 2:
            fill[0] = constrain(fill[0] + (direction * accel), 0, MAX_VAL);
            break;
        case 3:
            fill[1] = constrain(fill[1] + (direction * accel), 0, MAX_VAL);
            break;
        // x/y
        case 4:
            x = constrain(x + (direction * accel), 0, MAX_VAL);
            break;
        case 5:
            y = constrain(y + (direction * accel), 0, MAX_VAL);
            break;
        // chaos
        case 6:
            chaos = constrain(chaos + (direction * accel), 0, MAX_VAL);
            break;
        // cv assign
        case 7:
            cv_mode += direction;
            if (cv_mode > 2) cv_mode = 0;
            if (cv_mode < 0) cv_mode = 2;
            break;
        }

        // knob acceleration and value display for slider params
        if (cursor >= 2 && cursor <= 6 && knob_accel < 2049) {
          if (knob_accel < 300) {
            knob_accel = knob_accel << 1;
          }
          knob_accel = knob_accel << 2;
          value_animation = HEM_DRUMMAP_VALUE_ANIMATION_TICKS;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, fill[0]); 
        Pack(data, PackLocation {8,8}, fill[1]); 
        Pack(data, PackLocation {16,8}, x); 
        Pack(data, PackLocation {24,8}, y); 
        Pack(data, PackLocation {32,8}, chaos);
        Pack(data, PackLocation {40,8}, mode[0]);
        Pack(data, PackLocation {48,8}, mode[1]);
        Pack(data, PackLocation {56,8}, cv_mode);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        fill[0] = Unpack(data, PackLocation {0,8});
        fill[1] = Unpack(data, PackLocation {8,8});
        x = Unpack(data, PackLocation {16,8});
        y = Unpack(data, PackLocation {24,8});
        chaos = Unpack(data, PackLocation {32,8});
        mode[0] = Unpack(data, PackLocation {40,8});
        mode[1] = Unpack(data, PackLocation {48,8});
        cv_mode = Unpack(data, PackLocation {56,8});
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock   2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "A=Part A  B=Part B";
        help[HEMISPHERE_HELP_ENCODER]  = "Params/Config";
        //                               "------------------" <-- Size Guide
    }
    
private:
    const uint8_t *MODE_ICONS[3] = {BD_ICON,SN_ICON,HH_ICON};
    const uint8_t *MODE_PULSE_ICON[3] = {BD_HIT_ICON,SN_HIT_ICON,HH_HIT_ICON};
    const char *CV_MODE_NAMES[3] = {"FILL A/B", "X/Y", "FA/CHAOS"};
    const int *VALUE_MAP[5] = {&fill[0], &fill[1], &x, &y, &chaos};
    int cursor = 0;
    uint8_t step;
    uint8_t randomness[3] = {0, 0, 0};
    int pulse_animation[2] = {0, 0};
    int value_animation = 0;
    int knob_accel = 256;
    uint32_t last_clock;
    
    // settings
    int8_t mode[2] = {0, 1};
    int fill[2] = {128, 128}; 
    int _fill[2] = {128, 128};
    int x = 0;
    int _x = 0;
    int y = 0;
    int _y = 0;
    int chaos = 0;
    int _chaos = 0;
    int8_t cv_mode = 0; // 0 = Fill A/B, 1 = X/Y, 2 = Fill A/Chaos

    uint8_t ReadDrumMap(uint8_t step, uint8_t part, uint8_t x, uint8_t y) {
      uint8_t i = x >> 6;
      uint8_t j = y >> 6;
      const uint8_t* a_map = grids::drum_map[i][j];
      const uint8_t* b_map = grids::drum_map[i + 1][j];
      const uint8_t* c_map = grids::drum_map[i][j + 1];
      const uint8_t* d_map = grids::drum_map[i + 1][j + 1];
      uint8_t offset = (part * 32) + step;
      uint8_t a = a_map[offset];
      uint8_t b = b_map[offset];
      uint8_t c = c_map[offset];
      uint8_t d = d_map[offset];
      uint8_t quad_x = x << 2;
      uint8_t quad_y = y << 2;
      // return U8Mix(U8Mix(a, b, x << 2), U8Mix(c, d, x << 2), y << 2);
      // U8Mix returns b * x + a * (255 - x) >> 8 
      uint8_t ab_fade = (b * quad_x + a * (255 - quad_x)) >> 8;
      uint8_t cd_fade = (d * quad_x + c * (255 - quad_x)) >> 8;
      return (cd_fade * quad_y + ab_fade * (255 - quad_y)) >> 8;
    }
    
    void DrawInterface() {
        // output selection
        gfxPrint(1,15,"A:");
        gfxIcon(15,15, (pulse_animation[0] > 0)? MODE_PULSE_ICON[mode[0]] : MODE_ICONS[mode[0]] );

        gfxPrint(32,15,"B:");
        if (mode[1] == 3) {
            // accent
            gfxIcon(46,15,MODE_ICONS[mode[0]]);
            gfxPrint(53,15,">");
        } else {
            // standard
            gfxIcon(46,15,(pulse_animation[1] > 0)? MODE_PULSE_ICON[mode[1]] : MODE_ICONS[mode[1]]);
        }
        /*
        // pulse animation per channel
         ForEachChannel(ch){
             if (pulse_animation[ch] > 0) {
                 gfxInvert(1+ch*32,15,8,8);
             }
         }
        */

        // fill
        gfxPrint(1,25,"F");
        DrawSlider(9,25,20,_fill[0], MAX_VAL, cursor == 2);
        // don't show fill for channel b if it is an accent mode
        if (mode[1] < 3) {
            gfxPrint(32,25,"F");
            DrawSlider(40,25,20,_fill[1], MAX_VAL, cursor == 3);
        }
        
        // x & y
        gfxPrint(1,35,"X");
        DrawSlider(9,35,20,_x, MAX_VAL, cursor == 4);
        gfxPrint(32,35,"Y");
        DrawSlider(40,35,20,_y, MAX_VAL, cursor == 5);
        
        // chaos
        gfxPrint(1,45,"CHAOS");
        DrawSlider(32,45,28,_chaos, MAX_VAL, cursor == 6);
        
        // step count in header
        gfxPrint((step < 9 ? 49 : 43),2,step+1);

        // cursor for non-knobs
        if (cursor <= 1)
            gfxCursor(14+cursor*31,23,16); // Part A / B
        
        // display value for knobs
        if (value_animation > 0 && cursor >= 2 && cursor <= 6) {
          int val = *VALUE_MAP[cursor-2];
          int xPos = 27;
          if (val > 99) {
            xPos = 21;
          } else if (val > 9) {
            xPos = 24;
          }
          gfxPrint(xPos, 55, val);
          gfxInvert(1, 54, 63, 10);
        } else {
          // cv input assignment
          gfxIcon(1,57,CV_ICON);
          gfxPrint(10,55,CV_MODE_NAMES[cv_mode]);
          if (cursor == 7) gfxCursor(10,63,50); // CV Assign
        }

    }

    void Reset() {
        step = 0;
    }
};
