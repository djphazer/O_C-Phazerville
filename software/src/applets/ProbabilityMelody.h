// Copyright (c) 2022, Benjamin Rosenbach
// Modified (M) 2025, Beau Sterling
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

#include "../HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo

#define HEM_PROB_MEL_MAX_WEIGHT 10
#define HEM_PROB_MEL_MAX_RANGE 60
#define HEM_PROB_MEL_MAX_LOOP_LENGTH 32

class ProbabilityMelody : public HemisphereApplet {
public:

    enum ProbMeloCursor {
        LOWER, UPPER,
        FIRST_NOTE = 2,
        LAST_NOTE = 13,
        ROTATE
    };

    const char* applet_name() {
        return "ProbMeloD";
    }
    const uint8_t* applet_icon() { return PhzIcons::probMeloD; }

    void Start() {
        down = 1;
        up = 12;
        pitch = 0;
    }

    void Controller() {
        loop_linker->RegisterMelo(hemisphere);

        // stash these to check for regen
        int oldDown = down_mod;
        int oldUp = up_mod;

        if (cv_rotate) { // override original cv mod functions to rotate probabilities
            ForEachChannel(ch) {
                int last_rotation = rotation[ch];
                rotation[ch] = SemitoneIn(ch) / 3;
                int step = rotation[ch] - last_rotation;
                if (ch) RotatePositiveWeights(weights, step);
                else RotateAllWeights(weights, step);
            }
        } else {
            // CV modulation
            down_mod = down;
            up_mod = up;
            // down scales to the up setting
            Modulate(down_mod, 0, 1, up);
            // up scales full range, with down value as a floor
            Modulate(up_mod, 1, down_mod, HEM_PROB_MEL_MAX_RANGE);
        }

        // regen when looping was enabled from ProbDiv
        bool regen = loop_linker->IsLooping() && !isLooping;
        isLooping = loop_linker->IsLooping();

        // reseed from ProbDiv
        regen = regen || loop_linker->ShouldReseed();

        // reseed loop if range has changed due to CV
        regen = regen || (isLooping && (down_mod != oldDown || up_mod != oldUp));

        if (regen) {
            GenerateLoop();
        }

        ForEachChannel(ch) {
            if (loop_linker->TrigPop(ch) || Clock(ch)) {
                if (isLooping) {
                    pitch = seqloop[ch][loop_linker->GetLoopStep()] + 60;
                } else {
                    pitch = GetNextWeightedPitch() + 60;
                }
                if (pitch != -1) {
                    Out(ch, MIDIQuantizer::CV(pitch));
                    pulse_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
                } else {
                    Out(ch, 0);
                }
            }
        }

        if (pulse_animation > 0) {
            pulse_animation--;
        }

        // animate value changes
        if (value_animation > 0) {
          value_animation--;
        }
    }

    void View() {
        DrawParams();
        DrawKeyboard();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, ROTATE);
            return;
        }

        if (cursor == ROTATE) {
            RotateAllWeights(weights, direction);
        } else if (cursor >= FIRST_NOTE) {
            // editing note probability
            int i = cursor - FIRST_NOTE;
            weights[i] = constrain(weights[i] + direction, 0, HEM_PROB_MEL_MAX_WEIGHT);
            value_animation = HEMISPHERE_CURSOR_TICKS;
        } else {
            // editing scaling
            if (cv_rotate) {
                if (cursor == LOWER) down_mod = constrain(down_mod + direction, 1, up_mod);
                if (cursor == UPPER) up_mod = constrain(up_mod + direction, down_mod, 60);
            } else {
                if (cursor == LOWER) down = constrain(down + direction, 1, up);
                if (cursor == UPPER) up = constrain(up + direction, down, 60);
            }
        }
        if (isLooping) {
            GenerateLoop(); // regenerate loop on any param changes
        }
    }

    void AuxButton() {
        // if (cursor == ROTATE)
            cv_rotate = !cv_rotate;
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < 12; ++i) {
          Pack(data, PackLocation {i*4, 4}, weights[i]);
        }
        Pack(data, PackLocation {48, 6}, down);
        Pack(data, PackLocation {54, 6}, up);
        Pack(data, PackLocation {60, 1}, cv_rotate);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < 12; ++i) {
          weights[i] = Unpack(data, PackLocation {i*4,4});
        }
        down = Unpack(data, PackLocation{48,6});
        up = Unpack(data, PackLocation{54,6});
        cv_rotate = Unpack(data, PackLocation{60,1});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock 1";
    help[HELP_DIGITAL2] = "Clock 2";
    help[HELP_CV1]      = cv_rotate? "Rotate" : "Lo/Semi";
    help[HELP_CV2]      = cv_rotate? "RotMask" : "Hi/Posi";
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Pitch 2";
    help[HELP_EXTRA1]   = "Set: Range bounds /";
    help[HELP_EXTRA2]   = "     Note weights";
    //                    "---------------------" <-- Extra text size guide
  }

private:
    int cursor; // ProbMeloCursor
    uint8_t weights[12] = {10,0,0,2,0,0,0,2,0,0,4,0};
    int up, up_mod;
    int down, down_mod;
    int pitch;
    bool isLooping = false;
    int seqloop[2][HEM_PROB_MEL_MAX_LOOP_LENGTH];
    bool cv_rotate = false;
    int rotation[2] = {0};

    ProbLoopLinker *loop_linker = loop_linker->get();

    int pulse_animation = 0;
    int value_animation = 0;
    const uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
    const uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
    const char* n[12] = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};

    void RotateAllWeights(uint8_t weights[], int r) {
        if (r == 0) return;
        r = r % 12;
        if (r < 0) r += 12;

        uint8_t temp[12];
        for (int i = 0; i < 12; ++i) {
            temp[i] = weights[i];
        }

        for (int i = 0; i < 12; i++) {
            weights[(i + r) % 12] = temp[i];
        }
    }

    void RotatePositiveWeights(uint8_t weights[], int r) {
        if (r == 0) return;

        int count = 0;
        uint8_t indices[12];
        uint8_t values[12];
        for (int i = 0; i < 12; ++i) {
            if (weights[i] > 0) {
                indices[count] = i;
                values[count] = weights[i];
                ++count;
            }
        }
        if (count == 0) return;

        r = r % count;
        if (r < 0) r += count;
        for (int i = 0; i < count; ++i) {
            weights[indices[(i + r) % count]] = values[i];
        }
    }

    int GetNextWeightedPitch() {
        int total_weights = 0;

        for(int i = down_mod-1; i < up_mod; i++) {
            total_weights += weights[i % 12];
        }

        int rnd = random(0, total_weights + 1);
        for(int i = down_mod-1; i < up_mod; i++) {
            int weight = weights[i % 12];
            if (rnd <= weight && weight > 0) {
                return i;
            }
            rnd -= weight;
        }
        return -1;
    }

    void GenerateLoop() {
        // always fill the whole loop to make things easier
        for (int i = 0; i < HEM_PROB_MEL_MAX_LOOP_LENGTH; i++) {
            seqloop[0][i] = GetNextWeightedPitch();
            seqloop[1][i] = GetNextWeightedPitch();
        }
    }

    void DrawKeyboard() {
        // Border
        gfxFrame(0, 27, 63, 32);

        // White keys
        for (uint8_t x = 0; x < 8; x++)
        {
            if (x == 3 || x == 7) {
                gfxLine(x * 8, 27, x * 8, 58);
            } else {
                gfxLine(x * 8, 43, x * 8, 58);
            }
        }

        // Black keys
        for (uint8_t i = 0; i < 6; i++)
        {
            if (i != 2) { // Skip the third position
                uint8_t x = (i * 8) + 6;
                gfxInvert(x, 28, 5, 15);
            }
        }
    }

    void DrawParams() {
        int note = pitch % 12;
        int octave = (pitch - 60) / 12;

        for (uint8_t i = 0; i < 12; i++)
        {
            uint8_t xOffset = x[i] + (p[i] ? 1 : 2);
            uint8_t yOffset = p[i] ? 31 : 45;

            if (pulse_animation > 0 && note == i) {
                gfxRect(xOffset - 1, yOffset, 3, 10);
            } else {
                if (EditMode() && i == (cursor - FIRST_NOTE)) {
                    // blink line when editing
                    if (CursorBlink()) {
                        gfxLine(xOffset, yOffset, xOffset, yOffset + 10);
                    } else {
                        gfxDottedLine(xOffset, yOffset, xOffset, yOffset + 10);
                    }
                } else {
                    gfxDottedLine(xOffset, yOffset, xOffset, yOffset + 10);
                }
                gfxLine(xOffset - 1, yOffset + 10 - weights[i], xOffset + 1, yOffset + 10 - weights[i]);
            }
        }

        // cursor for keys
        if (!EditMode()) {
            if (cursor == ROTATE) {
                gfxCursor(56, 60, 7);
                gfxCursor(56, 61, 7);
            } else if (cursor >= FIRST_NOTE) {
                int i = cursor - FIRST_NOTE;
                gfxCursor(x[i] - 1, p[i] ? 24 : 60, p[i] ? 5 : 7);
                gfxCursor(x[i] - 1, p[i] ? 25 : 61, p[i] ? 5 : 7);
            }
        } else {
            if (cursor == ROTATE) {
                gfxRect(56, 60, 7, 4);
                gfxClear(57, 61, 5, 2);
            }
        }

        if (cv_rotate) {
            gfxRect(57, 61, 5, 2);
        }

        // scaling params

        gfxIcon(0, 13, DOWN_BTN_ICON);
        gfxPrint(8, 15, ((down_mod - 1) / 12) + 1);
        gfxPrint(13, 15, ".");
        gfxPrint(17, 15, ((down_mod - 1) % 12) + 1);

        gfxIcon(30, 16, UP_BTN_ICON);
        gfxPrint(38, 15, ((up_mod - 1) / 12) + 1);
        gfxPrint(43, 15, ".");
        gfxPrint(47, 15, ((up_mod - 1) % 12) + 1);

        if (cursor == LOWER) gfxCursor(9, 23, 21);
        if (cursor == UPPER) gfxCursor(39, 23, 21);

        if (pulse_animation > 0) {
        //     int note = pitch % 12;
        //     int octave = (pitch - 60) / 12;

        //     gfxRect(x[note] + (p[note] ? 0 : 1), p[note] ? 29 : 54, 3, 2);
            gfxRect(58, 54 - (octave * 6), 3, 3);
        }

        if (value_animation > 0 && cursor >= FIRST_NOTE) {
          int i = cursor - FIRST_NOTE;

          gfxRect(1, 15, 60, 10);
          gfxInvert(1, 15, 60, 10);

          gfxPrint(18, 16, n[i]);
          if (p[i]) {
            gfxPrint(24, 16, "#");
          }
          gfxPrint(34, 16, weights[i]);
          gfxInvert(1, 15, 60, 10);
        }
    }
};
