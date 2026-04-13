// Copyright (c) 2026, Andy Jenkinson (uglifruit)
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

// MarkoV: A Finite State Melodic Generator
// First-order Markov chain over 10 scale degrees with Tendency Profiles.
//
// Digital 1: Clock — advance to next state
// Digital 2: Reset — short press = replay from seed (same RNG sequence), long press = new random seed
// CV 1: Chaos — offsets chaos_base upward (0V = pure profile, +V = flatter)
// CV 2: Transpose — V/Oct offset applied BEFORE quantizer (shifts which scale notes are accessed)
// Out A: Quantized pitch
// Out B: Trigger pulse when quantized pitch changes

namespace MarkoVData {

// profiles[profile][from_state][to_state]
// 10 states: 0–7 span one octave (ONE_OCTAVE/7 per step); states 8–9 are upper
// extensions into the second octave. Every state maps to a unique scale degree.
// Weights use 20:1 ratios so profiles sound clearly different.
// At chaos=0 the dominant weights dominate hard; at chaos=100 all → 8 (flat).
static const uint8_t profiles[5][10][10] = {
    // 0: Pentatonic Stability
    // Root(0) and fifth(4) are strong attractors from any state.
    // Octave(9) is a common arrival point. High states (7-9) fall back down.
    {
        { 20,  2,  3,  1, 18,  1,  1,  1,  1,  8 }, // from 0 (root)
        { 18,  2, 14,  1, 16,  1,  1,  1,  1,  2 }, // from 1 (2nd)
        { 14,  1,  5,  1, 18,  1,  1,  1,  1,  4 }, // from 2 (3rd)
        {  8,  1,  4,  2, 20,  3,  1,  1,  1,  2 }, // from 3 (4th)
        { 20,  1,  3,  1, 18,  1,  1,  4,  2,  8 }, // from 4 (5th)
        { 16,  1,  4,  1, 14,  2,  1,  2,  1,  5 }, // from 5 (6th)
        { 20,  1,  2,  1, 12,  1,  1,  2,  1,  1 }, // from 6 (7th) — resolves to root
        { 18,  2,  3,  1, 14,  1,  1,  4,  2,  2 }, // from 7 — falls back
        { 16,  2,  3,  1, 12,  1,  1,  3,  2,  2 }, // from 8 — falls back
        { 18,  3,  4,  1, 14,  1,  1,  4,  3,  5 }, // from 9 (oct) — root/5th
    },
    // 1: Chromatic Tension
    // Stepwise motion dominates (±1 state). Repeating a note is common.
    // Leaps are very rare — snake-like line now extended over wider range.
    {
        {  8, 20,  1,  1,  1,  1,  1,  1,  1,  4 }, // from 0
        { 20,  8, 20,  1,  1,  1,  1,  1,  1,  1 }, // from 1
        {  1, 20,  8, 20,  1,  1,  1,  1,  1,  1 }, // from 2
        {  1,  1, 20,  8, 20,  1,  1,  1,  1,  1 }, // from 3
        {  1,  1,  1, 20,  8, 20,  1,  1,  1,  1 }, // from 4
        {  1,  1,  1,  1, 20,  8, 20,  1,  1,  1 }, // from 5
        {  1,  1,  1,  1,  1, 20,  8, 20,  1,  1 }, // from 6
        {  1,  1,  1,  1,  1,  1, 20,  8, 20,  1 }, // from 7
        {  1,  1,  1,  1,  1,  1,  1, 20,  8, 20 }, // from 8
        {  4,  1,  1,  1,  1,  1,  1,  1, 20,  8 }, // from 9
    },
    // 2: Jazz Tendencies
    // Strong pull toward the 7th(6) from almost anywhere — the defining
    // jazz gesture. Root resolution strong from 6. States 7-9 are upper
    // extensions (9th, #11, 13th) that add tension then resolve.
    {
        {  4,  1,  6,  1,  5,  1, 20,  3,  2,  3 }, // from 0 — leap to 7th
        {  4,  1, 12,  1,  4,  1, 18,  2,  2,  2 }, // from 1 — 3rd or 7th
        {  4,  1,  4,  1,  6,  1, 20,  3,  2,  2 }, // from 2 — leap to 7th
        {  2,  1,  4,  1,  3,  1, 22,  3,  2,  2 }, // from 3 — tritone pull to 7th
        { 10,  1,  4,  1,  4,  1, 14, 10,  4,  4 }, // from 4 — root or upper
        {  4,  1,  8,  1,  4,  1, 18,  4,  3,  3 }, // from 5 — 3rd or 7th
        { 22,  1,  4,  3,  5,  1,  4,  3,  2,  2 }, // from 6 — strong resolve to root
        { 14,  1,  4,  1,  8,  1, 10,  4,  4,  4 }, // from 7 — upper, resolve
        { 10,  1,  4,  1,  6,  1, 14,  5,  3,  4 }, // from 8 — pull to 7th
        { 12,  1,  4,  1,  8,  1, 12,  4,  4,  4 }, // from 9 — fall or leap to 7th
    },
    // 3: Glacial
    // Stays on current note most of the time. Only adjacent steps are possible.
    // Root and fifth are the only attractors; leaps are nearly impossible.
    {
        { 20, 10,  1,  1, 10,  1,  1,  2,  1,  1 }, // from 0 (root)
        { 14, 18, 10,  1,  4,  1,  1,  1,  1,  1 }, // from 1
        {  8, 10, 18, 10,  4,  1,  1,  1,  1,  1 }, // from 2
        {  6,  1, 10, 18, 10,  1,  1,  1,  1,  1 }, // from 3
        { 12,  1,  1, 10, 20,  8,  1,  1,  1,  1 }, // from 4 (fifth)
        {  6,  1,  1,  1,  8, 18, 10,  1,  1,  1 }, // from 5
        { 14,  1,  1,  1,  4,  8, 16,  5,  1,  1 }, // from 6 (7th resolves to root)
        { 10,  1,  1,  1,  6,  1,  6, 18, 10,  1 }, // from 7
        {  8,  1,  1,  1,  4,  1,  1,  8, 18, 10 }, // from 8
        { 10,  1,  1,  1,  4,  1,  1,  4,  8, 18 }, // from 9 (oct)
    },
    // 4: Drone
    // Overwhelming self-loop weight (~83%). No attractors.
    // The note barely ever changes — Out B stays silent unless chaos is raised.
    // Adjacent ±1 steps are the only realistic escape.
    {
        { 40,  2,  1,  1,  1,  1,  1,  1,  1,  1 }, // from 0 (root)
        {  2, 40,  2,  1,  1,  1,  1,  1,  1,  1 }, // from 1
        {  1,  2, 40,  2,  1,  1,  1,  1,  1,  1 }, // from 2
        {  1,  1,  2, 40,  2,  1,  1,  1,  1,  1 }, // from 3
        {  1,  1,  1,  2, 40,  2,  1,  1,  1,  1 }, // from 4
        {  1,  1,  1,  1,  2, 40,  2,  1,  1,  1 }, // from 5
        {  1,  1,  1,  1,  1,  2, 40,  2,  1,  1 }, // from 6
        {  1,  1,  1,  1,  1,  1,  2, 40,  2,  1 }, // from 7
        {  1,  1,  1,  1,  1,  1,  1,  2, 40,  2 }, // from 8
        {  1,  1,  1,  1,  1,  1,  1,  1,  2, 40 }, // from 9 (oct)
    },
};

static const char* const profile_names[5]  = { "S", "T", "J", "G", "D" };
static const char* const cursor_labels[4]  = { "Matrix", "Scale", "Chaos", "Seed" };

} // namespace MarkoVData


class MarkoV : public HemisphereApplet {
public:
    static constexpr int      NUM_STATES       = 10;
    static constexpr int      NUM_PROFILES     = 5;
    static constexpr int      HISTORY_SIZE     = 8;
    static constexpr uint32_t LONG_PRESS_TICKS = 5000;
    // CV units per state step: spans root(0) to octave(9) across 10 states
    static constexpr int      STATE_CV_STEP    = ONE_OCTAVE / 7;

    // Cursor positions
    static constexpr int CURSOR_MATRIX = 0;
    static constexpr int CURSOR_SCALE  = 1;
    static constexpr int CURSOR_CHAOS  = 2;
    static constexpr int CURSOR_SEED   = 3;
    static constexpr int CURSOR_LAST   = 3;

    const char* applet_name() { return "MarkoV"; }

    void Start() {
        cursor       = 0;
        profile      = 0;
        qselect      = io_offset;
        chaos_base   = 0;
        chaos_pct    = 0;
        state        = 0;
        seed         = 0;
        prev_cv      = 0;
        gate2_high   = false;
        gate2_ticks  = 0;
        randomized   = false;
        rng_seed     = (uint32_t)micros();
        seed_flash   = 0;
        reset_flash  = 0;
        for (int i = 0; i < HISTORY_SIZE; i++) history[i] = 0;
        history_head = 0;
    }

    void Controller() {
        // --- Digital In 2: short press = replay from seed, long press = new seed ---
        bool g2 = Gate(1);
        if (g2) {
            if (!gate2_high) gate2_high = true;
            gate2_ticks++;
            if (gate2_ticks >= LONG_PRESS_TICKS && !randomized) {
                rng_seed   = (uint32_t)micros();
                randomSeed(rng_seed);
                seed       = random(NUM_STATES);
                state      = seed;
                randomized = true;
            }
        } else if (gate2_high) {
            gate2_high = false;
            if (!randomized) {
                randomSeed(rng_seed);
                state       = seed; // short press: replay identical sequence
                reset_flash = 8000; // ~500ms visual indicator
            }
            randomized  = false;
            gate2_ticks = 0;
        }

        // --- Flash countdowns ---
        if (seed_flash  > 0) --seed_flash;
        if (reset_flash > 0) --reset_flash;

        // --- Digital In 1: Clock ---
        if (Clock(0)) StartADCLag(0);

        if (EndOfADCLag(0)) {
            // CV 1: Chaos — adds to chaos_base, clamped to 0-100
            int cv1      = constrain(In(0), 0, HEMISPHERE_MAX_INPUT_CV);
            int cv_chaos = Proportion(cv1, HEMISPHERE_MAX_INPUT_CV, 100);
            chaos_pct    = constrain(chaos_base + cv_chaos, 0, 100);

            // Map chaos_pct (0-100) to fixed-point 0-256 for NextState
            int chaos    = (chaos_pct * 256) / 100;

            // CV 2: Transpose — applied before quantizer so it shifts which
            // scale notes are accessed rather than transposing post-quantization.
            // Small CV jitter on In(1) is absorbed by the quantizer's note-snapping.
            int transpose = In(1);

            // Advance the Markov chain
            state = NextState(state, chaos);

            // Output A: transpose added before quantization; the quantizer maps
            // the transposed position to the nearest scale note.
            int pitch_cv = HS::Quantize(qselect, state * STATE_CV_STEP + transpose);
            Out(0, pitch_cv);

            // Output B: trigger when quantized pitch (including transpose) changes
            if (pitch_cv != prev_cv) ClockOut(1);
            prev_cv = pitch_cv;

            // Update scrolling note history
            history[history_head] = state;
            history_head = (history_head + 1) % HISTORY_SIZE;
        }
    }

    void View() {
        // --- Header: cursor parameter name, right-justified, shown only while editing ---
        if (EditMode()) {
            const char* label = MarkoVData::cursor_labels[cursor];
            gfxPrint(63 - (strlen(label) * 6), 2, label);
        }

        // --- Parameter line (y=15) ---
        // Layout: profile@1, scale@10, chaos@25, dice@54 (no overlap at 100%)
        gfxPrint(1,  15, MarkoVData::profile_names[profile]);
        gfxPrint(10, 15, "Q");
        gfxPrint(qselect + 1);
        gfxPos(25, 15);
        graphics.printf("%d%%", chaos_pct);
        // Seed indicator: dice icon at col 54
        if (seed_flash > 0)
            gfxIcon(54, 14, RANDOM_ICON); // flash: dice 1px higher
        else
            gfxIcon(54, 15, RANDOM_ICON);

        // Reset flash: briefly invert dice icon only
        if (reset_flash > 0)
            gfxInvert(53, 14, 10, 9);

        // Cursor underline — spicy (dotted) for Scale to hint at Aux edit
        switch (cursor) {
            case CURSOR_MATRIX: gfxCursor(1,  23, 6);  break;
            case CURSOR_SCALE:  gfxSpicyCursor(10, 23, 12); break;
            case CURSOR_CHAOS:  gfxCursor(25, 23, 24); break;
            case CURSOR_SEED:   gfxCursor(54, 23, 8);  break;
        }

        // Separator
        gfxLine(0, 25, 63, 25);

        // --- Scrolling bar graph ---
        // history_head points to the next write slot = oldest entry
        const int GRAPH_BOTTOM = 62;
        const int GRAPH_H      = 35;
        const int BAR_W        = 6;
        const int BAR_STRIDE   = 8;

        for (int i = 0; i < HISTORY_SIZE; i++) {
            int idx   = (history_head + i) % HISTORY_SIZE;
            int note  = history[idx];
            int bar_h = max(2, (note * GRAPH_H) / (NUM_STATES - 1));
            int x     = i * BAR_STRIDE;
            gfxRect(x, GRAPH_BOTTOM - bar_h + 1, BAR_W, bar_h);
        }

        gfxLine(0, GRAPH_BOTTOM + 1, 63, GRAPH_BOTTOM + 1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }
        switch (cursor) {
            case CURSOR_MATRIX:
                profile = constrain(profile + direction, 0, NUM_PROFILES - 1);
                break;
            case CURSOR_SCALE:
                qselect = constrain(qselect + direction, 0, QUANT_CHANNEL_COUNT - 1);
                HS::qview = qselect;
                HS::PokePopup(QUANTIZER_POPUP);
                break;
            case CURSOR_CHAOS:
                chaos_base = constrain((int)chaos_base + direction, 0, 100);
                chaos_pct  = chaos_base; // immediate display feedback before next clock
                break;
            case CURSOR_SEED:
                // Encoder re-rolls seed immediately; stay in edit mode for repeated rolls
                rng_seed   = (uint32_t)micros();
                randomSeed(rng_seed);
                seed       = random(NUM_STATES);
                state      = seed;
                seed_flash = 2700; // ~170ms visible flash
                break;
        }
    }

    void AuxButton() {
        if (cursor == CURSOR_SCALE) {
            HS::QuantizerEdit(qselect);
        } else if (cursor == CURSOR_SEED) {
            rng_seed = (uint32_t)micros();
            randomSeed(rng_seed);
            seed  = random(NUM_STATES);
            state = seed;
        }
        CancelEdit();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation{0,  3}, profile);    // 3 bits for 5 profiles
        Pack(data, PackLocation{3,  4}, state);      // 4 bits for 10 states (0-9)
        Pack(data, PackLocation{7,  2}, qselect);
        Pack(data, PackLocation{9,  7}, chaos_base);
        Pack(data, PackLocation{16, 4}, seed);       // 4 bits for 10 states (0-9)
        Pack(data, PackLocation{20, 32}, rng_seed);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        profile    = constrain((int)Unpack(data, PackLocation{0,  3}), 0, NUM_PROFILES - 1);
        state      = constrain((int)Unpack(data, PackLocation{3,  4}), 0, NUM_STATES - 1);
        qselect    = constrain((int)Unpack(data, PackLocation{7,  2}), 0, QUANT_CHANNEL_COUNT - 1);
        chaos_base = constrain((int)Unpack(data, PackLocation{9,  7}), 0, 100);
        seed       = constrain((int)Unpack(data, PackLocation{16, 4}), 0, NUM_STATES - 1);
        rng_seed   = (uint32_t)Unpack(data, PackLocation{20, 32});
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "RstSeed";
        help[HELP_CV1]      = "Chaos+";
        help[HELP_CV2]      = "Transp";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Trigger";
        help[HELP_EXTRA1]   = "Enc:Params";
        help[HELP_EXTRA2]   = "Aux:QEdit";
    }

private:
    uint8_t  cursor;
    uint8_t  profile;
    uint8_t  qselect;      // absolute quantizer channel
    uint8_t  chaos_base;   // encoder-set baseline chaos 0-100
    int      chaos_pct;    // live chaos (base + CV) for display
    uint8_t  state;
    uint8_t  seed;         // state reset returns here; long press sets new seed
    int      prev_cv;      // last quantized CV output, for trigger comparison
    uint8_t  history[HISTORY_SIZE];
    uint8_t  history_head;
    bool     gate2_high;
    uint32_t gate2_ticks;
    bool     randomized;
    uint32_t rng_seed;     // stored RNG seed for repeatable loop
    uint16_t seed_flash;   // >0: invert dice icon (encoder re-roll feedback)
    uint16_t reset_flash;  // >0: invert param row (Dig 2 reset feedback)

    // Advance the Markov chain.
    // chaos: 0-256 fixed-point (0=pure profile weights, 256=flat/uniform)
    // w[j] = (profile_weight[j] * (256-chaos) + 8 * chaos) >> 8
    // Max intermediate: 22 * 256 = 5632 — integer only, no floats.
    int NextState(uint8_t from, int chaos) {
        int weights[NUM_STATES];
        int total     = 0;
        int inv_chaos = 256 - chaos;

        for (int j = 0; j < NUM_STATES; j++) {
            int pw     = MarkoVData::profiles[profile][from][j];
            int w      = (pw * inv_chaos + 8 * chaos) >> 8;
            weights[j] = max(1, w);
            total     += weights[j];
        }

        int r   = random(total);
        int cum = 0;
        for (int j = 0; j < NUM_STATES; j++) {
            cum += weights[j];
            if (r < cum) return j;
        }
        return NUM_STATES - 1;
    }
};
