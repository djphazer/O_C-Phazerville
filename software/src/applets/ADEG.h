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

        // Inline precomputed LUT for monotonic attack/decay time mapping.
        // Index 0 gives instant response; indices 1..255 map to 1..33333 ticks.
        static constexpr uint16_t kStageTicks[HEM_ADEG_MAX_VALUE + 1] = {
            0, 1, 2, 5, 8, 13, 18, 25, 33, 41, 51, 62, 74, 86, 100, 115,
            131, 148, 166, 185, 205, 226, 248, 271, 295, 320, 346, 373, 401, 430, 461, 492,
            524, 558, 592, 627, 664, 701, 739, 779, 819, 861, 903, 947, 992, 1037, 1084, 1131,
            1180, 1230, 1281, 1332, 1385, 1439, 1494, 1550, 1606, 1664, 1723, 1783, 1844, 1906, 1969, 2033,
            2098, 2165, 2232, 2300, 2369, 2439, 2511, 2583, 2656, 2730, 2806, 2882, 2959, 3038, 3117, 3198,
            3279, 3362, 3445, 3530, 3616, 3702, 3790, 3879, 3968, 4059, 4151, 4243, 4337, 4432, 4528, 4625,
            4723, 4822, 4922, 5023, 5125, 5228, 5332, 5437, 5543, 5650, 5758, 5867, 5978, 6089, 6201, 6314,
            6429, 6544, 6660, 6778, 6896, 7016, 7136, 7257, 7380, 7504, 7628, 7754, 7880, 8008, 8137, 8266,
            8397, 8529, 8662, 8795, 8930, 9066, 9203, 9341, 9480, 9620, 9761, 9903, 10046, 10190, 10335, 10481,
            10628, 10776, 10925, 11075, 11227, 11379, 11532, 11686, 11842, 11998, 12156, 12314, 12473, 12634, 12795, 12958,
            13121, 13286, 13451, 13618, 13786, 13954, 14124, 14295, 14466, 14639, 14813, 14988, 15164, 15341, 15518, 15697,
            15877, 16058, 16240, 16423, 16607, 16792, 16978, 17166, 17354, 17543, 17733, 17924, 18116, 18310, 18504, 18699,
            18896, 19093, 19291, 19491, 19691, 19893, 20095, 20299, 20503, 20709, 20915, 21123, 21332, 21541, 21752, 21964,
            22177, 22390, 22605, 22821, 23038, 23256, 23475, 23695, 23916, 24137, 24361, 24585, 24810, 25036, 25263, 25491,
            25720, 25950, 26181, 26414, 26647, 26881, 27117, 27353, 27590, 27829, 28068, 28308, 28550, 28792, 29036, 29280,
            29526, 29773, 30020, 30269, 30519, 30769, 31021, 31274, 31527, 31782, 32038, 32295, 32553, 32812, 33072, 33333,
        };

        static int ScaleStageToTicks(int setting) {
            const int p = constrain(setting, 0, HEM_ADEG_MAX_VALUE);
            return int(kStageTicks[p]);
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
            last_ms_value = ScaleStageToTicks(attack) / 17;
        }
        else {
            decay = constrain(decay + direction, 0, HEM_ADEG_MAX_VALUE);
            last_ms_value = ScaleStageToTicks(decay) / 17;
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
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Trigger";
    help[HELP_DIGITAL2] = "Reverse";
    help[HELP_CV1]      = "Attack";
    help[HELP_CV2]      = "Decay";
    help[HELP_OUT1]     = "Output";
    help[HELP_OUT2]     = "EOC";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
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

#endif
