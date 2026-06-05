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

// MarkovPerc: Rhythmic Markov State Machine
// The rhythmic sibling to MarkoV. States are hit types, not pitches.
// A Markov chain chooses the next hit pattern based on the current one,
// creating a drummer with evolving style and internal memory.
//
// Digital 1: Clock — advance to next state
// Digital 2: Reset — short press = replay from seed (same RNG sequence), long press = new seed
// CV In 1: Chaos — flattens the transition distribution (more erratic fills)
// CV In 2: Density — biases toward hits vs. rests (positive V = more hits)
// Out A: Trigger — fires sub-triggers for ratchets/flams within the beat
// Out B: Accent CV — held for full clock period, level set by beat accent

// ---------------------------------------------------------------------------
// Hit States
// ---------------------------------------------------------------------------
// REST        (0): silence
// HIT         (1): single trigger, moderate accent
// ACC_HIT     (2): single trigger, full accent
// FLAM        (3): grace note + main hit, medium accent on main
// ACC_FLAM    (4): grace note + accented main (accent on downbeat)
// RATCHET_2   (5): 2 evenly-spaced triggers, accent on first
// RATCHET_3   (6): 3 evenly-spaced triggers, accent on first
// RATCHET_4   (7): 4 evenly-spaced triggers, accent on first
// ---------------------------------------------------------------------------

namespace MarkovPercData {

// Accent levels per hit event (0-5, multiplied by ONE_OCTAVE = ~1V each)
// Used by ScheduleHit() to set Output B for each sub-trigger.
static constexpr uint8_t ACCENT_NONE   = 0;
static constexpr uint8_t ACCENT_GRACE  = 1; // soft grace note
static constexpr uint8_t ACCENT_SOFT   = 2; // hit
static constexpr uint8_t ACCENT_MED    = 3; // accented hit
static constexpr uint8_t ACCENT_HARD   = 4; // ratchet lead
static constexpr uint8_t ACCENT_FULL   = 5; // maximum accent

// profiles[profile][from_state][to_state]
// Weights use strong ratios so each profile has a clearly distinct feel.
// At chaos=0 the dominant weights dominate hard; at chaos=100 all → 8 (flat).
//
//               R    H   AH    F   AF   R2   R3   R4
static const uint8_t profiles[4][8][8] = {
    // 0: Steady (Rock/Pop)
    // Gravitates toward hits and accented hits. Rests are brief.
    // Flams appear as ornaments. Ratchets are rare fills.
    {
        {  2, 18, 12,  4,  2,  3,  1,  1 }, // from REST   — rebound to hit
        {  4, 16, 10,  6,  2,  4,  1,  1 }, // from HIT    — likely another hit
        {  4, 14, 10,  4,  4,  4,  2,  1 }, // from ACC_HIT
        {  4, 16, 10,  4,  2,  5,  1,  1 }, // from FLAM
        {  6, 14, 10,  4,  2,  4,  2,  1 }, // from ACC_FLAM
        {  8, 14,  8,  4,  2,  4,  1,  1 }, // from RATCHET_2 — rest likely after
        { 12, 12,  6,  2,  2,  4,  2,  2 }, // from RATCHET_3
        { 14, 12,  6,  2,  2,  2,  2,  2 }, // from RATCHET_4 — exhausted, rest
    },
    // 1: Syncopated (Funk/Latin)
    // Rests are structurally meaningful. Flams are very common.
    // Ratchet_2 is a common syncopation device. Higher average energy.
    {
        {  4, 10,  6, 12,  8, 10,  2,  1 }, // from REST   — many options
        { 10,  6,  4, 14,  8, 10,  4,  2 }, // from HIT    — flam very likely
        {  8,  8,  4, 12, 10,  8,  4,  2 }, // from ACC_HIT
        {  8, 10,  4,  8,  8, 12,  4,  2 }, // from FLAM   — ratchet_2 common
        {  8,  8,  6, 10,  8, 10,  4,  2 }, // from ACC_FLAM
        { 12,  8,  4,  8,  6,  8,  4,  2 }, // from RATCHET_2 — rest follows
        { 14,  8,  4,  6,  4,  6,  4,  2 }, // from RATCHET_3
        { 16,  6,  4,  4,  4,  6,  4,  2 }, // from RATCHET_4
    },
    // 2: Jazz/Free
    // Complex patterns, ratchets are common fills. Accented flams are the
    // signature gesture. Extended rests followed by dense bursts.
    {
        {  4,  6,  4,  8, 14, 12,  8,  6 }, // from REST   — burst likely
        {  6,  4,  4,  8, 12, 14, 10,  6 }, // from HIT
        {  4,  4,  4,  8, 14, 12, 10,  6 }, // from ACC_HIT
        {  6,  6,  4,  6, 14, 12, 10,  6 }, // from FLAM
        {  4,  4,  4,  6, 12, 14, 12,  8 }, // from ACC_FLAM — more complexity
        {  8,  6,  4,  8, 10, 10, 12,  8 }, // from RATCHET_2
        { 10,  4,  4,  6,  8,  8, 14, 10 }, // from RATCHET_3 — R4 very likely
        { 12,  4,  4,  4,  6,  6, 12, 14 }, // from RATCHET_4 — self-reinforcing
    },
    // 3: Sparse
    // Long silences punctuated by single hits. Ratchets and flams essentially absent.
    // REST self-loops heavily. HIT always returns to REST.
    {
        { 12, 10,  2,  1,  1,  1,  1,  1 }, // from REST
        { 18,  6,  2,  1,  1,  1,  1,  1 }, // from HIT
        { 16,  6,  4,  1,  1,  1,  1,  1 }, // from ACC_HIT
        { 18,  4,  2,  2,  1,  1,  1,  1 }, // from FLAM
        { 18,  4,  2,  1,  2,  1,  1,  1 }, // from ACC_FLAM
        { 20,  4,  2,  1,  1,  2,  1,  1 }, // from RATCHET_2
        { 20,  4,  2,  1,  1,  1,  1,  1 }, // from RATCHET_3
        { 20,  4,  2,  1,  1,  1,  1,  1 }, // from RATCHET_4
    },
};

static const char* const profile_names[4] = { "S", "T", "J", "P" };
static const char* const cursor_labels[3] = { "Matrix", "Chaos", "Seed" };

// State name abbreviations for display
static const char* const state_names[8] = {
    "Rst", "Hit", "AcH", "Flm", "AcF", "Rc2", "Rc3", "Rc4"
};

} // namespace MarkovPercData


class MarkovPerc : public HemisphereApplet {
public:
    static constexpr int      NUM_STATES       = 8;
    static constexpr int      NUM_PROFILES     = 4;
    static constexpr int      HISTORY_SIZE     = 8;
    static constexpr uint32_t LONG_PRESS_TICKS = 5000;
    static constexpr int      MAX_SCHED        = 4; // max sub-triggers per beat

    // Cursor positions
    static constexpr int CURSOR_STYLE = 0;
    static constexpr int CURSOR_CHAOS = 1;
    static constexpr int CURSOR_SEED  = 2;
    static constexpr int CURSOR_LAST  = 2;

    // Hit state indices
    static constexpr int STATE_REST      = 0;
    static constexpr int STATE_HIT       = 1;
    static constexpr int STATE_ACC_HIT   = 2;
    static constexpr int STATE_FLAM      = 3;
    static constexpr int STATE_ACC_FLAM  = 4;
    static constexpr int STATE_RATCHET_2 = 5;
    static constexpr int STATE_RATCHET_3 = 6;
    static constexpr int STATE_RATCHET_4 = 7;

    const char* applet_name() { return "MarkovPrc"; }
    const uint8_t* applet_icon() { return PhzIcons::drumMap; }

    void Start() {
        cursor          = 0;
        profile         = 0;
        chaos_base      = 0;
        chaos_pct       = 0;
        hit_state       = STATE_HIT;
        seed            = STATE_HIT;
        clock_period    = 1000; // safe default ~60ms
        sub_tick        = 0;
        sched_count     = 0;
        accent_cv       = 0;
        accent_countdown = 0;
        gate2_high      = false;
        gate2_ticks     = 0;
        randomized      = false;
        rng_seed        = (uint32_t)micros();
        seed_flash      = 0;
        reset_flash     = 0;
        for (int i = 0; i < HISTORY_SIZE; i++) history[i] = STATE_REST;
        history_head    = 0;
    }

    void Controller() {
        // --- Digital In 2: short = replay from seed, long = new random seed ---
        bool g2 = Gate(1);
        if (g2) {
            if (!gate2_high) gate2_high = true;
            gate2_ticks++;
            if (gate2_ticks >= LONG_PRESS_TICKS && !randomized) {
                rng_seed   = (uint32_t)micros();
                randomSeed(rng_seed);
                seed       = random(NUM_STATES - 1) + 1; // never seed on REST
                hit_state  = seed;
                randomized = true;
            }
        } else if (gate2_high) {
            gate2_high = false;
            if (!randomized) {
                randomSeed(rng_seed);
                hit_state   = seed; // short press: replay identical sequence
                reset_flash = 8000; // ~500ms visual indicator
            }
            randomized  = false;
            gate2_ticks = 0;
        }

        // --- Flash countdowns ---
        if (seed_flash  > 0) --seed_flash;
        if (reset_flash > 0) --reset_flash;

        // --- Digital In 1: Clock ---
        if (Clock(0)) {
            StartADCLag(0);
            clock_period = ClockCycleTicks(0);
            if (clock_period < 1) clock_period = 1000; // guard against zero
            sub_tick = 0;
        }

        if (EndOfADCLag(0)) {
            // CV 1: Chaos — adds to chaos_base, clamped to 0-100
            int cv1      = constrain(In(0), 0, HEMISPHERE_MAX_INPUT_CV);
            int cv_chaos = Proportion(cv1, HEMISPHERE_MAX_INPUT_CV, 100);
            chaos_pct    = constrain(chaos_base + cv_chaos, 0, 100);
            int chaos    = (chaos_pct * 256) / 100;

            // CV 2: Density — bipolar: 0V = neutral (128), +V = more hits, -V = more rests
            // Proportion maps In(1) [-MAX..+MAX] → [-128..+128], offset by 128 → [0..256]
            int density = constrain(128 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 128), 0, 256);

            // Advance Markov chain and schedule sub-triggers
            hit_state = NextState(hit_state, chaos, density);
            ScheduleHit(hit_state, clock_period);

            // Set accent CV for full clock period based on beat accent level
            uint8_t beat_acc   = BeatAccent(hit_state);
            int     accent_val = (int)beat_acc * ONE_OCTAVE;
            Out(1, accent_val);
            accent_cv        = accent_val;
            accent_countdown = clock_period;

            // Fire tick-0 events immediately (Out A triggers only)
            for (int i = 0; i < sched_count; i++) {
                if (schedule[i].tick == 0) {
                    FireTrig();
                }
            }

            // Update scrolling history
            history[history_head] = hit_state;
            history_head = (history_head + 1) % HISTORY_SIZE;
        }

        // Count ISR ticks and fire scheduled sub-triggers
        sub_tick++;
        for (int i = 0; i < sched_count; i++) {
            if (schedule[i].tick > 0 && sub_tick == schedule[i].tick) {
                FireTrig();
            }
        }

        // Zero accent CV after clock period expires
        if (accent_countdown > 0 && --accent_countdown == 0) {
            Out(1, 0);
        }
    }

    void View() final;
    void MainView() {
        // --- Cursor label at top, right-justified, shown only while editing ---
        if (EditMode()) {
            const char* label = MarkovPercData::cursor_labels[cursor];
            gfxPrint(63 - (strlen(label) * 6), 2, label);
        }

        // --- Parameter line: [S/T/J/P]  [Chaos%]  [dice] ---
        // Layout: profile@1, chaos@22, dice@54 (no overlap at 100%)
        gfxPrint(1, 15, MarkovPercData::profile_names[profile]);
        gfxPos(22, 15);
        graphics.printf("%d%%", chaos_pct);
        // Seed indicator: dice icon at col 54
        if (seed_flash > 0)
            gfxIcon(54, 14, RANDOM_ICON); // flash: dice 1px higher
        else
            gfxIcon(54, 15, RANDOM_ICON);

        // Reset flash: briefly invert dice icon only
        if (reset_flash > 0)
            gfxInvert(53, 14, 10, 9);

        // Cursor underlines
        switch (cursor) {
            case CURSOR_STYLE: gfxCursor(1,  23, 6);  break;
            case CURSOR_CHAOS: gfxCursor(22, 23, 24); break;
            case CURSOR_SEED:  gfxCursor(54, 23, 8);  break;
        }

        // Separator
        gfxLine(0, 25, 63, 25);

        // --- Scrolling hit-type history ---
        // Bar height = accent level; horizontal bands for subdivided states.
        // history_head = next write slot = oldest entry.
        const int GY   = 25; // graph top y (below separator)
        const int GH   = 37; // graph height in pixels (baseline at y=62, line at y=63)
        const int ybot = GY + GH; // bottom of graph area

        for (int i = 0; i < HISTORY_SIZE; i++) {
            int idx = (history_head + i) % HISTORY_SIZE;
            int s   = history[idx];
            int x   = i * 8;

            if (s == STATE_REST) {
                // Dot at baseline
                gfxRect(x + 3, ybot - 1, 1, 1);
                continue;
            }

            uint8_t acc   = BeatAccent(s);
            int     bar_h = max(2, (int)acc * GH / (int)MarkovPercData::ACCENT_FULL);

            // Number of horizontal bands (subdivisions of bar into segments)
            int bands = 1;
            switch (s) {
                case STATE_FLAM:
                case STATE_ACC_FLAM:  bands = 2; break;
                case STATE_RATCHET_2: bands = 2; break;
                case STATE_RATCHET_3: bands = 3; break;
                case STATE_RATCHET_4: bands = 4; break;
                default: bands = 1; break;
            }

            // Width: wider for accented hits
            int bar_w = (s == STATE_ACC_HIT || s == STATE_ACC_FLAM) ? 4 : 3;
            int bar_x = x + (8 - bar_w) / 2;

            // Draw bar subdivided into bands with 1px gap between each
            int band_h_base = bar_h / bands;
            for (int b = 0; b < bands; b++) {
                int y_base  = ybot - bar_h + b * band_h_base;
                int this_h  = (b < bands - 1) ? (band_h_base - 1)
                                               : (bar_h - b * band_h_base);
                if (this_h < 1) this_h = 1;
                gfxRect(bar_x, y_base, bar_w, this_h);
            }
        }

        // Baseline
        gfxLine(0, ybot + 1, 63, ybot + 1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }
        switch (cursor) {
            case CURSOR_STYLE:
                profile = constrain(profile + direction, 0, NUM_PROFILES - 1);
                break;
            case CURSOR_CHAOS:
                chaos_base = constrain((int)chaos_base + direction, 0, 100);
                chaos_pct  = chaos_base; // immediate display feedback before next clock
                break;
            case CURSOR_SEED:
                // Encoder re-rolls seed immediately; stay in edit mode for repeated rolls
                rng_seed   = (uint32_t)micros();
                randomSeed(rng_seed);
                seed       = random(NUM_STATES - 1) + 1; // never seed on REST
                hit_state  = seed;
                seed_flash = 2700; // ~170ms visible flash
                break;
        }
    }

    void AuxButton() {
        if (cursor == CURSOR_SEED) {
            rng_seed  = (uint32_t)micros();
            randomSeed(rng_seed);
            seed      = random(NUM_STATES - 1) + 1;
            hit_state = seed;
        } else {
            hit_state = seed; // existing behavior: jump to seed
        }
        CancelEdit();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation{0,  2}, profile);
        Pack(data, PackLocation{2,  3}, hit_state);
        Pack(data, PackLocation{5,  7}, chaos_base);
        Pack(data, PackLocation{12, 3}, seed);
        Pack(data, PackLocation{15, 32}, rng_seed);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        profile    = constrain((int)Unpack(data, PackLocation{0,  2}), 0, NUM_PROFILES - 1);
        hit_state  = constrain((int)Unpack(data, PackLocation{2,  3}), 0, NUM_STATES - 1);
        chaos_base = constrain((int)Unpack(data, PackLocation{5,  7}), 0, 100);
        seed       = constrain((int)Unpack(data, PackLocation{12, 3}), 0, NUM_STATES - 1);
        rng_seed   = (uint32_t)Unpack(data, PackLocation{15, 32});
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "RstSeed";
        help[HELP_CV1]      = "Chaos+";
        help[HELP_CV2]      = "Density";
        help[HELP_OUT1]     = "Trigger";
        help[HELP_OUT2]     = "Accent";
        help[HELP_EXTRA1]   = "Enc:Params";
    }

private:
    // --- Scheduled sub-trigger entry ---
    struct SchedEntry {
        int16_t tick;   // ISR ticks after clock edge (0 = fire immediately)
        uint8_t accent; // kept for schedule compatibility (not used for Out B)
    };

    uint8_t   cursor;
    uint8_t   profile;
    uint8_t   chaos_base;    // encoder-set baseline chaos 0-100
    int       chaos_pct;     // live chaos for display
    uint8_t   hit_state;     // current Markov state
    uint8_t   seed;          // reset target state
    int32_t   clock_period;  // measured clock period in ISR ticks
    int32_t   sub_tick;      // ticks elapsed since last clock edge
    SchedEntry schedule[MAX_SCHED];
    int       sched_count;
    int       accent_cv;     // current accent CV level (raw DAC units)
    int       accent_countdown; // ticks until accent CV resets to 0
    uint8_t   history[HISTORY_SIZE];
    uint8_t   history_head;
    bool      gate2_high;
    uint32_t  gate2_ticks;
    bool      randomized;
    uint32_t  rng_seed;      // stored RNG seed for repeatable loop
    uint16_t  seed_flash;    // >0: invert dice icon (encoder re-roll feedback)
    uint16_t  reset_flash;   // >0: invert param row (Dig 2 reset feedback)

    // Return the accent level for a given hit state (used for Out B CV and View).
    uint8_t BeatAccent(uint8_t state) {
        switch (state) {
            case STATE_REST:      return MarkovPercData::ACCENT_NONE;
            case STATE_HIT:       return MarkovPercData::ACCENT_SOFT;
            case STATE_ACC_HIT:   return MarkovPercData::ACCENT_FULL;
            case STATE_FLAM:      return MarkovPercData::ACCENT_MED;
            case STATE_ACC_FLAM:  return MarkovPercData::ACCENT_FULL;
            case STATE_RATCHET_2: return MarkovPercData::ACCENT_HARD;
            case STATE_RATCHET_3: return MarkovPercData::ACCENT_MED;
            case STATE_RATCHET_4: return MarkovPercData::ACCENT_SOFT;
            default:              return MarkovPercData::ACCENT_NONE;
        }
    }

    // Fire Output A trigger only (Out B is set once per beat in Controller).
    void FireTrig() {
        ClockOut(0);
    }

    // Build the sub-trigger schedule for the given hit state.
    // All tick values are relative to the clock edge (tick 0 = immediate).
    // Flam grace note fires at tick 0; main hit fires at period/8.
    // Ratchet hits are evenly divided across the period.
    void ScheduleHit(uint8_t state, int32_t period) {
        sched_count = 0;
        int32_t p8  = max((int32_t)5, period / 8); // flam gap (≥5 ticks)

        switch (state) {
            case STATE_REST:
                break; // nothing scheduled

            case STATE_HIT:
                schedule[0] = {0, MarkovPercData::ACCENT_SOFT};
                sched_count = 1;
                break;

            case STATE_ACC_HIT:
                schedule[0] = {0, MarkovPercData::ACCENT_FULL};
                sched_count = 1;
                break;

            case STATE_FLAM:
                // Grace note immediately, main hit slightly after
                schedule[0] = {0,              MarkovPercData::ACCENT_GRACE};
                schedule[1] = {(int16_t)p8,    MarkovPercData::ACCENT_MED};
                sched_count = 2;
                break;

            case STATE_ACC_FLAM:
                // Grace note immediately, full accent on the downbeat hit
                schedule[0] = {0,              MarkovPercData::ACCENT_GRACE};
                schedule[1] = {(int16_t)p8,    MarkovPercData::ACCENT_FULL};
                sched_count = 2;
                break;

            case STATE_RATCHET_2: {
                int32_t step = period / 2;
                schedule[0] = {0,              MarkovPercData::ACCENT_HARD};
                schedule[1] = {(int16_t)step,  MarkovPercData::ACCENT_SOFT};
                sched_count = 2;
                break;
            }

            case STATE_RATCHET_3: {
                int32_t step = period / 3;
                schedule[0] = {0,                    MarkovPercData::ACCENT_HARD};
                schedule[1] = {(int16_t)step,         MarkovPercData::ACCENT_SOFT};
                schedule[2] = {(int16_t)(step * 2),   MarkovPercData::ACCENT_SOFT};
                sched_count = 3;
                break;
            }

            case STATE_RATCHET_4: {
                int32_t step = period / 4;
                schedule[0] = {0,                    MarkovPercData::ACCENT_HARD};
                schedule[1] = {(int16_t)step,         MarkovPercData::ACCENT_SOFT};
                schedule[2] = {(int16_t)(step * 2),   MarkovPercData::ACCENT_SOFT};
                schedule[3] = {(int16_t)(step * 3),   MarkovPercData::ACCENT_SOFT};
                sched_count = 4;
                break;
            }
        }
    }

    // Advance the Markov chain with chaos flattening and density bias.
    // chaos:   0-256 fixed-point (0=pure profile, 256=flat uniform)
    // density: 0-256 (128=neutral, >128=more hits, <128=more rests)
    // All integer arithmetic, no floats.
    int NextState(uint8_t from, int chaos, int density) {
        int weights[NUM_STATES];
        int total     = 0;
        int inv_chaos = 256 - chaos;

        for (int j = 0; j < NUM_STATES; j++) {
            int pw = MarkovPercData::profiles[profile][from][j];

            // Chaos interpolation: blend profile weight toward flat (8)
            int w  = (pw * inv_chaos + 8 * chaos) >> 8;

            // Density bias: symmetric around 128 (neutral).
            // density=128: both factors = 1.0 (no change)
            // density=256: REST → 0 (suppressed), hits → 2× (doubled)
            // density=0:   REST → 2× (doubled),  hits → 0 (suppressed)
            if (j == STATE_REST) {
                w = (w * (256 - density)) >> 7; // 0→2×, 128→1×, 256→0
            } else {
                w = (w * density) >> 7;          // 0→0,  128→1×, 256→2×
            }

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

FLASHMEM void MarkovPerc::View() {
  MainView();
}
