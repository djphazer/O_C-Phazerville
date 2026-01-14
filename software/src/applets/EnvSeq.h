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

class EnvSeq : public HemisphereApplet {
public:
    static constexpr int MAX_NUM_STEPS = 32;
    static constexpr int OFFSET_SCALE_INCREMENT = 64;
    static constexpr int GATE_STOP_TICKS = 10 * HEMISPHERE_CLOCK_TICKS;
    static constexpr int STEPSEL_MAX_CV = 5 * ONE_OCTAVE; // unipolar 0-5v

    enum EnvSeqCursor {
        MOD1_MODE,
        MOD2_MODE,
        OUTPUT2_MODE,
        RANDOM,
        NUM_STEPS, RESET, INIT,

        STEP_VIEW, STEP_PROBABILITY,
        STEP_CURVE,
        STEP_OFFSET,
        STEP_SCALE,

        STEP_TRIGGERS, STEP_CLOCKS,
        STEP_LENGTH,
        STEP_MOD_MARK,

        MAX_CURSOR,
    };

    enum RandomCursor {
        RANDOM_OFFSETS, RANDOM_SCALES,
        RANDOM_CURVES, RANDOM_LENGTHS,
        RANDOM_TRIGGERS, RANDOM_CLOCKS,
        RANDOM_MOD_MARKS, RANDOM_APPLY,

        MAX_RANDOM_CURSOR,
    };

    // Modulation mode for CV1
    enum Mod1Mode : uint8_t {
        MOD1 = 0, // Modulate all steps
        HOLD_STEP_START = 1, // Sample and hold the step start value as the modulation input
        HOLD_SEQ_START = 2, // Sample and hold the sequence start value as the modulation input

        MAX_MOD1_MODE,
    };

    // Modulation mode for CV2
    enum Mod2Mode : uint8_t {
        LENGTH = 0, // Modulate the step length
        STEP_SEL = 1, // Modulate the step selector
        MOD2 = 2, // Modulate all steps
        MOD_MARK = 3, // Modulate marked steps ONLY

        MAX_MOD2_MODE,
    };

    // Output mode for CV2
    enum Output2Mode : uint8_t {
        CV = 0, // Output the same CV as CV1
        CV_INVO = 1, // Output the inverse of CV1, centered at the step offset
        CV_INV = 2, // Output the inverse of CV1, centered at 0
        CV_STEP_START = 3, // Output the step CV start value and hold it until next step
        GATE_STEP = 4, // Output a gate at each playing step
        GATE_STEP_INCL_RETRIGGERS = 5, // Output a gate at each playing step, including retriggers
        GATE_SEQUENCE = 6, // Output a gate at the start of the sequence

        MAX_OUTPUT2_MODE,
    };

    // Curve types for step transitions
    enum Curve : uint8_t {
        NONE = 0,
        FLAT = 1,
        RAMP_UP = 2,
        RAMP_DOWN = 3,
        EXP_UP = 4,
        EXP_DOWN = 5,
        TRIANGLE = 6,

        MAX_CURVE,
    };

    // Single step of the envelope sequence
    struct Step {
        int16_t scale; // Scaled step scale CV (by OFFSET_SCALE_INCREMENT)
        int16_t offset; // Scaled step offset CV (by OFFSET_SCALE_INCREMENT)
        uint8_t probability; // Probability 0-100% of this step being played
        uint8_t length; // Length 1-200% of envelope duration for this step
        uint8_t clocks : 3; // Number of clocks this step lasts for (0-7)
        uint8_t triggers : 3; // Number of times to trigger the step (0-7)
        uint8_t curve : 3; // Curve shape for the step
        uint8_t mod_mark : 1; // Whether this step is marked for modulation
    };

    const char* applet_name() {
        return "EnvSeq";
    }
    const uint8_t* applet_icon() { return PhzIcons::AD_EG; }

    void Start() {
        init_steps();
        Reset();
    }

    void Reset() {
        // Only set flag, keep step playing until next clock
        reset_flag = true;
    }

    void Controller()
    {
        const uint32_t this_tick = OC::CORE::ticks;

        if (Clock(1)) {
            Reset();
        }

        bool sequence_restarted = false;
        bool new_step = false;
        bool clocked = Clock(0);
        if (clocked) {
            clock_ticks = ClockCycleTicks(0);

            bool next_step = true;
            if (!reset_flag && step != -1) {
                // Currently playing a step
                step_clocks++;
                if (step < num_steps && step_clocks < steps[step].clocks + 1) {
                    // Step is still valid and clocks not passed, it should still be playing
                    next_step = false;
                }
            }

            if (next_step) {
                // Assume sequence restarted
                sequence_restarted = true;

                // Get the next step to play
                if (mod2_mode == Mod2Mode::STEP_SEL) {
                    const int8_t step_pv = step;
                    step = get_cv_step(In(1));
                    sequence_restarted = reset_flag || step_pv == -1 || step <= step_pv;
                } else {
                    step = reset_flag ? get_first_step() : get_next_step(step == -1 ? 0 : step, &sequence_restarted);
                }

                if (step != -1) {
                    new_step = true;
                    reset_flag = false;
                    step_start_tick = this_tick;
                    step_clocks = 0;
                    last_retrig_index = 0;
                }
            }
        }

        if (step == -1) {
            // No step to play
            return;
        }

        int mod1_cv = DetentedIn(0);
        int mod2_cv = DetentedIn(1);
        if (new_step) {
            // Store the modulation input value for step and sequence start
            mod_cv_step = mod1_cv;
            if (sequence_restarted) {
                mod_cv_seq = mod1_cv;
            }
        }

        // Apply the modulation mode
        if (mod1_mode == Mod1Mode::HOLD_STEP_START) {
            mod1_cv = mod_cv_step;
        } else if (mod1_mode == Mod1Mode::HOLD_SEQ_START) {
            mod1_cv = mod_cv_seq;
        }

        int cv_out = 0;
        const Step& s = steps[step];
        const int scale_cv = s.scale * OFFSET_SCALE_INCREMENT;
        const uint32_t total_step_ticks = (s.clocks + 1) * clock_ticks;
        const uint32_t step_end_tick = step_start_tick + total_step_ticks;
        const uint16_t raw_step_progress = this_tick >= step_end_tick ? 65535 : Proportion(this_tick - step_start_tick, total_step_ticks, 65535);
        const uint16_t effective_length = effective_step_length(s, mod2_cv);

        // Map the step length (1-200%) into the curve progression: <100% speeds the curve up, >100% slows it down
        uint16_t step_progress = raw_step_progress;
        if (effective_length != 100) {
            uint32_t scaled = (static_cast<uint32_t>(raw_step_progress) * 100) / effective_length;
            if (scaled > 65535) scaled = 65535;
            step_progress = static_cast<uint16_t>(scaled);
        }

        // Retrigger: restart the curve multiple times within the same step.
        bool retrig_edge = false;
        if (s.triggers) {
            const uint8_t segments = s.triggers + 1;
            const uint32_t scaled = static_cast<uint32_t>(step_progress) * segments; // Q16
            const uint8_t retrig_index = static_cast<uint8_t>(scaled >> 16); // 0..segments-1
            retrig_edge = (!new_step && retrig_index != last_retrig_index);
            if (retrig_edge) last_retrig_index = retrig_index;

            // Intra-segment progress (Q16). Preserve "end" as 65535 rather than wrapping.
            if (step_progress != 65535) {
                step_progress = static_cast<uint16_t>(scaled);
            }
        }

        switch (s.curve) {
        case Curve::FLAT:
            cv_out = scale_cv;
            break;
        case Curve::RAMP_UP:
            // Ramp up from 0 to scale_cv
            cv_out = (scale_cv * step_progress) >> 16;
            break;
        case Curve::RAMP_DOWN:
            // Ramp down from scale_cv to 0
            cv_out = (scale_cv * ((1 << 16) - step_progress)) >> 16;
            break;
        case Curve::EXP_UP: {
            // Use 64-bit to keep precision; step_progress is Q16
            const int64_t t = step_progress;
            cv_out = static_cast<int>((static_cast<int64_t>(scale_cv) * t * t) >> 32);
            break;
        }
        case Curve::EXP_DOWN: {
            const int64_t inv = (1 << 16) - step_progress; // Q16
            cv_out = static_cast<int>((static_cast<int64_t>(scale_cv) * inv * inv) >> 32);
            break;
        }
        case Curve::TRIANGLE:
            // Full-height triangle: scale first half up, second half down
            if (step_progress < 32768) {
                cv_out = (scale_cv * (step_progress << 1)) >> 16;
            } else {
                cv_out = (scale_cv * ((65535 - step_progress) << 1)) >> 16;
            }
            break;
        }

        // Add the offset and modulation input
        int offset_cv = static_cast<int>(s.offset) * OFFSET_SCALE_INCREMENT;
        cv_out += offset_cv + mod1_cv;
        if (mod2_mode == Mod2Mode::MOD2 || (mod2_mode == Mod2Mode::MOD_MARK && s.mod_mark)) {
            // Modulate by the CV2 input
            cv_out += mod2_cv;
        }

        cv_out = constrain(cv_out, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
        if (s.curve == Curve::NONE) {
            cv_out = 0;
        } else if (new_step) {
            // Store the current step CV start value
            cv_step_start = cv_out;
        }

        // Output the current step CV value
        Out(0, cv_out);

        // Output to CV2
        bool gate_out = false, do_gate_out = false;
        switch (output2_mode) {
        case Output2Mode::CV:
            Out(1, cv_out);
            break;
        case Output2Mode::CV_INVO:
            Out(1, constrain(offset_cv * 2 - cv_out, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV));
            break;
        case Output2Mode::CV_INV:
            Out(1, constrain(-cv_out, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV));
            break;
        case Output2Mode::CV_STEP_START:
            Out(1, cv_step_start);
            break;
        case Output2Mode::GATE_STEP:
            gate_out = true;
            do_gate_out = new_step && s.curve != Curve::NONE;
            break;
        case Output2Mode::GATE_STEP_INCL_RETRIGGERS:
            gate_out = true;
            do_gate_out = (s.curve != Curve::NONE) && (new_step || retrig_edge);
            break;
        case Output2Mode::GATE_SEQUENCE:
            gate_out = true;
            do_gate_out = sequence_restarted && s.curve != Curve::NONE;
            break;
        }

        if (gate_out) {
            if (do_gate_out) {
                // Mark when gate should be stopped
                gate_stop_tick = this_tick + GATE_STOP_TICKS;
            }
            GateOut(1, this_tick <= gate_stop_tick);
        }
    }

    void View() {
        draw_interface();
    }

    void OnButtonPress() {
        if (random_cursor == RandomCursor::RANDOM_APPLY) {
            // Randomize the steps and return to the main view
            randomize_steps();
            random_cursor = RandomCursor::MAX_RANDOM_CURSOR;
            return;
        } else if (random_cursor != RandomCursor::MAX_RANDOM_CURSOR) {
            // Keep random view open and toggle the cursor so it edits the current option
            CursorToggle();
            return;
        }

        if (cursor == EnvSeqCursor::RANDOM) {
            // Open random view
            random_cursor = RandomCursor::RANDOM_APPLY;
            return;
        }

        if (cursor == EnvSeqCursor::RESET) {
            Reset();
            return;
        }
        if (cursor == EnvSeqCursor::INIT) {
            init_steps();
            return;
        }


        CursorToggle();
    }

    /* Pressing the select button after highlighting a parameter for editing
     * can invoke a secondary action here. By default, it just cancels editing. */
    // void AuxButton() { }

    void OnEncoderMove(int direction) {
        if (random_cursor < MAX_RANDOM_CURSOR) {
            if (!EditMode()) {
                // Move the cursor to the next random option. Going past the available options will return to the main view.
                MoveCursor(random_cursor, direction, RandomCursor::MAX_RANDOM_CURSOR);
                return;
            }

            switch (random_cursor) {
            case RandomCursor::RANDOM_OFFSETS:
                random_offsets = !random_offsets;
                break;
            case RandomCursor::RANDOM_SCALES:
                random_scales = !random_scales;
                break;
            case RandomCursor::RANDOM_CURVES:
                random_curves = !random_curves;
                break;
            case RandomCursor::RANDOM_LENGTHS:
                random_lengths = !random_lengths;
                break;
            case RandomCursor::RANDOM_TRIGGERS:
                random_triggers = !random_triggers;
                break;
            case RandomCursor::RANDOM_CLOCKS:
                random_clocks = !random_clocks;
                break;
            case RandomCursor::RANDOM_MOD_MARKS:
                random_mod_marks = !random_mod_marks;
                break;
            }
            
            return;
        }

        if (!EditMode()) {
            MoveCursor(cursor, direction, EnvSeqCursor::MAX_CURSOR - 1);
            return;
        }

        switch (cursor) {
        case EnvSeqCursor::MOD1_MODE:
            mod1_mode = (Mod1Mode)constrain(mod1_mode + direction, 0, Mod1Mode::MAX_MOD1_MODE - 1);
            break;
        case EnvSeqCursor::MOD2_MODE:
            mod2_mode = (Mod2Mode)constrain(mod2_mode + direction, 0, Mod2Mode::MAX_MOD2_MODE - 1);
            break;
        case EnvSeqCursor::OUTPUT2_MODE:
            output2_mode = (Output2Mode)constrain(output2_mode + direction, 0, Output2Mode::MAX_OUTPUT2_MODE - 1);
            break;
        case EnvSeqCursor::NUM_STEPS:
            num_steps = constrain(num_steps + direction, 1, MAX_NUM_STEPS);
            break;
        case EnvSeqCursor::STEP_VIEW:
            step_view = constrain(step_view + direction, 0, MAX_NUM_STEPS - 1);
            break;
        case EnvSeqCursor::STEP_PROBABILITY:
            steps[step_view].probability = constrain(steps[step_view].probability + direction, 0, 100);
            break;
        case EnvSeqCursor::STEP_CURVE:
            steps[step_view].curve = (Curve)constrain(steps[step_view].curve + direction, 0, Curve::MAX_CURVE - 1);
            break;
        case EnvSeqCursor::STEP_OFFSET:
            steps[step_view].offset = constrain(steps[step_view].offset + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
            break;
        case EnvSeqCursor::STEP_SCALE:
            steps[step_view].scale = constrain(steps[step_view].scale + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
            break;
        case EnvSeqCursor::STEP_TRIGGERS:
            steps[step_view].triggers = constrain(steps[step_view].triggers + direction, 0, 7);
            break;
        case EnvSeqCursor::STEP_CLOCKS:
            steps[step_view].clocks = constrain(steps[step_view].clocks + direction, 0, 7);
            break;
        case EnvSeqCursor::STEP_LENGTH:
            steps[step_view].length = constrain(steps[step_view].length + direction, 1, 200);
            break;
        case EnvSeqCursor::STEP_MOD_MARK:
            steps[step_view].mod_mark = !steps[step_view].mod_mark;
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 5}, num_steps - 1);
        Pack(data, PackLocation {5, 3}, mod1_mode);
        Pack(data, PackLocation {8, 3}, mod2_mode);
        Pack(data, PackLocation {11, 3}, output2_mode);
        Pack(data, PackLocation {14, 1}, random_offsets);
        Pack(data, PackLocation {15, 1}, random_scales);
        Pack(data, PackLocation {16, 1}, random_curves);
        Pack(data, PackLocation {17, 1}, random_lengths);
        Pack(data, PackLocation {18, 1}, random_triggers);
        Pack(data, PackLocation {19, 1}, random_clocks);
        Pack(data, PackLocation {20, 1}, random_mod_marks);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        num_steps = constrain(Unpack(data, PackLocation {0, 5}) + 1, 1, MAX_NUM_STEPS);
        mod1_mode = (Mod1Mode)constrain(Unpack(data, PackLocation {5, 3}), 0, Mod1Mode::MAX_MOD1_MODE - 1);
        mod2_mode = (Mod2Mode)constrain(Unpack(data, PackLocation {8, 3}), 0, Mod2Mode::MAX_MOD2_MODE - 1);
        output2_mode = (Output2Mode)constrain(Unpack(data, PackLocation {11, 3}), 0, Output2Mode::MAX_OUTPUT2_MODE - 1);
        random_offsets = Unpack(data, PackLocation {14, 1});
        random_scales = Unpack(data, PackLocation {15, 1});
        random_curves = Unpack(data, PackLocation {16, 1});
        random_lengths = Unpack(data, PackLocation {17, 1});
        random_triggers = Unpack(data, PackLocation {18, 1});
        random_clocks = Unpack(data, PackLocation {19, 1});
        random_mod_marks = Unpack(data, PackLocation {20, 1});
        Reset();
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = mod1_mode_string(mod1_mode);
        help[HELP_CV2]      = mod2_mode_string(mod2_mode);
        help[HELP_OUT1]     = "Output";
        help[HELP_OUT2]     = output2_mode_string(output2_mode);
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
        //                  "---------------------" <-- Extra text size guide
    }

private:
    int8_t cursor = 0, random_cursor = MAX_RANDOM_CURSOR;

    uint8_t num_steps = 8; // How many steps of the generated envelope to play before looping
    Mod1Mode mod1_mode = Mod1Mode::MOD1; // Modulation mode for CV1
    Mod2Mode mod2_mode = Mod2Mode::LENGTH; // Modulation mode for CV2
    Output2Mode output2_mode = Output2Mode::GATE_STEP; // Mode for CV2 output

    bool random_offsets = true; // Whether to randomize step levels (offset)
    bool random_scales = true; // Whether to randomize step levels (scale)
    bool random_curves = true; // Whether to randomize step curves
    bool random_lengths = false; // Whether to randomize step lengths
    bool random_triggers = false; // Whether to randomize the triggers flag for each step
    bool random_clocks = false; // Whether to randomize the clocks flag for each step
    bool random_mod_marks = false; // Whether to randomize the mod_mark flag for each step
    
    bool reset_flag = false; // Prevent stepping forward after a reset
    int8_t step = -1; // Current step
    int8_t step_view = 0; // Viewed step for cursor
    Step steps[MAX_NUM_STEPS]; // Steps of the envelope sequence

    uint32_t clock_ticks = 0; // Ticks between the last two clock inputs
    uint32_t step_start_tick = 0; // Tick when the current step started
    uint8_t step_clocks = 0; // Number of clocks since the current step started
    uint32_t gate_stop_tick = 0; // Tick when gate should be stopped
    uint8_t last_retrig_index = 0; // Which retrigger segment we're currently in (for gate pulses)

    int16_t mod_cv_step = 0; // Value of the modulation input at step start
    int16_t mod_cv_seq = 0; // Value of the modulation input at sequence start
    int16_t cv_step_start = 0; // Value of the step CV start value

    // Effective step length (1-200%) including optional global CV2 scaling.
    uint16_t effective_step_length(const Step& s, int mod2_cv) const {
        if (mod2_mode != Mod2Mode::LENGTH) {
            return s.length;
        }

        // Map CV2 (bipolar) to +/-100% change, then convert to a 1..200% scaler.
        const int cv_delta_pct = Proportion(mod2_cv, HEMISPHERE_MAX_INPUT_CV, 100); // -100..+100
        const int cv_scale_pct = constrain(100 + cv_delta_pct, 1, 200);

        // Multiply per-step length by CV scale and clamp to the same 1..200% range.
        const int eff = constrain(((static_cast<int>(s.length) * cv_scale_pct) + 50) / 100, 1, 200);
        return static_cast<uint16_t>(eff);
    }

    void draw_interface() {
        if (random_cursor < MAX_RANDOM_CURSOR) {
            draw_random_view();
            return;
        }

        if (cursor >= EnvSeqCursor::STEP_VIEW) {
            draw_step_view();
            return;
        }

        gfxStartCursor(0, 15);
        if (DetentedIn(0)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPos(10, 15);
        gfxPrint(mod1_mode_string(mod1_mode));
        gfxEndCursor(cursor == EnvSeqCursor::MOD1_MODE);

        gfxStartCursor(0, 25);
        if (DetentedIn(1)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPos(10, 25);
        gfxPrint(mod2_mode_string(mod2_mode));
        gfxEndCursor(cursor == EnvSeqCursor::MOD2_MODE);

        gfxStartCursor(0, 35);
        gfxPrintIcon(output2_mode == Output2Mode::GATE_STEP || output2_mode == Output2Mode::GATE_STEP_INCL_RETRIGGERS || output2_mode == Output2Mode::GATE_SEQUENCE ? GATE_ICON : CV_ICON);
        gfxPrint(output2_mode_string(output2_mode));
        gfxEndCursor(cursor == EnvSeqCursor::OUTPUT2_MODE);
    
        gfxStartCursor(0, 45);
        gfxPrintIcon(RANDOM_ICON);
        gfxPrint("Random");
        gfxEndCursor(cursor == EnvSeqCursor::RANDOM);

        gfxIcon(0, 55, PhzIcons::stairs);
        if (step >= 0) {
            gfxPrint(10 + pad(10, step + 1), 55, step + 1);
        }
        gfxPrint(22, 55, "/");
        gfxStartCursor(28, 55);
        gfxPrint(28 + pad(10, num_steps), 55, num_steps);
        gfxPos(40, 55);
        gfxEndCursor(cursor == EnvSeqCursor::NUM_STEPS);

        gfxStartCursor(46, 55);
        gfxPrintIcon(RESET_ICON);
        gfxEndCursor(cursor == EnvSeqCursor::RESET);

        gfxStartCursor(56, 55);
        gfxPrintIcon(LOOP_ICON);
        gfxEndCursor(cursor == EnvSeqCursor::INIT);
    }

    void draw_step_view() {
        if (cursor >= EnvSeqCursor::STEP_TRIGGERS) {
            draw_step_view_second_page();
            return;
        }

        const Step& s = steps[step_view];
        int display_step = step_view + 1;

        gfxIcon(0, 15, PhzIcons::stairs);
        gfxStartCursor(10, 15);
        gfxPrint(10 + pad(10, display_step), 15, display_step);
        gfxPos(22, 15);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_VIEW);

        gfxIcon(28, 15, RANDOM_ICON);
        gfxStartCursor(38, 15);
        gfxPrint(38 + pad(100, s.probability), 15, s.probability);
        gfxPos(56, 15);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_PROBABILITY);
    
        gfxIcon(0, 25, WAVEFORM_ICON);
        gfxStartCursor(10, 25);
        gfxPrint(10, 25, curve_string(s.curve));
        gfxPos(63, 25);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_CURVE);

        gfxPrint(0, 35, "Off");
        gfxStartCursor(18, 35);
        gfxPrintVoltage(s.offset * OFFSET_SCALE_INCREMENT);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_OFFSET);

        gfxPrint(0, 45, "Scl");
        gfxStartCursor(18, 45);
        gfxPrintVoltage(s.scale * OFFSET_SCALE_INCREMENT);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_SCALE);
    }

    void draw_step_view_second_page() {
        const Step& s = steps[step_view];

        gfxIcon(0, 15, GATE_ICON);
        gfxStartCursor(10, 15);
        gfxPrint(s.triggers + 1);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_TRIGGERS);

        gfxIcon(22, 15, CLOCK_ICON);
        gfxStartCursor(32, 15);
        gfxPrint(s.clocks + 1);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_CLOCKS);

        gfxIcon(0, 25, LENGTH_ICON);
        gfxStartCursor(10, 25);
        gfxPrint(10 + pad(100, s.length), 25, s.length);
        gfxPos(28, 25);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_LENGTH);

        gfxIcon(0, 35, s.mod_mark ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, 35);
        gfxPrint("Mod2");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_MOD_MARK);
    }

    void draw_random_view() {
        gfxIcon(0, 15, random_offsets ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, 15);
        gfxPrint("Ofs");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_OFFSETS);

        gfxIcon(31, 15, random_scales ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, 15);
        gfxPrint("Scl");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_SCALES);

        gfxIcon(0, 25, random_curves ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, 25);
        gfxPrint("Crv");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_CURVES);

        gfxIcon(31, 25, random_lengths ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, 25);
        gfxPrint("Len");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_LENGTHS);

        gfxIcon(0, 35, random_triggers ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, 35);
        gfxPrint("Trg");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_TRIGGERS);

        gfxIcon(31, 35, random_clocks ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, 35);
        gfxPrint("Clk");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_CLOCKS);

        gfxIcon(0, 45, random_mod_marks ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, 45);
        gfxPrint("Mod");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_MOD_MARKS);

        gfxStartCursor(0, 55);
        gfxPrintIcon(RANDOM_ICON);
        gfxPrint("RND");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_APPLY);
    }

    void init_steps() {
        int scale = HEMISPHERE_MAX_CV / (2 * OFFSET_SCALE_INCREMENT);
        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            steps[i].curve = Curve::RAMP_DOWN;
            steps[i].scale = scale;
            steps[i].offset = 0;
            steps[i].probability = 100;
            steps[i].clocks = 0;
            steps[i].length = 100;
            steps[i].triggers = 0;
            steps[i].mod_mark = false;
        }
    }

    void randomize_steps() {
        const int max_units = HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT;
        const int offset_max = max_units * 0.2;

        // Randomize scale only between 20% and 50%
        const int scale_min = max_units * 0.2;
        const int scale_max = max_units * 0.5;

        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            if (random_offsets) {
                steps[i].offset = random(0, offset_max + 1);
            }
            if (random_scales) {
                steps[i].scale = random(scale_min, scale_max + 1);
            }
            if (random_curves) {
                steps[i].curve = (Curve)random(Curve::MAX_CURVE);
            }
            if (random_lengths) {
                steps[i].length = random(75, 151); // Randomize length only between 75% and 150%
            }
            if (random_mod_marks) {
                steps[i].mod_mark = random(0, 2);
            }
            if (random_triggers && random(0, 4) == 0) {
                steps[i].triggers = random(0, 3);
            }
            if (random_clocks && random(0, 4) == 0) {
                steps[i].clocks = random(0, 3);
            }
        }
    }

    // Return the first step with probability > 0, or -1 if no such step exists
    int get_first_step() {
        for (int step = 0; step < num_steps; ++step) {
            if (steps[step].probability > 0) {
                return step;
            }
        }

        // No step with probability > 0 found, return -1
        return -1;
    }

    // Map CV in the 0–5V range to a step index 0..(num_steps-1).
    int8_t get_cv_step(int cv) const {
        if (num_steps <= 1) {
            return 0;
        }

        cv = constrain(cv, 0, STEPSEL_MAX_CV);
        const int idx = (static_cast<int64_t>(cv) * num_steps) / (static_cast<int64_t>(STEPSEL_MAX_CV) + 1);
        
        return static_cast<int8_t>(constrain(idx, 0, num_steps - 1));
    }

    // Return the next step with probability > 0 which got lucky, or the next step with probability > 0, or -1 if no such step exists
    int get_next_step(int current_step, bool* sequence_restarted) {
        if (current_step >= num_steps) {
            current_step = num_steps - 1;
        }

        int next_step = -1;
        *sequence_restarted = false;

        // Scan forward with wraparound, considering only steps with probability > 0
        for (int offset = 1; offset <= num_steps; ++offset) {
            const int candidate = (current_step + offset) % num_steps;
            const uint8_t probability = steps[candidate].probability;
            if (probability == 0) {
                // Skip step with probability 0
                continue;
            }

            if (next_step == -1) {
                // First step with probability > 0 found, save it
                next_step = candidate;
            }

            if (candidate <= current_step) {
                // Sequence is restarting, set flag
                *sequence_restarted = true;
            }

            if (probability >= 100 || random(0, 100) < probability) {
                // Probability is 100% or this step got lucky, play it
                return candidate;
            }
        }

        // No step with probability > 0 which got lucky, return the next step with probability > 0, or -1 if no such step exists
        return next_step;
    }

    const char* mod1_mode_string(Mod1Mode mode) {
        switch (mode) {
        case Mod1Mode::MOD1:
            return "Mod";
        case Mod1Mode::HOLD_STEP_START:
            return "H step";
        case Mod1Mode::HOLD_SEQ_START:
            return "H seq";
        default:
            return "";
        }
    }

    const char* mod2_mode_string(Mod2Mode mode) {
        switch (mode) {
        case Mod2Mode::LENGTH:
            return "Length";
        case Mod2Mode::STEP_SEL:
            return "StepSel";
        case Mod2Mode::MOD2:
            return "Mod";
        case Mod2Mode::MOD_MARK:
            return "ModMark";
        default:
            return "";
        }
    }

    const char* output2_mode_string(Output2Mode mode) {
        switch (mode) {
        case Output2Mode::CV:
            return "Cpy";
        case Output2Mode::CV_INVO:
            return "CpyInvO";
        case Output2Mode::CV_INV:
            return "CpyInv";
        case Output2Mode::CV_STEP_START:
            return "HStart";
        case Output2Mode::GATE_STEP:
            return "Step";
        case Output2Mode::GATE_STEP_INCL_RETRIGGERS:
            return "StepTrg";
        case Output2Mode::GATE_SEQUENCE:
            return "Seq";
        default:
            return "";
        }
    }

    const char* curve_string(Curve curve) {
        switch (curve) {
        case Curve::NONE:
            return "None";
        case Curve::FLAT:
            return "Flat";
        case Curve::RAMP_UP:
            return "RampUp";
        case Curve::RAMP_DOWN:
            return "RampDown";
        case Curve::EXP_UP:
            return "ExpUp";
        case Curve::EXP_DOWN:
            return "ExpDown";
        case Curve::TRIANGLE:
            return "Triangle";
        default:
            return "";
        }
    }
};
