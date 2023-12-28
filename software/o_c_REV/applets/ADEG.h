// Copyright (c) 2018, Jason Justian
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

#define HEM_ADEG_MAX_VALUE 255
#define HEM_ADEG_MAX_TICKS 33333

class ADEG : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "AD EG";
    }

    void Start() {
        signal = 0;
        phase = 0;
        attack = 50;
        decay = 50;
    }

    void Controller() {
        if (Clock(0)) {
            // Trigger the envelope
            phase = 1; // Return to attack phase
            effective_attack = attack;
            effective_decay = decay;
        } else if (Clock(1)) {
            // Trigger the envelope in reverse
            phase = 1;
            effective_attack = decay;
            effective_decay = attack;
        }

        if (phase > 0) {
            simfloat target;
            if (phase == 1) target = int2simfloat(HEMISPHERE_MAX_CV); // Rise to max for attack
            if (phase == 2) target = 0; // Fall to zero for decay

            //if (signal != target) { // Logarhythm fix 8/2020
                int segment = phase == 1
                    ? effective_attack + Proportion(DetentedIn(0), HEMISPHERE_MAX_INPUT_CV, HEM_ADEG_MAX_VALUE)
                    : effective_decay + Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, HEM_ADEG_MAX_VALUE);
                segment = constrain(segment, 0, HEM_ADEG_MAX_VALUE);
                simfloat remaining = target - signal;

                // The number of ticks it would take to get from 0 to HEMISPHERE_MAX_CV
                int max_change = Proportion(segment, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS);

                // The number of ticks it would take to move the remaining amount at max_change
                int ticks_to_remaining = Proportion(simfloat2int(remaining), HEMISPHERE_MAX_CV, max_change);
                if (ticks_to_remaining < 0) ticks_to_remaining = -ticks_to_remaining;

                simfloat delta;
                if (ticks_to_remaining <= 0) {
                    delta = remaining;
                } else {
                    delta = remaining / ticks_to_remaining;
                }
                signal += delta;

                if (simfloat2int(signal) >= HEMISPHERE_MAX_CV && phase == 1) phase = 2;

                // Check for EOC
                if (simfloat2int(signal) <= 0 && phase == 2) {
                    ClockOut(1);
                    phase = 0;
                }
            //}
            Out(0, simfloat2int(signal));
        }
    }

    void View() {
        DrawIndicator();
    }

    void OnButtonPress() {
        cursor = 1 - cursor;
    }

    void OnEncoderMove(int direction) {
        if (cursor == 0) {
            attack = constrain(attack + direction, 0, HEM_ADEG_MAX_VALUE);
            last_ms_value = Proportion(attack, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
        }
        else {
            decay = constrain(decay + direction, 0, HEM_ADEG_MAX_VALUE);
            last_ms_value = Proportion(decay, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
        }
        last_change_ticks = OC::CORE::ticks;
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, attack);
        Pack(data, PackLocation {8,8}, decay);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        attack = Unpack(data, PackLocation {0,8});
        decay = Unpack(data, PackLocation {8,8});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Trg 2=Trg Revers";
        help[HEMISPHERE_HELP_CVS]      = "1=A mod 2=D mod";
        help[HEMISPHERE_HELP_OUTS]     = "A=Output B=EOC";
        help[HEMISPHERE_HELP_ENCODER]  = "Attack/Decay";
        //                               "------------------" <-- Size Guide
    }
    
private:
    simfloat signal; // Current signal level for each channel
    int phase; // 0=Not running 1=Attack 2=Decay
    int cursor; // 0 = Attack, 1 = Decay
    int last_ms_value;
    int last_change_ticks;
    int effective_attack; // Attack and decay for this particular triggering
    int effective_decay;  // of the EG, so that it can be triggered in reverse!

    // Settings
    int attack; // Time to reach signal level if signal < 5V
    int decay; // Time to reach signal level if signal > 0V

    void DrawIndicator() {
        int a_x = Proportion(attack, HEM_ADEG_MAX_VALUE, 31);
        int d_x = a_x + Proportion(decay, HEM_ADEG_MAX_VALUE, 31);

        if (d_x > 0) { // Stretch to use the whole viewport
            a_x = Proportion(62, d_x, a_x);
            d_x = Proportion(62, d_x, d_x);
        }

        gfxLine(0, 62, a_x, 33, cursor == 1);
        gfxLine(a_x, 33, d_x, 62, cursor == 0);

        // Output indicators
        gfxRect(1, 15, ProportionCV(ViewOut(0), 62), 6);

        // Change indicator, if necessary
        if (OC::CORE::ticks - last_change_ticks < 20000) {
            gfxPrint(15, 43, last_ms_value);
            gfxPrint("ms");
        }
    }
};

