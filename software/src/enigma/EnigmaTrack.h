// Copyright (c) 2018, Jason Justian
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

#ifndef ENIGMATRACK_H
#define ENIGMATRACK_H

class EnigmaTrack {
public:
    byte data; // bits 0-4=Divide (1-32), bit 5=loop, bit 6,7=Track (1-4)

    void InitAs(byte track_) {
        set_track(track_);
        set_divide(1);
        set_loop(0);
    }

    byte divide() {return (data & 0x1f) + 1;}
    byte loop() {return ((data >> 5) & 0x01);}
    byte track() {return ((data >> 6) & 0x03) + 1;}

    void set_divide(byte divide_) {
        divide_ = constrain(divide_, 1, 32) - 1;
        data = data & (0xff - 0x1f); // Clear first five bits
        data = data | divide_;
    }
    void set_loop(bool loop_) {
        if (loop_) data = data | 0x20;
        else data = data & (0xff - 0x20); // Clear bit 5
    }
    void set_track(byte track_) {
        track_ = constrain(track_, 0, 3);
        data = data & (0xff - (0x03 << 6));
        data = data | (track_ << 6);
    }
};

#endif // ENIGMATRACK_H
