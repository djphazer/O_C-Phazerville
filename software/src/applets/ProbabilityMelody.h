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

        ForEachChannel(ch) { // prevent ranges from jumping to new values when changing mod modes
            if (mod_latch[ch]) {
                if (cv_rotate) {
                    // if (abs(SemitoneIn(ch) - last_rot_cv[ch]) < 2) mod_latch[ch] = false;
                } else {
                    if (abs(SemitoneIn(ch) - last_range_cv[ch]) < 2) mod_latch[ch] = false;
                }
            }
        }

        // stash these to check for regen
        int oldDown = down_mod;
        int oldUp = up_mod;

        if (cv_rotate && range_init) { // override original cv mod functions to rotate probabilities
            ForEachChannel(ch) rotation[ch] = SemitoneIn(ch);
            UpdateRotatedWeights(weights, rotated_weights, rotation[0], rotation[1]);
        } else { // CV modulation
            if (!mod_latch[0]) {
                down_mod = down;
                Modulate(down_mod, 0, 1, up); // down scales to the up setting
            }
            if (!mod_latch[1]) {
                up_mod = up;
                Modulate(up_mod, 1, down_mod, HEM_PROB_MEL_MAX_RANGE); // up scales full range, with down value as a floor
            }
            range_init = true;
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
            rotate_masked_left(weights, 0xffff, 12, -direction);
        } else if (cursor >= FIRST_NOTE) {
            // editing note probability
            int i = cursor - FIRST_NOTE;
            weights[i] = constrain(
                weights[i] + direction, -1, HEM_PROB_MEL_MAX_WEIGHT
            ); // -1 removes note from mask
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
        cv_rotate = !cv_rotate;
        ForEachChannel(ch) { // prevent ranges from jumping to new values when changing mod modes
            mod_latch[ch] = true;
            if (cv_rotate) last_range_cv[ch] = SemitoneIn(ch);
            // else last_rot_cv[ch] = SemitoneIn(ch);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < 12; ++i) {
            Pack(data, PackLocation {i*4, 4}, weights[i]+1);
        }
        Pack(data, PackLocation {48, 6}, down);
        Pack(data, PackLocation {54, 6}, up);
        Pack(data, PackLocation {60, 1}, cv_rotate);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < 12; ++i) {
            weights[i] = Unpack(data, PackLocation {i*4,4})-1;
        }
        down = constrain(Unpack(data, PackLocation{48,6}), 1, up);
        up = constrain(Unpack(data, PackLocation{54,6}), down, 60);
        cv_rotate = Unpack(data, PackLocation{60,1});

        if (cv_rotate) range_init = false;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock 1";
        help[HELP_DIGITAL2] = "Clock 2";
        help[HELP_CV1]      = cv_rotate? "RotKey" : "Lower";
        help[HELP_CV2]      = cv_rotate? "RotMask" : "Upper";
        help[HELP_OUT1]     = "Pitch 1";
        help[HELP_OUT2]     = "Pitch 2";
        help[HELP_EXTRA1]   = "Set: Range bounds /";
        help[HELP_EXTRA2]   = "     Note weights";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor; // ProbMeloCursor
    int8_t weights[12] = {10,0,0,2,0,0,0,2,0,0,4,0};
    int8_t rotated_weights[12];
    int up, up_mod;
    int down, down_mod;
    int pitch;
    bool isLooping = false;
    int seqloop[2][HEM_PROB_MEL_MAX_LOOP_LENGTH];
    bool cv_rotate = false;
    int8_t rotation[2] = {0};
    bool mod_latch[2] = {false};
    // int8_t last_rot_cv[2];
    int8_t last_range_cv[2];
    bool range_init = false;

    ProbLoopLinker *loop_linker = loop_linker->get();

    int pulse_animation = 0;
    int value_animation = 0;
    const uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
    const uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
    const char* n[12] = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};

    template <typename T>
    uint32_t get_non_neg_mask(T* arr, int n) {
        uint32_t mask = 0;
        for (int i = 0; i < n; ++i) {
            if (arr[i] >= 0) {
                mask |= 1 << i;
            }
        }
        return mask;
    }

    int semitones_to_degrees(uint32_t scale_mask, int semitones) {
        semitones = ((semitones % 12) + 12) % 12;
        semitones -= __builtin_ctz(scale_mask);
        scale_mask >>= __builtin_ctz(scale_mask);
        int degrees = 0;
        while (semitones > 0 && scale_mask) {
            int rot = __builtin_ctz(scale_mask>>1) + 1;
            semitones -= rot;
            scale_mask >>= rot;
            degrees++;
        }
        return scale_mask ? degrees : 0;
    }

    /**
     * Rotate the elements of arr indicated by the mask by r steps to the left.
     * To rotate right, use -r.
     * I like this implementation because its in-place, O(1) space, O(n) time
     * even if traversing the mask required traversing the underlying array, and
     * O(popcount) if traversing the mask is constant time (which, thanks to
     * ctz, it is). Even though it requires recursion, it's a tail call, so
     * should get TCOed away (though, the depth is going to be so shallow, it
     * doesn't really matter).
     */
    template <typename T>
    void rotate_masked_left(T* arr, uint32_t mask, int n, int r) {
        // Clear any bits that are out of range cause they'll mess ctz and
        // popcounts
        if (n < 32) mask = mask & ~(~0u << n);
        if (!mask) return;
        int count = __builtin_popcount(mask);
        r = (r % count + count) % count;
        if (r == 0) return;
        // j is always r steps ahead of i on the mask. So start j at the first bit
        // and hop bits r times.
        int j = __builtin_ctz(mask);
        for (int i = 0; i < r; ++i) j += __builtin_ctz(mask >> (j + 1)) + 1;
        int i = __builtin_ctz(mask);
        for (int c = 0; c < count - r; ++c) {
            std::swap(arr[i], arr[j]);
            uint16_t jm = mask >> (++j);
            // Check if we need to loop by seeing if we've run out of bits
            j = jm ? j + __builtin_ctz(jm) : __builtin_ctz(mask);
            // since i is only iterating count times, don't need to loop around
            i += __builtin_ctz(mask >> (i + 1)) + 1;
        }
        int m = count % r;
        if (m) rotate_masked_left(arr + i, mask >> i, n - i, -m);
    }

    void UpdateRotatedWeights(
        int8_t* src_weights, int8_t* rot_weights, int semitone_rot, int masked_rot
    ) {
      std::copy(src_weights, src_weights + 12, rot_weights);
      rotate_masked_left(rot_weights, 0xffff, 12, -semitone_rot);
      uint32_t scale_mask = get_non_neg_mask(rot_weights, 12);
      int degrees = semitones_to_degrees(scale_mask, masked_rot);
      // Serial.printf("degrees = %d, scale_mask = %x\n", degrees, scale_mask);
      rotate_masked_left(rot_weights, scale_mask, 12, -degrees);
    }

    int GetNextWeightedPitch() {
        int total_weights = 0;

        for(int i = down_mod-1; i < up_mod; ++i) {
            total_weights += max(0, rotated_weights[i % 12]);
        }

        int rnd = random(0, total_weights + 1);
        for(int i = down_mod-1; i < up_mod; ++i) {
            int weight = max(0, rotated_weights[i % 12]);
            if (rnd <= weight && weight > 0) {
                return i;
            }
            rnd -= weight;
        }
        return -1;
    }

    void GenerateLoop() {
        // always fill the whole loop to make things easier
        for (int i = 0; i < HEM_PROB_MEL_MAX_LOOP_LENGTH; ++i) {
            seqloop[0][i] = GetNextWeightedPitch();
            seqloop[1][i] = GetNextWeightedPitch();
        }
    }

    void DrawKeyboard() {
        // Border
        gfxFrame(0, 27, 63, 32);

        // White keys
        for (uint8_t x = 0; x < 8; ++x) {
            if (x == 3 || x == 7) {
                gfxLine(x * 8, 27, x * 8, 58);
            } else {
                gfxLine(x * 8, 43, x * 8, 58);
            }
        }

        // Black keys
        for (uint8_t i = 0; i < 6; ++i) {
            if (i != 2) { // Skip the third position
                uint8_t x = (i * 8) + 6;
                gfxInvert(x, 28, 5, 15);
            }
        }
    }

    void DrawParams() {
        int note = pitch % 12;
        int octave = (pitch - 60) / 12;
        int8_t* ws = FIRST_NOTE <= cursor && cursor <= LAST_NOTE ? weights : rotated_weights;

        for (uint8_t i = 0; i < 12; ++i) {
            uint8_t xOffset = x[i] + (p[i] ? 1 : 2);
            uint8_t yOffset = p[i] ? 31 : 45;
            bool unmasked = (ws[i] >= 0);

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
                } else if (unmasked) {
                    gfxDottedLine(xOffset, yOffset, xOffset, yOffset + 10);
                }
                if (unmasked)
                  gfxLine(xOffset - 1, yOffset + 10 - ws[i], xOffset + 1, yOffset + 10 - ws[i]);
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
            // int note = pitch % 12;
            // int octave = (pitch - 60) / 12;

            // gfxRect(x[note] + (p[note] ? 0 : 1), p[note] ? 29 : 54, 3, 2);
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
            if (ws[i] < 0) gfxPrint(34, 16, "X");
            else gfxPrint(34, 16, ws[i]);
            gfxInvert(1, 15, 60, 10);
        }
    }
};
