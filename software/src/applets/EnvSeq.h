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

#include "../HSEnvSeqManager.h"
#include "../OC_menus.h"
#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"

class EnvSeq : public HemisphereApplet {
public:
    static constexpr int MAX_NUM_STEPS = 32;
    static constexpr int OFFSET_SCALE_INCREMENT = 64;
    static constexpr int GATE_STOP_TICKS = 5 * HEMISPHERE_CLOCK_TICKS; // 5ms
    static constexpr int STEPSEL_MAX_CV = 5 * ONE_OCTAVE; // unipolar 0-5v

    enum EnvSeqCursor {
        MOD1_MODE, LINK,
        MOD2_MODE,
        OUTPUT2_MODE,
        RANDOM, TRIGGER2,
        NUM_STEPS, RESET, INIT,

        STEP_VIEW, STEP_SHAPE,
        STEP_PARAM, STEP_PARAM_VALUE,

        MAX_CURSOR,
    };

    enum StepParamCursor {
        STEP_PARAM_OFFSET,
        STEP_PARAM_AMP,
        STEP_PARAM_WAVEFORM_OFFSET,
        STEP_PARAM_WAVEFORM_REVERT,
        STEP_PARAM_WAVEFORM_INVERT,
        STEP_PARAM_WAVEFORM_OPTION,

        STEP_PARAM_TRIGGERS,
        STEP_PARAM_CLOCKS,
        STEP_PARAM_LENGTH,
        STEP_PARAM_PROBABILITY,
        STEP_PARAM_RETRIGGER_LEVEL,
        STEP_PARAM_MOD_MARK,
        STEP_PARAM_GATE_LENGTH,
        STEP_PARAM_COPY,
        STEP_PARAM_PASTE,

        MAX_STEP_PARAM_CURSOR,
    };
    static constexpr const char* const step_param_names[MAX_STEP_PARAM_CURSOR] = {
        "Offset", "Amp", "WaveOff", "Revert",
        "Invert", "Option", "Triggers", "Clocks",
        "Length", "Prob", "RetrgLvl", "ModMark",
        "GateLen", "Copy", "Paste",
    };

    enum LinkedCursor {
        LINKED_MOD1_MODE,
        LINKED_MOD2_MODE,
        LINKED_OUTPUT1_MODE,
        LINKED_OUTPUT2_MODE,
        UNLINK,

        MAX_LINKED_CURSOR,
    };

    enum RandomCursor {
        RANDOM_OFFSETS,
        RANDOM_AMPS,
        RANDOM_SHAPES,
        RANDOM_VOSC,
        RANDOM_LENGTHS,
        RANDOM_TRIGGERS,
        RANDOM_CLOCKS,
        RANDOM_MOD_MARKS,
        RANDOM_RETRIGGER_LEVELS,
        RANDOM_GATE_LENGTHS,
        RANDOM_APPLY,
        RANDOM_CANCEL,

        MAX_RANDOM_CURSOR,
    };

    // Modulation mode for CV1
    enum ModulationMode : uint8_t {
        MOD = 0, // Modulate all steps
        HOLD_STEP_START = 1, // Sample and hold the step start value as the modulation input
        HOLD_SEQ_START = 2, // Sample and hold the sequence start value as the modulation input

        MAX_MODULATION_MODE,
    };

    // Shape types for step transitions
    enum Shape : uint8_t {
        HOLD = 0,
        ZERO = 1,
        FLAT = 2,
        EXP_DOWN = 3,
        EXP_UP = 4,
        RAMP_DOWN = 5,
        RAMP_UP = 6,
        LOG_DOWN = 7,
        LOG_UP = 8,
        VOSC = 9,
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
        manager.Register(hemisphere);
        bool linked = manager.IsLinked(hemisphere);
        if (manager.CanLink(hemisphere)) {
            if (linked) {
                controller_linked();
                return;
            }

            // Unlinked, close linked view
            linked_cursor = MAX_LINKED_CURSOR;
        } else {
            // Fix if unlinked
            linked_cursor = MAX_LINKED_CURSOR;
            if (cursor == EnvSeqCursor::LINK) {
                cursor = EnvSeqCursor::MOD1_MODE;
            }
        }

        const uint32_t this_tick = OC::CORE::ticks;
        bool clock2 = Clock(1);
        if (!trigger2 && clock2) {
            // Trigger on TR2 as reset
            Reset();
        }

        bool sequence_restarted = false;
        bool new_step = false;
        bool clocked = Clock(0);
        if (clocked) {
            clock_ticks = ClockCycleTicks(0);
        }

        // Step if clocked or trigger2 and clock2
        if ((!trigger2 && clocked) || (trigger2 && clock2)) {
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
                EnvSeqManager::Output step_sel_mod = get_modulation(EnvSeqManager::ModulationMode::STEP_SEL);
                if (step_sel_mod.is_cv) {
                    const int8_t step_pv = step;
                    step = get_cv_step(step_sel_mod.cv);
                    sequence_restarted = reset_flag || step_pv == -1 || step <= step_pv;
                } else {
                    step = reset_flag ? get_first_step() : get_next_step(step == -1 ? 0 : step, sequence_restarted);
                }

                if (step != -1) {
                    new_step = true;
                    reset_flag = false;
                    step_start_tick = this_tick;
                    step_clocks = 0;
                    last_retrig_index = 0;

                    if (follow) {
                      step_view = step;
                      osc_draw_reinit = true;
                    }
                }
            }
        }

        if (step == -1) {
            // No step to play
            return;
        }

        const EnvSeqManager::Step& s = steps[step];
        const uint32_t total_step_ticks = update_step_progress(this_tick);
        uint32_t trigger_start_tick = step_start_tick; // Tick when the current trigger segment started. Will be updated if triggers are used.
        uint32_t total_trigger_ticks = total_step_ticks; // Number of ticks for the current trigger segment. Will be updated if triggers are used.

        // Retrigger: restart the shape multiple times within the same step.
        bool retrig_edge = false;
        uint8_t retrig_segments = 1;
        uint8_t retrig_index = 0;
        if (s.triggers) {
            retrig_segments = s.triggers + 1;
            const uint32_t scaled = static_cast<uint32_t>(step_progress) * retrig_segments; // Q16
            retrig_index = static_cast<uint8_t>(scaled >> 16); // 0..segments-1
            retrig_edge = (!new_step && retrig_index != last_retrig_index);
            if (retrig_edge) last_retrig_index = retrig_index;

            if (step_progress != 65535) {
                step_progress = static_cast<uint16_t>(scaled);
            }

            // Trigger is used, update the trigger start and total ticks
            total_trigger_ticks = total_step_ticks / retrig_segments;
            trigger_start_tick = step_start_tick + total_trigger_ticks * retrig_index;
        }

        int offset_cv = get_offset_cv(get_mod1_cv(new_step, sequence_restarted));
        int16_t amp_cv = s.amp * OFFSET_SCALE_INCREMENT; // CV scaled amplitude (not including offset or waveform offset)
        const uint16_t amp_abs = abs(amp_cv);
        const uint16_t amp_mag = amp_abs / 2;

        int16_t cv = 0;
        bool use_vosc = true;
        switch (s.shape) {
        case Shape::HOLD:
            use_vosc = false;
            offset_cv = amp_cv = 0;
            cv = last_output_cv;
            break;
        case Shape::ZERO:
            use_vosc = false;
            offset_cv = amp_cv = cv = 0;
            break;
        case Shape::FLAT:
            use_vosc = false;
            cv = amp_abs;
            break;
        default:
            // All other shapes are handled by the VOSC oscillator
            break;
        }

        const bool is_positive = amp_cv >= 0;

        uint16_t waveform_offset = 0;
        if (use_vosc) {
            // Prepare the parameters for the VOSC oscillator
            uint8_t offset = s.waveform_offset;
            bool revert = s.waveform_revert;
            bool invert = s.waveform_invert;
            EnvSeqManager::Option option = s.waveform_option;
            uint16_t number = prepare_shape(s.shape, offset, revert, invert, option);

            // Initialize the VOSC oscillator if needed
            if (new_step || osc_reinit) {
                osc_reinit = false;
                osc = WaveformManager::VectorOscillatorFromWaveform(number);
                osc.Sustain();
                osc.Cycle(0);
            }
            osc.SetScale(amp_mag);
    
            // Drive the waveform by step_progress (0..3600 tenths of a degree).
            const int phase_deg_tenths = (static_cast<uint32_t>(revert ? 65535 - step_progress : step_progress) * 3600) / 65535;
            cv = osc.Phase(phase_deg_tenths) + amp_mag;

            // Set waveform offset based on amplitude
            waveform_offset = Proportion(offset, 100, amp_abs);
    
            // Apply the waveform option
            switch (option) {
            case EnvSeqManager::Option::NO_OPTION:
                break;
            case EnvSeqManager::Option::FOLD_UP:
                if (cv < waveform_offset) {
                    cv = 2 * waveform_offset - cv;
                }
                break;
            case EnvSeqManager::Option::FOLD_DOWN:
                if (cv > waveform_offset) {
                    cv = 2 * waveform_offset - cv;
                }
                break;
            case EnvSeqManager::Option::ZERO_UP:
                if (cv > waveform_offset) {
                    cv = waveform_offset;
                }
                break;
            case EnvSeqManager::Option::ZERO_DOWN:
                if (cv < waveform_offset) {
                    cv = waveform_offset;
                }
                break;
            case EnvSeqManager::Option::MAX_OPTIONS:
                break;
            }

            // Invert the CV if needed (vertical flip)
            if (invert) {
                cv = 2 * amp_mag - cv;
            }
        }

        // Retrigger level: scale amplitude across retriggers (-15..15)
        if (retrig_segments > 1) {
            EnvSeqManager::Output lvl_mod = get_modulation(EnvSeqManager::ModulationMode::RETRIGGER_LEVEL, true);
            const int lvl = constrain(
                s.retrigger_level + (lvl_mod.is_cv ? Proportion(lvl_mod.cv, HEMISPHERE_MAX_INPUT_CV, 15) : 0),
                -15,
                15
            );
            int lvl_pct = 100;
            if (lvl > 0) {
                const int lvl_target = 100 - (lvl * 100) / 15;
                lvl_pct = 100 - ((100 - lvl_target) * retrig_index) / (retrig_segments - 1);
            } else if (lvl < 0) {
                const int lvl_start = 100 - ((-lvl) * 100) / 15;
                lvl_pct = lvl_start + ((100 - lvl_start) * retrig_index) / (retrig_segments - 1);
            }
            cv = (static_cast<int32_t>(cv) * lvl_pct) / 100;
        }

        // Output the step CV value
        const int output_cv = offset_cv + (is_positive ? cv : -cv);
        Out(0, constrain(output_cv, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV));

        if (s.shape != Shape::HOLD) {
            last_output_cv = output_cv;
        }

        if (new_step) {
            // Store the current step CV start value
            cv_step_start = output_cv;
        }

        EnvSeqManager::LinkedData *linked_data = manager.GetLinkedData(hemisphere);

        // Output to CV2 and linked outputs
        const uint8_t output_count = manager.IsLinked(hemisphere) ? 3 : 1;
        for (uint8_t i = 0; i < output_count; i++) {
            EnvSeqManager::Output &output = (i == 0) ? output2 : linked_data[i - 1].output;
            bool do_gate = false;
            const EnvSeqManager::OutputMode output_mode = i == 0 ? output2_mode : linked_data[i - 1].modulation.output_mode;

            switch (output_mode) {
            case EnvSeqManager::OutputMode::COPY:
                output.is_cv = true;
                output.cv = output_cv;
                break;
            case EnvSeqManager::OutputMode::INV:
                output.is_cv = true;
                output.cv = offset_cv + (is_positive ? (amp_abs - cv) : -(amp_abs - cv));
                break;
            case EnvSeqManager::OutputMode::INVO:
                output.is_cv = true;
                output.cv = offset_cv + (is_positive ? (waveform_offset - cv) : -(waveform_offset - cv));
                break;
            case EnvSeqManager::OutputMode::CV_STEP_START:
                output.is_cv = true;
                output.cv = cv_step_start;
                break;
            case EnvSeqManager::OutputMode::GATE_STEP:
                output.is_cv = false;
                do_gate = new_step && s.shape != Shape::HOLD && s.shape != Shape::ZERO;
                output.cv = do_gate * calculate_gate_ticks(this_tick, step_start_tick, total_step_ticks, s.gate_length);
                break;
            case EnvSeqManager::OutputMode::GATE_STEP_INCL_RETRIGGERS:
                output.is_cv = false;
                do_gate = s.shape != Shape::HOLD && s.shape != Shape::ZERO && (new_step || retrig_edge);
                output.cv = do_gate * calculate_gate_ticks(this_tick, trigger_start_tick, total_trigger_ticks, s.gate_length);
                break;
            case EnvSeqManager::OutputMode::GATE_SEQUENCE:
                output.is_cv = false;
                do_gate = sequence_restarted && s.shape != Shape::HOLD && s.shape != Shape::ZERO;
                output.cv = do_gate * calculate_gate_ticks(this_tick, step_start_tick, total_step_ticks, s.gate_length);
                break;
            case EnvSeqManager::OutputMode::MAX_OUTPUT_MODE:
                break;
            }

            if (i == 0) {
                if (output.is_cv) {
                    Out(1, output.cv);
                } else if (output.cv > 0) {
                    ClockOut(1, output.cv);
                    output.cv = 0;
                }
            }
        }
    }

    void Unload() {
        manager.Unload(hemisphere);
    }

    void View() {
        draw_interface();
    }

    void OnButtonPress() {
        if (random_menu_active) {
            switch (random_menu_cursor.cursor_pos()) {
            case RandomCursor::RANDOM_APPLY:
                // Randomize the steps
                randomize_steps();
                osc_draw_reinit = true;
                // No break so it falls through and returns to the main view
            case RandomCursor::RANDOM_CANCEL:
                random_menu_active = false;
                return;
            case RandomCursor::RANDOM_OFFSETS:
                random_offsets = !random_offsets;
                return;
            case RandomCursor::RANDOM_AMPS:
                random_amps = !random_amps;
                return;
            case RandomCursor::RANDOM_SHAPES:
                random_shapes = !random_shapes;
                return;
            case RandomCursor::RANDOM_VOSC:
                random_vosc = !random_vosc;
                return;
            case RandomCursor::RANDOM_LENGTHS:
                random_lengths = !random_lengths;
                return;
            case RandomCursor::RANDOM_TRIGGERS:
                random_triggers = !random_triggers;
                return;
            case RandomCursor::RANDOM_CLOCKS:
                random_clocks = !random_clocks;
                return;
            case RandomCursor::RANDOM_MOD_MARKS:
                random_mod_marks = !random_mod_marks;
                return;
            case RandomCursor::RANDOM_RETRIGGER_LEVELS:
                random_retrigger_levels = !random_retrigger_levels;
                return;
            case RandomCursor::RANDOM_GATE_LENGTHS:
                random_gate_lengths = !random_gate_lengths;
                return;
            }
        }

        if (linked_cursor == LinkedCursor::UNLINK) {
            // Unlink and return to the main view
            manager.SetLink(hemisphere, false);
            linked_cursor = LinkedCursor::MAX_LINKED_CURSOR;
            return;
        }

        if (linked_cursor != LinkedCursor::MAX_LINKED_CURSOR || random_menu_active) {
            // Keep other view open and toggle the cursor so it edits the current option
            CursorToggle();
            return;
        }

        switch (cursor) {
        case EnvSeqCursor::LINK:
            // Link and open linked view
            manager.SetLink(hemisphere, true);
            linked_cursor = LinkedCursor::UNLINK;
            return;

        case EnvSeqCursor::RANDOM:
            // Open random view
            random_menu_active = true;
            random_menu_cursor.Init(0, RandomCursor::MAX_RANDOM_CURSOR - 1);
            random_menu_cursor.Scroll(RandomCursor::RANDOM_APPLY);
            return;

        case EnvSeqCursor::TRIGGER2:
            trigger2 = !trigger2;
            return;

        case EnvSeqCursor::RESET:
            Reset();
            return;
        case EnvSeqCursor::INIT:
            init_steps();
            return;

        case EnvSeqCursor::STEP_PARAM_VALUE:
            switch (step_param_cursor) {
            case StepParamCursor::STEP_PARAM_WAVEFORM_REVERT:
                steps[step_view].waveform_revert = !steps[step_view].waveform_revert;
                return;
            case StepParamCursor::STEP_PARAM_WAVEFORM_INVERT:
                steps[step_view].waveform_invert = !steps[step_view].waveform_invert;
                return;
            case StepParamCursor::STEP_PARAM_MOD_MARK:
                steps[step_view].mod_mark = !steps[step_view].mod_mark;
                return;
            case StepParamCursor::STEP_PARAM_COPY:
                manager.CopyStep(steps[step_view]);
                return;
            case StepParamCursor::STEP_PARAM_PASTE:
                manager.PasteStep(steps[step_view]);
                osc_draw_reinit = true;
                if (step == step_view) {
                    osc_reinit = true;
                }
                return;
            }

          default:
              CursorToggle();
        }
    }

    void AuxButton() {
        if (cursor > EnvSeqCursor::STEP_VIEW) {
            step_select = !step_select;
        }
        if (cursor == EnvSeqCursor::STEP_VIEW) {
            follow = !follow;
        }
    }

    void OnEncoderMove(int direction) {
        if (linked_cursor < MAX_LINKED_CURSOR) {
            if (!EditMode()) {
                MoveCursor(linked_cursor, direction, LinkedCursor::MAX_LINKED_CURSOR - 1);
                return;
            }

            const EnvSeqManager::LinkedData* linked_data = manager.GetLinkedData(hemisphere);
            switch (linked_cursor) {
            case LinkedCursor::LINKED_MOD1_MODE:
                manager.SetModulationMode(hemisphere, 0, (EnvSeqManager::ModulationMode)constrain(linked_data[0].modulation.mode + direction, 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1));
                break;
            case LinkedCursor::LINKED_MOD2_MODE:
                manager.SetModulationMode(hemisphere, 1, (EnvSeqManager::ModulationMode)constrain(linked_data[1].modulation.mode + direction, 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1));
                break;
            case LinkedCursor::LINKED_OUTPUT1_MODE:
                manager.SetModulationOutputMode(hemisphere, 0, (EnvSeqManager::OutputMode)constrain(linked_data[0].modulation.output_mode + direction, 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1));
                break;
            case LinkedCursor::LINKED_OUTPUT2_MODE:
                manager.SetModulationOutputMode(hemisphere, 1, (EnvSeqManager::OutputMode)constrain(linked_data[1].modulation.output_mode + direction, 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1));
                break;
            }

            return;
        }

        if (random_menu_active) {
            random_menu_cursor.Scroll(direction);
            return;
        }

        if (!EditMode()) {
            MoveCursor(cursor, direction, EnvSeqCursor::MAX_CURSOR - 1);
            if (cursor == EnvSeqCursor::LINK && !manager.CanLink(hemisphere)) {
                // Cannot link, skip cursor 
                cursor += direction > 0 ? 1 : -1;
            }
            return;
        }

        if (cursor > EnvSeqCursor::STEP_VIEW && step_select) {
            step_view = constrain(step_view + direction, 0, MAX_NUM_STEPS - 1);
            osc_draw_reinit = true;
            return;
        }

        switch (cursor) {
        case EnvSeqCursor::MOD1_MODE:
            mod1_mode = (ModulationMode)constrain(mod1_mode + direction, 0, ModulationMode::MAX_MODULATION_MODE - 1);
            break;
        case EnvSeqCursor::MOD2_MODE:
            mod2_mode = (EnvSeqManager::ModulationMode)constrain(mod2_mode + direction, 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1);
            break;
        case EnvSeqCursor::OUTPUT2_MODE:
            output2_mode = (EnvSeqManager::OutputMode)constrain(output2_mode + direction, 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1);
            break;
        case EnvSeqCursor::NUM_STEPS:
            num_steps = (uint8_t)constrain(num_steps + direction, 1, MAX_NUM_STEPS);
            break;
        case EnvSeqCursor::STEP_VIEW:
            step_view = (uint8_t)constrain(step_view + direction, 0, MAX_NUM_STEPS - 1);
            osc_draw_reinit = true;
            break;
        case EnvSeqCursor::STEP_SHAPE: {
            int shape = steps[step_view].shape + direction;
            if (shape < 0) {
                shape = 0;
            } else if (shape >= Shape::VOSC) {
                if (steps[step_view].shape < Shape::VOSC) {
                    // Entering into VOSC mode
                    shape = get_nth_waveform(shape - Shape::VOSC);
                } else {
                    // Already in VOSC mode
                    shape = WaveformManager::GetNextWaveform(steps[step_view].shape - Shape::VOSC, direction);
                }
                shape += Shape::VOSC;
            }

            steps[step_view].shape = shape;
            reinit_osc();
            break;
        }
        case EnvSeqCursor::STEP_PARAM:
            MoveCursor(step_param_cursor, direction, StepParamCursor::MAX_STEP_PARAM_CURSOR - 1);
            if (step_param_cursor == StepParamCursor::STEP_PARAM_PASTE && !manager.HasClipboard()) {
                // No clipboard, set to copy cursor
                step_param_cursor = StepParamCursor::STEP_PARAM_COPY;
            }
            break;
        case EnvSeqCursor::STEP_PARAM_VALUE:
            switch (step_param_cursor) {
            case StepParamCursor::STEP_PARAM_OFFSET:
                steps[step_view].offset = (int16_t)constrain(steps[step_view].offset + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
                break;
            case StepParamCursor::STEP_PARAM_AMP:
                steps[step_view].amp = (int16_t)constrain(steps[step_view].amp + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
                break;
            case StepParamCursor::STEP_PARAM_WAVEFORM_OFFSET:
                steps[step_view].waveform_offset = (uint8_t)constrain(steps[step_view].waveform_offset + direction, 0, 100);
                break;
            case StepParamCursor::STEP_PARAM_WAVEFORM_OPTION:
                steps[step_view].waveform_option = (EnvSeqManager::Option)constrain(steps[step_view].waveform_option + direction, 0, EnvSeqManager::Option::MAX_OPTIONS - 1);
                break;
            case StepParamCursor::STEP_PARAM_TRIGGERS:
                steps[step_view].triggers = (uint8_t)constrain(steps[step_view].triggers + direction, 0, 7);
                break;
            case StepParamCursor::STEP_PARAM_CLOCKS:
                steps[step_view].clocks = (uint8_t)constrain(steps[step_view].clocks + direction, 0, 7);
                break;
            case StepParamCursor::STEP_PARAM_LENGTH:
                steps[step_view].length = (uint8_t)constrain(steps[step_view].length + direction, 1, 200);
                break;
            case StepParamCursor::STEP_PARAM_PROBABILITY:
                steps[step_view].probability = (uint8_t)constrain(steps[step_view].probability + direction, 0, 100);
                break;
            case StepParamCursor::STEP_PARAM_RETRIGGER_LEVEL:
                steps[step_view].retrigger_level = (int8_t)constrain(steps[step_view].retrigger_level + direction, -15, 15);
                break;
            case StepParamCursor::STEP_PARAM_GATE_LENGTH:
                steps[step_view].gate_length = (int8_t)constrain(steps[step_view].gate_length + direction, 0, 255);
                break;
            }
            break;
        }

        reinit_osc();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 1}, trigger2);
        Pack(data, PackLocation {1, 5}, num_steps - 1);
        Pack(data, PackLocation {6, 3}, mod1_mode);
        Pack(data, PackLocation {9, 4}, mod2_mode);
        Pack(data, PackLocation {13, 3}, output2_mode);
        
        const EnvSeqManager::LinkedData* linked_data = manager.GetLinkedData(hemisphere);
        Pack(data, PackLocation {16, 1}, manager.IsLinked(hemisphere));
        Pack(data, PackLocation {17, 4}, linked_data[0].modulation.mode);
        Pack(data, PackLocation {21, 4}, linked_data[1].modulation.mode);
        Pack(data, PackLocation {25, 4}, linked_data[0].modulation.output_mode);
        Pack(data, PackLocation {29, 4}, linked_data[1].modulation.output_mode);

        Pack(data, PackLocation {33, 1}, random_offsets);
        Pack(data, PackLocation {34, 1}, random_amps);
        Pack(data, PackLocation {35, 1}, random_shapes);
        Pack(data, PackLocation {36, 1}, random_vosc);
        Pack(data, PackLocation {37, 1}, random_lengths);
        Pack(data, PackLocation {38, 1}, random_triggers);
        Pack(data, PackLocation {39, 1}, random_clocks);
        Pack(data, PackLocation {40, 1}, random_mod_marks);
        Pack(data, PackLocation {41, 1}, random_retrigger_levels);
        Pack(data, PackLocation {42, 1}, random_gate_lengths);

        return data;
    }

    void OnDataReceive(uint64_t data) {
        trigger2 = Unpack(data, PackLocation {0, 1});
        num_steps = constrain(Unpack(data, PackLocation {1, 5}) + 1, 1, MAX_NUM_STEPS);
        mod1_mode = (ModulationMode)constrain(Unpack(data, PackLocation {6, 3}), 0, ModulationMode::MAX_MODULATION_MODE - 1);
        mod2_mode = (EnvSeqManager::ModulationMode)constrain(Unpack(data, PackLocation {9, 4}), 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1);
        output2_mode = (EnvSeqManager::OutputMode)constrain(Unpack(data, PackLocation {13, 3}), 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1);

        manager.SetLink(hemisphere, Unpack(data, PackLocation {16, 1}));
        if (manager.IsLinked(hemisphere)) {
            linked_cursor = LinkedCursor::UNLINK;
        }
        manager.SetModulationMode(hemisphere, 0, (EnvSeqManager::ModulationMode)constrain(Unpack(data, PackLocation {17, 4}), 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1));
        manager.SetModulationMode(hemisphere, 1, (EnvSeqManager::ModulationMode)constrain(Unpack(data, PackLocation {21, 4}), 0, EnvSeqManager::ModulationMode::MAX_MODULATION_MODE - 1));
        manager.SetModulationOutputMode(hemisphere, 0, (EnvSeqManager::OutputMode)constrain(Unpack(data, PackLocation {25, 4}), 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1));
        manager.SetModulationOutputMode(hemisphere, 1, (EnvSeqManager::OutputMode)constrain(Unpack(data, PackLocation {29, 4}), 0, EnvSeqManager::OutputMode::MAX_OUTPUT_MODE - 1));

        random_offsets = Unpack(data, PackLocation {33, 1});
        random_amps = Unpack(data, PackLocation {34, 1});
        random_shapes = Unpack(data, PackLocation {35, 1});
        random_vosc = Unpack(data, PackLocation {36, 1});
        random_lengths = Unpack(data, PackLocation {37, 1});
        random_triggers = Unpack(data, PackLocation {38, 1});
        random_clocks = Unpack(data, PackLocation {39, 1});
        random_mod_marks = Unpack(data, PackLocation {40, 1});
        random_retrigger_levels = Unpack(data, PackLocation {41, 1});
        random_gate_lengths = Unpack(data, PackLocation {42, 1});

        Reset();
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = trigger2 ? "Trigger" : "Reset";
        help[HELP_CV1]      = mod1_mode_string(mod1_mode);
        help[HELP_CV2]      = mod2_mode_string(mod2_mode);
        help[HELP_OUT1]     = "Output";
        help[HELP_OUT2]     = output2_mode_string(output2_mode);
        help[HELP_EXTRA1] = "Can be linked for";
        help[HELP_EXTRA2] = "extra i/o functions";
        //                  "---------------------" <-- Extra text size guide
    }

private:
    int8_t cursor = 0, step_param_cursor = 0, linked_cursor = MAX_LINKED_CURSOR;
    bool random_menu_active = false; // Whether the random menu is active
    OC::menu::ScreenCursor<5> random_menu_cursor;
    EnvSeqManager& manager = EnvSeqManager::get();

    bool trigger2 = false; // Whether to trigger on TR2
    uint8_t num_steps = 8; // How many steps of the generated envelope to play before looping
    ModulationMode mod1_mode = ModulationMode::MOD; // Modulation mode for CV1
    EnvSeqManager::ModulationMode mod2_mode = EnvSeqManager::ModulationMode::LENGTH; // Modulation mode for CV2
    EnvSeqManager::OutputMode output2_mode = EnvSeqManager::OutputMode::GATE_STEP; // Mode for CV2 output
    EnvSeqManager::Output output2 = EnvSeqManager::Output{}; // Output for CV2

    // bit set?
    bool random_offsets = true; // Whether to randomize step levels (offset)
    bool random_amps = true; // Whether to randomize step amplitudes
    bool random_shapes = true; // Whether to randomize step shapes
    bool random_vosc = false; // Whether to randomize the VOSC flag for each step
    bool random_lengths = false; // Whether to randomize step lengths
    bool random_triggers = false; // Whether to randomize the triggers flag for each step
    bool random_clocks = false; // Whether to randomize the clocks flag for each step
    bool random_mod_marks = false; // Whether to randomize the mod_mark flag for each step
    bool random_retrigger_levels = false; // Whether to randomize the retrigger levels flag for each step
    bool random_gate_lengths = false; // Whether to randomize the gate lengths flag for each step
    
    bool reset_flag = false; // Prevent stepping forward after a reset
    bool step_select = false; // Toggle for switching step_view while editing params
    bool follow = false; // update step view on clock advance
    int8_t step = -1; // Current step
    int8_t step_view = 0; // Viewed step for cursor
    uint16_t step_progress = 0; // Progress of the current step
    EnvSeqManager::Step steps[MAX_NUM_STEPS]; // Steps of the envelope sequence
    VectorOscillator osc; // VOSC oscillator
    bool osc_reinit = true; // Whether to re-initialize the VOSC oscillator
    VectorOscillator osc_draw; // VOSC oscillator for drawing the waveform
    bool osc_draw_reinit = true; // Whether to re-initialize the drawing VOSC oscillator

    uint32_t clock_ticks = 0; // Ticks between the last two clock inputs
    uint32_t step_start_tick = 0; // Tick when the current step started
    uint8_t step_clocks = 0; // Number of clocks since the current step started
    uint8_t last_retrig_index = 0; // Which retrigger segment we're currently in (for gate pulses)

    int16_t mod_cv_step = 0; // Value of the modulation input at step start
    int16_t mod_cv_seq = 0; // Value of the modulation input at sequence start
    int16_t cv_step_start = 0; // Value of the step CV start value
    int16_t last_output_cv = 0; // Last output CV value (to use for Shape::HOLD steps)

    // Controller for linked applet
    void controller_linked() {
        manager.SetModulationCV(hemisphere, 0, In(0));
        manager.SetModulationCV(hemisphere, 1, In(1));

        EnvSeqManager::LinkedData *linked_data = manager.GetLinkedData(hemisphere);
        for (uint8_t i = 0; i < 2; i++) {
            if (linked_data[i].output.is_cv) {
                Out(i, constrain(linked_data[i].output.cv, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV));
            } else if (linked_data[i].output.cv > 0) {
                ClockOut(i, linked_data[i].output.cv);
                linked_data[i].output.cv = 0;
            }
        }
    }

    // Get the modulation value for CV1
    int get_mod1_cv(bool new_step, bool sequence_restarted) {
        int cv = DetentedIn(0);
        if (new_step) {
            // Store the modulation input value for step and sequence start
            mod_cv_step = cv;
            if (sequence_restarted) {
                mod_cv_seq = cv;
            }
        }

        // Apply the modulation mode
        if (mod1_mode == ModulationMode::HOLD_STEP_START) {
            cv = mod_cv_step;
        } else if (mod1_mode == ModulationMode::HOLD_SEQ_START) {
            cv = mod_cv_seq;
        }

        return cv;
    }

    // Get the offset CV for the given step
    int get_offset_cv(int mod1_cv) {
        const EnvSeqManager::Step& s = steps[step];
        int cv = static_cast<int>(s.offset) * OFFSET_SCALE_INCREMENT + mod1_cv;

        // Modulate the offset CV
        EnvSeqManager::Output modulation = get_modulation(EnvSeqManager::ModulationMode::MOD);
        if (modulation.is_cv) {
            cv += modulation.cv;
        }

        if (s.mod_mark) {
            // Modulate the offset CV for marked steps
            modulation = get_modulation(EnvSeqManager::ModulationMode::MOD_MARK);
            if (modulation.is_cv) {
                cv += modulation.cv;
            }
        }

        return cv;
    }

    // Update the step progress for the current step. Returns the total step ticks.
    uint32_t update_step_progress(uint32_t this_tick) {
        const EnvSeqManager::Step& s = steps[step];
        const uint32_t total_step_ticks = (s.clocks + 1) * clock_ticks;
        const uint32_t step_end_tick = step_start_tick + total_step_ticks;
        step_progress = this_tick >= step_end_tick ? 65535 : Proportion(this_tick - step_start_tick, total_step_ticks, 65535);

        // Effective step length (1-200%) with CV scaling
        EnvSeqManager::Output length_mod = get_modulation(EnvSeqManager::ModulationMode::LENGTH, true);
        uint16_t effective_length = s.length;
        if (length_mod.is_cv) {
            effective_length = effective_step_length(effective_length, length_mod.cv);
        }

        // Map the step length (1-200%) into the shape progression: <100% speeds the shape up, >100% slows it down
        if (effective_length != 100) {
            uint32_t scaled = (static_cast<uint32_t>(step_progress) * 100) / effective_length;
            if (scaled > 65535) {
                scaled = 65535;
            }

            step_progress = static_cast<uint16_t>(scaled);
        }

        return total_step_ticks;
    }

    // Get the modulation value for the given modulation mode
    EnvSeqManager::Output get_modulation(EnvSeqManager::ModulationMode mod_mode, bool detented = false) {
        EnvSeqManager::Output output = EnvSeqManager::Output{};
        if (mod2_mode == mod_mode) {
            output.is_cv = true;
            output.cv = detented ? DetentedIn(1) : In(1);
        }

        if (manager.IsLinked(hemisphere)) {
            const EnvSeqManager::LinkedData *linked_data = manager.GetLinkedData(hemisphere);
            for (int i = 0; i < 2; i++) {
                if (linked_data[i].modulation.mode == mod_mode) {
                    output.is_cv = true;
                    int16_t cv = linked_data[i].modulation.cv;
                    if (detented) {
                        if (NorthernLightModular && cv < HEMISPHERE_CENTER_DETENT) {
                            cv = 0;
                        } else if (cv >= (HEMISPHERE_CENTER_INPUT_CV - HEMISPHERE_CENTER_DETENT) && cv <= (HEMISPHERE_CENTER_INPUT_CV + HEMISPHERE_CENTER_DETENT)) {
                            cv = HEMISPHERE_CENTER_INPUT_CV;
                        }
                    }

                    output.cv += cv;
                }
            }
        }

        return output;
    }

    // Effective step length (1-200%) with CV scaling
    uint16_t effective_step_length(uint8_t length, int mod_cv) const {
        // Map CV (bipolar) to +/-100% change, then convert to a 1..200% scaler.
        const int cv_delta_pct = Proportion(mod_cv, HEMISPHERE_MAX_INPUT_CV, 100); // -100..+100
        const int cv_scale_pct = constrain(100 + cv_delta_pct, 1, 200);

        // Multiply per-step length by CV scale and clamp to the same 1..200% range.
        const uint16_t eff = constrain(((length * cv_scale_pct) + 50) / 100, 1, 200);

        return eff;
    }

    // Prepare the shape parameters for the VOSC oscillator. Returns the waveform number.
    uint16_t prepare_shape(const uint16_t& shape, uint8_t& offset, bool& revert, bool& invert, EnvSeqManager::Option& option) {
        switch (shape) {
        case Shape::EXP_DOWN:
        case Shape::EXP_UP:
            offset = 0;
            revert = shape == Shape::EXP_UP;
            invert = false;
            option = EnvSeqManager::Option::NO_OPTION;
            return HS::Exponential;
        case Shape::RAMP_DOWN:
        case Shape::RAMP_UP:
            offset = 0;
            revert = shape == Shape::RAMP_UP;
            invert = false;
            option = EnvSeqManager::Option::NO_OPTION;
            return HS::Sawtooth;
        case Shape::LOG_DOWN:
        case Shape::LOG_UP:
            offset = 0;
            revert = shape == Shape::LOG_UP;
            invert = false;
            option = EnvSeqManager::Option::NO_OPTION;
            return HS::Logarithmic;
        default:
            return shape - Shape::VOSC;
        }
    }

    // Calculate the number of ticks for the gate pulse.
    uint32_t calculate_gate_ticks(uint32_t this_tick, uint32_t start_tick, uint32_t total_ticks, uint8_t gate_length) {
        if (this_tick >= start_tick + total_ticks) {
            return 0;
        }

        uint32_t gate_ticks = 0;
        if (gate_length < 156) {
            // 0-155 -> ~0.05..~1000ms, exponential curve
            const uint32_t ms_x100 = 5 + (1000ULL * gate_length * gate_length) / 241;
            gate_ticks = (ms_x100 * HEMISPHERE_CLOCK_TICKS + 50) / 100;
        } else {
            // 156-255 -> 1-100% of total_ticks
            const uint32_t percent = gate_length - 155;
            gate_ticks = (total_ticks * percent) / 100;
            if (gate_ticks == 0) {
                gate_ticks = 1;
            }
        }

        EnvSeqManager::Output gate_length_mod = get_modulation(EnvSeqManager::ModulationMode::GATE_LENGTH);
        if (gate_length_mod.is_cv) {
            // Modulate gate ticks as 1-200% of the original gate ticks
            const int cv_delta_pct = Proportion(gate_length_mod.cv, HEMISPHERE_MAX_INPUT_CV, 100); // -100..+100
            const int cv_scale_pct = constrain(100 + cv_delta_pct, 1, 200);
            gate_ticks = (gate_ticks * cv_scale_pct + 50) / 100;
        }

        const uint32_t remaining = (start_tick + total_ticks) - this_tick;
        if (gate_ticks > remaining) {
            gate_ticks = remaining;
        }

        return gate_ticks;
    }

    void draw_interface_linked() {
        uint8_t y = 5;
        const EnvSeqManager::LinkedData* linked_data = manager.GetLinkedData(hemisphere);

        y += 10;
        if (DetentedIn(0)) {
            gfxIcon(0, y, MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(linked_data[0].modulation.mode));
        if (linked_cursor == LinkedCursor::LINKED_MOD1_MODE) gfxSpicyCursor(10, y + 8, 53, "Mod3 Mode");

        y += 10;
        if (DetentedIn(1)) {
            gfxIcon(0, y, MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(linked_data[1].modulation.mode));
        if (linked_cursor == LinkedCursor::LINKED_MOD2_MODE) gfxSpicyCursor(10, y + 8, 53, "Mod4 Mode");

        y += 10;
        gfxIcon(0, y, output2_mode_icon(linked_data[0].modulation.output_mode));
        gfxPrint(10, y, output2_mode_string(linked_data[0].modulation.output_mode));
        if (linked_cursor == LinkedCursor::LINKED_OUTPUT1_MODE) gfxSpicyCursor(10, y + 8, 53, "Out3 Mode");

        y += 10;
        gfxIcon(0, y, output2_mode_icon(linked_data[1].modulation.output_mode));
        gfxPrint(10, y, output2_mode_string(linked_data[1].modulation.output_mode));
        if (linked_cursor == LinkedCursor::LINKED_OUTPUT2_MODE) gfxSpicyCursor(10, y + 8, 53, "Out4 Mode");

        y += 10;
        gfxIcon(0, y, LINK_ICON);
        gfxPrint(10, y, "Unlink");
        if (linked_cursor == LinkedCursor::UNLINK) gfxSpicyCursor(10, y + 8, 53);
    }

    void draw_interface() {
        bool can_link = manager.CanLink(hemisphere);
        if (can_link && manager.IsLinked(hemisphere)) {
            draw_interface_linked();
            return;
        }

        if (random_menu_active) {
            draw_random_view();
            return;
        }

        if (cursor >= EnvSeqCursor::STEP_VIEW) {
            draw_step_view();
            if (step_select && EditMode()) {
              gfxIcon(25, 15, LEFT_ICON, true);
              gfxFrame(10, 14, 14, 11, true);
            }
            return;
        }

        uint8_t y = 5;

        y += 10;
        if (DetentedIn(0)) {
            gfxIcon(0, y, MOD_ICON);
        }
        gfxPrint(10, y, mod1_mode_string(mod1_mode));
        if (cursor == EnvSeqCursor::MOD1_MODE) gfxSpicyCursor(10, y + 8, 40, "Mod1 Mode");

        if (can_link) {
            gfxIcon(53, y, LINK_ICON);
            if (cursor == EnvSeqCursor::LINK) gfxSpicyCursor(50, y + 8, 13);
        }

        y += 10;
        if (DetentedIn(1)) {
            gfxIcon(0, y, MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(mod2_mode));
        if (cursor == EnvSeqCursor::MOD2_MODE) gfxSpicyCursor(10, y + 8, 53, "Mod2 Mode");

        y += 10;
        gfxIcon(0, y, output2_mode_icon(output2_mode));
        gfxPrint(10, y, output2_mode_string(output2_mode));
        if (cursor == EnvSeqCursor::OUTPUT2_MODE) gfxSpicyCursor(10, y + 8, 53, "Out2 Mode");
    
        y += 10;
        gfxIcon(0, y, RANDOM_ICON);
        gfxPrint(10, y, "Random");
        if (cursor == EnvSeqCursor::RANDOM) gfxSpicyCursor(10, y + 8, 40);

        if (trigger2) {
            gfxIcon(50, y, TR_ICON);
            gfxIcon(60, y, SUB_TWO);
        }
        if (cursor == EnvSeqCursor::TRIGGER2) gfxSpicyCursor(50, y + 8, 13);

        y += 10;
        gfxIcon(0, y, PhzIcons::stairs);
        if (step >= 0) {
            gfxPrint(10 + pad(10, step + 1), y, step + 1);
        }
        gfxPrint(22, y, "/");
        gfxPrint(28 + pad(10, num_steps), y, num_steps);
        if (cursor == EnvSeqCursor::NUM_STEPS) gfxSpicyCursor(28, y + 8, 16, "Steps");

        gfxIcon(44, y, RESET_ICON);
        if (cursor == EnvSeqCursor::RESET) gfxSpicyCursor(44, y + 8, 10);

        gfxIcon(54, y, LOOP_ICON);
        if (cursor == EnvSeqCursor::INIT) gfxSpicyCursor(54, y + 8, 10);
    }

    void draw_waveform() {
        const EnvSeqManager::Step& s = steps[step_view];

        const uint8_t top = 25;
        const uint8_t bottom = 51;
        const uint8_t mirror = top + bottom;

        if (s.shape >= Shape::VOSC) {
            // Draw the waveform offset line
            int offset_y = bottom - Proportion(s.waveform_offset, 100, 26);
            if (s.waveform_invert) {
                offset_y = mirror - offset_y;
            }
            gfxDottedLine(0, offset_y, 63, offset_y);
        }

        bool draw_vosc = true;
        switch (s.shape) {
        case Shape::HOLD:
            draw_vosc = false;
            break;
        case Shape::ZERO:
        case Shape::FLAT:
            draw_vosc = false;
            gfxLine(0, 38, 63, 38);
            break;
        default:
            // All other shapes are handled by the VOSC oscillator
            break;
        }

        uint8_t offset = s.waveform_offset;
        bool revert = s.waveform_revert;
        bool invert = s.waveform_invert;
        EnvSeqManager::Option option = s.waveform_option;
        uint16_t number = prepare_shape(s.shape, offset, revert, invert, option);

        if (osc_draw_reinit) {
            osc_draw_reinit = false;
            osc_draw = WaveformManager::VectorOscillatorFromWaveform(number);
            osc_draw.Sustain();
            osc_draw.Cycle(0);
        }

        if (draw_vosc) {
            const uint8_t segment_count = osc_draw.SegmentCount();
            uint16_t total_time = osc_draw.TotalTime();
            VOSegment seg = osc_draw.GetSegment(segment_count - 1);
            uint8_t prev_x = 0; // Starting coordinates
            uint8_t prev_y = bottom - Proportion(seg.level, 255, 26);
            for (int i = 0; i < segment_count; i++) {
                seg = osc_draw.GetSegment(i);
                uint8_t y = bottom - Proportion(seg.level, 255, 26);
                uint8_t seg_x = Proportion(seg.time, total_time, 62);
                uint8_t x = prev_x + seg_x;
                x = constrain(x, 0, 63);
                y = constrain(y, top, bottom);
                const uint8_t draw_prev_x = revert ? 63 - prev_x : prev_x;
                const uint8_t draw_x = revert ? 63 - x : x;
                const uint8_t draw_prev_y = invert ? mirror - prev_y : prev_y;
                const uint8_t draw_y = invert ? mirror - y : y;
                gfxLine(draw_prev_x, draw_prev_y, draw_x, draw_y);
                prev_x = x;
                prev_y = y;
            }
        }

        if (step == step_view) {
            // Draw step progress
            int x = Proportion(step_progress, 65535, 63);
            gfxLine(x, 25, x, 51);
        }
    }

    void draw_step_view() {
        const EnvSeqManager::Step& s = steps[step_view];
        uint8_t display_step = step_view + 1;
        uint8_t y = 15;

        gfxIcon(0, y, PhzIcons::stairs);
        if (follow) gfxInvert(0, y, 8, 8);
        gfxPrint(10 + pad(10, display_step), y, display_step);
        if (cursor == EnvSeqCursor::STEP_VIEW) gfxSpicyCursor(10, y + 8, 12, "Step#");
        gfxPrint(22, y, shape_string(s.shape));
        if (s.shape >= Shape::VOSC) {
            gfxPrint(46, y, s.shape - Shape::VOSC + 1);
        }
        if (cursor == EnvSeqCursor::STEP_SHAPE) gfxSpicyCursor(22, y + 8, 41, step_select?"Step#":"Shape");

        draw_waveform();

        y = 55;
        switch (step_param_cursor) {
        case StepParamCursor::STEP_PARAM_OFFSET:
            gfxIcon(0, y, OFFSET_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            gfxPrintVoltage(s.offset * OFFSET_SCALE_INCREMENT);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_AMP:
            gfxIcon(0, y, RANGE_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            gfxPrintVoltage(s.amp * OFFSET_SCALE_INCREMENT);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_WAVEFORM_OFFSET:
            gfxIcon(0, y, UP_DOWN_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            gfxPrint(s.waveform_offset);
            gfxPrint("%");
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 24, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_WAVEFORM_REVERT:
            gfxIcon(0, y, LEFT_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxIcon(10, y, s.waveform_revert ? CHECK_ON_ICON : CHECK_OFF_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 10, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_WAVEFORM_INVERT:
            gfxIcon(0, y, DOWN_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxIcon(10, y, s.waveform_invert ? CHECK_ON_ICON : CHECK_OFF_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 10, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_WAVEFORM_OPTION:
            gfxPrint(2, y, "?");
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPrint(10, y, option_string(s.waveform_option));
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_TRIGGERS:
            gfxIcon(0, y, GATE_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPrint(10, y, s.triggers + 1);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 6, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_CLOCKS:
            gfxIcon(0, y, CLOCK_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPrint(10, y, s.clocks + 1);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 6, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_LENGTH:
            gfxIcon(0, y, LENGTH_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            gfxPrint(s.length);
            gfxPrint("%");
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 24, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_PROBABILITY:
            gfxIcon(0, y, TOSS_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            gfxPrint(s.probability);
            gfxPrint("%");
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 24, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_RETRIGGER_LEVEL:
            gfxIcon(0, y, GAUGE_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            if (s.retrigger_level == 0) {
                gfxPrint("no");
            } else {
                if (s.retrigger_level < 0) {
                    gfxPrint("from ");
                } else if (s.retrigger_level > 0) {
                    gfxPrint("to ");
                }

                gfxPrint(abs(s.retrigger_level));
            }
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_MOD_MARK:
            gfxIcon(0, y, CHECK_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxIcon(10, y, s.mod_mark ? CHECK_ON_ICON : CHECK_OFF_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 10, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_GATE_LENGTH:
            gfxIcon(0, y, GATE_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPos(10, y);
            if (s.gate_length < 156) {
                graphics.printf("%1d.%02dms", SPLIT_INT_DEC(0.05f + 10.0f*s.gate_length*s.gate_length/241.0f, 100));
            } else {
                gfxPrint(s.gate_length - 155);
                gfxPrint("%");
            }
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_COPY:
            gfxIcon(0, y, UP_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPrint(10, y, step_param_names[step_param_cursor]);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        case StepParamCursor::STEP_PARAM_PASTE:
            gfxIcon(0, y, DOWN_ICON);
            if (cursor == EnvSeqCursor::STEP_PARAM) gfxSpicyCursor(0, 63, 10, step_param_names[step_param_cursor]);
            gfxPrint(10, y, step_param_names[step_param_cursor]);
            if (cursor == EnvSeqCursor::STEP_PARAM_VALUE) gfxSpicyCursor(10, 63, 53, step_param_names[step_param_cursor]);
            break;
        }
    }

    void draw_random_view() {
        for (int line = 0; line < 5; ++line) {
            const int item = random_menu_cursor.first_visible() + line;
            if (item > random_menu_cursor.last_visible()) {
                break;
            }

            const uint8_t y = 15 + (line * 10);
            switch (item) {
            case RandomCursor::RANDOM_OFFSETS:
                gfxIcon(0, y, random_offsets ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_OFFSET]);
                break;
            case RandomCursor::RANDOM_AMPS:
                gfxIcon(0, y, random_amps ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_AMP]);
                break;
            case RandomCursor::RANDOM_SHAPES:
                gfxIcon(0, y, random_shapes ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, "Shape");
                break;
            case RandomCursor::RANDOM_VOSC:
                gfxIcon(0, y, random_vosc ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, "VOSC");
                break;
            case RandomCursor::RANDOM_LENGTHS:
                gfxIcon(0, y, random_lengths ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_LENGTH]);
                break;
            case RandomCursor::RANDOM_TRIGGERS:
                gfxIcon(0, y, random_triggers ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_TRIGGERS]);
                break;
            case RandomCursor::RANDOM_CLOCKS:
                gfxIcon(0, y, random_clocks ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_CLOCKS]);
                break;
            case RandomCursor::RANDOM_MOD_MARKS:
                gfxIcon(0, y, random_mod_marks ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_MOD_MARK]);
                break;
            case RandomCursor::RANDOM_RETRIGGER_LEVELS:
                gfxIcon(0, y, random_retrigger_levels ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_RETRIGGER_LEVEL]);
                break;
            case RandomCursor::RANDOM_GATE_LENGTHS:
                gfxIcon(0, y, random_gate_lengths ? CHECK_ON_ICON : CHECK_OFF_ICON);
                gfxPrint(10, y, step_param_names[StepParamCursor::STEP_PARAM_GATE_LENGTH]);
                break;
            case RandomCursor::RANDOM_APPLY:
                gfxIcon(0, y, CHECK_ICON);
                gfxPrint(10, y, "Apply");
                break;
            case RandomCursor::RANDOM_CANCEL:
                gfxIcon(0, y, LEFT_ICON);
                gfxPrint(10, y, "Back");
                break;
            }

            if (item == random_menu_cursor.cursor_pos()) {
                gfxSpicyCursor(10, y + 8, 53);
            }
        }
    }

    void init_steps() {
        int amp = HEMISPHERE_MAX_CV / (2 * OFFSET_SCALE_INCREMENT);
        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            steps[i].offset = 0;
            steps[i].amp = amp;
            steps[i].shape = Shape::EXP_DOWN;
            steps[i].waveform_offset = 0;
            steps[i].waveform_revert = false;
            steps[i].waveform_invert = false;
            steps[i].waveform_option = EnvSeqManager::Option::NO_OPTION;
            steps[i].triggers = 0;
            steps[i].clocks = 0;
            steps[i].length = 100;
            steps[i].probability = 100;
            steps[i].retrigger_level = 0;
            steps[i].gate_length = 205;
            steps[i].mod_mark = false;
        }
    }

    // Randomize the steps
    void randomize_steps() {
        const int max_units = HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT;
        const int offset_max = max_units * 0.2;

        // Randomize amp only between 20% and 50%
        const int amp_min = max_units * 0.2;
        const int amp_max = max_units * 0.5;

        const uint8_t total_waveform_count = WaveformManager::WaveformCount() + HS::WAVEFORM_LIBRARY_COUNT;

        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            if (random_offsets) {
                steps[i].offset = random(0, offset_max + 1);
            }
            if (random_amps) {
                steps[i].amp = random(amp_min, amp_max + 1);
            }
            if (random_shapes) {
                // Randomize shape, but make sure VOSC is allowed
                steps[i].shape = random(0, Shape::VOSC + (random_vosc ? 1 : 0));
            }
            if (steps[i].shape == Shape::VOSC) {
                steps[i].shape += get_nth_waveform(random(0, total_waveform_count));
            }
            if (random_lengths) {
                steps[i].length = random(75, 151); // Randomize length only between 75% and 150%
            }
            if (random_triggers && random(0, 4) == 0) {
                steps[i].triggers = random(0, 3);
            }
            if (random_clocks && random(0, 4) == 0) {
                steps[i].clocks = random(0, 3);
            }
            if (random_mod_marks) {
                steps[i].mod_mark = random(0, 2);
            }
            if (random_retrigger_levels) {
                steps[i].retrigger_level = random(-15, 16);
            }
            if (random_gate_lengths) {
                steps[i].gate_length = random(156, 206);
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
    int get_next_step(int current_step, bool &sequence_restarted) {
        if (current_step >= num_steps) {
            current_step = num_steps - 1;
        }

        int next_step = -1;
        sequence_restarted = false;

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
                sequence_restarted = true;
            }

            if (probability >= 100 || random(0, 100) < probability) {
                // Probability is 100% or this step got lucky, play it
                return candidate;
            }
        }

        // No step with probability > 0 which got lucky, return the next step with probability > 0, or -1 if no such step exists
        return next_step;
    }

    uint8_t get_nth_waveform(uint8_t n, uint8_t start_waveform = 0) const {
        for (uint8_t i = 0; i < n; i++) {
            start_waveform = WaveformManager::GetNextWaveform(start_waveform, 1);
        }

        return start_waveform;
    }

    void reinit_osc() {
        osc_draw_reinit = true;
        if (step == step_view) {
            osc_reinit = true;
        }
    }

    const char* mod1_mode_string(ModulationMode mode) {
        switch (mode) {
        case ModulationMode::MOD:
            return "Mod";
        case ModulationMode::HOLD_STEP_START:
            return "H step";
        case ModulationMode::HOLD_SEQ_START:
            return "H seq";
        default:
            return "";
        }
    }

    const char* mod2_mode_string(EnvSeqManager::ModulationMode mode) {
        switch (mode) {
        case EnvSeqManager::ModulationMode::LENGTH:
            return step_param_names[StepParamCursor::STEP_PARAM_LENGTH];
        case EnvSeqManager::ModulationMode::STEP_SEL:
            return "StepSel";
        case EnvSeqManager::ModulationMode::RETRIGGER_LEVEL:
            return step_param_names[StepParamCursor::STEP_PARAM_RETRIGGER_LEVEL];
        case EnvSeqManager::ModulationMode::MOD:
            return "Mod";
        case EnvSeqManager::ModulationMode::MOD_MARK:
            return step_param_names[StepParamCursor::STEP_PARAM_MOD_MARK];
        case EnvSeqManager::ModulationMode::GATE_LENGTH:
            return step_param_names[StepParamCursor::STEP_PARAM_GATE_LENGTH];
        default:
            return "";
        }
    }

    const uint8_t* output2_mode_icon(EnvSeqManager::OutputMode mode) {
        return mode == EnvSeqManager::OutputMode::GATE_STEP || mode == EnvSeqManager::OutputMode::GATE_STEP_INCL_RETRIGGERS || mode == EnvSeqManager::OutputMode::GATE_SEQUENCE ? GATE_ICON : CV_ICON;
    }

    const char* output2_mode_string(EnvSeqManager::OutputMode mode) {
        switch (mode) {
        case EnvSeqManager::OutputMode::COPY:
            return "Cpy";
        case EnvSeqManager::OutputMode::INV:
            return "Inv";
        case EnvSeqManager::OutputMode::INVO:
            return "InvO";
        case EnvSeqManager::OutputMode::CV_STEP_START:
            return "HStart";
        case EnvSeqManager::OutputMode::GATE_STEP:
            return "Step";
        case EnvSeqManager::OutputMode::GATE_STEP_INCL_RETRIGGERS:
            return "StepTrg";
        case EnvSeqManager::OutputMode::GATE_SEQUENCE:
            return "Seq";
        default:
            return "";
        }
    }

    const char* shape_string(uint16_t shape) {
        switch (shape) {
        case Shape::HOLD:
            return "Hold";
        case Shape::ZERO:
            return "Zero";
        case Shape::FLAT:
            return "Flat";
        case Shape::RAMP_UP:
            return "RampUp";
        case Shape::RAMP_DOWN:
            return "RampDw";
        case Shape::EXP_UP:
            return "ExpUp";
        case Shape::EXP_DOWN:
            return "ExpDw";
        case Shape::LOG_UP:
            return "LogUp";
        case Shape::LOG_DOWN:
            return "LogDw";
        default:
            return "VOSC";
        }
    }

    const char* option_string(EnvSeqManager::Option option) {
        switch (option) {
        case EnvSeqManager::Option::NO_OPTION:
            return "None";
        case EnvSeqManager::Option::FOLD_UP:
            return "FoldUp";
        case EnvSeqManager::Option::FOLD_DOWN:
            return "FoldDw";
        case EnvSeqManager::Option::ZERO_UP:
            return "ZeroUp";
        case EnvSeqManager::Option::ZERO_DOWN:
            return "ZeroDw";
        default:
            return "";
        }
    }
};
