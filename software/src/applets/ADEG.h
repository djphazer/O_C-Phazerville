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

#ifndef _HEM_ADEG_H_
#define _HEM_ADEG_H_

#define HEM_ADEG_MAX_VALUE 255
#define HEM_ADEG_MAX_TICKS 33333

class ADEG : public HemisphereApplet {
public:
    // CV adds to a stage like the encoder does: 0-5V covers the whole
    // range, so 5V reaches the longest time along the same curve
    int CVToStage(int ch) {
        return Proportion(DetentedIn(ch), 5 * ONE_OCTAVE, HEM_ADEG_MAX_VALUE);
    }

    static int ScaleStageToTicks(int setting) {
        const int p = constrain(setting, 0, HEM_ADEG_MAX_VALUE);
        return int(HS::kStageTicks[p]);
    }

    const char* applet_name() { // Maximum 10 characters
        return "AD EG";
    }
    const uint8_t* applet_icon() { return PhzIcons::AD_EG; }

    void Start() {
        signal = 0;
        phase = 0;
        attack = 50;
        decay = 50;
    }

    void Controller() {
        // knob + CV, as heard and as shown
        attack_cv = constrain(attack + CVToStage(0), 0, HEM_ADEG_MAX_VALUE);
        decay_cv = constrain(decay + CVToStage(1), 0, HEM_ADEG_MAX_VALUE);

        if (Clock(0)) {
            // Trigger the envelope
            phase = 1; // Return to attack phase
            reverse = false;
            effective_attack = attack;
            effective_decay = decay;
        } else if (Clock(1)) {
            // Trigger the envelope in reverse
            phase = 1;
            reverse = true;
            effective_attack = decay;
            effective_decay = attack;
        }

        if (phase > 0) {
            simfloat target;
            if (phase == 1) target = int2simfloat(HEMISPHERE_MAX_CV); // Rise to max for attack
            if (phase == 2) target = 0; // Fall to zero for decay

            //if (signal != target) { // Logarhythm fix 8/2020
                int segment = phase == 1
                    ? effective_attack + CVToStage(0)
                    : effective_decay + CVToStage(1);
                segment = constrain(segment, 0, HEM_ADEG_MAX_VALUE);
                simfloat remaining = target - signal;

                // The number of ticks it would take to get from 0 to HEMISPHERE_MAX_CV
                int max_change = ScaleStageToTicks(segment);

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
                    reverse = false;
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
        if (cursor == 0) attack = constrain(attack + direction, 0, HEM_ADEG_MAX_VALUE);
        else decay = constrain(decay + direction, 0, HEM_ADEG_MAX_VALUE);
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
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Trigger";
    help[HELP_DIGITAL2] = "Reverse";
    help[HELP_CV1]      = "Attack";
    help[HELP_CV2]      = "Decay";
    help[HELP_OUT1]     = "Output";
    help[HELP_OUT2]     = "EOC";
    help[HELP_EXTRA1]   = "";
    help[HELP_EXTRA2]   = "";
    //                    "---------------------" <-- Extra text size guide
  }

private:
    simfloat signal; // Current signal level for each channel
    int phase; // 0=Not running 1=Attack 2=Decay
    int cursor; // 0 = Attack, 1 = Decay
    int effective_attack; // Attack and decay for this particular triggering
    int effective_decay;  // of the EG, so that it can be triggered in reverse!
    bool reverse = false;  // for indicator graphics

    // Settings
    int attack; // Time to reach signal level if signal < 5V
    int decay; // Time to reach signal level if signal > 0V
    int attack_cv = 0; // the settings plus CV
    int decay_cv = 0;

    void DrawIndicator() {
        // Peak sits in the middle; attack grows to the left, decay to the right,
        // each on a fixed scale so the line length shows the stage length
        const int mid = 31;
        const int a_x = mid - Proportion(attack_cv, HEM_ADEG_MAX_VALUE, 31);
        const int d_x = mid + Proportion(decay_cv, HEM_ADEG_MAX_VALUE, 31);

        gfxLine(a_x, 62, mid, 33, cursor == 1);
        gfxLine(mid, 33, d_x, 62, cursor == 0);

        // scale ticks at 10ms, 100ms, 1s on both sides
        for (int idx : {19, 58, 181}) {
            const int dx = Proportion(idx, HEM_ADEG_MAX_VALUE, 31);
            gfxLine(mid - dx, 63, mid - dx, (idx == 181) ? 59 : 61);
            gfxLine(mid + dx, 63, mid + dx, (idx == 181) ? 59 : 61);
        }

        // Output indicators
        int rect_x = 1 + reverse * (61 - ProportionCV(ViewOut(0), 62));
        gfxRect(rect_x, 15, ProportionCV(ViewOut(0), 62), 6);

        // Length of the selected stage, same as ADSR
        gfxPrint(1, 23, cursor == 0 ? "A=" : "D=");
        const int ms_value = ScaleStageToTicks(cursor == 0 ? attack_cv : decay_cv) * 10 / 17;
        gfxPrint(ms_value / 10);
        gfxPrint(".");
        gfxPrint(ms_value % 10);
        gfxPrint("ms");
    }
};

#endif
