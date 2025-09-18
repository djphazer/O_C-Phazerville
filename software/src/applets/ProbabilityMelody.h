// Copyright (c) 2022, Benjamin Rosenbach
// Modified (M) 2025, Beau Sterling, Nicholas Michalek, Bryan Head
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

#pragma once
#include "../HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo
#include "../HemisphereApplet.h"

#define HEM_PROB_MEL_MAX_WEIGHT 10
#define HEM_PROB_MEL_MAX_RANGE 60
#define HEM_PROB_MEL_MAX_LOOP_LENGTH 32

namespace probmelod {
enum CV_SOURCE : uint8_t {
    // yes these assignments are unnecessary: just making the bitmasking explicit
    NONE = 0b00,
    CV1 = 0b01,
    CV2 = 0b10,
    BOTH = 0b11
};

struct LabelledCvConfig {
    char cv1_label[6];
    char cv2_label[6];
    uint8_t cv_config;
};

static constexpr uint8_t cv_config(CV_SOURCE semi, CV_SOURCE mask, CV_SOURCE lower, CV_SOURCE upper) {
    return (semi << 6) | (mask << 4) | (lower << 2) | upper;
}

static constexpr LabelledCvConfig cv_modes[] = {
    {"Lower", "Upper", cv_config(NONE, NONE, CV1,  CV2)},
    {"Semi",  "Mask",  cv_config(CV1,  CV2,  NONE, NONE)},
    {"Semi",  "Upper", cv_config(CV1,  NONE, NONE, CV2)},
    {"Semi",  "Lower", cv_config(CV1,  NONE, CV2,  NONE)},
    {"Semi",  "Pos",   cv_config(CV1,  NONE, CV2,  CV2)},
    {"Mask",  "Upper", cv_config(NONE, CV1,  NONE, CV2)},
    {"Mask",  "Lower", cv_config(NONE, CV1,  CV2,  NONE)},
    {"Mask",  "Pos",   cv_config(NONE, CV1,  CV2,  CV2)},
    {"Semi+", "Mask",  cv_config(CV1,  CV2,  NONE, CV1)},
    {"Semi-", "Mask",  cv_config(CV1,  CV2,  CV1,  NONE)},
    {"Semi*", "Mask",  cv_config(CV1,  CV2,  CV1,  CV1)},
    {"Semi",  "Mask+", cv_config(CV1,  CV2,  NONE, CV2)},
    {"Semi",  "Mask-", cv_config(CV1,  CV2,  CV2,  NONE)},
    {"Semi",  "Mask*", cv_config(CV1,  CV2,  CV2,  CV2)},
    {"Semi*", "Mask*", cv_config(CV1,  CV2,  BOTH, BOTH)},
};
}


class ProbabilityMelody : public HemisphereApplet {
public:

    enum ProbMeloCursor {
        LOWER, UPPER,
        FIRST_NOTE = 2,
        LAST_NOTE = 13,
        ROTATE,
        CV_MODE
    };

    const char* applet_name() {
        return "ProbMeloD";
    }
    const uint8_t* applet_icon() { return PhzIcons::probMeloD; }

    void Start() {
        down = 1;
        up = 12;
        pitch[0] = 0;
        pitch[1] = 0;
    }
    void Unload() {
        loop_linker.UnloadMelo(hemisphere);
    }

    void Controller() {
        loop_linker.RegisterMelo(hemisphere);

        // stash these to check for regen
        int oldDown = down_mod;
        int oldUp = up_mod;
        down_mod = down;
        up_mod = up;

        uint8_t cvm = probmelod::cv_modes[cv_mode].cv_config;
        rotation[0] = semitone_cv_in((cvm >> 6) & 0b11);
        rotation[1] = semitone_cv_in((cvm >> 4) & 0b11);
        down_mod = constrain(down + semitone_cv_in((cvm >> 2) & 0b11), 1, up);
        up_mod = constrain(up + semitone_cv_in(cvm & 0b11), down_mod, HEM_PROB_MEL_MAX_RANGE);

        // regen when looping was enabled from ProbDiv
        bool regen = loop_linker.IsLooping() && !isLooping;
        isLooping = loop_linker.IsLooping();

        // reseed from ProbDiv
        regen = regen || loop_linker.ShouldReseed();

        // reseed loop if range has changed due to CV
        regen = regen || (isLooping && (down_mod != oldDown || up_mod != oldUp));

        if (regen) {
            GenerateLoop();
        }

        ForEachChannel(ch) {
            if (Clock(ch)) StartADCLag(ch);
            if (loop_linker.TrigPop(ch) || EndOfADCLag(ch)) {
                if (isLooping) {
                    pitch[ch] = seqloop[ch][loop_linker.GetLoopStep()];
                } else {
                    pitch[ch] = GetNextWeightedPitch();
                }
                Out(ch, MIDIQuantizer::CV(pitch[ch] + (12*OC::DAC::kOctaveZero)));
            }
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
            MoveCursor(cursor, direction, CV_MODE);
            return;
        }

        switch (cursor) {
            case LOWER:
                down = constrain(down + direction, 1, up);
                break;
            case UPPER:
                up = constrain(up + direction, down, 60);
                break;
            case ROTATE:
                rotate_masked_left(weights, 0xffff, 12, -direction);
                break;
            case CV_MODE:
                cv_mode = constrain(
                    cv_mode + direction,
                    0,
                    static_cast<int8_t>(std::size(probmelod::cv_modes) - 1)
                );
                break;
            default:
              // editing note probability
              int i = cursor - FIRST_NOTE;
              weights[i] = constrain(
                weights[i] + direction, -1, HEM_PROB_MEL_MAX_WEIGHT
              ); // -1 removes note from mask
              value_animation = HEMISPHERE_CURSOR_TICKS;
        }
        if (isLooping) {
            GenerateLoop(); // regenerate loop on any param changes
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (size_t i = 0; i < 12; ++i) {
            Pack(data, PackLocation {i*4, 4}, weights[i]+1);
        }
        Pack(data, PackLocation {48, 6}, down);
        Pack(data, PackLocation {54, 6}, up);
        Pack(data, PackLocation {60, 4}, cv_mode);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (size_t i = 0; i < 12; ++i) {
            weights[i] = Unpack(data, PackLocation {i*4,4})-1;
        }
        down = constrain(Unpack(data, PackLocation{48,6}), 1, 60);
        up = constrain(Unpack(data, PackLocation{54,6}), down, 60);
        cv_mode = Unpack(data, PackLocation{60,4});

    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock 1";
        help[HELP_DIGITAL2] = "Clock 2";
        help[HELP_CV1]      = probmelod::cv_modes[cv_mode].cv1_label;
        help[HELP_CV2]      = probmelod::cv_modes[cv_mode].cv2_label;
        help[HELP_OUT1]     = "Pitch 1";
        help[HELP_OUT2]     = "Pitch 2";
        help[HELP_EXTRA1]   = "Set: Range bounds /";
        help[HELP_EXTRA2]   = "     Note weights";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int8_t cursor; // ProbMeloCursor
    int8_t weights[12] = {10,-1,0,2,-1,0,-1,2,0,-1,4,-1}; // scale=Cmin, chord=Cmin7
    int8_t up, up_mod;
    int8_t down, down_mod;
    uint8_t pitch[2] = {0};
    bool isLooping = false;
    uint8_t seqloop[2][HEM_PROB_MEL_MAX_LOOP_LENGTH];
    int8_t rotation[2] = {0};
    int8_t cv_mode = 0;

    ProbLoopLinker &loop_linker = ProbLoopLinker::get();

    int value_animation = 0;
    static constexpr uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
    static constexpr uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
    static constexpr char n[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};

    template <typename T>
    static uint32_t get_non_neg_mask(T* arr, int n) {
        uint32_t mask = 0;
        for (int i = 0; i < n; ++i) {
            if (arr[i] >= 0) {
                mask |= 1 << i;
            }
        }
        return mask;
    }

    static int semitones_to_degrees(uint32_t scale_mask, int semitones) {
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
    static void rotate_masked_left(T* arr, uint32_t mask, int n, int r) {
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
      masked_rot -= semitone_rot;
      uint32_t scale_mask = get_non_neg_mask(rot_weights, 12);
      int degrees = semitones_to_degrees(scale_mask, masked_rot);
      rotate_masked_left(rot_weights, scale_mask, 12, -degrees);
      rotate_masked_left(rot_weights, 0xffff, 12, -semitone_rot);
    }

    uint8_t GetNextWeightedPitch() {
        int total_weights = 0;
        int8_t rotated_weights[12];
        UpdateRotatedWeights(weights, rotated_weights, rotation[0], rotation[1]);

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
        return 0;
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
        int8_t ws[12];
        if (FIRST_NOTE <= cursor && cursor <= LAST_NOTE) {
            std::copy(weights, weights + 12, ws);
        } else {
            UpdateRotatedWeights(weights, ws, rotation[0], rotation[1]);
        }

        for (uint8_t i = 0; i < 12; ++i) {
            uint8_t xOffset = x[i] + (p[i] ? 1 : 2);
            uint8_t yOffset = p[i] ? 31 : 45;
            bool unmasked = (ws[i] >= 0);

            if (EditMode() && i == (cursor - FIRST_NOTE)) {
                gfxDottedLine(xOffset, yOffset, xOffset, yOffset + 10);
            } else if (unmasked) {
                gfxDottedLine(xOffset, yOffset, xOffset, yOffset + 10);
            }
            if (unmasked)
                gfxLine(xOffset - 1, yOffset + 10 - ws[i], xOffset + 1, yOffset + 10 - ws[i]);
        }
        ForEachChannel(ch) {
            int note = pitch[ch] % 12;
            uint8_t xOffset = x[note] + (p[note] ? 1 : 2) - 3;
            gfxIcon(xOffset, 59, ch ? UP_TRI_R_HALF : UP_TRI_L_HALF);
        }

        if (cursor == ROTATE) {
            if (EditMode()) {
                gfxRect(56, 60, 7, 4);
                gfxClear(57, 61, 5, 2);
            } else {
                gfxCursor(56, 60, 7);
                gfxCursor(56, 61, 7);
            }
        } else if (FIRST_NOTE <= cursor && cursor <= LAST_NOTE) {
            int i = cursor - FIRST_NOTE;
            uint8_t xOffset = x[i] + (p[i] ? 1 : 2);
            uint8_t yOffset = p[i] ? 31 : 45;
            if (EditMode() || CursorBlink()) {
                gfxLine(xOffset, yOffset, xOffset, yOffset + 10);
            }
        }

        // scaling params

        if (cursor == CV_MODE) {
            gfxPos(1, 15);
            graphics.printf("%s", probmelod::cv_modes[cv_mode].cv1_label);
            gfxPos(32, 15);
            graphics.printf("%5s", probmelod::cv_modes[cv_mode].cv2_label);
            gfxCursor(1, 23, 62);
        } else {
            gfxIcon(0, 13, DOWN_BTN_ICON);
            gfxIcon(30, 16, UP_BTN_ICON);

            if (cursor == LOWER) {
                gfxPrintOctDotSemi(8, 15, down);
                gfxCursor(9, 23, 21);
            } else {
                gfxPrintOctDotSemi(8, 15, down_mod);
            }
            if (cursor == UPPER) {
                gfxPrintOctDotSemi(38, 15, up);
                gfxCursor(39, 23, 21);
            } else {
                gfxPrintOctDotSemi(38, 15, up_mod);
            }
        }

        ForEachChannel(ch) {
            int octave = (pitch[ch] - 60) / 12;
            gfxRect(58 + ch, 54 - (octave * 6), 2, 3);
        }

        if (value_animation > 0 && cursor >= FIRST_NOTE) {
            int i = cursor - FIRST_NOTE;

            gfxRect(1, 15, 60, 10);
            gfxInvert(1, 15, 60, 10);

            gfxPos(18, 16);
            graphics.printf("%c", n[i]);
            if (p[i]) {
                gfxPrint(24, 16, "#");
            }
            if (ws[i] < 0) gfxPrint(34, 16, "X");
            else gfxPrint(34, 16, ws[i]);
            gfxInvert(1, 15, 60, 10);
        }
    }

    void gfxPrintOctDotSemi(int x, int y, int semitone) {
        gfxPrint(x, y, ((semitone - 1) / 12) + 1);
        gfxPrint(x + 5, y, ".");
        gfxPrint(x + 9, y, ((semitone - 1) % 12) + 1);
    }

    int semitone_cv_in(uint8_t cv_mask) {
        int out = 0;
        if (cv_mask & probmelod::CV1) out += SemitoneIn(0);
        if (cv_mask & probmelod::CV2) out += SemitoneIn(1);
        return out;
    }
};
