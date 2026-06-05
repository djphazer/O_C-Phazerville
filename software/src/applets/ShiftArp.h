// ShiftArp v1.3.0 — shift-register arpeggiator inspired by the Flatsix Arp of
// Darkness. Written by Victor Kuznetsov for O_C-Phazerville.
// Stage 1 (rev 2): CV2 is a gate input for capture, DIG2 is reset.
//
// v1.1: rename exp/inp → pnd/ppg to match Metropolix v1.4 terminology.
// v1.2: mode icons replaced with 3-char uppercase text (right-aligned),
//       4 new modes added (PDL Pedal Point, BRW Brownian walk, SHF Shuffle,
//       WIN Shift Window). Total 10 modes. Modal buffer editor (EDT cursor).
//       Fixed persistence bug: len now stored in 5 bits (was 4, truncated len=16).
// v1.3: per-preset note buffer persistence (via the new Hemisphere extra-slots
//       API). Local braids::Quantizer with per-preset scale/root/octave/mask —
//       independent of the global Hemisphere q_engine, so changing scale on
//       one ShiftArp doesn't affect anything else and is saved with the preset.
//       Modal scale editor with cursor order Scale → Mask bits → Root → Oct.
//       Mask cells: filled = enabled, empty = disabled (last enabled can't
//       be turned off). Buffer-editor exit via side button (AuxButton route);
//       header now uses ↔/pencil icons; main UI uses note icon for EDT cursor.
//       Buffer editor bars normalised to active note range, like the main view.
//
// I/O:
//   DIG1 = clock (advance playback position)
//   DIG2 = reset (return playhead to first position)
//   CV1  = pitch in (source notes)
//   CV2  = gate: rising edge captures CV1 (quantised) into the buffer head
//   OUT A = pitch V/oct
//   OUT B = gate (length = gate_pct % of clock period, min 10 ms)

namespace shiftarp {
    static constexpr uint8_t kLenMin = 3;
    static constexpr uint8_t kLenMax = 16;

    // 10 playback modes, 3-char uppercase labels. Text-only (no icons) for
    // consistency with MiniArp and readability at the 8×8 pixel budget.
    // Ordered by "increasing freedom": Direction → Patterned → Generative.
    //
    //   Direction (linear traversals):
    //     FWD  Forward: 0, 1, 2, …, N-1, 0 …
    //     BCK  Backward: N-1, N-2, …, 0, N-1 …
    //     PND  Pendulum — endpoints play once (per Metropolix)
    //     PPG  Ping Pong — endpoints repeat (per Metropolix)
    //
    //   Patterned (deterministic but non-linear):
    //     PDL  Pedal Point — root against ascending others: 0,1,0,2,0,3,…
    //     WIN  Shift Window (size 3) — 012, 123, 234, … (Metropolix "Arp 3 Shift -1")
    //     PHI  Golden ratio (Weyl sequence, jump ≈ N·(φ−1) ≈ N·0.618)
    //
    //   Generative (stochastic):
    //     SHF  Shuffle — one-time permutation replayed cyclically until RESET
    //     BRW  Brownian drunken walk (50% fwd / 25% stay / 25% back)
    //     RND  Random — uniform over [0, N) on every step
    static const char* const kModeNames[] = {
        "FWD", "BCK", "PND", "PPG",
        "PDL", "WIN", "PHI",
        "SHF", "BRW", "RND"
    };
    static const uint8_t kNumModes = 10;

    static constexpr uint8_t kBufferCapacity = 16;
    static constexpr uint8_t kShiftWindow = 3;  // window size for WIN mode

    // Buffer slot sentinel: empty slot (rest). Plays no gate, holds previous pitch.
    // Valid notes are 0..127 (MIDI), so 0xFF is safe as out-of-range sentinel.
    static constexpr uint8_t kRest = 0xFF;
}

class ShiftArp : public HemisphereApplet {
public:

    enum ShiftArpCursor {
        LEN,
        EDT,
        MODE,
        APPLY,
        SCALE,
        GATE_PCT,
        CURSOR_LAST = GATE_PCT
    };

    enum ApplyMode { APPLY_IMMEDIATE = 0, APPLY_QUEUED = 1 };

    const char* applet_name() { return "ShiftArp"; }
    const uint8_t* applet_icon() { return PhzIcons::shiftArp; }

    // Declarations only — bodies are out-of-line below the class so FLASHMEM
    // attribute actually moves them to QSPI Flash and frees ITCM. Inline
    // class methods get pulled into ITCM regardless of FLASHMEM.
    void Start();
    void Reset() override;
    void View();
    void OnButtonPress();
    void AuxButton() override;
    void OnEncoderMove(int direction);
    uint64_t OnDataRequest();
    void OnDataReceive(uint64_t data);

    void Controller() {
        const uint8_t L = len;

        // DIG2 = Reset playhead. Always immediate — the user controls timing by
        // choosing when to send the trigger. Also reshuffle SHF (new random
        // permutation, per Metropolix semantics) and rewind BRW walker.
        if (Clock(1)) {
            step = 0;
            brownian_pos = 0;
            shuffle_valid = false;
        }

        // CV2 = capture gate with Schmitt-trigger hysteresis to reject noise.
        // Rise: require > 2.0 V. Fall: require < 0.8 V. Plain GATE_THRESHOLD
        // (1.25 V) gives multiple false rising edges when CV2 dithers.
        static constexpr int CV_GATE_HIGH = 25 << 7;  // ~2.0 V
        static constexpr int CV_GATE_LOW  = 10 << 7;  // ~0.8 V
        const int cv2 = In(1);
        const bool new_high = cv2_was_high ? (cv2 > CV_GATE_LOW) : (cv2 > CV_GATE_HIGH);

        if (new_high && !cv2_was_high) {
            // Rising edge: wait for CV1 to settle before reading the pitch.
            // Default ADC lag is only ~2 ms — not enough for all sources.
            StartADCLag(0, HEMISPHERE_CLOCK_TICKS * 5);  // 5 ms
            capture_pending = true;
        }
        cv2_was_high = new_high;

        if (capture_pending && EndOfADCLag(0)) {
            // SemitoneIn has built-in hysteresis — rejects micro-drift around
            // semitone boundaries that plain NoteNumber() falls for.
            const int st = SemitoneIn(0);
            int midi = st + 12 * OC::DAC::kOctaveZero;
            midi = constrain(midi, 0, 127);
            // FIFO: oldest note at buffer[0], newest appended. When full, shift
            // left to drop the oldest. Forward playback plays in entry order.
            if (buf_count < L) {
                buffer[buf_count++] = (uint8_t)midi;
            } else {
                for (int i = 0; i < L - 1; ++i) buffer[i] = buffer[i + 1];
                buffer[L - 1] = (uint8_t)midi;
            }
            capture_pending = false;
        }

        // DIG1 = clock: advance playhead
        if (Clock(0)) {
            uint8_t L_now = len;
            uint8_t N = (buf_count < L_now) ? buf_count : L_now;
            if (N == 0) return;

            uint8_t idx = step % N;

            // Cycle start — apply any queued changes.
            if (idx == 0) {
                if (pending_len_valid)  { len = pending_len_val;   pending_len_valid = false; }
                if (pending_mode_valid) { mode = pending_mode_val; pending_mode_valid = false; }
                // Recompute N in case LEN just changed.
                L_now = len;
                N = (buf_count < L_now) ? buf_count : L_now;
                if (N == 0) return;
                idx = step % N;
            }

            // Playback mode decides which buffer slot to play.
            cur_idx = compute_idx(step, N, mode);
            const uint8_t slot = buffer[cur_idx];

            if (slot == shiftarp::kRest) {
                // Rest: keep previous pitch on OUT A (sample-and-hold), no gate.
                ++step;
                return;
            }

            // Raw (chromatic) note → local quantizer. Process(pitch, root, transpose):
            //   root      — semitones (in 1/128 units), sets where the scale is rooted.
            //   transpose — note INDEX offset within the scale (NOT semitones! e.g.
            //               for major it shifts whole scale degrees). Don't use for octaves.
            // Octave shift is applied to the CV result instead — clean +12*N semitones.
            const int raw_cv = MIDIQuantizer::CV(slot);
            const int32_t root_offset = (int32_t)local_root << 7;
            const int q_cv0 = local_q.Process(raw_cv, root_offset, 0);
            const int q_cv  = q_cv0 + ((int32_t)local_octave * 12 << 7);
            last_midi = MIDIQuantizer::NoteNumber(q_cv);
            Out(0, q_cv);

            const uint32_t period_ticks = ClockCycleTicks(0);
            uint32_t gate_ticks = period_ticks * gate_pct / 100;
            const uint32_t min_gate_ticks = HEMISPHERE_CLOCK_TICKS * 10;
            if (gate_ticks < min_gate_ticks) gate_ticks = min_gate_ticks;
            ClockOut(1, gate_ticks);

            ++step;
        }
    }

protected:
    void SetHelp() {
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Pitch";
        help[HELP_CV2]      = "Capture";
        help[HELP_OUT1]     = "Pitch";
        help[HELP_OUT2]     = "Gate";
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
    }

private:
    int cursor;

    // Persisted
    uint8_t len;          // 3..16 inclusive
    uint8_t mode;
    uint8_t gate_pct;
    uint8_t apply_mode;   // 0=Immediate (zap), 1=Queued (clock)

    // Runtime (volatile)
    uint8_t buffer[shiftarp::kBufferCapacity];
    uint8_t buf_count;
    uint32_t step;
    uint8_t cur_idx;
    uint8_t last_midi;
    bool cv2_was_high;
    bool capture_pending;
    uint32_t click_tick = 0;

    // Queued-change pending state (applied at next cycle start).
    uint8_t pending_len_val;
    bool pending_len_valid;
    uint8_t pending_mode_val;
    bool pending_mode_valid;

    // BRW state: current walker position. Reset on Reset()/RESET trigger.
    uint8_t brownian_pos;

    // SHF state: random permutation of current N indices. Regenerated when
    // N changes or on Reset(). Not persisted across power cycles (fresh
    // order each session, matching the fresh-buffer philosophy).
    uint8_t shuffle_perm[shiftarp::kBufferCapacity];
    bool    shuffle_valid;
    uint8_t shuffle_N;

    // Modal buffer editor state. Not persisted (editor is session-only).
    bool    editing_buffer;
    uint8_t edit_pos;          // slot under cursor, 0..len-1
    bool    edit_pitch_mode;   // false = NAV (encoder moves slot), true = PITCH (encoder changes note)

    // Per-applet local quantizer — NOT shared with the global Hemisphere q_engine[].
    // Allows scale/root/octave/mask to live in the preset payload, independent of
    // (and unaffected by) other applets on the same side. Persists per-preset.
    braids::Quantizer local_q;
    uint8_t  local_scale;     // 0..NUM_SCALES-1
    uint8_t  local_root;      // 0..11 semitones (C..B)
    int8_t   local_octave;    // -3..+3 octave shift
    uint16_t local_mask;      // 16-bit bitmask of enabled scale degrees

    // Modal scale editor state.
    bool    editing_scale;
    uint8_t scale_edit_cursor;  // 0=Scale, 1=Root, 2=Octave, 3..18=Mask bits 0..15
    bool    scale_edit_pitch_mode;  // NAV vs EDIT

    // Choose which buffer slot to play at the current step, per playback mode.
    // Instance method (some modes carry state — brownian walk position,
    // shuffle permutation). Stateless part is the step counter.
    // Order matches kModeNames: Direction (0..3) → Patterned (4..6) → Generative (7..9).
    // Inline (in ITCM) — called from Controller every clock tick.
    uint8_t compute_idx(uint32_t step, uint8_t N, uint8_t mode) {
        if (N == 0) return 0;
        switch (mode) {
            case 0: return step % N;                                // FWD
            case 1: return (N - 1) - (step % N);                    // BCK
            case 2: {                                               // PND — Pendulum
                if (N <= 1) return 0;
                const int period = 2 * (N - 1);
                const int s = step % period;
                return (s < N) ? s : (2 * (N - 1) - s);
            }
            case 3: {                                               // PPG — Ping Pong
                const int period = 2 * N;
                const int s = step % period;
                return (s < N) ? s : (2 * N - 1 - s);
            }
            case 4: {                                               // PDL — Pedal Point
                if (N <= 1) return 0;
                return (step & 1)
                    ? (1 + ((step >> 1) % (N - 1)))
                    : 0;
            }
            case 5: {                                               // WIN — Shift Window
                if (N <= 1) return 0;
                const uint8_t W = shiftarp::kShiftWindow;
                if (N < W) return step % N;  // buffer smaller than window
                const uint32_t start  = (step / W) % N;
                const uint32_t offset = step % W;
                return (uint8_t)((start + offset) % N);
            }
            case 6: {                                               // PHI — Golden ratio
                // Weyl sequence — evenly distributed but non-linear,
                // gives AoD's "organic but non-random" feel.
                const uint32_t jump = 1 + ((uint32_t)N * 618) / 1000;
                return (step * jump) % N;
            }
            case 7: {                                               // SHF — Shuffle
                if (N <= 1) return 0;
                // Regenerate permutation on reset or when N changed.
                if (!shuffle_valid || shuffle_N != N) {
                    for (uint8_t i = 0; i < N; ++i) shuffle_perm[i] = i;
                    for (int i = N - 1; i > 0; --i) {
                        const int j = random(i + 1);
                        const uint8_t tmp = shuffle_perm[i];
                        shuffle_perm[i] = shuffle_perm[j];
                        shuffle_perm[j] = tmp;
                    }
                    shuffle_N = N;
                    shuffle_valid = true;
                }
                return shuffle_perm[step % N];
            }
            case 8: {                                               // BRW — Brownian 50/25/25
                if (N <= 1) return 0;
                if (step > 0) {
                    const int r = random(100);
                    if (r < 50)      brownian_pos = (brownian_pos + 1) % N;
                    else if (r < 75) { /* stay */ }
                    else             brownian_pos = (brownian_pos + N - 1) % N;
                }
                if (brownian_pos >= N) brownian_pos = 0;
                return brownian_pos;
            }
            case 9: return random(N);                               // RND
            default: return random(N);
        }
    }

    // Out-of-line UI helpers (FLASHMEM, see below).
    void OpenEditor();
    void DrawEditor();
    void DrawBuffer(uint8_t N, int y_top, int h);
    void ApplyQuantizerConfig();
    void OpenScaleEditor();
    void DrawScaleEditor();
};

// =============================================================================
// Out-of-line method definitions, marked FLASHMEM so they live in QSPI Flash
// instead of ITCM. Crucial for ITCM budget — without this the bodies inline
// into hemisphere_config.h's translation unit and burn ITCM.
// Only non-realtime methods go here. Controller() and compute_idx() stay
// inline above — they run on every clock tick.
// =============================================================================

FLASHMEM void ShiftArp::Start() {
    len = 4;
    mode = 0;
    gate_pct = 50;
    apply_mode = APPLY_QUEUED;
    step = 0;
    buf_count = 0;
    cur_idx = 0;
    last_midi = 60;
    cv2_was_high = false;
    capture_pending = false;
    pending_len_valid = false;
    pending_mode_valid = false;
    brownian_pos = 0;
    shuffle_valid = false;
    shuffle_N = 0;
    editing_buffer = false;
    edit_pos = 0;
    edit_pitch_mode = false;
    editing_scale = false;
    scale_edit_cursor = 0;
    scale_edit_pitch_mode = false;
    for (uint8_t i = 0; i < shiftarp::kBufferCapacity; ++i) buffer[i] = 60;

    // Local quantizer defaults: SEMI (chromatic pass-through), root C, no octave
    // shift, all degrees enabled. Independent from the global Hemisphere quantizer.
    local_q.Init();
    local_scale  = OC::Scales::SCALE_SEMI;
    local_root   = 0;
    local_octave = 0;
    local_mask   = 0xFFFF;
    ApplyQuantizerConfig();
}

FLASHMEM void ShiftArp::ApplyQuantizerConfig() {
    local_q.Configure(OC::Scales::GetScale(local_scale), local_mask);
    // Force the quantizer to drop its cached codeword so the next Process()
    // call recomputes with the new scale / mask / (and re-evaluates root and
    // transpose against fresh boundaries).
    local_q.Requantize();
}

FLASHMEM void ShiftArp::OpenScaleEditor() {
    editing_scale = true;
    scale_edit_cursor = 0;
    scale_edit_pitch_mode = false;
    enc_edit[hemisphere].isEditing = true;
}

// Modal scale editor: three rows — Scale, Root, Octave. Single click toggles
// NAV ↔ EDIT sub-mode; side button exits. Mask editing not wired (TBD).
//
// Cursor draws as a blinking underline. We don't use gfxCursor() because that
// renders an inverted block when EditMode() is true — and we keep EditMode true
// here so the side button routes via AuxButton.
FLASHMEM void ShiftArp::DrawScaleEditor() {
    static const char* const kRootNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    auto rightAlignedX = [](const char* s) { return 60 - (int)strlen(s) * 6; };

    // Cursor matches the main UI convention:
    //   NAV  — blinking underline below the value (selecting a field).
    //   EDIT — solid inversion of the value box (actively changing the value).
    // We don't use gfxCursor() because that would route through global
    // EditMode(), and we keep isEditing=true here so the side button routes
    // via AuxButton for the exit shortcut. Instead we mimic gfxCursor's two
    // visuals explicitly. The h_value parameter controls inversion height
    // (default 9 = text height; mask cells use 5).
    auto drawCursor = [&](int x, int y_underline, int w, int h_value = 9) {
        if (scale_edit_pitch_mode) {
            gfxInvert(x, y_underline - h_value, w, h_value);
        } else if (CursorBlink()) {
            gfxLine(x, y_underline, x + w - 1, y_underline);
            gfxPixel(x, y_underline - 1);
            gfxPixel(x + w - 1, y_underline - 1);
        }
    };

    const auto& scale = OC::Scales::GetScale(local_scale);
    const int N = scale.num_notes;

    // No NAV/EDIT header — cursor visual conveys the mode.
    // Cursor positions: 0=Scale, 1..N=Mask bits, N+1=Root, N+2=Octave.

    // Row Scale (top)
    gfxPrint(1, 15, "Scale");
    {
        const char* const name = OC::scale_names_short[local_scale];
        const int x = rightAlignedX(name);
        gfxPrint(x, 15, name);
        if (scale_edit_cursor == 0) drawCursor(x - 1, 24, 60 - x + 1);
    }

    // Row Mask (right-aligned, no label). Each cell 4×5 px, gap 1, total 5*N px.
    {
        const int cell_w = 5;        // 4 px cell + 1 px gap
        const int total = N * cell_w;
        const int x_start = 60 - total;  // right-align to col 60
        const int mask_y = 27;
        for (int i = 0; i < N; ++i) {
            const int x = x_start + i * cell_w;
            const bool on = (local_mask >> i) & 1;
            if (on) gfxRect(x, mask_y, cell_w - 1, 5);
            // off: nothing — empty cell.
        }
        if (scale_edit_cursor >= 1 && scale_edit_cursor <= N) {
            const int bit = scale_edit_cursor - 1;
            const int cx = x_start + bit * cell_w;
            // Inversion height = 5 (cell height) for EDIT mode.
            drawCursor(cx, mask_y + 6, cell_w - 1, 5);
        }
    }

    // Row Root
    gfxPrint(1, 39, "Root");
    {
        const char* const rname = kRootNames[local_root];
        const int x = rightAlignedX(rname);
        gfxPrint(x, 39, rname);
        if (scale_edit_cursor == N + 1) drawCursor(x - 1, 48, 60 - x + 1);
    }

    // Row Octave
    gfxPrint(1, 51, "Oct");
    {
        char buf[4];
        if      (local_octave > 0) snprintf(buf, sizeof(buf), "+%d", local_octave);
        else if (local_octave < 0) snprintf(buf, sizeof(buf), "%d",  local_octave);
        else                       snprintf(buf, sizeof(buf), " 0");
        const int x = rightAlignedX(buf);
        gfxPrint(x, 51, buf);
        if (scale_edit_cursor == N + 2) drawCursor(x - 1, 60, 60 - x + 1);
    }
}

FLASHMEM void ShiftArp::Reset() {
    step = 0;
    brownian_pos = 0;
    shuffle_valid = false;
}

FLASHMEM void ShiftArp::View() {
    if (editing_buffer) { DrawEditor(); return; }
    if (editing_scale)  { DrawScaleEditor(); return; }

    const uint8_t L = len;
    const uint8_t N = (buf_count < L) ? buf_count : L;

    // Row 1 layout: [LEN right-aligned in 12 px] [pend dot] [EDT icon] [...gap...] [MODE right-aligned to col 60]
    const uint8_t disp_len = pending_len_valid ? pending_len_val : len;
    const int len_digits = (disp_len < 10) ? 1 : 2;
    const int len_x = 12 - len_digits * 6;
    gfxPrint(len_x, 15, disp_len);
    if (pending_len_valid) gfxRect(13, 16, 2, 2);
    if (cursor == LEN) gfxCursor(0, 23, 13, 9, "Length");

    // EDT cursor — note icon, opens modal buffer editor on press.
    gfxIcon(17, 15, NOTE_ICON);
    if (cursor == EDT) gfxCursor(16, 23, 10, 9, "Edit");

    const uint8_t disp_mode = pending_mode_valid ? pending_mode_val : mode;
    const char* const mode_name = shiftarp::kModeNames[disp_mode];
    const int mode_w = (int)strlen(mode_name) * 6;
    const int mode_x = 60 - mode_w;
    gfxPrint(mode_x, 15, mode_name);
    if (pending_mode_valid) gfxRect(mode_x - 4, 16, 2, 2);
    if (cursor == MODE) gfxCursor(mode_x - 1, 23, mode_w + 1, 9, "Mode");

    // Row 2: buffer visualisation
    DrawBuffer(N, 26, 10);

    // Row 3: apply icon + scale 4-char + gate icon + gate %.
    gfxIcon(1, 39, apply_mode == APPLY_QUEUED ? CLOCK_ICON : ZAP_ICON);
    if (cursor == APPLY) gfxCursor(0, 47, 10, 9, "Apply");

    gfxPrint(10, 39, OC::scale_names_short[local_scale]);
    if (cursor == SCALE) gfxCursor(9, 47, 26, 9, "Scale");

    gfxIcon(36, 39, GATE_ICON);
    gfxPrint(44, 39, gate_pct);
    gfxPrint("%");
    {
        const int digits = (gate_pct < 10) ? 1 : 2;
        const int width = digits * 6 + 6;
        if (cursor == GATE_PCT) gfxCursor(43, 47, width + 1, 9, "Gt.len");
    }

    gfxLine(0, 50, 62, 50);

    // Status: current note + step counter
    if (N > 0) {
        gfxPrint(1, 54, midi_note_numbers[last_midi]);
        const int digits_idx = (cur_idx + 1 < 10) ? 1 : 2;
        const int digits_tot = (N < 10) ? 1 : 2;
        const int width = (digits_idx + 1 + digits_tot) * 6;
        const int x = 61 - width;
        gfxPrint(x, 54, cur_idx + 1);
        gfxPrint("/");
        gfxPrint(N);
    } else {
        gfxPrint(1, 54, "empty");
    }
}

// UX:
//   - In a modal (buffer or scale editor):
//       single click  — toggle NAV ↔ EDIT
//       double click  — no special meaning (does NOT wipe the buffer)
//       side button   — exit the modal (AuxButton route, reliable, no timing)
//   - Outside any modal:
//       single click  — toggle main-cursor edit, or open modal on EDT/SCALE
//       double click  — clear the buffer
FLASHMEM void ShiftArp::OnButtonPress() {
    // In-modal clicks: only toggle. No double-click handling here so that
    // accidental fast clicks don't wipe the buffer or take unexpected action.
    if (editing_buffer) { edit_pitch_mode = !edit_pitch_mode; return; }
    if (editing_scale) {
        // On a mask bit: click directly toggles the bit (no NAV/EDIT step).
        const int N = OC::Scales::GetScale(local_scale).num_notes;
        if (scale_edit_cursor >= 1 && scale_edit_cursor <= N) {
            const int bit = scale_edit_cursor - 1;
            const uint16_t bm = (uint16_t)(1 << bit);
            if (local_mask & bm) {
                // Refuse to disable the last enabled bit (would silence the quantizer).
                const uint16_t scale_full = (uint16_t)((1u << N) - 1);
                if ((local_mask & ~bm & scale_full) != 0) local_mask &= ~bm;
            } else {
                local_mask |= bm;
            }
            ApplyQuantizerConfig();
        } else {
            scale_edit_pitch_mode = !scale_edit_pitch_mode;
        }
        return;
    }

    // Outside modal: double-click clears buffer (works on every cursor field).
    if (OC::CORE::ticks - click_tick < HS::HEMISPHERE_DOUBLE_CLICK_TIME) {
        buf_count = 0;
        step = 0;
        cur_idx = 0;
        click_tick = 0;
        return;
    }
    click_tick = OC::CORE::ticks;

    if (cursor == EDT)   { OpenEditor(); return; }
    if (cursor == SCALE) { OpenScaleEditor(); return; }
    CursorToggle();
}

// AuxButton fires when a side button is pressed while the applet is in
// EditMode. We use this as a reliable "exit modal" gesture (no timing).
FLASHMEM void ShiftArp::AuxButton() {
    if (editing_buffer) {
        editing_buffer = false;
        edit_pitch_mode = false;
        enc_edit[hemisphere].isEditing = false;
        return;
    }
    if (editing_scale) {
        editing_scale = false;
        scale_edit_pitch_mode = false;
        enc_edit[hemisphere].isEditing = false;
        return;
    }
    CancelEdit();
}

FLASHMEM void ShiftArp::OnEncoderMove(int direction) {
    if (editing_scale) {
        const int N = OC::Scales::GetScale(local_scale).num_notes;
        const int cursor_max = 2 + N;  // 0=Scale, 1..N=Mask, N+1=Root, N+2=Oct

        if (!scale_edit_pitch_mode) {
            // NAV: move cursor with wrap.
            scale_edit_cursor = (uint8_t)(((int)scale_edit_cursor + direction + (cursor_max + 1)) % (cursor_max + 1));
            return;
        }
        // EDIT: change current value.
        if (scale_edit_cursor == 0) {
            // Scale change — adjust cursor if it lies past the new num_notes range.
            local_scale = constrain((int)local_scale + direction, 0, OC::Scales::NUM_SCALES - 1);
            const int new_max = 2 + OC::Scales::GetScale(local_scale).num_notes;
            if (scale_edit_cursor > new_max) scale_edit_cursor = new_max;
        } else if (scale_edit_cursor >= 1 && scale_edit_cursor <= N) {
            // Mask bit toggle. Any encoder movement flips the bit. Refuse to
            // turn off the last enabled bit so the quantizer never goes silent.
            if (direction != 0) {
                const int bit = scale_edit_cursor - 1;
                const uint16_t bm = (uint16_t)(1 << bit);
                if (local_mask & bm) {
                    const uint16_t scale_full = (uint16_t)((1u << N) - 1);
                    if ((local_mask & ~bm & scale_full) != 0) local_mask &= ~bm;
                    // else: this is the last enabled bit — keep it on.
                } else {
                    local_mask |= bm;
                }
            }
        } else if (scale_edit_cursor == N + 1) {
            local_root = (uint8_t)(((int)local_root + direction + 12) % 12);
        } else if (scale_edit_cursor == N + 2) {
            local_octave = constrain((int)local_octave + direction, -3, 3);
        }
        ApplyQuantizerConfig();
        return;
    }
    if (editing_buffer) {
        if (!edit_pitch_mode) {
            edit_pos = (uint8_t)(((int)edit_pos + direction + len) % len);
        } else {
            // Lazy expansion: if editing a slot beyond current buf_count,
            // fill the gap with rests and grow buf_count to include it.
            const bool inactive = (edit_pos >= buf_count);
            if (inactive) {
                for (int i = buf_count; i < edit_pos; ++i) buffer[i] = shiftarp::kRest;
                buf_count = edit_pos + 1;
            }
            const uint8_t cur = inactive ? shiftarp::kRest : buffer[edit_pos];
            int next;
            if (cur == shiftarp::kRest) {
                if (direction > 0) next = 0;     // first turn up out of rest = C-1
                else               next = -1;    // stay rest (or stay rest if was inactive)
            } else {
                next = (int)cur + direction;
            }
            if (next < 0) buffer[edit_pos] = shiftarp::kRest;
            else if (next > 127) buffer[edit_pos] = 127;
            else buffer[edit_pos] = (uint8_t)next;
        }
        return;
    }
    if (!EditMode()) {
        MoveCursor(cursor, direction, CURSOR_LAST);
        return;
    }
    switch ((ShiftArpCursor)cursor) {
        case LEN: {
            const int cur = pending_len_valid ? pending_len_val : len;
            const int nv  = constrain(cur + direction, shiftarp::kLenMin, shiftarp::kLenMax);
            if (apply_mode == APPLY_IMMEDIATE) {
                len = nv;
                pending_len_valid = false;
            } else if (nv == len) {
                pending_len_valid = false;
            } else {
                pending_len_val = nv;
                pending_len_valid = true;
            }
            break;
        }
        case MODE: {
            const int cur = pending_mode_valid ? pending_mode_val : mode;
            const int n   = shiftarp::kNumModes;
            const int nv  = ((cur + direction) % n + n) % n;
            if (apply_mode == APPLY_IMMEDIATE) {
                mode = nv;
                pending_mode_valid = false;
            } else if (nv == mode) {
                pending_mode_valid = false;
            } else {
                pending_mode_val = nv;
                pending_mode_valid = true;
            }
            break;
        }
        case GATE_PCT: gate_pct = constrain((int)gate_pct + direction, 1, 99); break;
        case APPLY:    apply_mode = constrain((int)apply_mode + direction, 0, 1); break;
        default: break;
    }
}

// Open the modal buffer editor. Doesn't touch buf_count — playback stays
// the same until the user actually edits an inactive slot (lazy expansion).
// Sets EditMode so AuxButton (side button) can be used as exit shortcut.
FLASHMEM void ShiftArp::OpenEditor() {
    if (edit_pos >= len) edit_pos = 0;
    edit_pitch_mode = false;
    editing_buffer = true;
    enc_edit[hemisphere].isEditing = true;
}

// Modal editor: takes over the full applet side (64×64). Bar graph of all
// `len` slots. Active slot has a cursor frame. Header shows mode + index +
// note name (or "rest").
FLASHMEM void ShiftArp::DrawEditor() {
    const uint8_t L = len;
    if (L == 0) { gfxPrint(1, 15, "empty"); return; }

    // Mode icon: ↔ for NAV (cursor moves between slots), pencil for PITCH edit.
    gfxIcon(1, 13, edit_pitch_mode ? EDIT_ICON : LEFT_RIGHT_ICON);

    // Counter right-aligned to col 60. Width depends on digit counts.
    const int d_pos = (edit_pos + 1 < 10) ? 1 : 2;
    const int d_len = (L < 10) ? 1 : 2;
    const int counter_x = 60 - (d_pos + 1 + d_len) * 6;
    gfxPrint(counter_x, 14, (int)(edit_pos + 1));
    gfxPrint("/");
    gfxPrint((int)L);

    if (edit_pos >= buf_count) {
        gfxPrint(1, 24, "- empty -");
    } else {
        const uint8_t slot = buffer[edit_pos];
        if (slot == shiftarp::kRest) {
            gfxPrint(1, 24, "- rest -");
        } else {
            gfxPrint(1, 24, midi_note_numbers[slot]);
        }
    }

    // Bar graph: heights normalised to the min/max of the active (non-rest)
    // notes so small pitch differences stay visible — same idea as the main
    // view's DrawBuffer. Rests sit as a thin line at the bottom; inactive
    // slots get a single mid-height dot.
    uint8_t lo = 127, hi = 0;
    bool any_note = false;
    for (uint8_t i = 0; i < buf_count; ++i) {
        const uint8_t v = buffer[i];
        if (v == shiftarp::kRest) continue;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        any_note = true;
    }
    if (!any_note) { lo = 60; hi = 60; }  // fallback for all-rest / empty
    const int span = (hi > lo) ? (hi - lo) : 1;

    const int y_top = 34;
    const int y_bot = 62;
    const int h = y_bot - y_top;  // 28
    const int w = 63 / L;
    for (uint8_t i = 0; i < L; ++i) {
        const int x = i * w;
        if (i >= buf_count) {
            gfxPixel(x + (w - 1) / 2, (y_top + y_bot) / 2);
        } else {
            const uint8_t v = buffer[i];
            if (v == shiftarp::kRest) {
                gfxLine(x, y_bot, x + w - 2, y_bot);
            } else {
                const int bar_h = 1 + ((int)(v - lo) * (h - 1)) / span;
                gfxRect(x, y_bot - bar_h, w - 1, bar_h);
            }
        }
        if (i == edit_pos) {
            gfxFrame(x - 1, y_top, w + 1, h + 1);
        }
    }
}

// Buffer visualisation — tiny bar graph of note values. Used in the main view
// (Row 2). Bar heights normalised to the min/max of the active window so small
// pitch differences are visible. Playhead is framed.
FLASHMEM void ShiftArp::DrawBuffer(uint8_t N, int y_top, int h) {
    if (N == 0) {
        gfxPrint(1, y_top + 1, "- empty -");
        return;
    }

    uint8_t lo = 127, hi = 0;
    for (uint8_t i = 0; i < N; ++i) {
        // Skip rest slots when computing range.
        if (buffer[i] == shiftarp::kRest) continue;
        if (buffer[i] < lo) lo = buffer[i];
        if (buffer[i] > hi) hi = buffer[i];
    }
    if (lo > hi) { lo = 60; hi = 60; }  // all-rest fallback
    const int span = (hi > lo) ? (hi - lo) : 1;

    const int w = 63 / N;
    for (uint8_t i = 0; i < N; ++i) {
        const int x = i * w;
        const uint8_t v = buffer[i];
        if (v != shiftarp::kRest) {
            const int bar_h = 1 + ((int)(v - lo) * (h - 1)) / span;
            const int y = y_top + h - bar_h;
            gfxRect(x, y, w - 1, bar_h);
        } else {
            // Rest mark: thin line at the bottom of the row.
            gfxLine(x, y_top + h - 1, x + w - 2, y_top + h - 1);
        }
        if (i == cur_idx) {
            gfxFrame(x - 1, y_top - 1, w + 1, h + 2);
        }
    }
}

// Persistence: base 64-bit payload holds settings + buf_count + local quantizer.
// Two extra SetData/GetData blobs (keys 0 and 1) hold the 16-byte note buffer.
// Empty buffer (buf_count=0, all blobs zero) is a valid persisted state.
//
// Layout (52 bits used, 12 bits free for future):
//   0..4   len           (5 bit)
//   5..7   mode          (3 bit)
//   8..14  gate_pct      (7 bit)
//   15     apply_mode    (1 bit)
//   16..20 buf_count     (5 bit)
//   21..28 local_scale   (8 bit)
//   29..32 local_root    (4 bit)
//   33..35 octave_stored (3 bit, = local_octave + 3, range 0..6 → -3..+3)
//   36..51 local_mask    (16 bit)
FLASHMEM uint64_t ShiftArp::OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation{0, 5},   len);
    Pack(data, PackLocation{5, 3},   mode);
    Pack(data, PackLocation{8, 7},   gate_pct);
    Pack(data, PackLocation{15, 1},  apply_mode);
    Pack(data, PackLocation{16, 5},  buf_count);
    Pack(data, PackLocation{21, 8},  local_scale);
    Pack(data, PackLocation{29, 4},  local_root);
    // 3-bit signed (range -3..+3): bit 2 = sign. 0 = 0 → default-friendly.
    Pack(data, PackLocation{33, 3},  (uint8_t)(local_octave & 0x7));
    Pack(data, PackLocation{36, 16}, local_mask);

    // Two extra blobs hold the 16-byte note buffer (notes 0..127 + kRest=0xFF
    // sentinel): key 0 = buffer[0..7], key 1 = buffer[8..15], each byte at bit
    // offset i*8. SetData is automatically scoped per preset/slot by the base.
    uint64_t buf_lo = 0, buf_hi = 0;
    for (int i = 0; i < 8; ++i) {
        buf_lo |= ((uint64_t)buffer[i])     << (i * 8);
        buf_hi |= ((uint64_t)buffer[8 + i]) << (i * 8);
    }
    SetData(0, buf_lo);
    SetData(1, buf_hi);

    return data;
}

FLASHMEM void ShiftArp::OnDataReceive(uint64_t data) {
    if (data == 0) {
        // Empty payload — preset never saved with this applet, or saved by an
        // older firmware that doesn't understand our layout. Either way, do
        // not overwrite the Start() defaults (SEMI scale, C root, oct 0,
        // mask all-on). Buffer and buf_count likewise stay at fresh state.
        return;
    }
    len         = constrain((int)Unpack(data, PackLocation{0, 5}),  shiftarp::kLenMin, shiftarp::kLenMax);
    mode        = constrain((int)Unpack(data, PackLocation{5, 3}),  0, shiftarp::kNumModes - 1);
    gate_pct    = constrain((int)Unpack(data, PackLocation{8, 7}),  1, 99);
    apply_mode  = constrain((int)Unpack(data, PackLocation{15, 1}), 0, 1);
    buf_count   = constrain((int)Unpack(data, PackLocation{16, 5}), 0, (int)shiftarp::kBufferCapacity);
    local_scale = constrain((int)Unpack(data, PackLocation{21, 8}), 0, OC::Scales::NUM_SCALES - 1);
    local_root  = constrain((int)Unpack(data, PackLocation{29, 4}), 0, 11);
    // Sign-extend 3-bit signed value: bit 2 high → negative.
    const int oct_stored = Unpack(data, PackLocation{33, 3});
    local_octave = constrain((oct_stored & 0x4) ? (oct_stored - 8) : oct_stored, -3, 3);
    local_mask  = (uint16_t)Unpack(data, PackLocation{36, 16});
    if (local_mask == 0) local_mask = 0xFFFF;  // safety: never silence everything
    ApplyQuantizerConfig();

    // Restore the 16-byte note buffer from the two extra blobs (see OnDataRequest).
    // GetData returns false for a preset saved before these blobs existed; in that
    // case the buffer keeps its Start() defaults and buf_count (above) is the guard
    // against reading stale slots. Each byte sits at bit offset i*8.
    uint64_t buf_lo = 0, buf_hi = 0;
    if (GetData(0, buf_lo)) {
        for (int i = 0; i < 8; ++i) buffer[i] = (uint8_t)((buf_lo >> (i * 8)) & 0xFF);
    }
    if (GetData(1, buf_hi)) {
        for (int i = 0; i < 8; ++i) buffer[8 + i] = (uint8_t)((buf_hi >> (i * 8)) & 0xFF);
    }
}
