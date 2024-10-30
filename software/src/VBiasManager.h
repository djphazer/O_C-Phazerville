// Copyright (c) 2019, Jason Justian
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

// This singleton class has a few jobs related to keeping track of the bias value
// selected by the user and/or the app:
//
// (1) It allows advancing bias through three settings, one at a time
// (2) It allows setting the bias directly with a state
// (3) It shows a popup indicator for one second when the setting is advanced

#ifndef VBIAS_MANAGER_H
#define VBIAS_MANAGER_H

#include "OC_options.h"

#ifdef VOR 

#define BIAS_EDITOR_TIMEOUT 20000

namespace HS {
extern int octave_max;
}

class VBiasManager {
    static VBiasManager *instance;
    int bias_state;
    uint32_t last_advance_tick;

    VBiasManager() {
        bias_state = 0;
        last_advance_tick = 0;
    }

public:
    enum VState {
        BI = 0,
        ASYM,
        UNI,
    };
    /*
    static const int BI = 0;
    static const int ASYM = 1;
    static const int UNI = 2;
    */
    const int OCTAVE_BIAS[3] = {5, 3, 0};
    const int OCTAVE_MAX[3] = {5, 7, 10};

    static VBiasManager *get() {
        if (!instance) instance = new VBiasManager;
        return instance;
    }

    /*
     * Advance to the next state, when the button is pushed
     */
    void AdvanceBias() {
        // Only advance the bias if it's been less than a second since the last button press.
        // This is so that the first button press shows the popup without changing anything.
        if (OC::CORE::ticks - last_advance_tick < BIAS_EDITOR_TIMEOUT) {
            if (++bias_state > 2) bias_state = 0;
            instance->SetState(VBiasManager::VState(bias_state));
        }
        last_advance_tick = OC::CORE::ticks;
    }

    int IsEditing() {
        return (OC::CORE::ticks - last_advance_tick < BIAS_EDITOR_TIMEOUT);
    }

    /*
     * Change to a specific state. This should replace a direct call to OC::DAC::set_Vbias(), because it
     * allows VBiasManager to keep track of the current state so that the button advances the state as
     * expected. For example:
     *
     * #ifdef VOR
     *     VBiasManager *vbias_m = vbias_m->get();
     *     vbias_m->ChangeBiasToState(VBiasManager::BI);
     * #endif
     *
     */
    void SetState(VState new_bias_state) {
        int new_bias_value = OC::calibration_data.v_bias & 0xFFFF; // Bipolar = lower 2 bytes
        if (new_bias_state == VBiasManager::UNI) new_bias_value = OC::DAC::VBiasUnipolar;
        if (new_bias_state == VBiasManager::ASYM) new_bias_value = (OC::calibration_data.v_bias >> 16); // asym. = upper 2 bytes
        OC::DAC::set_Vbias(new_bias_value);
        bias_state = new_bias_state;

        OC::DAC::kOctaveZero = OCTAVE_BIAS[bias_state];
        HS::octave_max = OCTAVE_MAX[bias_state];
    }
    int GetState() {
        return bias_state;
    }

    // Vbias auto-config helper
    // Cross-reference OC_apps.ino for app IDs
    void SetStateForApp(const OC::App *app) {
        VState new_state = VBiasManager::ASYM; // default case
        
        switch (app->id)
        {
        /* Default cases can be omitted
        case TWOCC<'C','8'>::value: // Calibr8or
        case TWOCC<'A','S'>::value: // CopierMachine (or) ASR
        case TWOCC<'H','A'>::value: // Harrington 1200 (or) Triads
        case TWOCC<'A','T'>::value: // Automatonnetz (or) Vectors
        case TWOCC<'Q','Q'>::value: // Quantermain (or) 4x Quantizer
        case TWOCC<'M','!'>::value: // Meta-Q (or) 2x Quantizer
        case TWOCC<'S','Q'>::value: // Sequins (or) 2x Sequencer
        case TWOCC<'A','C'>::value: // Acid Curds (or) Chords
            new_state = VBiasManager::ASYM;
            break;
        */
        // Bi-polar +/-5V
        case TWOCC<'L','R'>::value: // Low-rents (or) Lorenz
            new_state = VBiasManager::BI;
            break;
        // Uni-polar 0-10V
        case TWOCC<'E','G'>::value: // Piqued (or) 4x EG
        case TWOCC<'B','B'>::value: // Dialectic Ping Pong (or) Balls
        case TWOCC<'B','Y'>::value: // Viznutcracker sweet (or) Bytebeats
        case TWOCC<'R','F'>::value: // References
            new_state = VBiasManager::UNI;
            break;
        case TWOCC<'P','L'>::value: // Quadraturia (or) Quadrature LFO
        case TWOCC<'H','S'>::value: // Hemisphere
            return; // cancel, it has its own VBias setting
        }
        instance->SetState(new_state);
    }

    /*
     * If the last state advance (with the button) was less than a second ago, draw the popup indicator
     */
    void DrawPopupPerhaps() {
        if (OC::CORE::ticks - last_advance_tick < BIAS_EDITOR_TIMEOUT) {
            graphics.clearRect(17, 7, 82, 43);
            graphics.drawFrame(18, 8, 80, 42);

            graphics.setPrintPos(20, 10);
            graphics.print("Range:");

            // Bipolar state
            graphics.setPrintPos(30, 20);
            graphics.print("-5V -> 5V");
            // Asym State
            graphics.setPrintPos(30, 30);
            graphics.print("-3V -> 7V");
            // Unipolar state
            graphics.setPrintPos(30, 40);
            graphics.print(" 0V -> 10V");

            graphics.setPrintPos(20, 20 + (bias_state * 10));
            graphics.print("> ");
        }
    }
};

#endif

#endif // VBIAS_MANAGER_H
