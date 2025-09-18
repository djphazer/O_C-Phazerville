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

// How fast the axon pulses when active
#define HEM_TLN_ACTIVE_TICKS 1500

class TLNeuron : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "TL Neuron";
    }
    const uint8_t* applet_icon() { return PhzIcons::thresholdLogicNeuron; }

    void Start() {
        selected = 0;
    }

    void Controller() {
        // Summing function: add up the three weights
        int sum = 0;
        ForEachChannel(ch)
        {
            if (Gate(ch)) {
                sum += dendrite_weight[ch];
                dendrite_activated[ch] = 1;
            } else {
                dendrite_activated[ch] = 0;
            }
        }
        if (In(0) > (HEMISPHERE_MAX_INPUT_CV / 2)) {
            sum += dendrite_weight[2];
            dendrite_activated[2] = 1;
        } else {
            dendrite_activated[2] = 0;
        }

        // Threshold function: fire the axon if the sum is GREATER THAN the threshhold
        // Both outputs have the same signal, in case you want to feed an output back
        // to an input.
        if (!axon_activated) axon_radius = 5;
        axon_activated = (sum > threshold);
        ForEachChannel(ch) GateOut(ch, axon_activated);

        // Increase the axon radius via timer
        if (--axon_countdown < 0) {
            axon_countdown = HEM_TLN_ACTIVE_TICKS;
            ++axon_radius;
            if (axon_radius > 14) axon_radius = 5;
        }
    }

    void View() {
        DrawDendrites();
        DrawAxon();
        DrawStates();
    }

    void OnButtonPress() {
        if (++selected > 3) selected = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        if (selected < 3) {
            dendrite_weight[selected] = constrain(dendrite_weight[selected] + direction, -9, 9);
        } else {
            threshold = constrain(threshold + direction, -27, 27);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,5}, dendrite_weight[0] + 9);
        Pack(data, PackLocation {5,5}, dendrite_weight[1] + 9);
        Pack(data, PackLocation {10,5}, dendrite_weight[2] + 9);
        Pack(data, PackLocation {15,6}, threshold + 27);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        dendrite_weight[0] = Unpack(data, PackLocation {0,5}) - 9;
        dendrite_weight[1] = Unpack(data, PackLocation {5,5}) - 9;
        dendrite_weight[2] = Unpack(data, PackLocation {10,5}) - 9;
        threshold = Unpack(data, PackLocation {15,6}) - 27;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Dendrt1";
        help[HELP_DIGITAL2] = "Dendrt2";
        help[HELP_CV1]      = "Dendrt3";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "AxonOut";
        help[HELP_OUT2]     = "AxonOut";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "Gate if wght > thresh";
       //                   "---------------------" <-- Extra text size guide
    }
    
private:
    int selected; // Which thing is selected (Dendrite 1, 2, 3 weights; Axon threshold)
    int dendrite_weight[3] = {5, 5, 0};
    int threshold = 9;
    bool dendrite_activated[3];
    bool axon_activated;
    int axon_radius = 5;
    int axon_countdown;

    void DrawDendrites() {
        for (int d = 0; d < 3; d++)
        {
            int weight = dendrite_weight[d];
            byte indent = d == 1 ? 4 : 0;
            gfxCircle(9 + indent, 21 + (16 * d), 8); // Dendrite
            gfxPrint((weight < 0 ? 1 : 6) + indent , 18 + (16 * d), weight);
            if (selected == d && CursorBlink()) gfxCircle(9 + indent, 21 + (16 * d), 7);
        }
    }

    void DrawAxon() {
        gfxCircle(48, 37, 12);
        int x = 41; // Starting x position for number
        if (threshold < 10 && threshold > -10) x += 5; // Shove over a bit if a one-digit number
        if (threshold < 0) x -= 5; // Pull back if a sign is necessary
        gfxPrint(x, 34, threshold);
        if (selected == 3 && CursorBlink()) gfxCircle(48, 37, 11);
    }

    void DrawStates() {
        for (int d = 0; d < 3; d++)
        {
            byte indent = d == 1 ? 4 : 0;
            gfxDottedLine(17 + indent, 21 + (16 * d), 36, 37, dendrite_activated[d] ? 1 : 3); // Synapse
        }

        if (axon_activated) {
            gfxCircle(48, 37, 12);
            gfxCircle(48, 37, axon_radius);
        }
    }
};
