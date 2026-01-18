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
#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"
#include <cstdint>
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

        STEP_VIEW,
        STEP_OFFSET,
        STEP_SCALE,
        STEP_CURVE,

        STEP_WAVEFORM_NUMBER,
        STEP_WAVEFORM_OFFSET,
        STEP_WAVEFORM_REVERT,
        STEP_WAVEFORM_INVERT,
        STEP_WAVEFORM_OPTION,

        STEP_TRIGGERS, STEP_CLOCKS,
        STEP_LENGTH,
        STEP_PROBABILITY,
        STEP_RETRIGGER_FADE,
        STEP_MOD_MARK,

        MAX_CURSOR,
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
        RANDOM_OFFSETS, RANDOM_SCALES,
        RANDOM_CURVES, RANDOM_VOSC,
        RANDOM_LENGTHS, RANDOM_TRIGGERS,
        RANDOM_CLOCKS, RANDOM_MOD_MARKS,
        RANDOM_RETRIGGER_FADES, RANDOM_APPLY,

        MAX_RANDOM_CURSOR,
    };

    // Modulation mode for CV1
    enum ModulationMode : uint8_t {
        MOD = 0, // Modulate all steps
        HOLD_STEP_START = 1, // Sample and hold the step start value as the modulation input
        HOLD_SEQ_START = 2, // Sample and hold the sequence start value as the modulation input

        MAX_MODULATION_MODE,
    };

    // Curve types for step transitions
    enum Curve : uint8_t {
        NONE = 0,
        ZERO = 1,
        FLAT = 2,
        RAMP_UP = 3,
        RAMP_DOWN = 4,
        TRIANGLE = 5,
        EXP_UP = 6,
        EXP_DOWN = 7,
        LOG_UP = 8,
        LOG_DOWN = 9,
        VOSC = 10,

        MAX_CURVE,
    };

    enum Option : uint8_t {
        NO_OPTION = 0,
        FOLD_UP = 1,
        FOLD_DOWN = 2,
        ZERO_UP = 3,
        ZERO_DOWN = 4,

        MAX_OPTIONS,
    };

    // Single step of the envelope sequence
    struct Step {
        int16_t offset; // Scaled step offset CV (by OFFSET_SCALE_INCREMENT)
        int16_t scale; // Scaled step scale CV (by OFFSET_SCALE_INCREMENT)
        int waveform_number; // Waveform number (VOSC only)
        uint8_t waveform_offset; // Offset where the envelope offset is on the waveform (0..100) in 1% steps (VOSC only)
        bool waveform_revert; // Reverts the waveform (VOSC only)
        bool waveform_invert; // Inverts the waveform (VOSC only)
        uint8_t length; // Length 1-200% of envelope duration for this step
        uint8_t probability; // Probability 0-100% of this step being played
        uint8_t retrigger_fade; // Retrigger fade (last retrigger gets to this level)
        bool mod_mark; // Whether this step is marked for modulation

        Curve curve : 4; // Curve shape for the step
        Option waveform_option : 3; // Option for the waveform (VOSC only)
        uint8_t triggers : 3; // Number of times to trigger the step (0-7)
        uint8_t clocks : 3; // Number of clocks this step lasts for (0-7)
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
                }
            }
        }

        if (step == -1) {
            // No step to play
            return;
        }

        int mod1_cv = DetentedIn(0);
        if (new_step) {
            // Store the modulation input value for step and sequence start
            mod_cv_step = mod1_cv;
            if (sequence_restarted) {
                mod_cv_seq = mod1_cv;
            }
        }

        // Apply the modulation mode
        if (mod1_mode == ModulationMode::HOLD_STEP_START) {
            mod1_cv = mod_cv_step;
        } else if (mod1_mode == ModulationMode::HOLD_SEQ_START) {
            mod1_cv = mod_cv_seq;
        }

        const EnvSeqManager::LinkedData *linked_data = manager.GetLinkedData(hemisphere);
        int16_t cv_out = 0;
        Step s = steps[step];
        const int16_t scale_cv = s.scale * OFFSET_SCALE_INCREMENT;
        const uint32_t total_step_ticks = (s.clocks + 1) * clock_ticks;
        const uint32_t step_end_tick = step_start_tick + total_step_ticks;
        const uint16_t raw_step_progress = this_tick >= step_end_tick ? 65535 : Proportion(this_tick - step_start_tick, total_step_ticks, 65535);
        
        // Effective step length (1-200%) with CV scaling
        EnvSeqManager::Output length_mod = get_modulation(EnvSeqManager::ModulationMode::LENGTH, true);
        uint16_t effective_length = s.length;
        if (length_mod.is_cv) {
            effective_length = effective_step_length(effective_length, length_mod.cv);
        }

        // Map the step length (1-200%) into the curve progression: <100% speeds the curve up, >100% slows it down
        uint16_t step_progress = raw_step_progress;
        if (effective_length != 100) {
            uint32_t scaled = (static_cast<uint32_t>(raw_step_progress) * 100) / effective_length;
            if (scaled > 65535) {
                scaled = 65535;
            }
            step_progress = static_cast<uint16_t>(scaled);
        }

        // Retrigger: restart the curve multiple times within the same step.
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
        }

        // Modulate the offset CV
        int offset_cv = static_cast<int>(s.offset) * OFFSET_SCALE_INCREMENT + mod1_cv;
        EnvSeqManager::Output modulation = get_modulation(EnvSeqManager::ModulationMode::MOD);
        if (modulation.is_cv) {
            offset_cv += modulation.cv;
        }
        if (s.mod_mark) {
            modulation = get_modulation(EnvSeqManager::ModulationMode::MOD_MARK);
            if (modulation.is_cv) {
                offset_cv += modulation.cv;
            }
        }

        bool use_vosc = true;
        switch (s.curve) {
        case Curve::NONE:
            use_vosc = false;
            cv_out = 0;
            break;
        case Curve::ZERO:
            use_vosc = false;
            offset_cv = 0;
            cv_out = 0;
            break;
        case Curve::FLAT:
            use_vosc = false;
            cv_out = scale_cv;
            break;
        case Curve::RAMP_UP:
            init_step_vosc(s);
            s.waveform_number = HS::Ramp;
            break;
        case Curve::RAMP_DOWN:
            init_step_vosc(s);
            s.waveform_number = HS::Sawtooth;
            break;
        case Curve::TRIANGLE:
            init_step_vosc(s);
            s.waveform_number = HS::Triangle;
            break;
        case Curve::EXP_UP:
            init_step_vosc(s);
            s.waveform_revert = true;
            s.waveform_number = HS::Exponential;
            break;
        case Curve::EXP_DOWN:
            init_step_vosc(s);
            s.waveform_number = HS::Exponential;
            break;
        case Curve::LOG_UP:
            init_step_vosc(s);
            s.waveform_revert = true;
            s.waveform_number = HS::Logarithmic;
            break;
        case Curve::LOG_DOWN:
            init_step_vosc(s);
            s.waveform_number = HS::Logarithmic;
            break;
        case Curve::VOSC:
            break;
        default:
            use_vosc = false;
            break;
        }

        if (new_step && use_vosc) {
            // Set the VOSC waveform
            osc = WaveformManager::VectorOscillatorFromWaveform(s.waveform_number);
            osc.Sustain();
            osc.Cycle(0);
        }

        const bool is_positive = scale_cv >= 0;
        if (use_vosc) {
            const uint16_t scale_abs = abs(scale_cv);
            const uint16_t scale_mag = scale_abs / 2;
            osc.SetScale(scale_mag);
    
            // Drive the waveform by step progress (0..3600 tenths of a degree).
            const int phase_deg_tenths = (static_cast<uint32_t>(s.waveform_revert ? 65535 - step_progress : step_progress) * 3600) / 65535;
            cv_out = osc.Phase(phase_deg_tenths);
            cv_out += scale_mag;
    
            uint16_t waveform_offset = Proportion(s.waveform_offset, 100, scale_abs);
            if (s.waveform_offset != 0) {
                cv_out -= waveform_offset;
            }
    
            // Apply the waveform option
            switch (s.waveform_option) {
            case Option::FOLD_UP:
                if (cv_out < 0) {
                    cv_out = -cv_out;
                }
                break;
            case Option::FOLD_DOWN:
                if (cv_out > 0) {
                    cv_out = -cv_out;
                }
                break;
            case Option::ZERO_UP:
                if (cv_out > 0) {
                    cv_out = 0;
                }
                break;
            case Option::ZERO_DOWN:
                if (cv_out < 0) {
                    cv_out = 0;
                }
                break;
            default:
                break;
            }
        }

        if (s.waveform_invert) {
            cv_out = -cv_out;
        }
        if (!is_positive) {
            cv_out = -cv_out;
        }

        // Retrigger fade: scale amplitude so the last retrigger reaches retrigger_fade%
        if (retrig_segments > 1) {
            EnvSeqManager::Output fade_mod = get_modulation(EnvSeqManager::ModulationMode::RETRIGGER_FADE, true);
            const int fade_target = constrain(
                s.retrigger_fade + (fade_mod.is_cv ? Proportion(fade_mod.cv, HEMISPHERE_MAX_INPUT_CV, 100) : 0),
                0,
                100
            );
            const int fade_pct = 100 - ((100 - fade_target) * retrig_index) / (retrig_segments - 1);
            cv_out = (static_cast<int32_t>(cv_out) * fade_pct) / 100;
        }

        int cv = constrain(offset_cv + cv_out, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
        if (new_step) {
            // Store the current step CV start value
            cv_step_start = cv;
        }

        // Output the current step CV value
        Out(0, cv);

        // Output to CV2 and linked outputs
        for (uint8_t i = 0; i < (manager.IsLinked(hemisphere) ? 3 : 1); i++) {
            EnvSeqManager::Output output = EnvSeqManager::Output{
                is_cv: i == 0 ? output2.is_cv : linked_data[i - 1].output.is_cv,
                cv: i == 0 ? output2.cv : linked_data[i - 1].output.cv,
            };
            bool do_gate = false;
            const bool was_cv = output.is_cv;
            const EnvSeqManager::OutputMode output_mode = i == 0 ? output2_mode : linked_data[i - 1].modulation.output_mode;

            switch (output_mode) {
            case EnvSeqManager::OutputMode::COPY:
                output.is_cv = true;
                output.cv = cv;
                break;
            case EnvSeqManager::OutputMode::COPY_INV:
                output.is_cv = true;
                output.cv = constrain(offset_cv + (scale_cv - cv_out), HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
                break;
            case EnvSeqManager::OutputMode::COPY_INV0:
                output.is_cv = true;
                output.cv = constrain(-cv, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
                break;
            case EnvSeqManager::OutputMode::COPY_INVO:
                output.is_cv = true;
                output.cv = constrain(offset_cv - cv_out, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
                break;
            case EnvSeqManager::OutputMode::COPY_ABOVE0:
                output.is_cv = true;
                output.cv = constrain(abs(cv_out), HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
                break;
            case EnvSeqManager::OutputMode::CV_STEP_START:
                output.is_cv = true;
                output.cv = cv_step_start;
                break;
            case EnvSeqManager::OutputMode::GATE_STEP:
                output.is_cv = false;
                do_gate = new_step && s.curve != Curve::NONE;
                break;
            case EnvSeqManager::OutputMode::GATE_STEP_INCL_RETRIGGERS:
                output.is_cv = false;
                do_gate = s.curve != Curve::NONE && (new_step || retrig_edge);
                break;
            case EnvSeqManager::OutputMode::GATE_SEQUENCE:
                output.is_cv = false;
                do_gate = sequence_restarted && s.curve != Curve::NONE;
                break;
            default:
                break;
            }

            if (!output.is_cv) {
                if (do_gate) {
                    output.cv = GATE_STOP_TICKS;
                } else if (was_cv) {
                    output.cv = 0;
                }
            }

            if (i == 0) {
                output2.is_cv = output.is_cv;
                output2.cv = output.cv;
                if (output2.is_cv) {
                    Out(1, output2.cv);
                } else {
                    GateOut(1, output2.cv != 0);
                    if (output2.cv > 0) {
                        output2.cv--;
                    }
                }
            } else {
                if (!output.is_cv && output.cv > 0) {
                    output.cv--;
                }
                manager.SetOutput(hemisphere, i - 1, output);
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
        if (random_cursor == RandomCursor::RANDOM_APPLY) {
            // Randomize the steps and return to the main view
            randomize_steps();
            random_cursor = RandomCursor::MAX_RANDOM_CURSOR;
            return;
        }

        if (linked_cursor == LinkedCursor::UNLINK) {
            // Unlink and return to the main view
            manager.SetLink(hemisphere, false);
            linked_cursor = LinkedCursor::MAX_LINKED_CURSOR;
            return;
        }

        if (linked_cursor != LinkedCursor::MAX_LINKED_CURSOR || random_cursor != RandomCursor::MAX_RANDOM_CURSOR) {
            // Keep other view open and toggle the cursor so it edits the current option
            CursorToggle();
            return;
        }

        if (cursor == EnvSeqCursor::LINK) {
            // Link and open linked view
            manager.SetLink(hemisphere, true);
            linked_cursor = LinkedCursor::UNLINK;
            return;
        }

        if (cursor == EnvSeqCursor::RANDOM) {
            // Open random view
            random_cursor = RandomCursor::RANDOM_APPLY;
            return;
        }

        if (cursor == EnvSeqCursor::TRIGGER2) {
            trigger2 = !trigger2;
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
    void AuxButton() {
        if (cursor > EnvSeqCursor::STEP_VIEW) {
          step_select = !step_select;
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

        if (random_cursor < MAX_RANDOM_CURSOR) {
            if (!EditMode()) {
                // Going past the available options will return to the main view.
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
            case RandomCursor::RANDOM_VOSC:
                random_vosc = !random_vosc;
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
            case RandomCursor::RANDOM_RETRIGGER_FADES:
                random_retrigger_fades = !random_retrigger_fades;
                break;
            }
            
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
            break;
        case EnvSeqCursor::STEP_OFFSET:
            steps[step_view].offset = (int16_t)constrain(steps[step_view].offset + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
            break;
        case EnvSeqCursor::STEP_SCALE:
            steps[step_view].scale = (int16_t)constrain(steps[step_view].scale + direction, HEMISPHERE_MIN_CV / OFFSET_SCALE_INCREMENT, HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT);
            break;
        case EnvSeqCursor::STEP_CURVE:
            steps[step_view].curve = (Curve)constrain(steps[step_view].curve + direction, 0, Curve::MAX_CURVE - 1);
            break;
        case EnvSeqCursor::STEP_WAVEFORM_NUMBER:
            steps[step_view].waveform_number = WaveformManager::GetNextWaveform(steps[step_view].waveform_number, direction);
            break;
        case EnvSeqCursor::STEP_WAVEFORM_OFFSET:
            steps[step_view].waveform_offset = (uint8_t)constrain(steps[step_view].waveform_offset + direction, 0, 100);
            break;
        case EnvSeqCursor::STEP_WAVEFORM_REVERT:
            steps[step_view].waveform_revert = !steps[step_view].waveform_revert;
            break;
        case EnvSeqCursor::STEP_WAVEFORM_INVERT:
            steps[step_view].waveform_invert = !steps[step_view].waveform_invert;
            break;
        case EnvSeqCursor::STEP_WAVEFORM_OPTION:
            steps[step_view].waveform_option = (Option)constrain(steps[step_view].waveform_option + direction, 0, Option::MAX_OPTIONS - 1);
            break;
        case EnvSeqCursor::STEP_TRIGGERS:
            steps[step_view].triggers = (uint8_t)constrain(steps[step_view].triggers + direction, 0, 7);
            break;
        case EnvSeqCursor::STEP_CLOCKS:
            steps[step_view].clocks = (uint8_t)constrain(steps[step_view].clocks + direction, 0, 7);
            break;
        case EnvSeqCursor::STEP_LENGTH:
            steps[step_view].length = (uint8_t)constrain(steps[step_view].length + direction, 1, 200);
            break;
        case EnvSeqCursor::STEP_PROBABILITY:
            steps[step_view].probability = (uint8_t)constrain(steps[step_view].probability + direction, 0, 100);
            break;
        case EnvSeqCursor::STEP_RETRIGGER_FADE:
            steps[step_view].retrigger_fade = (uint8_t)constrain(steps[step_view].retrigger_fade + direction, 0, 100);
            break;
        case EnvSeqCursor::STEP_MOD_MARK:
            steps[step_view].mod_mark = !steps[step_view].mod_mark;
            break;
        }
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
        Pack(data, PackLocation {34, 1}, random_scales);
        Pack(data, PackLocation {35, 1}, random_curves);
        Pack(data, PackLocation {36, 1}, random_vosc);
        Pack(data, PackLocation {37, 1}, random_lengths);
        Pack(data, PackLocation {38, 1}, random_triggers);
        Pack(data, PackLocation {39, 1}, random_clocks);
        Pack(data, PackLocation {40, 1}, random_mod_marks);
        Pack(data, PackLocation {41, 1}, random_retrigger_fades);

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
        random_scales = Unpack(data, PackLocation {34, 1});
        random_curves = Unpack(data, PackLocation {35, 1});
        random_vosc = Unpack(data, PackLocation {36, 1});
        random_lengths = Unpack(data, PackLocation {37, 1});
        random_triggers = Unpack(data, PackLocation {38, 1});
        random_clocks = Unpack(data, PackLocation {39, 1});
        random_mod_marks = Unpack(data, PackLocation {40, 1});
        random_retrigger_fades = Unpack(data, PackLocation {41, 1});

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
    int8_t cursor = 0, linked_cursor = MAX_LINKED_CURSOR, random_cursor = MAX_RANDOM_CURSOR;
    EnvSeqManager& manager = EnvSeqManager::get();

    bool trigger2 = false; // Whether to trigger on TR2
    uint8_t num_steps = 8; // How many steps of the generated envelope to play before looping
    ModulationMode mod1_mode = ModulationMode::MOD; // Modulation mode for CV1
    EnvSeqManager::ModulationMode mod2_mode = EnvSeqManager::ModulationMode::LENGTH; // Modulation mode for CV2
    EnvSeqManager::OutputMode output2_mode = EnvSeqManager::OutputMode::GATE_STEP; // Mode for CV2 output
    EnvSeqManager::Output output2 = EnvSeqManager::Output{}; // Output for CV2

    bool random_offsets = true; // Whether to randomize step levels (offset)
    bool random_scales = true; // Whether to randomize step levels (scale)
    bool random_curves = true; // Whether to randomize step curves
    bool random_vosc = false; // Whether to randomize the VOSC flag for each step
    bool random_lengths = false; // Whether to randomize step lengths
    bool random_triggers = false; // Whether to randomize the triggers flag for each step
    bool random_clocks = false; // Whether to randomize the clocks flag for each step
    bool random_mod_marks = false; // Whether to randomize the mod_mark flag for each step
    bool random_retrigger_fades = false; // Whether to randomize the retrigger fade flag for each step
    
    bool reset_flag = false; // Prevent stepping forward after a reset
    bool step_select = false; // toggle for switching step_view while editing params
    int8_t step = -1; // Current step
    int8_t step_view = 0; // Viewed step for cursor
    Step steps[MAX_NUM_STEPS]; // Steps of the envelope sequence
    VectorOscillator osc; // VOSC oscillator

    uint32_t clock_ticks = 0; // Ticks between the last two clock inputs
    uint32_t step_start_tick = 0; // Tick when the current step started
    uint8_t step_clocks = 0; // Number of clocks since the current step started
    uint8_t last_retrig_index = 0; // Which retrigger segment we're currently in (for gate pulses)

    int16_t mod_cv_step = 0; // Value of the modulation input at step start
    int16_t mod_cv_seq = 0; // Value of the modulation input at sequence start
    int16_t cv_step_start = 0; // Value of the step CV start value

    // Controller for linked applet
    void controller_linked() {
        manager.SetModulationCV(hemisphere, 0, In(0));
        manager.SetModulationCV(hemisphere, 1, In(1));

        const EnvSeqManager::LinkedData *linked_data = manager.GetLinkedData(hemisphere);
        for (uint8_t i = 0; i < 2; i++) {
            if (linked_data[i].output.is_cv) {
                Out(i, linked_data[i].output.cv);
            } else {
                GateOut(i, linked_data[i].output.cv != 0);
            }
        }
    }

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

    // Effective step length (1-200%) with CV scaling.
    uint16_t effective_step_length(uint8_t length, int mod_cv) const {
        // Map CV (bipolar) to +/-100% change, then convert to a 1..200% scaler.
        const int cv_delta_pct = Proportion(mod_cv, HEMISPHERE_MAX_INPUT_CV, 100); // -100..+100
        const int cv_scale_pct = constrain(100 + cv_delta_pct, 1, 200);

        // Multiply per-step length by CV scale and clamp to the same 1..200% range.
        const uint16_t eff = constrain(((length * cv_scale_pct) + 50) / 100, 1, 200);

        return eff;
    }

    void draw_interface_linked() {
        uint8_t y = 5;
        const EnvSeqManager::LinkedData* linked_data = manager.GetLinkedData(hemisphere);

        y += 10;
        gfxStartCursor(0, y);
        if (DetentedIn(0)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(linked_data[0].modulation.mode));
        gfxEndCursor(linked_cursor == LinkedCursor::LINKED_MOD1_MODE);

        y += 10;
        gfxStartCursor(0, y);
        if (DetentedIn(1)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(linked_data[1].modulation.mode));
        gfxEndCursor(linked_cursor == LinkedCursor::LINKED_MOD2_MODE);

        y += 10;
        gfxStartCursor(0, y);
        gfxPrintIcon(output2_mode_icon(linked_data[0].modulation.output_mode));
        gfxPrint(output2_mode_string(linked_data[0].modulation.output_mode));
        gfxEndCursor(linked_cursor == LinkedCursor::LINKED_OUTPUT1_MODE);

        y += 10;
        gfxStartCursor(0, y);
        gfxPrintIcon(output2_mode_icon(linked_data[1].modulation.output_mode));
        gfxPrint(output2_mode_string(linked_data[1].modulation.output_mode));
        gfxEndCursor(linked_cursor == LinkedCursor::LINKED_OUTPUT2_MODE);

        y += 10;
        gfxStartCursor(0, y);
        gfxPrintIcon(LINK_ICON);
        gfxPrint("Unlink");
        gfxEndCursor(linked_cursor == LinkedCursor::UNLINK);
    }

    void draw_interface() {
        bool can_link = manager.CanLink(hemisphere);
        if (can_link && manager.IsLinked(hemisphere)) {
            draw_interface_linked();
            return;
        }

        if (random_cursor < MAX_RANDOM_CURSOR) {
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
        gfxStartCursor(0, y);
        if (DetentedIn(0)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPrint(10, y, mod1_mode_string(mod1_mode));
        gfxEndCursor(cursor == EnvSeqCursor::MOD1_MODE);

        if (can_link) {
            gfxStartCursor(54, y);
            gfxPrintIcon(LINK_ICON);
            gfxEndCursor(cursor == EnvSeqCursor::LINK);
        }

        y += 10;
        gfxStartCursor(0, y);
        if (DetentedIn(1)) {
            gfxPrintIcon(MOD_ICON);
        }
        gfxPrint(10, y, mod2_mode_string(mod2_mode));
        gfxEndCursor(cursor == EnvSeqCursor::MOD2_MODE);

        y += 10;
        gfxStartCursor(0, y);
        gfxPrintIcon(output2_mode_icon(output2_mode));
        gfxPrint(output2_mode_string(output2_mode));
        gfxEndCursor(cursor == EnvSeqCursor::OUTPUT2_MODE);
    
        y += 10;
        gfxStartCursor(0, y);
        gfxPrintIcon(RANDOM_ICON);
        gfxPrint("Random");
        gfxEndCursor(cursor == EnvSeqCursor::RANDOM);

        gfxStartCursor(50, y);
        if (trigger2) {
            gfxPrintIcon(TR_ICON);
            gfxPrintIcon(SUB_TWO);
        }
        gfxPos(63, y);
        gfxEndCursor(cursor == EnvSeqCursor::TRIGGER2);

        y += 10;
        gfxIcon(0, y, PhzIcons::stairs);
        if (step >= 0) {
            gfxPrint(10 + pad(10, step + 1), y, step + 1);
        }
        gfxPrint(22, y, "/");
        gfxStartCursor(28, y);
        gfxPrint(28 + pad(10, num_steps), y, num_steps);
        gfxPos(40, y);
        gfxEndCursor(cursor == EnvSeqCursor::NUM_STEPS);

        gfxStartCursor(44, y);
        gfxPrintIcon(RESET_ICON);
        gfxEndCursor(cursor == EnvSeqCursor::RESET);

        gfxStartCursor(54, y);
        gfxPrintIcon(LOOP_ICON);
        gfxEndCursor(cursor == EnvSeqCursor::INIT);
    }

    void draw_step_view() {
        const Step& s = steps[step_view];
        uint8_t display_step = step_view + 1;
        uint8_t y = 15;

        gfxIcon(0, y, PhzIcons::stairs);
        gfxStartCursor(10, y);
        gfxPrint(10 + pad(10, display_step), y, display_step);
        gfxPos(22, y);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_VIEW);

        if (cursor >= EnvSeqCursor::STEP_TRIGGERS) {
          draw_step_view_page3();
          return;
        }
        if (cursor >= EnvSeqCursor::STEP_WAVEFORM_NUMBER) {
          draw_step_view_page2();
          return;
        }

        y += 10;
        gfxPrint(0, y, "Off");
        gfxStartCursor(18, y);
        gfxPrintVoltage(s.offset * OFFSET_SCALE_INCREMENT);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_OFFSET);

        y += 10;
        gfxPrint(0, y, "Scl");
        gfxStartCursor(18, y);
        gfxPrintVoltage(s.scale * OFFSET_SCALE_INCREMENT);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_SCALE);

        y += 10;
        gfxIcon(0, y, WAVEFORM_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10, y, curve_string(s.curve));
        gfxPos(63, y);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_CURVE);
    }

    void draw_step_view_page2() {
        const Step& s = steps[step_view];
        uint8_t y = 25;

        gfxIcon(0, y, WAVEFORM_ICON);
        gfxStartCursor(10, y);
        gfxPrint(s.waveform_number + 1);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_WAVEFORM_NUMBER);

        y += 10;
        gfxIcon(0, y, OFFSET_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10 + pad(100, s.waveform_offset), y, s.waveform_offset);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_WAVEFORM_OFFSET);

        y += 10;
        gfxIcon(0, y, s.waveform_revert ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("Rev");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_WAVEFORM_REVERT);

        gfxIcon(30, y, s.waveform_invert ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(40, y);
        gfxPrint("Inv");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_WAVEFORM_INVERT);

        y += 10;
        gfxIcon(0, y, WAVEFORM_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10, y, option_string(s.waveform_option));
        gfxPos(63, y);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_WAVEFORM_OPTION);
    }

    void draw_step_view_page3() {
        const Step& s = steps[step_view];
        uint8_t y = 25;

        gfxIcon(0, y, GATE_ICON);
        gfxStartCursor(10, y);
        gfxPrint(s.triggers + 1);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_TRIGGERS);
        gfxIcon(22, y, CLOCK_ICON);
        gfxStartCursor(32, y);
        gfxPrint(s.clocks + 1);
        gfxEndCursor(cursor == EnvSeqCursor::STEP_CLOCKS);

        y += 10;
        gfxIcon(0, y, LENGTH_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10 + pad(100, s.length), y, s.length);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_LENGTH);

        y += 10;
        gfxIcon(0, y, RANDOM_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10 + pad(100, s.probability), y, s.probability);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_PROBABILITY);

        y += 10;
        gfxIcon(0, y, GAUGE_ICON);
        gfxStartCursor(10, y);
        gfxPrint(10 + pad(100, s.retrigger_fade), y, s.retrigger_fade);
        gfxPrint("%");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_RETRIGGER_FADE);

        //y += 10;
        gfxIcon(55, y, s.mod_mark ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(40, y-10);
        gfxPrint("Mod2");
        gfxEndCursor(cursor == EnvSeqCursor::STEP_MOD_MARK);
    }

    void draw_random_view() {
        uint8_t y = 5;

        y += 10;
        gfxIcon(0, y, random_offsets ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("Ofs");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_OFFSETS);

        gfxIcon(31, y, random_scales ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, y);
        gfxPrint("Scl");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_SCALES);

        y += 10;
        gfxIcon(0, y, random_curves ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("Crv");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_CURVES);

        gfxIcon(31, y, random_vosc ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, y);
        gfxPrint("Osc");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_VOSC);

        y += 10;
        gfxIcon(0, y, random_lengths ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("Len");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_LENGTHS);

        gfxIcon(31, y, random_triggers ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, y);
        gfxPrint("Trg");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_TRIGGERS);

        y += 10;
        gfxIcon(0, y, random_clocks ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("Clk");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_CLOCKS);

        gfxIcon(31, y, random_mod_marks ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(41, y);
        gfxPrint("Mod");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_MOD_MARKS);

        y += 10;
        gfxIcon(0, y, random_retrigger_fades ? CHECK_ON_ICON : CHECK_OFF_ICON);
        gfxStartCursor(10, y);
        gfxPrint("RFd");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_RETRIGGER_FADES);

        gfxStartCursor(31, y);
        gfxPrintIcon(RANDOM_ICON);
        gfxPrint("RND");
        gfxEndCursor(random_cursor == RandomCursor::RANDOM_APPLY);
    }

    void init_steps() {
        int scale = HEMISPHERE_MAX_CV / (2 * OFFSET_SCALE_INCREMENT);
        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            steps[i].offset = 0;
            steps[i].scale = scale;
            steps[i].curve = Curve::RAMP_DOWN;
            init_step_vosc(steps[i]);
            steps[i].triggers = 0;
            steps[i].clocks = 0;
            steps[i].length = 100;
            steps[i].probability = 100;
            steps[i].retrigger_fade = 100;
            steps[i].mod_mark = false;
        }
    }

    void init_step_vosc(Step& s) {
        s.waveform_number = HS::Sawtooth;
        s.waveform_offset = 0;
        s.waveform_revert = false;
        s.waveform_invert = false;
        s.waveform_option = Option::NO_OPTION;
    }

    void randomize_steps() {
        const int max_units = HEMISPHERE_MAX_CV / OFFSET_SCALE_INCREMENT;
        const int offset_max = max_units * 0.2;

        // Randomize scale only between 20% and 50%
        const int scale_min = max_units * 0.2;
        const int scale_max = max_units * 0.5;

        const uint8_t total_waveform_count = WaveformManager::WaveformCount() + HS::WAVEFORM_LIBRARY_COUNT;

        for (uint8_t i = 0; i < MAX_NUM_STEPS; i++) {
            if (random_offsets) {
                steps[i].offset = random(0, offset_max + 1);
            }
            if (random_scales) {
                steps[i].scale = random(scale_min, scale_max + 1);
            }
            if (random_curves) {
                // Randomize curve, but make sure VOSC is allowed
                do {
                    steps[i].curve = (Curve)random(0, Curve::MAX_CURVE);
                } while (steps[i].curve == Curve::VOSC && !random_vosc);
            }
            if (random_vosc && steps[i].curve == Curve::VOSC) {
                steps[i].waveform_number = WaveformManager::GetNextWaveform(random(0, total_waveform_count), 1);
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
            if (random_retrigger_fades) {
                steps[i].retrigger_fade = random(0, 101);
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
            return "Length";
        case EnvSeqManager::ModulationMode::STEP_SEL:
            return "StepSel";
        case EnvSeqManager::ModulationMode::RETRIGGER_FADE:
            return "RetrgFd";
        case EnvSeqManager::ModulationMode::MOD:
            return "Mod";
        case EnvSeqManager::ModulationMode::MOD_MARK:
            return "ModMark";
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
        case EnvSeqManager::OutputMode::COPY_INV:
            return "CpyInv";
        case EnvSeqManager::OutputMode::COPY_INV0:
            return "CpyInv0";
        case EnvSeqManager::OutputMode::COPY_INVO:
            return "CpyInvO";
        case EnvSeqManager::OutputMode::COPY_ABOVE0:
            return "CpyAbv0";
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

    const char* curve_string(Curve curve) {
        switch (curve) {
        case Curve::NONE:
            return "None";
        case Curve::ZERO:
            return "Zero";
        case Curve::FLAT:
            return "Flat";
        case Curve::RAMP_UP:
            return "RampUp";
        case Curve::RAMP_DOWN:
            return "RampDown";
        case Curve::TRIANGLE:
            return "Triangle";
        case Curve::EXP_UP:
            return "ExpUp";
        case Curve::EXP_DOWN:
            return "ExpDown";
        case Curve::LOG_UP:
            return "LogUp";
        case Curve::LOG_DOWN:
            return "LogDown";
        case Curve::VOSC:
            return "VOSC";
        default:
            return "";
        }
    }

    const char* option_string(Option option) {
        switch (option) {
        case Option::NO_OPTION:
            return "None";
        case Option::FOLD_UP:
            return "FoldUp";
        case Option::FOLD_DOWN:
            return "FoldDwn";
        case Option::ZERO_UP:
            return "ZeroUp";
        case Option::ZERO_DOWN:
            return "ZeroDwn";
        default:
            return "";
        }
    }
};
