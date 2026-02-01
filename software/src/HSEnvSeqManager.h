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

class EnvSeqManager {
public:
    enum ModulationMode : uint8_t {
        LENGTH = 0, // Modulate the step length
        STEP_SEL = 1, // Modulate the step selector
        RETRIGGER_LEVEL = 2, // Modulate the retrigger level
        MOD = 3, // Modulate all steps
        MOD_MARK = 4, // Modulate marked steps ONLY

        MAX_MODULATION_MODE,
    };

    enum OutputMode : uint8_t {
        COPY = 0, // Output the same CV as CV1
        INV = 1, // Output the inverse of CV1, centered at the amplitude center
        INVO = 2, // Output the inverse of CV1, centered at the waveform offset
        CV_STEP_START = 3, // Output the step CV start value and hold it until next step
        GATE_STEP = 4, // Output a gate at each playing step
        GATE_STEP_INCL_RETRIGGERS = 5, // Output a gate at each playing step, including retriggers
        GATE_SEQUENCE = 6, // Output a gate at the start of the sequence

        MAX_OUTPUT_MODE,
    };

    struct Modulation {
        ModulationMode mode = ModulationMode::MOD;
        OutputMode output_mode = OutputMode::COPY;
        int cv = 0;
    };

    struct Output {
        bool is_cv = false;
        int cv = 0; // When not is_cv, cv represents the number of ticks for which the gate is high
    };

    struct LinkedData {
        Modulation modulation;
        Output output;
    };

    struct LinkedState {
        bool linked = false;
        LinkedData data[2] = {
            LinkedData{Modulation{ModulationMode::MOD_MARK, OutputMode::INV}, Output{}},
            LinkedData{Modulation{ModulationMode::STEP_SEL, OutputMode::CV_STEP_START}, Output{}},
        };
    };

    enum Option : uint8_t {
        NO_OPTION = 0,
        FOLD_UP = 1,
        FOLD_DOWN = 2,
        ZERO_UP = 3,
        ZERO_DOWN = 4,

        MAX_OPTIONS,
    };
    struct Step {
        uint16_t shape; // Shape for the step. 0..VOSC-1 is the enum value, >=VOSC is the VOSC waveform number.
        int16_t offset; // Scaled step offset CV (by OFFSET_SCALE_INCREMENT)
        int16_t amp; // Scaled step amp CV (by OFFSET_SCALE_INCREMENT)
        bool waveform_revert; // Reverts the waveform (VOSC only)
        bool waveform_invert; // Inverts the waveform (VOSC only)
        uint8_t length; // Length 1-200% of envelope duration for this step
        bool mod_mark; // Whether this step is marked for modulation

        uint8_t waveform_offset : 7; // Offset where the envelope offset is on the waveform (0..100) in 1% steps (VOSC only)
        Option waveform_option : 3; // Option for the waveform (VOSC only)
        uint8_t triggers : 3; // Number of times to trigger the step (0-7)
        uint8_t clocks : 3; // Number of clocks this step lasts for (0-7)
        uint8_t probability : 7; // Probability 0-100% of this step being played
        int8_t retrigger_level : 5; // Retrigger level (-15..15). -15: fade in 0->100, 0: no fade, 15: fade out 100->0
    };

private:
    EnvSeqManager() = default;
    static EnvSeqManager* instance;
    bool registered[4] = {false, false, false, false};
    LinkedState linked_states[2] = {LinkedState{}, LinkedState{}};
    Step* clipboard = nullptr;

public:
    static EnvSeqManager& get() {
        if (!instance) {
            instance = new EnvSeqManager;
        }

        return *instance;
    }

    // Register the applet in the manager
    void Register(HEM_SIDE hemisphere) {
        registered[hemisphere] = true;
    }

    // Unregister the applet from the manager
    void Unload(HEM_SIDE hemisphere) {
        registered[hemisphere] = false;
        SetLink(hemisphere, false);
        if (hemisphere % 2 == 0) {
            // Unloaded left applet
            linked_states[hemisphere / 2].linked = false;
        }
    }

    // Check if the applet can be linked to the other applet
    // The applet can be linked if it's in the right hemisphere and both applets are registered
    bool CanLink(HEM_SIDE hemisphere) {
        return hemisphere % 2 == 1
            && registered[hemisphere]
            && registered[hemisphere - 1];
    }

    // Set the link state for the applet. Only the right applet can be linked
    void SetLink(HEM_SIDE hemisphere, bool value) {
        if (hemisphere % 2 == 1) {
            LinkedState& state = linked_states[(hemisphere - 1) / 2];
            state.linked = value;
            if (!value) {
                state.data[0] = LinkedData{};
                state.data[1] = LinkedData{};
            }
        }
    }

    // Check if the applet is linked to the other applet
    bool IsLinked(HEM_SIDE hemisphere) {
        return linked_states[(hemisphere - hemisphere % 2) / 2].linked;
    }

    // Get the linked data for the applet
    const LinkedData* GetLinkedData(HEM_SIDE hemisphere) const {
        return linked_states[(hemisphere - hemisphere % 2) / 2].data;
    }

    // Set the modulation mode for the applet (only the right applet can do it)
    void SetModulationMode(HEM_SIDE hemisphere, uint8_t part, const ModulationMode& mode) {
        if (hemisphere % 2 == 1) {
            linked_states[(hemisphere - 1) / 2].data[part % 2].modulation.mode = mode;
        }
    }

    // Set the modulation output mode for the applet (only the right applet can do it)
    void SetModulationOutputMode(HEM_SIDE hemisphere, uint8_t part, const OutputMode& output_mode) {
        if (hemisphere % 2 == 1) {
            linked_states[(hemisphere - 1) / 2].data[part % 2].modulation.output_mode = output_mode;
        }
    }

    // Set the modulation CV for the applet (only the right applet can do it)
    void SetModulationCV(HEM_SIDE hemisphere, uint8_t part, const int16_t& cv) {
        if (hemisphere % 2 == 1) {
            linked_states[(hemisphere - 1) / 2].data[part % 2].modulation.cv = cv;
        }
    }

    // Set the output for the applet (only the left applet can do it)
    void SetOutput(HEM_SIDE hemisphere, uint8_t part, const Output& output) {
        if (hemisphere % 2 == 0) {
            linked_states[hemisphere / 2].data[part % 2].output = output;
        }
    }

    // Copy a step to the clipboard
    void CopyStep(const Step& step) {
        if (clipboard) {
            delete clipboard;
        }

        clipboard = new Step{step};
    }

    // Paste a step from the clipboard
    void PasteStep(Step& step) {
        if (clipboard) {
            step = *clipboard;
        }
    }

    // Check if there is a step in the clipboard
    bool HasClipboard() const {
        return clipboard != nullptr;
    }
};

EnvSeqManager* EnvSeqManager::instance = 0;
