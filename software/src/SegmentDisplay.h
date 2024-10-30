/*
 * 7-segment numeric display for Hemisphere Suite. Why? Because it looks sweet, that's why.
 *
 * By Jason Justian (c)2018
 *
 * MIT License, see HemisphereSuite License info on GitHub
 */

#ifndef SEGMENTDISPLAY_H
#define SEGMENTDISPLAY_H

enum SegmentSize {
    BIG_SEGMENTS,
    TINY_SEGMENTS,
};

/*
 * PixO is an offset pixel location
 */
struct PixO {
    uint8_t ox;
    uint8_t oy;

    void DrawAt(uint8_t x, uint8_t y) {
        graphics.setPixel(x + ox, y + oy);
    }
};

struct Segment {
    PixO pixels[6];
    int8_t size;

    void DrawAt(uint8_t x, uint8_t y) {
        for (uint8_t i = 0; i < size; i++) pixels[i].DrawAt(x, y);
    }
};

class SegmentDisplay {
public:
    void Init(uint8_t segment_size) {
        size = segment_size;

        if (size == SegmentSize::BIG_SEGMENTS) {
            segment[0] = {PixO{2,0}, PixO{3,0}, PixO{4,0}, PixO{5,0}, PixO{3,1}, PixO{4,1}, 6};
            segment[1] = {PixO{6,2}, PixO{6,3}, PixO{7,1}, PixO{7,2}, PixO{7,3}, PixO{7,4}, 6};
            segment[2] = {PixO{6,8}, PixO{6,9}, PixO{7,7}, PixO{7,8}, PixO{7,9}, PixO{7,10}, 6};
            segment[3] = {PixO{3,10}, PixO{4,10}, PixO{2,11}, PixO{3,11}, PixO{4,11}, PixO{5,11}, 6};
            segment[4] = {PixO{0,7}, PixO{0,8}, PixO{0,9}, PixO{0,10}, PixO{1,8}, PixO{1,9}, 6};
            segment[5] = {PixO{0,1}, PixO{0,2}, PixO{0,3}, PixO{0,4}, PixO{1,2}, PixO{1,3}, 6};
            segment[6] = {PixO{3,5}, PixO{4,5}, PixO{2,6}, PixO{3,6}, PixO{4,6}, PixO{5,6}, 6};
        } else {
            segment[0] = {PixO{1,0}, PixO{2,0}, PixO{3,0}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[1] = {PixO{3,0}, PixO{3,1}, PixO{3,2}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[2] = {PixO{3,2}, PixO{3,3}, PixO{3,4}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[3] = {PixO{1,4}, PixO{2,4}, PixO{3,4}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[4] = {PixO{1,2}, PixO{1,3}, PixO{1,4}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[5] = {PixO{1,0}, PixO{1,1}, PixO{1,2}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
            segment[6] = {PixO{1,2}, PixO{2,2}, PixO{3,2}, PixO{0,0}, PixO{0,0}, PixO{0,0}, 3};
        }

        uint8_t digits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67};
        memcpy(&digit, &digits, sizeof(digits));
    }

    void SetPosition(uint8_t x, uint8_t y) {
        x_pos = x;
        y_pos = y;
    }

    void PrintWhole(uint8_t x, uint8_t y, int number, int range = 10000) {
        SetPosition(x, y);

        uint8_t to_print[5];
        uint8_t q = 0;

        // Make a list of digits to print
        int pwrs[] = {10000, 1000, 100, 10, 1};
        int tmp = number;
        for (int r = 0; r < 5; r++)
        {
            if (pwrs[r] <= range) {
                if (number >= pwrs[r]) {
                    to_print[q] = tmp / pwrs[r];
                    tmp -= (to_print[q++] * pwrs[r]);
                } else {
                    // Padding for left of decimal
                    if (pwrs[r] == 1) to_print[q++] = 0;
                    else x_pos += DigitWidth();
                }
            }
        }

        // Are there any digits to print? Then print them!
        if (q) {
            for (int d = 0; d < q; d++)
            {
                PrintDigit(to_print[d]);
            }
        }
    }

    void PrintDecimal(int number, int places, int range) {
        uint8_t to_print[5];
        uint8_t q = 0;

        int pwrs[] = {10000, 1000, 100, 10, 1};
        int tmp = number;
        for (int r = 0; r < 5; r++)
        {
            if (pwrs[r] < range && places > 0) {
                to_print[q] = tmp / pwrs[r];
                tmp -= (to_print[q++] * pwrs[r]);
                places--;
            }
        }

        DrawDecimalPoint();
        // Are there any digits to print? Then print them!
        if (q) {
            for (int d = 0; d < q; d++)
            {
                PrintDigit(to_print[d]);
            }
        }
    }

    void PrintDigit(uint8_t d) {
        for (uint8_t b = 0; b < 7; b++)
        {
            if ((digit[d] >> b) & 0x01) segment[b].DrawAt(x_pos, y_pos);
        }
        x_pos += DigitWidth();
    }

    void PrintDigit(uint8_t x, uint8_t y, uint8_t d) {
        SetPosition(x, y);
        PrintDigit(d);
    }

private:
    Segment segment[7];
    uint8_t size;
    uint8_t digit[10];
    uint8_t x_pos;
    uint8_t y_pos;
    int decimal;



    uint8_t DigitWidth() {
        return (size == SegmentSize::BIG_SEGMENTS ? 10 : 4);
    }

    uint8_t DecimalWidth() {
        return (size == SegmentSize::BIG_SEGMENTS ? 6 : 4);
    }

    void DrawDecimalPoint() {
        if (size == SegmentSize::BIG_SEGMENTS) {
            for (int x = 0; x < 2; x++)
            {
                for (int y = 0; y < 3; y++)
                {
                    graphics.setPixel(x_pos + x + 1, y_pos + y + 9);
                }
            }
        } else {
            graphics.setPixel(x_pos + 2, y_pos + 4);
        }
        x_pos += DecimalWidth();
    }
};



#endif /* SEGMENTDISPLAY_H */
