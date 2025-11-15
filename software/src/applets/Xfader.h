// Copyright (c) 2018, Jason Justian
// Hacky gate features copyright (C) 2025, Beau Sterling
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

class Xfader : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Xfader";  // formerly Mixer:Bal
    }
    const uint8_t* applet_icon() { return PhzIcons::mixerBal; }

    void Start() {
        balance = (128 << 8);  // midpoint
        rate = 128;  // 50 % per second
        center = 128;
        center_reset_enable = false;
        center_reset = false;
    }

    void Controller() {
        int signal1 = In(0);
        int signal2 = In(1);

        uint32_t t = millis();
        if (t - last_balance_update > 0) {
            last_balance_update = t;
            if (Gate(0) && !Gate(1)) {
                balance = constrain(balance - (rate / 4), 0, 65535);  // (65536 / 16384) = 4 counts per ms
                if (center_reset_enable) center_reset = true;
            }
            if (Gate(1) && !Gate(0)) {
                balance = constrain(balance + (rate / 4), 0, 65535);
                if (center_reset_enable) center_reset = true;
            }
            if (center_reset && !(Gate(0) || Gate(1))) {
                center_reset = false;
                balance = center << 8;
            }
        }

        uint8_t _balance = balance >> 8;

        int mix1 = Proportion(_balance, MIXER_MAX_VALUE, signal2)
                 + Proportion(MIXER_MAX_VALUE - _balance, MIXER_MAX_VALUE, signal1);

        int mix2 = Proportion(_balance, MIXER_MAX_VALUE, signal1)
                 + Proportion(MIXER_MAX_VALUE - _balance, MIXER_MAX_VALUE, signal2);

        Out(0, mix1);
        Out(1, mix2);
    }

    void View() {
        DrawBalanceIndicator();
        gfxSkyline();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            balance = constrain(balance + direction * (1 << 8), 0, 65535);
            center = balance >> 8;
            return;
        }
        int rate_inc =  !(rate >> 15) ?
                            !(rate >> 11) ?
                                !(rate >> 7) ?
                                    1 :
                                16 :
                            512 :
                        2048;
        rate = constrain(rate + direction * rate_inc, 0, 65535);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, balance >> 8);
        Pack(data, PackLocation {8,16}, rate);
        Pack(data, PackLocation {24,8}, center);
        Pack(data, PackLocation {32,1}, center_reset_enable);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        balance = Unpack(data, PackLocation {0,8}) << 8;
        rate = Unpack(data, PackLocation {8,16});
        center = Unpack(data, PackLocation {24,8});
        center_reset_enable = Unpack(data, PackLocation {32,1});
    }

    void AuxButton() {
        center_reset_enable = !center_reset_enable;
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Fade L";
    help[HELP_DIGITAL2] = "Fade R";
    help[HELP_CV1]      = "Sig 1";
    help[HELP_CV2]      = "Sig 2";
    help[HELP_OUT1]     = "Mix 1+2";
    help[HELP_OUT2]     = "Mix 2+1";
    help[HELP_EXTRA1]   = "Set: Balance";
    help[HELP_EXTRA2]   = "Press: Fade Rate";
    //                    "---------------------" <-- Extra text size guide
  }

private:
    int cursor;
    uint16_t balance;
    uint16_t rate;
    uint32_t last_balance_update = millis();
    uint8_t center;
    bool center_reset_enable;
    bool center_reset;

    void DrawBalanceIndicator() {
        if (!EditMode()) {
            gfxFrame(1, 15, 62, 6);
            if (center_reset_enable) gfxInvert(Proportion(center, MIXER_MAX_VALUE, 62)+1, 13, 1, 10);
            int x = Proportion(balance >> 8, MIXER_MAX_VALUE, 62);
            gfxRect((x<32)?x+1:32, 16, (x<32)?32-x:x-31, 4);
        } else {
            gfxPos(1, 15);
            if (rate < 4) {
                gfxPrint(0);
            } else {
                uint percent_int = (rate * 100) / 256;
                gfxPrint(percent_int);
                if (rate < 256) {
                    uint percent_frac = ((rate * 10000) / 256) % 100;
                    gfxPrint("."); gfxPrint(percent_frac);
                }
            }
            gfxPrint("%/sec");
        }
    }
};
