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

#include "HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo

class ProbabilityDivider : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_WEIGHT = 15;
    static constexpr uint8_t MAX_LOOP_LENGTH = 32;

    enum ProbDivCursor {
        WEIGHT1, WEIGHT2, WEIGHT4, WEIGHT8,
        LOOP_LENGTH,
        LAST_SETTING = LOOP_LENGTH
    };

    const char* applet_name() {
        return "ProbDiv";
    }

    void Start() {
        weight_1 = 0;
        weight_2 = 0;
        weight_4 = 0;
        weight_8 = 0;
        loop_length = 0;
        loop_index = 0;
        loop_step = 0;
        skip_steps = 0;
        ForEachChannel(ch) {
            GateOut(ch, false);
        }
    }

    void Controller() {
        loop_linker->RegisterDiv(hemisphere);

        // CV 1 control over loop length
        loop_length_mod = loop_length;
        if (DetentedIn(0)) {
            Modulate(loop_length_mod, 0, 0, MAX_LOOP_LENGTH);
            // TODO: regenerate if changing from 0?
        }

        loop_linker->SetLooping(loop_length_mod > 0);

        // reset
        if (Clock(1)) {
            loop_step = 0;
            loop_index = 0;
            skip_steps = 0;
            reset_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
        }

        if (Clock(0)) {
            int reseed = DetentedIn(1);
            // trigger reseed if CV2 is > 2.5v
            if (reseed > (HEMISPHERE_MAX_CV >> 1) && !reseed_high) {
                GenerateLoop(false);
                loop_linker->Reseed();
                reseed_high = true;
                reseed_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            }

            if (reseed < (HEMISPHERE_MAX_CV >> 1) && reseed_high) {
                reseed_high = false;
            }

            // reset loop
            if (loop_length_mod > 0 && loop_step >= loop_length_mod) {
                loop_step = 0;
                loop_index = 0;
                skip_steps = 0;
                reset_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            } 

            loop_linker->SetLoopStep(loop_index);

            // continue with active division
            if (--skip_steps > 0) {
                if (loop_length_mod > 0) {
                    loop_step++;
                }
                ClockOut(1);
                loop_linker->Trigger(1);
                return;
            }

            // get next weighted div or next div from loop
            if (loop_length_mod > 0) {
                skip_steps = GetNextLoopDiv();
            } else {
                skip_steps = GetNextWeightedDiv();
            }

            // no weights are set, don't do anything
            if (skip_steps == 0) {
                return;
            }

            ClockOut(0);
            loop_linker->Trigger(0);
            pulse_animation = HEMISPHERE_PULSE_ANIMATION_TIME;
        }

        if (pulse_animation > 0) {
            pulse_animation--;
        }
        if (reseed_animation > 0) {
            reseed_animation--;
        }
        if (reset_animation > 0) {
            reset_animation--;
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((ProbDivCursor)cursor) {
        case WEIGHT1: weight_1 = constrain(weight_1 + direction, 0, MAX_WEIGHT); break;
        case WEIGHT2: weight_2 = constrain(weight_2 + direction, 0, MAX_WEIGHT); break;
        case WEIGHT4: weight_4 = constrain(weight_4 + direction, 0, MAX_WEIGHT); break;
        case WEIGHT8: weight_8 = constrain(weight_8 + direction, 0, MAX_WEIGHT); break;
        case LOOP_LENGTH: {
            int old = loop_length;
            loop_length = constrain(loop_length + direction, 0, MAX_LOOP_LENGTH);
            if (old == 0 && loop_length > 0) {
                // seed loop
                GenerateLoop(true);
            }
            break;
        }
        default: break;
        }

        if (cursor < LOOP_LENGTH && loop_length > 0) {
          GenerateLoop(false);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // example: pack property_name at bit 0, with size of 8 bits
        Pack(data, PackLocation {0,4}, weight_1); 
        Pack(data, PackLocation {4,4}, weight_2); 
        Pack(data, PackLocation {8,4}, weight_4); 
        Pack(data, PackLocation {12,4}, weight_8); 
        Pack(data, PackLocation {16,8}, loop_length);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        // example: unpack value at bit 0 with size of 8 bits to property_name
        weight_1 = Unpack(data, PackLocation {0,4});
        weight_2 = Unpack(data, PackLocation {4,4});
        weight_4 = Unpack(data, PackLocation {8,4});
        weight_8 = Unpack(data, PackLocation {12,4});
        loop_length = Unpack(data, PackLocation {16,8});
        if (loop_length > 0) {
            // seed loop
            GenerateLoop(true);
        }
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=Length 2=Reseed";
        help[HEMISPHERE_HELP_OUTS]     = "A=Div    B=Skips";
        help[HEMISPHERE_HELP_ENCODER]  = "Weights/Loop";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // ProbDivCursor 
    int weight_1;
    int weight_2;
    int weight_4;
    int weight_8;
    int loop_length, loop_length_mod;
    int loop[MAX_LOOP_LENGTH];
    int loop_index;
    int loop_step;
    // used to keep track of reseed cv inputs so it only reseeds on rising edge
    bool reseed_high;

    int skip_steps;
    int pulse_animation = 0;
    int reseed_animation = 0;
    int reset_animation = 0;

    ProbLoopLinker *loop_linker = loop_linker->get();

    // pointer arrays that make loops easier
    const int *weights[4] = {&weight_1, &weight_2, &weight_4, &weight_8};
    const int divs[4] = {1, 2, 4, 8};
    
    void DrawInterface() {
        // divisions
        for(int i = 0; i < 4; i++) {
          gfxPrint(1, 15 + (i*10), "/");
          gfxPrint(divs[i]);
          DrawSlider(20, 15 + (i*10), 40, *weights[i], MAX_WEIGHT, cursor == i);

          // flash division when triggered
          if (pulse_animation > 0 && skip_steps == divs[i]) {
            gfxInvert(1, 15 + (i*10), 12, 8);
          }
        }

        // loop
        gfxIcon(4, 55, LOOP_ICON);
        // flash icon on reseed
        if (reseed_animation > 0) {
            gfxInvert(4, 55, 12, 8);
        }
        if (loop_length_mod == 0) {
            gfxPrint(19, 55, "off");
        } else {
            gfxPrint(19, 55, loop_length_mod);
        }
        if (cursor == LOOP_LENGTH) gfxCursor(19, 63, 18);

        if (reset_animation > 0) {
            gfxPrint(52, 55, "R");
        }
    }

    int GetNextWeightedDiv() {
        int total_weights = 0;

        for(int i = 0; i < 4; i++) {
            total_weights += *weights[i];
        }

        int rnd = random(0, total_weights + 1);
        for(int i = 0; i < 4; i++) {
          if (rnd <= *weights[i] && *weights[i] > 0) {
            return divs[i];
          }
          rnd -= *weights[i];
        }
        return 0;
    }

    void GenerateLoop(bool restart) {
        memset(loop, 0, sizeof(loop)); // reset loop
        if (restart) {
            loop_step = 0;
            loop_index = 0;
        }
        int index = 0;
        int counter = 0;
        while (counter < MAX_LOOP_LENGTH) {
            int div = GetNextWeightedDiv();
            if (div == 0) {
                break;
            }
            loop[index] = div;
            index++;
            counter += div;
        }
    }

    int GetNextLoopDiv() {
        int value = loop[loop_index];
        if (value == 0) {
            // we reseeded loop and now nothing is at this index, start over
            loop_index = 0;
            loop_step = 0;
            value = loop[loop_index];
        }
        loop_index++;
        loop_step++;
        return value;
    }
};
