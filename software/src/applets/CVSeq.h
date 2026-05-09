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
        LOOP_SWAP,

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
        CHANNEL_LOOP_START,
        CHANNEL_LOOP_END,
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
        // Only set flag, keep step playing until next clock.
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

        bool new_step = false;
        if (clocked && reset_flag) {
            // Latched reset takes effect on the first clock.
            do_reset();
            new_step = true;
        } else if (clocked || half_boundary) {
            // Both whole- and half-clock events advance the half-clock counter by 1.
            bar_half_clocks++;
            for (uint8_t ch = 0; ch < 2; ch++) {
                // Always advance the live sequencer first; this also drives the
                // pre-engage no-loop tick at an engage clock (loop_active is
                // still false at that point, so the advance is no-loop).
                current_half_clocks[ch]++;
                if (current_half_clocks[ch] >= target_half_clocks[ch]) {
                    current_step[ch] = next_step_index(ch);
                    current_half_clocks[ch] = 0;
                    enter_current_step(ch);
                    if (ch == 0) new_step = true;
                }
                // Advance the shadow whenever a loop is driving playback.
                if (loop_active[ch]) advance_shadow(ch);
                // Phantom always advances — it's the phase-locked reference for
                // outside-engages, anchored at sequence reset.
                advance_phantom(ch);

                // Handle pending jumps on whole-clock events. By this point all
                // three sequencers have already taken their tick, so we're
                // working with post-tick positions.
                if (clocked && pending_loop[ch] == LP_ENTER) {
                    shadow_step[ch] = current_step[ch];
                    shadow_half_clocks[ch] = current_half_clocks[ch];
                    shadow_target_half_clocks[ch] = target_half_clocks[ch];
                    shadow_run_count[ch] = run_count[ch];

                    if (is_inside_loop(ch, current_step[ch])) {
                        // Stay where we are; just retarget under loop semantics.
                        retarget_step(ch, current_step[ch], target_half_clocks[ch], run_count[ch], true);
                    } else {
                        // Snap to phantom's current phase-locked position.
                        current_step[ch] = phantom_step[ch];
                        current_half_clocks[ch] = phantom_half_clocks[ch];
                        target_half_clocks[ch] = phantom_target_half_clocks[ch];
                        run_count[ch] = phantom_run_count[ch];
                    }
                    loop_active[ch] = true;
                    pending_loop[ch] = LP_NONE;
                    if (ch == 0) new_step = true;
                } else if (clocked && pending_loop[ch] == LP_EXIT) {
                    current_step[ch] = shadow_step[ch];
                    current_half_clocks[ch] = shadow_half_clocks[ch];
                    target_half_clocks[ch] = shadow_target_half_clocks[ch];
                    run_count[ch] = shadow_run_count[ch];
                    loop_active[ch] = false;
                    pending_loop[ch] = LP_NONE;
                    if (ch == 0) new_step = true;
                }
            }
        }

        if (channel_follow) step_index = current_step[channel_index];

        write_outputs(new_step);
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
        } else if (cursor == CVSeqCursor::LOOP_SWAP) {
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
        } else if (cursor == CVSeqCursor::CHANNEL_LOOP_START) {
            loops[channel_index].start = step_index;
        } else if (cursor == CVSeqCursor::CHANNEL_LOOP_END) {
            loops[channel_index].end = step_index;
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
            CancelEdit();
            return;
        }

        if (cursor == CVSeqCursor::QSELECT) {
            HS::QuantizerEdit(qselect);
            return;
        }

        if (cursor == CVSeqCursor::VALUE_CV || cursor == CVSeqCursor::VALUE_NOTE) {
            // Toggle between selecting CV value or selecting which CV value to edit.
            cv_value_select ^= true;
            return;
        }

        if (cursor == CVSeqCursor::CHANNEL_INDEX) {
            channel_follow ^= true;
            return;
        }

        if (cursor > CVSeqCursor::CHANNEL_INDEX && cursor <= CVSeqCursor::CHANNEL_CLOCKS) {
            // Toggle between selecting step or selecting which step to edit.
            step_select ^= true;
            return;
        }
    }

    void OnEncoderMove(int direction) override {
        if (random_menu_active) {
            random_menu_cursor.Scroll(direction);
            return;
        }

        if (!EditMode()) {
            // Move cursor.
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
            qselect = constrain(qselect + direction, 0, QUANT_CHANNEL_COUNT - 1);
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

        Pack(data, PackLocation {0, 2}, input2_mode);
        Pack(data, PackLocation {2, 3}, output2_mode);
        Pack(data, PackLocation {5, 3}, qselect);
        Pack(data, PackLocation {8, 1}, transpose_in_semitones);
        Pack(data, PackLocation {9, 1}, random_channels[0]);
        Pack(data, PackLocation {10, 1}, random_channels[1]);

        return data;
    }

    void OnDataReceive(uint64_t data) override {
        input2_mode = (Input2Mode)constrain(Unpack(data, PackLocation {0, 2}), 0, Input2Mode::MAX_INPUT2_MODE - 1);
        output2_mode = (Output2Mode)constrain(Unpack(data, PackLocation {2, 3}), 0, Output2Mode::MAX_OUTPUT2_MODE - 1);
        qselect = constrain(Unpack(data, PackLocation {5, 3}), 0, QUANT_CHANNEL_COUNT - 1);
        transpose_in_semitones = Unpack(data, PackLocation {8, 1});
        random_channels[0] = Unpack(data, PackLocation {9, 1});
        random_channels[1] = Unpack(data, PackLocation {10, 1});

        // Suppress half-boundary events on stale state, then reset everything
        // immediately so playback starts cleanly from the new preset.
        clock_ticks = 0;
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
    uint8_t bar_half_clocks = 0; // Independent bar position counter, advances every half-clock.
    uint8_t num_steps[2] = {1, 1}; // Number of steps in the sequence for each channel.
    uint8_t current_step[2] = {0, 0}; // Current sequencer step for each channel.
    uint8_t current_half_clocks[2] = {0, 0}; // Half-clock events elapsed since the current step started, for each channel.
    uint8_t target_half_clocks[2] = {2, 2}; // Half-clock duration of the current step, for each channel.
    uint8_t run_count[2] = {0, 0}; // Length of the current consecutive half-clock run, for each channel.

    // Shadow sequencer: while a loop is engaged, this advances as if the loop
    // were off, so we can resume there when the user releases the loop.
    uint8_t shadow_step[2] = {0, 0};
    uint8_t shadow_half_clocks[2] = {0, 0};
    uint8_t shadow_target_half_clocks[2] = {2, 2};
    uint8_t shadow_run_count[2] = {0, 0};
    // Phantom sequencer: always plays the loop region (regardless of loop_active),
    // anchored at eff_start at sequence reset. Used to phase-lock the entry
    // position when the loop is engaged from outside the loop region.
    uint8_t phantom_step[2] = {0, 0};
    uint8_t phantom_half_clocks[2] = {0, 0};
    uint8_t phantom_target_half_clocks[2] = {2, 2};
    uint8_t phantom_run_count[2] = {0, 0};
    bool loop_active[2] = {false, false}; // Whether the loop is currently driving playback (lags loops[].enabled by one clock).
    enum LoopPending : uint8_t { LP_NONE, LP_ENTER, LP_EXIT }; // Latched action for the next Clock(0): enter the loop, exit to shadow, or do nothing.
    LoopPending pending_loop[2] = {LP_NONE, LP_NONE};

    struct Step {
        uint8_t value = 0; // Index from `cv_values`.
        uint8_t clocks = 4; // Number of clocks this step lasts for.
    };

    struct Loop {
        uint8_t start = 0; // Index of loop start step.
        uint8_t end = 0; // Index of loop end step.
        bool enabled = false; // Whether the loop is enabled.
    };

    Input2Mode input2_mode = Input2Mode::CV2_IN; // What CV2 input is used for.
    Output2Mode output2_mode = Output2Mode::CV2; // What CV2 outputs.
    int qselect = io_offset; // Quantizer channel selection for both channels, since they share the quantizer.
    bool transpose_in_semitones = false; // Whether CV1 transpose (or root note if scale is off) is in semitones or scale degrees.
    int32_t transpose_amt = 0; // Always in semitones; quantized into a scale degree when transpose_in_semitones is false.
    bool reset_flag = false; // Latched: do_reset() runs on the next Clock(0).
    bool random_menu_active = false; // Whether the random menu is active.
    OC::menu::ScreenCursor<5> random_menu_cursor; // Cursor for the random menu.
    bool random_channels[2] = {true, true}; // Randomization settings for each channel.
    int8_t cv_values[MAX_CV_VALUES] = {}; // CV values available for the sequence, stored as -127 to 127 for easier editing and display as voltage. The actual CV output will be this value multiplied by CV_INCREMENT.
    uint8_t cv_value_index = 0; // The index of CV value currently being edited, from the cv_values array.
    bool cv_value_select = false; // Whether the CV value is being selected with the AUX button.
    int8_t cv_value_clipboard = 0; // A clipboard for copying and pasting CV values.
    uint8_t channel_index = 0; // The currently selected channel for editing.
    bool channel_follow = false; // Whether to follow the sequencer position of the selected channel.
    uint8_t step_index = 0; // The currently selected step for editing.
    bool step_select = false; // Whether the step is being selected with the AUX button.
    Loop loops[2] = {}; // Loop points for each channel.
    Step steps[2][MAX_STEPS]; // The sequence itself for each channel.

    void draw_interface() {
        if (random_menu_active) {
            draw_random_menu();
            return;
        }

        uint8_t y = 11;

        const uint8_t ph1 = Proportion(total_half_clocks(0, current_step[0]) + current_half_clocks[0], total_half_clocks(0), 64);
        const uint8_t ph2 = Proportion(total_half_clocks(1, current_step[1]) + current_half_clocks[1], total_half_clocks(1), 64);
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
            const char txt[] = { 'Q', char('1' + qselect), '\0' };
            gfxPrint(6, y, txt);
            gfxPrint(0, y + 10, transpose_in_semitones ? "Semi" : "Deg");
            if (cursor == CVSeqCursor::TRANS_MODE) {
                gfxSpicyCursor(0, y + 18, 24);
            }
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
        gfxPrint(40, y, "Swp");
        if (cursor == CVSeqCursor::LOOP_SWAP) gfxSpicyCursor(40, y + 8, 18);

        if (cursor == CVSeqCursor::QSELECT) {
            gfxSpicyCursor(6, y_qselect + 8, 12, "Q-engine");
            if (EditMode()) {
                // Overlay preview of scale + root.
                gfxPrint(1, y_qselect + 9, HS::GetQuantEngine(qselect));
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

        y += 10;
        gfxPos(0, y);
        gfxPrint("val #");
        gfxPrint(steps[channel_index][step_index].value + 1);
        if (cursor == CVSeqCursor::CHANNEL_VALUE) gfxSpicyCursor(30, y + 8, 12, "Step val.");
        if (loops[channel_index].start == step_index) gfxPrint(48, y, ">");
        if (cursor == CVSeqCursor::CHANNEL_LOOP_START) gfxSpicyCursor(48, y + 8, 6);
        if (loops[channel_index].end == step_index) gfxPrint(54, y, "<");
        if (cursor == CVSeqCursor::CHANNEL_LOOP_END) gfxSpicyCursor(54, y + 8, 6);

        y += 10;
        if (steps[channel_index][step_index].clocks == 0) {
            gfxPrint(0, y, "1/2");
        } else {
            gfxPrint(0, y, steps[channel_index][step_index].clocks);
        }
        gfxPrint(24, y, "clocks");
        if (cursor == CVSeqCursor::CHANNEL_CLOCKS) gfxSpicyCursor(0, y + 8, 18, "Clocks");

        y += 10;
        gfxPrint(0, y, total_clocks(channel_index));
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

    // Total length of the sequence in whole clocks.
    // Always a whole number because trailing half-clock runs are auto-extended.
    uint16_t total_clocks(uint8_t ch) {
        return total_half_clocks(ch) / 2;
    }

    // Returns the total length of the sequence in half clocks
    // by summing the lengths of all steps.
    // Successive half-clock steps come in pairs (each contributing 1 half-clock).
    // If a run of consecutive half-clock steps has an odd count,
    // the last one is automatically extended to a full clock
    // so that the next step always starts at the beginning of a clock.
    uint16_t total_half_clocks(uint8_t ch, uint8_t up_to_step = MAX_STEPS) {
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

    // Copy all step data from one channel to another.
    void copy_channel(uint8_t from, uint8_t to) {
        num_steps[to] = num_steps[from];
        memcpy(steps[to], steps[from], sizeof(steps[0]));
        loops[to] = loops[from];
    }

    // Reset all sequencer state to a clean starting position. Called by the
    // latched reset on the first clock after Reset() / Clock(1), and directly
    // by OnDataReceive when a preset is loaded.
    void do_reset() {
        reset_flag = false;
        bar_half_clocks = 0;
        for (uint8_t ch = 0; ch < 2; ch++) {
            loop_active[ch] = loops[ch].enabled;
            pending_loop[ch] = LP_NONE;

            current_step[ch] = 0;
            current_half_clocks[ch] = 0;
            run_count[ch] = 0;
            enter_current_step(ch);

            shadow_step[ch] = 0;
            shadow_half_clocks[ch] = 0;
            shadow_run_count[ch] = 0;
            enter_shadow_step(ch);

            phantom_step[ch] = eff_start(ch);
            phantom_half_clocks[ch] = 0;
            phantom_run_count[ch] = 0;
            enter_phantom_step(ch);
        }
    }

    // Loop start/end clamped to the active step range.
    uint8_t eff_start(uint8_t ch) const {
        return loops[ch].start < num_steps[ch] ? loops[ch].start : (uint8_t)(num_steps[ch] - 1);
    }
    uint8_t eff_end(uint8_t ch) const {
        return loops[ch].end < num_steps[ch] ? loops[ch].end : (uint8_t)(num_steps[ch] - 1);
    }

    // Compute the next step index from a given step, optionally honoring loops.
    // Pure (does not mutate state). Loop points beyond num_steps are clamped to
    // the last enabled step.
    uint8_t step_after(uint8_t ch, uint8_t step, bool loop_on) const {
        if (loop_on && step == eff_end(ch)) return eff_start(ch);

        const uint8_t next = step + 1;
        if (next >= num_steps[ch]) return 0;

        return next;
    }

    // Next step for the live sequencer, honoring the actual playback loop state.
    uint8_t next_step_index(uint8_t ch) const {
        return step_after(ch, current_step[ch], loop_active[ch]);
    }

    // Compute target_half_clocks for a step that's already pointed at by the
    // run counter (i.e., do NOT increment run). Used both during a fresh entry
    // (after enter_step bumps the run) and to retarget a step in place when
    // the loop state flips mid-step.
    void retarget_step(uint8_t ch, uint8_t step, uint8_t& target_hc, uint8_t& run, bool loop_on) const {
        const Step& s = steps[ch][step];
        if (s.clocks > 0) {
            run = 0;
            target_hc = s.clocks * 2;
            return;
        }

        if ((run & 1) == 0) {
            // Even position in the run: paired with previous half-clock step.
            target_hc = 1;
            return;
        }

        // Odd position: pair with the next half-clock step (target=1), unless
        // this step ends the playback cycle — in which case extend to a whole
        // clock so each cycle starts on a clock boundary (matches
        // total_half_clocks() display logic).
        const uint8_t cycle_end = loop_on ? eff_end(ch) : (uint8_t)(num_steps[ch] - 1);
        if (step == cycle_end) {
            run = 0;
            target_hc = 2;
            return;
        }

        const uint8_t next = step_after(ch, step, loop_on);
        target_hc = (steps[ch][next].clocks == 0) ? 1 : 2;
    }

    // Enter a step: bump the run counter for half-clock steps, then compute
    // the target. Drives the live, shadow, and phantom sequencers.
    void enter_step(uint8_t ch, uint8_t step, uint8_t& target_hc, uint8_t& run, bool loop_on) {
        if (steps[ch][step].clocks == 0) run++;
        retarget_step(ch, step, target_hc, run, loop_on);
    }

    void enter_current_step(uint8_t ch) {
        enter_step(ch, current_step[ch], target_half_clocks[ch], run_count[ch], loop_active[ch]);
    }

    void enter_shadow_step(uint8_t ch) {
        enter_step(ch, shadow_step[ch], shadow_target_half_clocks[ch], shadow_run_count[ch], false);
    }

    void enter_phantom_step(uint8_t ch) {
        enter_step(ch, phantom_step[ch], phantom_target_half_clocks[ch], phantom_run_count[ch], true);
    }

    // Advance the shadow sequencer one half-clock event, ignoring loops.
    void advance_shadow(uint8_t ch) {
        shadow_half_clocks[ch]++;
        if (shadow_half_clocks[ch] >= shadow_target_half_clocks[ch]) {
            shadow_step[ch] = step_after(ch, shadow_step[ch], false);
            shadow_half_clocks[ch] = 0;
            enter_shadow_step(ch);
        }
    }

    // Advance the phantom sequencer one half-clock event, always honoring loops.
    void advance_phantom(uint8_t ch) {
        phantom_half_clocks[ch]++;
        if (phantom_half_clocks[ch] >= phantom_target_half_clocks[ch]) {
            phantom_step[ch] = step_after(ch, phantom_step[ch], true);
            phantom_half_clocks[ch] = 0;
            enter_phantom_step(ch);
        }
    }

    // True if `step` falls within the loop region, regardless of loop direction.
    bool is_inside_loop(uint8_t ch, uint8_t step) const {
        const uint8_t a = eff_start(ch);
        const uint8_t b = eff_end(ch);
        const uint8_t lo = a < b ? a : b;
        const uint8_t hi = a < b ? b : a;
        return step >= lo && step <= hi;
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
    // the global CV2 transpose are optionally applied.
    int pitch_for_step(uint8_t ch, uint8_t step, int input_cv, bool apply_global_transpose) {
        const int8_t v = cv_values[steps[ch][step].value];
        // v is in half-semitones; whole semitones go through the quantizer,
        // the +50¢ residue stays chromatic and is applied after.
        const int seq_semitones = (v >> 1) * 128;
        const int seq_residue   = (v & 1) * CV_INCREMENT;
        const int transpose_offset = apply_global_transpose ? (transpose_amt << 7) : 0;
        const bool scale_off = OC::Scales::GetScale(HS::GetScale(qselect)).num_notes == 0;

        int cv;
        if (scale_off) {
            cv = input_cv + seq_semitones + seq_residue + transpose_offset;
        } else if (transpose_in_semitones) {
            // Semitone mode: chromatic transpose + residue applied after quantization.
            cv = HS::q_engine[qselect].Process(input_cv + seq_semitones, 0, 0)
               + transpose_offset + seq_residue;
        } else {
            // Degree mode: transpose snaps onto a scale note; residue is still chromatic.
            cv = HS::q_engine[qselect].Process(input_cv + seq_semitones + transpose_offset, 0, 0)
               + seq_residue;
        }

        return constrain(cv, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
    }

    // Write CV outputs for both channels based on the current step and input CV, according to the current mode settings.
    void write_outputs(bool new_step) {
        const int ch1_input_cv = In(0);
        const int play_cv = pitch_for_step(0, current_step[0], ch1_input_cv, true);
        Out(0, play_cv);

        const int ch2_input_cv = (input2_mode == Input2Mode::CV2_IN) ? In(1) : 0;
        const int ch2_seq_cv = pitch_for_step(1, current_step[1], ch2_input_cv, false);

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
                if (new_step) ClockOut(1);
                break;
            case Output2Mode::CV1_IN:
                Out(1, ch1_input_cv);
                break;
            default: break;
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

    // Initialize all steps to 0.
    void zero_steps() {
        num_steps[0] = 1;
        num_steps[1] = 1;
        loops[0] = Loop{};
        loops[1] = Loop{};

        for (uint8_t i = 0; i < MAX_CV_VALUES; i++) cv_values[i] = 0;
        for (uint8_t i = 0; i < MAX_STEPS; i++) {
            steps[0][i] = Step();
            steps[1][i] = Step();
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
