// Copyright (c) 2026, Daniel Gorgan
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

class CVSeq: public HemisphereApplet {
public:
    static constexpr int MAX_CV_VALUES = 32;
    static constexpr int MAX_STEPS = 64;
    static constexpr int CV_INCREMENT = 64;

    enum CVSeqCursor {
        IN2_MODE,
        OUT2_MODE,
        QSELECT,
        TRANS_MODE,
        RESET,
        RANDOM,
        LOOP1,
        LOOP2,
        LOOP_FLIP,

        VALUE_INDEX,
        VALUE_CV,
        VALUE_NOTE,
        VALUE_ZERO,
        VALUE_COPY,
        VALUE_PASTE,

        CHANNEL_INDEX,
        CHANNEL_STEPS,
        CHANNEL_STEP,
        CHANNEL_VALUE,
        CHANNEL_LOOP,
        CHANNEL_CLOCKS,

        GENERAL_COPY_0_TO_1,
        GENERAL_COPY_1_TO_0,

        MAX_CURSOR,
    };

    enum RandomCursor {
        RANDOM_CH1, // Apply random values to channel 1.
        RANDOM_CH2, // Apply random values to channel 2.
        RANDOM_VALUES, // Generate random values.
        RANDOM_TRANSPOSE, // Transpose some values by a random interval.
        RANDOM_STEPS, // Generate random steps (lengths and positions).
        RANDOM_CANCEL,
        RANDOM_ZERO,

        RANDOM_INIT,

        MAX_RANDOM_CURSOR,
    };

    enum Input2Mode {
        CV2_IN, // Use CV2 as input for second channel.
        CV1_TRANSPOSE, // Use CV2 as modulation for CV1 (before quantization).

        MAX_INPUT2_MODE,
    };
    static constexpr const char* const input2_mode_names[MAX_INPUT2_MODE] = {
        "CV2", "Trnsp",
    };

    // Output mode for CV2.
    enum Output2Mode : uint8_t {
        CV2, // Output processed CV2.
        CV1_2OCT_DOWN, // Output CV1 two octaves down.
        CV1_1OCT_DOWN, // Output CV1 one octave down.
        CV1, // Same output as CV1.
        CV1_1OCT_UP, // Output CV1 one octave up.
        CV1_2OCT_UP, // Output CV1 two octaves up.
        CV1_GATE, // Output a gate when CV1 starts a new sequence step.
        CV1_IN, // Output the CV1 input directly.

        MAX_OUTPUT2_MODE,
    };
    static constexpr const char* const output2_mode_names[MAX_OUTPUT2_MODE] = {
        "CV2", "CV1-2", "CV1-1", "CV1", "CV1+1", "CV1+2", "Gate", "CV1in",
    };

    const char * applet_name() {
        return "CVSeq";
    }
    const uint8_t* applet_icon() { return PhzIcons::stairs; }

    void Start() override {
        qselect = io_offset;
        init_steps();
        Reset();
    }

    void Reset() override {
        reset_flag = true;
    }

    void Controller() override {
        const uint32_t this_tick = OC::CORE::ticks;

        transpose_amt = input2_mode == Input2Mode::CV1_TRANSPOSE ? SemitoneIn(1) : 0;

        if (Clock(1)) Reset();

        const bool clocked = Clock(0);
        bool half_boundary = false;
        if (clocked) {
            clock_ticks = ClockCycleTicks(0);
            last_clock_tick = this_tick;
            half_fired = false;
        } else if (!half_fired && clock_ticks > 0 && (this_tick - last_clock_tick) >= clock_ticks / 2) {
            // We are at the half-clock boundary.
            half_boundary = true;
            half_fired = true;
        }

        bool new_ch0_step = false;
        if (reset_flag) {
            // Latched reset: freeze playback until the next whole clock, then snap to start.
            if (clocked) {
                do_reset();
                new_ch0_step = true;
            }
        } else if (clocked || half_boundary) {
            // Both whole- and half-clock events advance the half-clock counter by 1.
            bar_half_clocks++;
            for (uint8_t ch = 0; ch < 2; ch++) {
                // Capture the pre-tick step so we can decide new_step after any
                // engage/disengage retargeting has settled.
                const uint8_t prev_step = live(ch).step;

                // Always advance the live sequencer first; this also drives the
                // pre-engage no-loop tick at an engage clock (loop_active is
                // still false at that point, so the advance is no-loop).
                advance_seq(ch, live(ch), loop_active[ch]);

                // Advance the shadow whenever a loop is driving playback.
                if (loop_active[ch]) advance_seq(ch, shadow(ch), false);
                // Phantom always advances — it's the phase-locked reference for
                // outside-engages, anchored at sequence reset.
                advance_seq(ch, phantom(ch), true);

                // Handle pending jumps on whole-clock events. By this point all
                // three sequencers have already taken their tick, so we're
                // working with post-tick positions.
                if (clocked) {
                    if (pending_loop[ch] == LP_ENTER) {
                        shadow(ch) = live(ch);

                        if (loops[ch].contains(live(ch).step)) {
                            // Stay where we are; just retarget under loop semantics.
                            retarget_step(ch, live(ch), true);
                        } else {
                            // Snap to phantom's current phase-locked position.
                            live(ch) = phantom(ch);
                        }
                        loop_active[ch] = true;
                        pending_loop[ch] = LP_NONE;
                    } else if (pending_loop[ch] == LP_EXIT) {
                        live(ch) = shadow(ch);
                        loop_active[ch] = false;
                        pending_loop[ch] = LP_NONE;
                    }
                }

                if (ch == 0 && live(ch).step != prev_step) new_ch0_step = true;
            }
        }

        if (channel_follow) step_index = live(channel_index).step;

        write_outputs(new_ch0_step);
    }

    void View() override {
        draw_interface();
    }

    void OnButtonPress() override {
        if (random_menu_active) {
            switch (random_menu_cursor.cursor_pos()) {
                case RandomCursor::RANDOM_CH1:
                    random_channels[0] ^= true;
                    break;
                case RandomCursor::RANDOM_CH2:
                    random_channels[1] ^= true;
                    break;
                case RandomCursor::RANDOM_VALUES:
                    random_values();
                    random_menu_active = false;
                    break;
                case RandomCursor::RANDOM_TRANSPOSE:
                    random_transpose();
                    random_menu_active = false;
                    break;
                case RandomCursor::RANDOM_STEPS:
                    random_steps();
                    Reset();
                    random_menu_active = false;
                    break;
                case RandomCursor::RANDOM_CANCEL:
                    random_menu_active = false;
                    break;
                case RandomCursor::RANDOM_ZERO:
                    zero_steps();
                    Reset();
                    random_menu_active = false;
                    break;
                case RandomCursor::RANDOM_INIT:
                    init_steps();
                    Reset();
                    random_menu_active = false;
                    break;
            }

            return;
        }

        if (cursor == CVSeqCursor::RESET) {
            Reset();
        } else if (cursor == CVSeqCursor::RANDOM) {
            // Open random view.
            random_menu_active = true;
            random_menu_cursor.Init(0, RandomCursor::MAX_RANDOM_CURSOR - 1);
            random_menu_cursor.Scroll(RandomCursor::RANDOM_CANCEL);
        } else if (cursor == CVSeqCursor::TRANS_MODE) {
            transpose_in_semitones ^= true;
        } else if (cursor == CVSeqCursor::LOOP1) {
            loops[0].enabled ^= true;
            on_loop_toggle(0);
        } else if (cursor == CVSeqCursor::LOOP2) {
            loops[1].enabled ^= true;
            on_loop_toggle(1);
        } else if (cursor == CVSeqCursor::LOOP_FLIP) {
            loops[0].enabled ^= true;
            loops[1].enabled ^= true;
            on_loop_toggle(0);
            on_loop_toggle(1);
        } else if (cursor == CVSeqCursor::VALUE_ZERO) {
            cv_values[cv_value_index] = 0;
        } else if (cursor == CVSeqCursor::VALUE_COPY) {
            cv_value_clipboard = cv_values[cv_value_index];
        } else if (cursor == CVSeqCursor::VALUE_PASTE) {
            cv_values[cv_value_index] = cv_value_clipboard;
        } else if (cursor == CVSeqCursor::CHANNEL_LOOP) {
            // Replace whichever point is closer to step_index. Naturally
            // extends when outside, contracts the nearer side when inside,
            // and collapses to a single step when step_index sits on a point
            // (in which case we move the *other* marker, since snapping the
            // one already on this step would be a no-op).
            const int d0 = abs((int)step_index - (int)loops[channel_index].points[0]);
            const int d1 = abs((int)step_index - (int)loops[channel_index].points[1]);
            const uint8_t which = (d0 == 0) ? 1 : (d1 == 0 ? 0 : (d1 < d0 ? 1 : 0));
            loops[channel_index].points[which] = step_index;
            on_loop_points_changed(channel_index);
        } else if (cursor == CVSeqCursor::GENERAL_COPY_0_TO_1) {
            copy_channel(0, 1);
        } else if (cursor == CVSeqCursor::GENERAL_COPY_1_TO_0) {
            copy_channel(1, 0);
        } else {
            if (EditMode()) {
                cv_value_select = step_select = false;
            }

            CursorToggle();
        }
    }

    void CancelEdit() {
        cv_value_select = step_select = false;
        HemisphereApplet::CancelEdit();
    }

    void AuxButton() override {
        if (random_menu_active) {
            // Just exit random menu on aux button.
            random_menu_active = false;
        } else if (cursor == CVSeqCursor::QSELECT) {
            // Open quantizer selection.
            HS::QuantizerEdit(qselect);
        } else if (cursor == CVSeqCursor::VALUE_CV || cursor == CVSeqCursor::VALUE_NOTE) {
            // Toggle between selecting CV value or selecting which CV value to edit.
            cv_value_select ^= true;
        } else if (cursor == CVSeqCursor::CHANNEL_INDEX) {
            // Toggle following the channel's current step in the live sequencer.
            channel_follow ^= true;
        } else if (cursor > CVSeqCursor::CHANNEL_INDEX && cursor <= CVSeqCursor::CHANNEL_CLOCKS) {
            // Toggle between selecting step or selecting which step to edit.
            step_select ^= true;
        } else CancelEdit();
    }

    void OnEncoderMove(int direction) override {
        if (random_menu_active) {
            random_menu_cursor.Scroll(direction);
            return;
        }

        if (!EditMode()) {
            MoveCursor(cursor, direction, CVSeqCursor::MAX_CURSOR - 1);
            return;
        }

        if (cv_value_select) {
            // If selecting which CV value to edit, just move the index.
            cv_value_index = constrain(cv_value_index + direction, 0, MAX_CV_VALUES - 1);
            return;
        }

        if (step_select) {
            // If selecting which step to edit, just move the index.
            step_index = constrain(step_index + direction, 0, MAX_STEPS - 1);
            return;
        }

        // Edit parameter.
        switch (cursor) {
        case CVSeqCursor::IN2_MODE:
            input2_mode = (Input2Mode)constrain(input2_mode + direction, 0, Input2Mode::MAX_INPUT2_MODE - 1);
            break;
        case CVSeqCursor::OUT2_MODE:
            output2_mode = (Output2Mode)constrain(output2_mode + direction, 0, Output2Mode::MAX_OUTPUT2_MODE - 1);
            break;
        case CVSeqCursor::QSELECT:
            // Encoder ramp is OFF, Q1, Q2, … — OFF sits one click below Q1.
            if (quantize_off) {
                if (direction > 0) quantize_off = false;
            } else if (qselect == 0 && direction < 0) {
                quantize_off = true;
            } else {
                qselect = constrain(qselect + direction, 0, QUANT_CHANNEL_COUNT - 1);
            }
            break;
        case CVSeqCursor::VALUE_INDEX:
            cv_value_index = constrain(cv_value_index + direction, 0, MAX_CV_VALUES - 1);
            break;
        case CVSeqCursor::VALUE_CV:
            cv_values[cv_value_index] = constrain(cv_values[cv_value_index] + direction, -127, 127);
            break;
        case CVSeqCursor::VALUE_NOTE:
            cv_values[cv_value_index] = constrain(((cv_values[cv_value_index] >> 1) + direction) * 2, -127, 127);
            break;
        case CVSeqCursor::CHANNEL_INDEX:
            channel_index = constrain(channel_index + direction, 0, 1);
            break;
        case CVSeqCursor::CHANNEL_STEPS:
            num_steps[channel_index] = constrain(num_steps[channel_index] + direction, 1, MAX_STEPS);
            break;
        case CVSeqCursor::CHANNEL_STEP:
            step_index = constrain(step_index + direction, 0, MAX_STEPS - 1);
            break;
        case CVSeqCursor::CHANNEL_VALUE:
            steps[channel_index][step_index].value = constrain(steps[channel_index][step_index].value + direction, 0, MAX_CV_VALUES - 1);
            break;
        case CVSeqCursor::CHANNEL_CLOCKS:
            steps[channel_index][step_index].clocks = constrain(steps[channel_index][step_index].clocks + direction, 0, 31);
            break;
        }
    }

    uint64_t OnDataRequest() override {
        uint64_t data = 0;

        uint8_t* p = (uint8_t*)cv_values;
        size_t i = 0;
        while (i < sizeof(cv_values) + sizeof(steps)) {
            if (i == sizeof(cv_values)) p = (uint8_t*)steps;
            Pack(data, PackLocation{(i % 8) * 8, 8}, *p++);
            if ((i % 8) == 7) {
                SetData(i / 8, data);
                data = 0;
            }
            ++i;
        }
        if (i % 8 != 0) SetData(i / 8, data);

        data = 0;
        Pack(data, PackLocation {0, 2}, input2_mode);
        Pack(data, PackLocation {2, 3}, output2_mode);
        Pack(data, PackLocation {5, 3}, qselect);
        Pack(data, PackLocation {8, 1}, quantize_off);
        Pack(data, PackLocation {9, 1}, transpose_in_semitones);
        Pack(data, PackLocation {10, 1}, random_channels[0]);
        Pack(data, PackLocation {11, 1}, random_channels[1]);
        Pack(data, PackLocation {12, 6}, num_steps[0] - 1);
        Pack(data, PackLocation {18, 6}, num_steps[1] - 1);
        Pack(data, PackLocation {24, 6}, loops[0].points[0]);
        Pack(data, PackLocation {30, 6}, loops[0].points[1]);
        Pack(data, PackLocation {36, 1}, loops[0].enabled);
        Pack(data, PackLocation {37, 6}, loops[1].points[0]);
        Pack(data, PackLocation {43, 6}, loops[1].points[1]);
        Pack(data, PackLocation {49, 1}, loops[1].enabled);

        return data;
    }

    void OnDataReceive(uint64_t data) override {
        input2_mode = (Input2Mode)constrain(Unpack(data, PackLocation {0, 2}), 0, Input2Mode::MAX_INPUT2_MODE - 1);
        output2_mode = (Output2Mode)constrain(Unpack(data, PackLocation {2, 3}), 0, Output2Mode::MAX_OUTPUT2_MODE - 1);
        qselect = constrain(Unpack(data, PackLocation {5, 3}), 0, QUANT_CHANNEL_COUNT - 1);
        quantize_off = Unpack(data, PackLocation {8, 1});
        transpose_in_semitones = Unpack(data, PackLocation {9, 1});
        random_channels[0] = Unpack(data, PackLocation {10, 1});
        random_channels[1] = Unpack(data, PackLocation {11, 1});
        num_steps[0] = constrain(Unpack(data, PackLocation {12, 6}) + 1, 1, MAX_STEPS);
        num_steps[1] = constrain(Unpack(data, PackLocation {18, 6}) + 1, 1, MAX_STEPS);
        loops[0].points[0] = constrain(Unpack(data, PackLocation {24, 6}), 0, MAX_STEPS - 1);
        loops[0].points[1] = constrain(Unpack(data, PackLocation {30, 6}), 0, MAX_STEPS - 1);
        loops[0].enabled = Unpack(data, PackLocation {36, 1});
        loops[1].points[0] = constrain(Unpack(data, PackLocation {37, 6}), 0, MAX_STEPS - 1);
        loops[1].points[1] = constrain(Unpack(data, PackLocation {43, 6}), 0, MAX_STEPS - 1);
        loops[1].enabled = Unpack(data, PackLocation {49, 1});

        uint8_t* p = (uint8_t*)cv_values;
        size_t i = 0;
        while (i < sizeof(cv_values) + sizeof(steps)) {
            if (i == sizeof(cv_values)) p = (uint8_t*)steps;
            if ((i % 8) == 0 && !GetData(i / 8, data)) break;
            *p++ = Unpack(data, PackLocation{(i % 8) * 8, 8});
            ++i;
        }

        // Suppress any half-boundary event from stale clock state, then reset
        // everything immediately so playback starts cleanly from the new preset.
        clock_ticks = 0;
        half_fired = true;
        do_reset();
    }

protected:
    void SetHelp() override {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "CV1";
        help[HELP_CV2]      = input2_mode_names[input2_mode];
        help[HELP_OUT1]     = "CV1";
        help[HELP_OUT2]     = output2_mode_names[output2_mode];
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor = 0;

    uint32_t clock_ticks = 0; // Ticks between the last two clock inputs.
    uint32_t last_clock_tick = 0; // Tick of last Clock(0) rising edge.
    bool half_fired = true; // Mid-clock half-boundary already emitted this cycle.
    uint8_t bar_half_clocks = 0; // Independent bar position counter, advances on every whole- and half-clock event (8 ticks = one 4-clock bar).
    uint8_t num_steps[2] = {1, 1}; // Number of steps in the sequence for each channel.

    enum SeqRole : uint8_t { SEQ_LIVE, SEQ_SHADOW, SEQ_PHANTOM, SEQ_COUNT };
    struct SeqState {
        uint8_t step = 0;
        uint8_t half_clocks = 0;
        uint8_t target_half_clocks = 2;
        uint8_t run_count = 0;
    };
    // live: what you hear; shadow: where we'd be without the loop (resume point on exit);
    // phantom: always plays the loop region, phase-locked from reset (engage reference).
    SeqState seq[2][SEQ_COUNT];

    bool loop_active[2] = {false, false}; // Whether the loop is currently driving playback (lags loops[].enabled by one clock).
    enum LoopPending : uint8_t { LP_NONE, LP_ENTER, LP_EXIT }; // Latched action for the next Clock(0): enter the loop, exit to shadow, or do nothing.
    LoopPending pending_loop[2] = {LP_NONE, LP_NONE};

    struct Step {
        uint8_t value = 0; // Index from `cv_values`.
        uint8_t clocks = 4; // Number of clocks this step lasts for.
    };

    struct Loop {
        uint8_t points[2] = {0, 0}; // Two unordered markers; the loop region is [lo(), hi()].
        bool enabled = false; // Whether the loop is enabled.
        uint8_t lo() const { return points[0] < points[1] ? points[0] : points[1]; }
        uint8_t hi() const { return points[0] < points[1] ? points[1] : points[0]; }
        bool contains(uint8_t step) const { return step >= lo() && step <= hi(); }
    };

    Input2Mode input2_mode = Input2Mode::CV2_IN; // What CV2 input is used for.
    Output2Mode output2_mode = Output2Mode::CV2; // What CV2 outputs.
    int qselect = io_offset; // Quantizer channel selection for both channels, since they share the quantizer.
    bool quantize_off = false; // When true, skip quantization entirely (sits one click below Q1 in the encoder ramp).
    bool transpose_in_semitones = false; // Whether CV1 transpose (or root note if scale is off) is in semitones or scale degrees.
    int32_t transpose_amt = 0; // Always in semitones; quantized into a scale degree when transpose_in_semitones is false.
    bool reset_flag = false; // Latched: do_reset() runs on the next Clock(0).
    bool random_menu_active = false; // Whether the random menu is active.
    OC::menu::ScreenCursor<5> random_menu_cursor; // Cursor for the random menu.
    bool random_channels[2] = {true, true}; // Randomization settings for each channel.
    int8_t cv_values[MAX_CV_VALUES] = {}; // CV values available for the sequence, in half-semitone units (-127..127). At output, (v >> 1) whole semitones go through the quantizer and (v & 1) adds a chromatic 50¢ residue (CV_INCREMENT DAC units).
    uint8_t cv_value_index = 0; // The index of CV value currently being edited, from the cv_values array.
    bool cv_value_select = false; // Whether the CV value is being selected with the AUX button.
    int8_t cv_value_clipboard = 0; // A clipboard for copying and pasting CV values.
    uint8_t channel_index = 0; // The currently selected channel for editing.
    bool channel_follow = false; // Whether to follow the sequencer position of the selected channel.
    uint8_t step_index = 0; // The currently selected step for editing.
    bool step_select = false; // Whether the step is being selected with the AUX button.
    Loop loops[2] = {}; // Loop points for each channel.
    Step steps[2][MAX_STEPS]; // The sequence itself for each channel.

    SeqState&       live(uint8_t ch)          { return seq[ch][SEQ_LIVE]; }
    const SeqState& live(uint8_t ch)    const { return seq[ch][SEQ_LIVE]; }
    SeqState&       shadow(uint8_t ch)        { return seq[ch][SEQ_SHADOW]; }
    const SeqState& shadow(uint8_t ch)  const { return seq[ch][SEQ_SHADOW]; }
    SeqState&       phantom(uint8_t ch)       { return seq[ch][SEQ_PHANTOM]; }
    const SeqState& phantom(uint8_t ch) const { return seq[ch][SEQ_PHANTOM]; }

    void draw_interface() {
        if (random_menu_active) {
            draw_random_menu();
            return;
        }

        uint8_t y = 11;

        // Clamp to 64: when a loop pulls in steps past num_steps the live
        // position can exceed total_half_clocks(ch), which would overflow the
        // uint8_t and draw a wrong-width bar.
        const uint16_t total0 = total_half_clocks(0);
        const uint16_t total1 = total_half_clocks(1);
        const uint8_t ph1 = constrain(Proportion(total_half_clocks(0, live(0).step) + live(0).half_clocks, total0, 64), 0, 64);
        const uint8_t ph2 = constrain(Proportion(total_half_clocks(1, live(1).step) + live(1).half_clocks, total1, 64), 0, 64);
        const uint8_t phbar = Proportion(bar_half_clocks % 8, 8, 64);

        y += 4;
        gfxRect(0, y - 2, ph1, 2);
        gfxDottedLine(0, y, 63, y, 3);
        gfxRect(0, y + 1, ph2, 2);

        y += 4;
        gfxDottedLine(16, y, 16, y + 2);
        gfxDottedLine(32, y, 32, y + 2);
        gfxDottedLine(48, y, 48, y + 2);
        gfxRect(0, y, phbar, 2);

        y += 6;
        if (cursor >= CVSeqCursor::GENERAL_COPY_0_TO_1) {
            draw_general_screen(y);
        } else if (cursor >= CVSeqCursor::CHANNEL_INDEX) {
            draw_channel_screen(y);
        } else if (cursor >= CVSeqCursor::VALUE_INDEX) {
            draw_values_screen(y);
        } else {
            draw_main_screen(y);
        }
    }

    void draw_random_menu() {
        for (int line = 0; line < 5; line++) {
            const int item = random_menu_cursor.first_visible() + line;
            if (item > random_menu_cursor.last_visible()) {
                break;
            }

            const uint8_t y = 15 + (line * 10);
            uint8_t x = 0;
            gfxPos(0, y);
            switch (item) {
            case RandomCursor::RANDOM_CH1:
                gfxPrintIcon(random_channels[0] ? CHECK_ON_ICON : CHECK_OFF_ICON);
                x = 10;
                gfxPrint("CH1");
                break;
            case RandomCursor::RANDOM_CH2:
                gfxPrintIcon(random_channels[1] ? CHECK_ON_ICON : CHECK_OFF_ICON);
                x = 10;
                gfxPrint("CH2");
                break;
            case RandomCursor::RANDOM_VALUES:
                gfxPrint("Values");
                break;
            case RandomCursor::RANDOM_TRANSPOSE:
                gfxPrint("Transpose");
                break;
            case RandomCursor::RANDOM_STEPS:
                gfxPrint("Steps");
                break;
            case RandomCursor::RANDOM_CANCEL:
                gfxPrintIcon(LEFT_ICON);
                x = 10;
                gfxPrint("Back");
                break;
            case RandomCursor::RANDOM_ZERO:
                gfxPrint("Zero");
                break;
            case RandomCursor::RANDOM_INIT:
                gfxPrint("Init");
                break;
            }

            if (item == random_menu_cursor.cursor_pos()) {
                gfxSpicyCursor(x, y + 8, x == 0 ? 63 : 53);
            }
        }
    }

    /**
     * IN2  OUT2 - current mode for CV2 input and output, from the Input2Mode and Output2Mode enums
     * SCAL _ch1 - scale, icon when loop is enabled for channel 1
     *  C#  _ch2 - root note, icon when loop is enabled for channel 2
     * _ _  _inv - reset button, random button, loop invert button
     */
    void draw_main_screen(uint8_t y) {
        gfxPrint(0, y, input2_mode_names[input2_mode]);
        if (cursor == CVSeqCursor::IN2_MODE) gfxSpicyCursor(0, y + 8, 30, "In2 Mode");
        gfxPrint(32, y, output2_mode_names[output2_mode]);
        if (cursor == CVSeqCursor::OUT2_MODE) gfxSpicyCursor(32, y + 8, 30, "Out2 Mode");

        y += 10;
        const uint8_t y_qselect = y; // saved for the overlay drawn after all other rows
        if (cursor == CVSeqCursor::QSELECT || cursor == CVSeqCursor::TRANS_MODE) {
            if (quantize_off) {
                gfxPrint(0, y, "OFF");
            } else {
                const char txt[] = { 'Q', char('1' + qselect), '\0' };
                gfxPrint(6, y, txt);
            }
            gfxPrint(0, y + 10, transpose_in_semitones ? "Root" : "Deg");
            if (cursor == CVSeqCursor::TRANS_MODE) {
                gfxSpicyCursor(0, y + 18, 24);
            }
        } else if (quantize_off) {
            gfxPrint(0, y, "OFF");
        } else {
            // Show scale and root note like old times.
            gfxPrint(0, y, HS::GetQuantEngine(qselect), false);
        }

        if (loops[0].enabled) gfxIcon(32, y, LOOP_ICON);
        gfxPrint(40, y, "ch1");
        if (cursor == CVSeqCursor::LOOP1) gfxSpicyCursor(40, y + 8, 18);

        y += 10;
        if (loops[1].enabled) gfxIcon(32, y, LOOP_ICON);
        gfxPrint(40, y, "ch2");
        if (cursor == CVSeqCursor::LOOP2) gfxSpicyCursor(40, y + 8, 18);

        y += 10;
        gfxIcon(4, y, RESET_ICON);
        if (cursor == CVSeqCursor::RESET) gfxSpicyCursor(4, y + 8, 8);
        gfxIcon(18, y, RANDOM_ICON);
        if (cursor == CVSeqCursor::RANDOM) gfxSpicyCursor(18, y + 8, 8);
        gfxIcon(32, y, LOOP_ICON);
        gfxPrint(40, y, "Flp");
        if (cursor == CVSeqCursor::LOOP_FLIP) gfxSpicyCursor(40, y + 8, 18);

        if (cursor == CVSeqCursor::QSELECT) {
            if (quantize_off) {
                gfxSpicyCursor(0, y_qselect + 8, 18, "Q-engine");
            } else {
                gfxSpicyCursor(6, y_qselect + 8, 12, "Q-engine");
                if (EditMode()) {
                    // Overlay preview of scale + root.
                    gfxPrint(1, y_qselect + 9, HS::GetQuantEngine(qselect));
                }
            }
        }
    }

    /**
     * Value #16  - index of the CV value being edited
     * CV +10.00V - the CV value being edited, shown as voltage with 0.08V resolution, from the cv_values array
     * C#0 +50c   - the CV value as a note, no quantizer applied. Octave is signed (e.g. "B-1"). Appends " +50c" for quarter-tones (odd cv values).
     * 0 Cpy Past - reset to 0 button, copy the current value to clipboard button, paste from clipboard button (clipboard defaults to 0).
     */
    void draw_values_screen(uint8_t y) {
        const int v = cv_values[cv_value_index];

        const uint8_t value_y = y;
        gfxPos(0, y);
        gfxPrint("Value #");
        gfxPrint(cv_value_index + 1);
        if (cursor == CVSeqCursor::VALUE_INDEX) gfxSpicyCursor(42, y + 8, 12, "Value #");

        y += 10;
        gfxPos(0, y);
        gfxPrint("CV ");
        gfxPrintVoltage(v * CV_INCREMENT);
        if (cursor == CVSeqCursor::VALUE_CV) gfxSpicyCursor(18, y + 8, 42, "CV Value");

        y += 10;
        gfxPos(0, y);
        const int semi = v >> 1;               // floor(v / 2)
        int oct = semi / 12;
        int pc  = semi % 12;
        if (pc < 0) { pc += 12; oct--; } // floor(semi / 12)
        gfxPrint(OC::Strings::note_names_unpadded[pc]);
        gfxPrint(oct);
        if (v & 1) gfxPrint(" +50c");
        if (cursor == CVSeqCursor::VALUE_NOTE) gfxSpicyCursor(0, y + 8, 54, "Note");

        if (cv_value_select && EditMode()) {
            gfxIcon(34, value_y, RIGHT_ICON, true);
            gfxFrame(41, value_y - 2, 15, 11, true);
        }

        y += 10;
        gfxPrint(0, y, "0 Cpy Past");
        if (cursor == CVSeqCursor::VALUE_ZERO) gfxSpicyCursor(0, y + 8, 6);
        if (cursor == CVSeqCursor::VALUE_COPY) gfxSpicyCursor(12, y + 8, 18);
        if (cursor == CVSeqCursor::VALUE_PASTE) gfxSpicyCursor(36, y + 8, 24);
    }

    /**
     * CH1 _32#32 - channel (highlighted when following), how many steps, current editing step
     * val #16 >< - what value will be playing at this step, from the cv_values array, icon that this is the start of the loop, icon that this is the end of the loop
     * 1/2 clocks - for how many clocks this step will last, from 1/2 (encoded as 0) up to 31
     * 1024 1024/ - total length of the sequence in clocks (always a whole number, since trailing 1/2 steps are auto-extended), when the current step starts in clocks relative to start of sequence (displays "-" when the step will not play at all due to num_steps). "/" suffix on the second number means the step starts on a 1/2-clock boundary.
     *
     * A 1/2-clock step is automatically extended to 1 clock if the next step is not also 1/2, so that the next step always starts on a clock boundary.
     */
    void draw_channel_screen(uint8_t y) {
        const uint8_t value_y = y;
        gfxPos(0, y);
        gfxPrint("CH");
        if (channel_follow) gfxInvert(0, y, 12, 8);
        gfxPrint(channel_index + 1);
        gfxPos(24, y);
        gfxPrintIcon(LENGTH_ICON);
        gfxPrint(num_steps[channel_index]);
        gfxPos(44, y);
        gfxPrint("#");
        gfxPrint(step_index + 1);
        if (cursor == CVSeqCursor::CHANNEL_INDEX) gfxSpicyCursor(12, y + 8, 6, "Channel");
        if (cursor == CVSeqCursor::CHANNEL_STEPS) gfxSpicyCursor(32, y + 8, 12, "Num steps");
        if (cursor == CVSeqCursor::CHANNEL_STEP) gfxSpicyCursor(50, y + 8, 12, "Step");

        const Step& step = steps[channel_index][step_index];
        y += 10;
        gfxPos(0, y);
        gfxPrint("val #");
        gfxPrint(step.value + 1);
        if (cursor == CVSeqCursor::CHANNEL_VALUE) gfxSpicyCursor(30, y + 8, 12, "Step val.");
        if (loops[channel_index].lo() == step_index) gfxPrint(48, y, ">");
        if (loops[channel_index].hi() == step_index) gfxPrint(54, y, "<");
        if (cursor == CVSeqCursor::CHANNEL_LOOP) gfxSpicyCursor(48, y + 8, 12);

        y += 10;
        if (step.clocks == 0) {
            gfxPrint(0, y, "1/2");
        } else {
            gfxPrint(0, y, step.clocks);
        }
        gfxPrint(24, y, "clocks");
        if (cursor == CVSeqCursor::CHANNEL_CLOCKS) gfxSpicyCursor(0, y + 8, 18, "Clocks");

        y += 10;
        gfxPrint(0, y, total_half_clocks(channel_index) / 2);
        gfxPos(30, y);
        if (step_index < num_steps[channel_index]) {
            // Show when the current step starts in clocks, relative to the start of the sequence.
            uint16_t half_clocks = total_half_clocks(channel_index, step_index);
            gfxPrint(half_clocks / 2);
            if (half_clocks & 1) gfxPrint("/");
        } else {
            // Step will not play at all, show "-" instead of clocks.
            gfxPrint("-");
        }

        if (step_select && EditMode()) {
            gfxIcon(43, value_y, RIGHT_ICON, true);
            gfxFrame(50, value_y - 2, 15, 11, true);
        }
    }

    /**
     * CH1 > CH2 - copy channel 1 to channel 2
     * CH2 > CH1 - copy channel 2 to channel 1
     */
    void draw_general_screen(uint8_t y) {
        gfxPrint(0, y, "CH1 > CH2");
        if (cursor == CVSeqCursor::GENERAL_COPY_0_TO_1) gfxSpicyCursor(0, y + 8, 60);

        y += 10;
        gfxPrint(0, y, "CH2 > CH1");
        if (cursor == CVSeqCursor::GENERAL_COPY_1_TO_0) gfxSpicyCursor(0, y + 8, 60);
    }

    uint16_t total_half_clocks(uint8_t ch, uint8_t up_to_step = MAX_STEPS) const {
        uint16_t half_clocks = 0;
        uint8_t run = 0; // length of the current consecutive half-clock run
        const uint8_t limit = num_steps[ch] < up_to_step ? num_steps[ch] : up_to_step;
        for (uint8_t i = 0; i < limit; i++) {
            if (steps[ch][i].clocks == 0) {
                run++;
                half_clocks++;
            } else {
                if (run & 1) half_clocks++; // extend trailing half in odd run
                run = 0;
                half_clocks += steps[ch][i].clocks * 2;
            }
        }

        // The trailing run ends only at end-of-range or when the next step is whole-clock.
        const bool ends_run = (limit >= num_steps[ch]) || steps[ch][limit].clocks != 0;
        if ((run & 1) && ends_run) half_clocks++;

        return half_clocks;
    }

    void copy_channel(uint8_t from, uint8_t to) {
        num_steps[to] = num_steps[from];
        memcpy(steps[to], steps[from], sizeof(Step) * MAX_STEPS);
        loops[to] = loops[from];
    }

    void reset_phantom(uint8_t ch) {
        phantom(ch) = SeqState{};
        phantom(ch).step = loops[ch].lo();
        enter_step(ch, phantom(ch), true);
    }

    // Latched reset: called on the first clock after Reset()/Clock(1) or when a preset is loaded.
    void do_reset() {
        reset_flag = false;
        bar_half_clocks = 0;
        for (uint8_t ch = 0; ch < 2; ch++) {
            loop_active[ch] = loops[ch].enabled;
            pending_loop[ch] = LP_NONE;

            live(ch) = SeqState{};
            enter_step(ch, live(ch), loop_active[ch]);

            shadow(ch) = SeqState{};
            enter_step(ch, shadow(ch), false);

            reset_phantom(ch);
        }
    }

    // Forward-only: loop_on causes hi→lo wrap; otherwise wraps at num_steps.
    uint8_t step_after(uint8_t ch, uint8_t step, bool loop_on) const {
        if (loop_on) {
            if (step == loops[ch].hi()) return loops[ch].lo();
            // step < hi() <= MAX_STEPS - 1, so step + 1 is always in range.
            return step + 1;
        }

        const uint8_t next = step + 1;
        if (next >= num_steps[ch]) return 0;

        return next;
    }

    void retarget_step(uint8_t ch, SeqState& ss, bool loop_on) const {
        const Step& s = steps[ch][ss.step];
        if (s.clocks > 0) {
            ss.run_count = 0;
            ss.target_half_clocks = s.clocks * 2;
            return;
        }

        if ((ss.run_count & 1) == 0) {
            // Even position in the run: paired with previous half-clock step.
            ss.target_half_clocks = 1;
            return;
        }

        // Odd position: pair with the next half-clock step (target=1), unless
        // this step ends the playback cycle — in which case extend to a whole
        // clock so each cycle starts on a clock boundary (matches
        // total_half_clocks() display logic).
        const uint8_t cycle_end = loop_on ? loops[ch].hi() : (uint8_t)(num_steps[ch] - 1);
        if (ss.step == cycle_end) {
            ss.run_count = 0;
            ss.target_half_clocks = 2;
            return;
        }

        const uint8_t next = step_after(ch, ss.step, loop_on);
        ss.target_half_clocks = (steps[ch][next].clocks == 0) ? 1 : 2;
    }

    void enter_step(uint8_t ch, SeqState& ss, bool loop_on) {
        if (steps[ch][ss.step].clocks == 0) ss.run_count++;
        retarget_step(ch, ss, loop_on);
    }

    void advance_seq(uint8_t ch, SeqState& s, bool loop_on) {
        if (++s.half_clocks >= s.target_half_clocks) {
            s.step = step_after(ch, s.step, loop_on);
            s.half_clocks = 0;
            enter_step(ch, s, loop_on);
        }
    }

    // Called after a loop start/end point changes. Always re-anchors the
    // phantom at the new region's start so the next engage stays in phase
    // (otherwise phantom may sit outside the new bounds and take a full lap
    // to wrap back in). When the loop is active, also corrects the live
    // sequencer: retargets in place if still inside the new region, or snaps
    // to the phantom if it has wandered outside. Shadow is left alone — it
    // remains the valid no-loop resume point for when the loop is released.
    void on_loop_points_changed(uint8_t ch) {
        reset_phantom(ch);

        if (!loop_active[ch]) return;

        if (loops[ch].contains(live(ch).step)) {
            retarget_step(ch, live(ch), true);
        } else {
            live(ch) = phantom(ch);
        }
    }

    // Handle a press on a LOOP toggle button. The Loop's enabled flag has
    // already been flipped by the caller; this latches the appropriate
    // pending jump for the next Clock(0) (or cancels a stale pending jump
    // if the user double-pressed before the clock arrived).
    void on_loop_toggle(uint8_t ch) {
        if (loops[ch].enabled) {
            if (pending_loop[ch] == LP_EXIT) {
                pending_loop[ch] = LP_NONE;
            } else if (!loop_active[ch]) {
                pending_loop[ch] = LP_ENTER;
            }
        } else {
            if (pending_loop[ch] == LP_ENTER) {
                pending_loop[ch] = LP_NONE;
            } else if (loop_active[ch]) {
                pending_loop[ch] = LP_EXIT;
            }
        }
    }

    // Compute the CV output for a channel: sequence value offsets the input CV
    // in semitone units, the result is quantized, then sub-semitone residue and
    // the global CV2 transpose are applied (ch1 only).
    int pitch_for_step(uint8_t ch, uint8_t step, int input_cv) const {
        const int8_t v = cv_values[steps[ch][step].value];
        // v is in half-semitones; whole semitones go through the quantizer,
        // the +50¢ residue stays chromatic and is applied after.
        const int seq_semitones = (v >> 1) * 128;
        const int seq_residue   = (v & 1) * CV_INCREMENT;
        const int transpose_offset = (ch == 0) ? (transpose_amt << 7) : 0;

        int cv;
        if (quantize_off) {
            cv = input_cv + seq_semitones + seq_residue + transpose_offset;
        } else if (transpose_in_semitones) {
            // Root mode: chromatic transpose + residue applied after quantization.
            cv = HS::q_engine[qselect].Process(input_cv + seq_semitones, 0, 0)
               + transpose_offset + seq_residue;
        } else {
            // Degree mode: transpose snaps onto a scale note; residue is still chromatic.
            cv = HS::q_engine[qselect].Process(input_cv + seq_semitones + transpose_offset, 0, 0)
               + seq_residue;
        }

        return constrain(cv, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
    }

    void write_outputs(bool new_ch0_step) {
        const int ch1_input_cv = In(0);
        const int play_cv = pitch_for_step(0, live(0).step, ch1_input_cv);
        Out(0, play_cv);

        const int ch2_input_cv = (input2_mode == Input2Mode::CV2_IN) ? In(1) : 0;
        const int ch2_seq_cv = pitch_for_step(1, live(1).step, ch2_input_cv);

        switch (output2_mode) {
            case Output2Mode::CV2:
                Out(1, ch2_seq_cv);
                break;
            case Output2Mode::CV1_2OCT_DOWN:
                Out(1, play_cv - 2 * ONE_OCTAVE);
                break;
            case Output2Mode::CV1_1OCT_DOWN:
                Out(1, play_cv - ONE_OCTAVE);
                break;
            case Output2Mode::CV1:
                Out(1, play_cv);
                break;
            case Output2Mode::CV1_1OCT_UP:
                Out(1, play_cv + ONE_OCTAVE);
                break;
            case Output2Mode::CV1_2OCT_UP:
                Out(1, play_cv + 2 * ONE_OCTAVE);
                break;
            case Output2Mode::CV1_GATE:
                if (new_ch0_step) ClockOut(1);
                break;
            case Output2Mode::CV1_IN:
                Out(1, ch1_input_cv);
                break;
        }
    }

    // Randomize CV values within an octave and a half (±18 semitones).
    void random_values() {
        for (int i = 0; i < MAX_CV_VALUES; i++) {
            cv_values[i] = (int8_t)(random(-18, 19) * 2);
        }
    }

    // Randomly transpose some CV values up or down by up to 6 semitones (12 if in degree mode), in either direction.
    void random_transpose() {
        for (int i = 0; i < MAX_CV_VALUES; i++) {
            if (random(0, 2) == 0) continue;
            const int delta = random(-6, 7) * 2;
            cv_values[i] = (int8_t)constrain((int)cv_values[i] + delta, -127, 127);
        }
    }

    // Randomize steps by choosing a random target length in clocks, then randomly filling steps until that length is reached.
    // Target lengths are chosen to be musically reasonable (e.g. not 7 clocks).
    void random_steps() {
        static constexpr uint8_t clock_opts[] = {2, 4, 4};
        static constexpr uint8_t ch0_targets[] = {8, 16, 16, 32};
        static constexpr uint8_t divisors[]    = {1, 2, 4};

        const uint8_t ch0_target = ch0_targets[random(0, 4)];
        const uint8_t div        = divisors[random(0, 3)];
        const uint8_t targets[2] = {ch0_target, (uint8_t)(ch0_target / div)};

        for (uint8_t ch = 0; ch < 2; ch++) {
            if (!random_channels[ch]) continue;

            uint8_t remaining = targets[ch];
            uint8_t s = 0;
            while (remaining > 0 && s < MAX_STEPS) {
                uint8_t c = clock_opts[random(0, 3)];
                if (c > remaining) c = 2; // targets are always even; 2 always fits
                steps[ch][s].clocks = c;
                steps[ch][s].value = (uint8_t)random(0, MAX_CV_VALUES);
                remaining -= c;
                s++;
            }
            num_steps[ch] = s;
            loops[ch] = Loop{};
        }
    }

    void zero_steps() {
        memset(cv_values, 0, sizeof(cv_values));
        for (uint8_t ch = 0; ch < 2; ch++) {
            num_steps[ch] = 1;
            loops[ch] = Loop{};
            for (uint8_t i = 0; i < MAX_STEPS; i++) steps[ch][i] = Step();
        }
    }

    // Init sequence.
    void init_steps() {
        // Root / 4th / 5th — classic driving techno
        const int8_t init_values[] = {
            0, // root
            10, // +5 st (perfect 4th)
            14, // +7 st (perfect 5th)
            24, // +12 st (octave)
            -24, // -12 st (octave down)
            -14, // -7 st (5th down)
            -10, // -5 st (4th down)
            20, // +10 st (minor 7th)
            -2, // -1 st (half-step down — chromatic slide)
        };

        constexpr int init_count = sizeof(init_values) / sizeof(init_values[0]);
        // Fill the first values with a musically useful set, then fill the rest with random values from that set.
        for (int i = 0; i < init_count; i++) cv_values[i] = init_values[i];
        for (int i = init_count; i < MAX_CV_VALUES; i++) cv_values[i] = init_values[random(0, init_count)];

        num_steps[0] = 3;
        loops[0] = Loop{};
        steps[0][0] = Step{0, 8};
        steps[0][1] = Step{1, 4};
        steps[0][2] = Step{2, 4};

        num_steps[1] = 2;
        loops[1] = Loop{};
        steps[1][0] = Step{4, 4};
        steps[1][1] = Step{5, 4};
    }
};
