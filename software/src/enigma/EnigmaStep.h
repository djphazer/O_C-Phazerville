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

#ifndef ENIGMASTEP_H
#define ENIGMASTEP_H

class EnigmaStep {
public:
    byte tk; // High 2 bits=Track, low 6 bits=TM
    byte pr; // Probability 0-100
    byte re; // Repeats 1-255
    byte tr; // Transpose -48 ~ 48 (0-96)

    void Init(byte track) {
        set_track(track);
        set_tm(PickTM());
        set_p(0);
        set_repeats(1);
        set_transpose(0);
    }

    // Getters
    byte track() {return (tk >> 6) & 0x03;} // High two bits
    byte tm() {return (tk & 0x3f);} // Low six bits
    byte p() {return pr;}
    byte repeats() {return re;}
    int8_t transpose() {return static_cast<int8_t>(tr - 48);}

    // Setters
    void set_track(byte track_) {
        if (track_ > 3) track_ = 3;
        tk = (track_ << 6) | (tk & 0x3f);
    }
    void set_tm(byte tm_) {
        if (tm_ > 39) tm_ = 39;
        if (tm_ == 0xff) tm_ = 0;
        tk = (tk & 0xc0) | tm_;
    }
    void set_p(byte p_) {
        if (p_ > 100) p_ = 100;
        if (p_ == 0xff) p_ = 0;
        pr = p_;
    }
    void set_repeats(byte repeats_) {
        repeats_ = constrain(repeats_, 1, 99);
        re = repeats_;
    }
    void set_transpose(int8_t transpose_) {
        transpose_ = constrain(transpose_, -48, 48);
        tr = static_cast<byte>(transpose_ + 48);
    }

private:
    // Randomly choose one of the favorite Turing Machines for initialization
    byte PickTM() {
        byte tm = 0; // Default of A-1

        // Number of favorites
        byte favorites = 0;
        for (byte i = 0; i < HS::TURING_MACHINE_COUNT; i++) favorites += HS::user_turing_machines[i].favorite;

        if (favorites > 0) {
            byte pick = random(0, favorites);
            favorites = 0;
            for (byte i = 0; i < HS::TURING_MACHINE_COUNT; i++)
            {
                favorites += HS::user_turing_machines[i].favorite;
                if (favorites == pick) {
                    tm = i;
                    break;
                }
            }
        }
        return tm;
    }
};

#endif // ENIGMASTEP_H
