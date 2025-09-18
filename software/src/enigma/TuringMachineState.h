// (c) 2018, Jason Justian (chysn), Beize Maze
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

// TuringMachineState is a class for containing the current state of a Turing Machine
// and advancing it to a different state by the following operations:
// 1) Bit shifting
// 2) Probability processing

#ifndef TURINGMACHINESTATE_H
#define TURINGMACHINESTATE_H

class TuringMachineState {
public:
    void Init(byte ix_) {
        ix = constrain(ix_, 0, HS::TURING_MACHINE_COUNT - 1);
        if (HS::user_turing_machines[ix].len == 0 || HS::user_turing_machines[ix].len > 17) {
            HS::user_turing_machines[ix].reg = (random(0, 0xff) << 8) + random(0, 0xff);
            HS::user_turing_machines[ix].len = 16;
            HS::user_turing_machines[ix].favorite = 0;
        }
        reg = HS::user_turing_machines[ix].reg;
        len = HS::user_turing_machines[ix].len;
        fav = HS::user_turing_machines[ix].favorite;
        write = 0;
    }

    void SetWriteMode(bool write_) {write = write_;}

    byte GetTMIndex() {return ix;}

    /*
     * Length here is from 1 - 16
     */
    void ChangeLength(int direction) {
        len = constrain(len + direction, 1, 16);
        if (write && !fav) HS::user_turing_machines[ix].len = len;
    }

    void Advance(byte p) {
        // Grab the bit that's about to be shifted away
        uint16_t last = (reg >> (len - 1)) & 0x01;

        // Does it change?
        if (!fav && random(0, 99) < p) last = 1 - last;

        // Shift left, then potentially add the bit from the other side
        reg = (reg << 1) + last;

        if (write && !fav) HS::user_turing_machines[ix].reg = reg;
    }

    void SetFavorite(bool favorite) {
        fav = favorite;
        if (write) HS::user_turing_machines[ix].favorite = favorite;
    }

    void Rotate(int direction) {
        if (write) { // Rotate requires write access, but doesn't care about favorite status
            if (direction == 1) { // rotate right
                uint16_t bit0 = reg & 0x0001;
                reg = (reg >> 1) | (bit0 << 15);
            }
            if (direction == -1) { // rotate left
                uint16_t bit15 = (reg & 0x8000);
                reg = (reg << 1) | (bit15 >> 15);
            }
            HS::user_turing_machines[ix].reg = reg;
        }
    }

    /*
     * This might be a CV option in a single-TM applet
     */
    void Reset() {
        reg = HS::user_turing_machines[ix].reg;
    }

    uint16_t GetRegister() {return reg;}

    byte GetLength() {return len;}

    bool IsFavorite() {return fav;}

    void DrawAt(byte x, byte y) {
        // Adjust height if the display needs to go lower
        byte height = 19;
        if (y > 40) height -= (y - 40);

        graphics.drawLine(x, y, x + 63, y);
        graphics.drawLine(x, y + height + 3, x + 63, y + height + 3);
        for (byte b = 0; b < 16; b++)
        {
            int v = (reg >> b) & 0x01;
            if (v) graphics.drawRect(60 + x - (4 * b), y + 2, 3, height);
        }
    }

    void DrawSmallAt(byte x, byte y) {
        for (byte b = 0; b < 16; b++)
            if ((reg >> b) & 0x01) graphics.setPixel(16 - b + x, y);
    }

private:
    byte ix; // Turing machine index
    uint16_t reg; // Shift register
    byte len; // Length in steps
    bool fav;
    bool write; // Write mode; the source TuringMachine may be changed
};

#endif // TURINGMACHINESTATE_H
