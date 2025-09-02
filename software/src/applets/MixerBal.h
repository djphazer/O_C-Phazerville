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

#define MIXER_MAX_VALUE 255

class MixerBal : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Mixer:Bal";
    }
    const uint8_t* applet_icon() { return PhzIcons::mixerBal; }

    void Start() {
        balance = 127;
    }

    void Controller() {
        int signal1 = In(0);
        int signal2 = In(1);

        int mix1 = Proportion(balance, MIXER_MAX_VALUE, signal2)
                 + Proportion(MIXER_MAX_VALUE - balance, MIXER_MAX_VALUE, signal1);

        int mix2 = Proportion(balance, MIXER_MAX_VALUE, signal1)
                 + Proportion(MIXER_MAX_VALUE - balance, MIXER_MAX_VALUE, signal2);

        Out(0, mix1);
        Out(1, mix2);
    }

    void View() {
        DrawBalanceIndicator();
        gfxSkyline();
    }

    void OnButtonPress() {
    }

    void OnEncoderMove(int direction) {
        balance = constrain(balance + direction, 0, 255);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, balance);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        balance = Unpack(data, PackLocation {0,8});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "Sig 1";
    help[HELP_CV2]      = "Sig 2";
    help[HELP_OUT1]     = "Mix 1+2";
    help[HELP_OUT2]     = "Mix 2+1";
    help[HELP_EXTRA1] = "Set: Balance";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor;
    int balance;
    
    void DrawBalanceIndicator() {
        gfxFrame(1, 15, 62, 6);
        int x = Proportion(balance, MIXER_MAX_VALUE, 62);
        gfxLine(x, 15, x, 20);
    }
};
