// MiniArp v1.1.1 — gate-driven arpeggiator inspired by the 2hp Arp.
// 19 tone sets × 7 modes × 5 octave ranges. Octave is now an independent
// parameter (Keystep-style {-1, 0, +1, +2, +3}), not folded into mode.
// Mode names (2-ch): up / dn / pn / pp / pd / br / rn
//   pn = Pendulum endpoints-once, pp = Ping Pong endpoints-repeat
//   pd = Pedal Point (root against ascending others, Metropolix-style)
//   br = Brownian drunken walk (50/25/25, Metropolix-style)
// Terminology follows Intellijel Metropolix v1.4.
//
// Tone sets: 14 chords + 3 pentatonic scales + 2 intervals (5th, Oct).
// Clock advances step; gate length is a percentage of the clock period (min 10 ms).
//
// Breaking change from v1.0.0 (no real presets exist yet).
//
// I/O:
//   DIG1 = clock     DIG2 = reset
//   CV1  = root +    CV2  = chord +
//   OUT A = pitch    OUT B = gate
//
// Written by Victor Kuznetsov for O_C-Phazerville.

namespace miniarp {
    struct ChordDef { const char* name; int8_t tones[5]; uint8_t n_notes; };

    static const ChordDef kChords[] = {
        {"Maj",   {0, 4, 7, 0, 0},     3},
        {"Maj7",  {0, 4, 7, 11, 0},    4},
        {"Dom7",  {0, 4, 7, 10, 0},    4},
        {"min",   {0, 3, 7, 0, 0},     3},
        {"min7",  {0, 3, 7, 10, 0},    4},
        {"dim",   {0, 3, 6, 0, 0},     3},
        {"hdim7", {0, 3, 6, 10, 0},    4},
        {"dim7",  {0, 3, 6, 9, 0},     4},
        {"aug",   {0, 4, 8, 0, 0},     3},
        {"aug7",  {0, 4, 8, 10, 0},    4},
        {"sus2",  {0, 2, 7, 0, 0},     3},
        {"sus4",  {0, 5, 7, 0, 0},     3},
        {"s4M7",  {0, 5, 7, 11, 0},    4},
        {"s4m7",  {0, 5, 7, 10, 0},    4},
        {"majP",  {0, 2, 4, 7, 9},     5},
        {"minP",  {0, 3, 5, 7, 10},    5},
        {"bluM",  {0, 3, 5, 6, 10},    5},
        {"5th",   {0, 7, 0, 0, 0},     2},
        {"Oct",   {0, 12, 0, 0, 0},    2},
    };
    static const uint8_t kNumChords = sizeof(kChords) / sizeof(kChords[0]);

    static const char* const kModeNames[] = {
        "UP", "DN", "PN", "PP", "PD", "BR", "RN"
    };
    static const uint8_t kNumModes = 7;

    // Octave shift: -1, 0, +1, +2, +3 (Keystep-style)
    static constexpr int8_t kOctMin = -1;
    static constexpr int8_t kOctMax = 3;
    static constexpr uint8_t kNumOct = 5;

    // 2-char signed display: "-1", " 0", "+1", "+2", "+3"
    static const char* const kOctNames[] = {
        "-1", " 0", "+1", "+2", "+3"
    };
}

class MiniArp : public HemisphereApplet {
public:

    enum MiniArpCursor {
        ROOT,
        CHORD,
        MODE,
        OCTAVE,
        GATE_PCT,
        CURSOR_LAST = GATE_PCT
    };

    const char* applet_name() { return "MiniArp"; }
    const uint8_t* applet_icon() { return PhzIcons::miniArp; }

    void Start() {
        root = 48;  // C3
        chord = 0;
        mode = 0;
        oct_shift = 0;  // default: 1 octave, no expansion
        gate_pct = 50;
        step = 0;
        brownian_pos = 0;
        eff_root = root;
        eff_chord = 0;
        cur_idx = 0;
        cur_total = 3;
        last_midi = root;
        last_clip = 0;
    }

    void Controller() {
        if (Clock(1)) { step = 0; brownian_pos = 0; }

        // Chord: proportional sweep (2hp-style), DetentedIn rejects idle noise
        const int cv_chord_off = Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, miniarp::kNumChords);
        eff_chord = constrain((int)chord + cv_chord_off, 0, miniarp::kNumChords - 1);

        // Root: 1V/oct semitones, SemitoneIn has built-in hysteresis
        eff_root = constrain((int)root + SemitoneIn(0), 0, 127);

        if (Clock(0)) {
            const miniarp::ChordDef& cd = miniarp::kChords[eff_chord];
            const uint8_t N = cd.n_notes;
            const int base_oct = (oct_shift < 0) ? oct_shift : 0;
            const int total_oct = (oct_shift < 0 ? -oct_shift : oct_shift) + 1;
            const int M = N * total_oct;
            cur_total = M;

            int idx;
            switch (mode) {
                case 0: idx = step % M; break;                      // up
                case 1: idx = (M - 1) - (step % M); break;          // dn
                case 2: {                                           // pn — Pendulum (endpoints once)
                    const int period = (M > 1) ? (2 * M - 2) : 1;
                    const int s = step % period;
                    idx = (s < M) ? s : (2 * M - 2 - s);
                    break;
                }
                case 3: {                                           // pp — Ping Pong (endpoints repeat)
                    const int period = 2 * M;
                    const int s = step % period;
                    idx = (s < M) ? s : (2 * M - 1 - s);
                    break;
                }
                case 4: {                                           // pd — Pedal Point
                    if (M <= 1) { idx = 0; break; }
                    idx = (step & 1)
                        ? (1 + ((step >> 1) % (M - 1)))
                        : 0;
                    break;
                }
                case 5: {                                           // br — Brownian (50/25/25)
                    if (M <= 1) { idx = 0; break; }
                    if (step > 0) {
                        const int r = random(100);
                        if (r < 50)      brownian_pos = (brownian_pos + 1) % M;
                        else if (r < 75) { /* stay */ }
                        else             brownian_pos = (brownian_pos + M - 1) % M;
                    }
                    if (brownian_pos >= M) brownian_pos = 0;
                    idx = brownian_pos;
                    break;
                }
                default: idx = random(M); break;                    // rn — Random
            }
            cur_idx = idx;

            const int semi = cd.tones[idx % N] + 12 * ((idx / N) + base_oct);
            const int raw_midi = eff_root + semi;
            last_clip = (raw_midi > 127) ? 1 : (raw_midi < 0 ? -1 : 0);
            const int midi = constrain(raw_midi, 0, 127);
            last_midi = midi;
            Out(0, MIDIQuantizer::CV((uint8_t)midi));

            const uint32_t period_ticks = ClockCycleTicks(0);
            uint32_t gate_ticks = period_ticks * gate_pct / 100;
            const uint32_t min_gate_ticks = HEMISPHERE_CLOCK_TICKS * 10;
            if (gate_ticks < min_gate_ticks) gate_ticks = min_gate_ticks;
            ClockOut(1, gate_ticks);

            ++step;
        }
    }

    void View() {
        // While a parameter is being edited, show its base value (not the CV-modulated one)
        // so the display matches what the encoder actually changes. CV still affects audio.
        const bool editing_root  = (cursor == ROOT)  && EditMode();
        const bool editing_chord = (cursor == CHORD) && EditMode();
        const uint8_t show_root  = editing_root  ? root  : eff_root;
        const uint8_t show_chord = editing_chord ? chord : eff_chord;

        // Row 1: Root (note-name) + CV mod indicator
        gfxPrint(1, 15, midi_note_numbers[show_root]);
        if (eff_root != root) gfxIcon(54, 15, CV_ICON);
        if (cursor == ROOT) gfxCursor(0, 23, 27, 9, "Root");

        // Row 2: Chord name + CV mod indicator
        gfxPrint(1, 26, miniarp::kChords[show_chord].name);
        if (eff_chord != chord) gfxIcon(54, 26, CV_ICON);
        if (cursor == CHORD) gfxCursor(0, 34, 33, 9, "Chord");

        // Row 3: Mode (2ch) + Octave (2ch) + gate icon + gate%
        gfxPrint(1, 39, miniarp::kModeNames[mode]);
        if (cursor == MODE) gfxCursor(0, 47, 13, 9, "Mode");

        gfxPrint(16, 39, miniarp::kOctNames[oct_shift - miniarp::kOctMin]);
        if (cursor == OCTAVE) gfxCursor(15, 47, 13, 9, "Oct");

        gfxIcon(30, 39, GATE_ICON);
        gfxPrint(40, 39, gate_pct);
        gfxPrint("%");
        {
            const int digits = (gate_pct < 10) ? 1 : 2;
            const int width = digits * 6 + 6; // digits + '%'
            if (cursor == GATE_PCT) gfxCursor(39, 47, width + 1, 9, "Gt.len");
        }

        gfxLine(0, 50, 62, 50);

        // Row 4: current playing note + clip indicator + step counter (right-aligned)
        gfxPrint(1, 54, midi_note_numbers[last_midi]);
        if (last_clip > 0)      gfxIcon(25, 54, UP_ICON);
        else if (last_clip < 0) gfxIcon(25, 54, DOWN_ICON);

        {
            const int digits_idx = (cur_idx + 1 < 10) ? 1 : 2;
            const int digits_tot = (cur_total < 10) ? 1 : 2;
            const int width = (digits_idx + 1 + digits_tot) * 6; // N/M
            const int right = 61;
            const int x = right - width;
            gfxPrint(x, 54, cur_idx + 1);
            gfxPrint("/");
            gfxPrint(cur_total);
        }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, CURSOR_LAST);
            return;
        }
        switch ((MiniArpCursor)cursor) {
            case ROOT:      root      = constrain((int)root + direction, 0, 127); break;
            case CHORD:     chord     = constrain((int)chord + direction, 0, miniarp::kNumChords - 1); break;
            case MODE:      mode      = constrain((int)mode + direction, 0, miniarp::kNumModes - 1); break;
            case OCTAVE:    oct_shift = constrain((int)oct_shift + direction, miniarp::kOctMin, miniarp::kOctMax); break;
            case GATE_PCT:  gate_pct  = constrain((int)gate_pct + direction, 1, 99); break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation{0, 7},  root);
        Pack(data, PackLocation{7, 5},  chord);
        Pack(data, PackLocation{12, 3}, mode);
        // oct_shift is signed (-1..+3); store as unsigned offset (0..4)
        Pack(data, PackLocation{15, 3}, (uint8_t)(oct_shift - miniarp::kOctMin));
        Pack(data, PackLocation{18, 7}, gate_pct);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        root      = constrain((int)Unpack(data, PackLocation{0, 7}),  0, 127);
        chord     = constrain((int)Unpack(data, PackLocation{7, 5}),  0, miniarp::kNumChords - 1);
        mode      = constrain((int)Unpack(data, PackLocation{12, 3}), 0, miniarp::kNumModes - 1);
        const int oct_stored = Unpack(data, PackLocation{15, 3});
        oct_shift = constrain(oct_stored + miniarp::kOctMin, (int)miniarp::kOctMin, (int)miniarp::kOctMax);
        gate_pct  = constrain((int)Unpack(data, PackLocation{18, 7}), 1, 99);
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Root +";
        help[HELP_CV2]      = "Chord +";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Gate";
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
    }

private:
    int cursor;

    uint8_t root;
    uint8_t chord;
    uint8_t mode;
    int8_t  oct_shift;  // -1..+3
    uint8_t gate_pct;

    uint32_t step;
    uint8_t brownian_pos;
    uint8_t eff_root;
    uint8_t eff_chord;
    uint8_t cur_idx;
    uint8_t cur_total;
    uint8_t last_midi;
    int8_t  last_clip;  // 0 = none, +1 = clipped up (>127), -1 = clipped down (<0)
};
