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

struct RingBufferManager {
    int buffer[256];
    uint8_t position = 0;
    uint8_t index = 0;
    uint32_t registered[2];

    void Register(bool hemisphere) {
        registered[hemisphere] = OC::CORE::ticks;
    }

    bool IsLinked() {
        const uint32_t t = OC::CORE::ticks;
        return ( (t - registered[LEFT_HEMISPHERE] < 160)
            && (t - registered[RIGHT_HEMISPHERE] < 160));
    }

    inline void SetIndex(uint8_t ix) {index = ix;}

    inline uint8_t GetIndex() {return index;}

    inline void WriteValue(int cv) {
        buffer[position] = cv;
    }

    int ReadValue(uint8_t output, int index_mod = 0) {
        uint8_t ix = position - (index * output) - index_mod;
        int cv = buffer[ix];
        return cv;
    }

    inline void Advance() {
        ++position; // No need to check range; 256 positions and an 8-bit counter
    }

    int GetYAt(const uint8_t &x, bool hemisphere) {
        // If there are two instances, and this is the left hemisphere, start further back
        uint8_t offset = (IsLinked() && hemisphere == LEFT_HEMISPHERE) ? -128 : -64;
        uint8_t ix = (x + offset + position);
        int cv = buffer[ix] + HEMISPHERE_MAX_INPUT_CV; // Force this positive
        int y = (((cv << 7) / (HEMISPHERE_MAX_INPUT_CV * 2)) * 23) >> 7;
        y = constrain(y, 0, 23);
        return 23 - y;
    }
} buffer_m;

