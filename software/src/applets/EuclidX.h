// Copyright (c) 2022, Bryan Head
// Copyright (c) 2022, Nicholas J. Michalek
// Copyright (c) 2022, Alessio Degani
// Copyright (c) 2018, Jason Justian
//
// Bjorklund pattern filter, Copyright (c) 2016 Tim Churches
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

/* This applet is a replacement for the original Annular Fusion Euclidean Drummer,
 * redesigned by qiemem and modified by djphazer to add CV modulation.
 * The CV input logic as well as the name were copied from a separate rewrite by adegani.
 */

#include "bjorklund.h"

const int NUM_PARAMS = 5;
const int PARAM_SIZE = 6;

class EuclidX : public HemisphereApplet {
public:

    enum EuclidXParam {
        LENGTH1, BEATS1, OFFSET1, PADDING1,
        LENGTH2, BEATS2, OFFSET2, PADDING2,
        CV_DEST1,
        CV_DEST2,
        LAST_SETTING = CV_DEST2
    };

    const char* applet_name() {
        return "EuclidX";
    }

    void Start() {
        ForEachChannel(ch)
        {
            length[ch] = 16;
            beats[ch] = 4 + ch*4;
            offset[ch] = 0;
            padding[ch] = ch*16;
            pattern[ch] = EuclideanPattern(length[ch], beats[ch], offset[ch], padding[ch]);
        }
        step = 0;
    }

    void Controller() {
        if (Clock(1)) step = 0; // Reset

        // continuously recalculate pattern with CV offsets
        ForEachChannel(ch) {
            actual_length[ch] = length[ch];
            actual_beats[ch] = beats[ch];
            actual_offset[ch] = offset[ch];
            actual_padding[ch] = padding[ch];

            // process CV inputs
            ForEachChannel(cv_ch) {
                switch (cv_dest[cv_ch] - ch * LENGTH2) { // this is dumb, but efficient
                case LENGTH1:
                    Modulate(actual_length[ch], ch, 2, 32);

                    if (actual_beats[ch] > actual_length[ch])
                        actual_beats[ch] = actual_length[ch];
                    if (actual_padding[ch] > 32 - actual_length[ch])
                        actual_padding[ch] = 32 - actual_length[ch];
                    if (actual_offset[ch] >= actual_length[ch] + actual_padding[ch])
                        actual_offset[ch] = actual_length[ch] + actual_padding[ch] - 1;

                    break;
                case BEATS1:
                    Modulate(actual_beats[ch], ch, 0, actual_length[ch]);
                    break;
                case OFFSET1:
                    Modulate(actual_offset[ch], ch, 0, actual_length[ch] + padding[ch]);
                    break;
                case PADDING1:
                    Modulate(actual_padding[ch], ch, 0, 32 - actual_length[ch]);
                    if (actual_offset[ch] >= actual_length[ch] + actual_padding[ch])
                        actual_offset[ch] = actual_length[ch] + actual_padding[ch] - 1;
                    break;
                default: break;
                }
            }

            // Store the pattern for display
            pattern[ch] = EuclideanPattern(actual_length[ch], actual_beats[ch], actual_offset[ch], actual_padding[ch]);
        }

        // Process triggers and step forward on clock
        if (Clock(0)) {

            ForEachChannel(ch) {
                // actually output the triggers
                int sb = step % (actual_length[ch] + actual_padding[ch]);
                if ((pattern[ch] >> sb) & 0x01) {
                    ClockOut(ch);
                }
            }

            // Plan for the thing to run forever and ever
            if (++step >= (actual_length[0]+actual_padding[0]) * (actual_length[1]+actual_padding[1])) step = 0;
        }
    }

    void View() {
        DrawSteps();
        DrawEditor();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        int ch = cursor < LENGTH2 ? 0 : 1;
        switch (cursor) {
        case LENGTH1:
        case LENGTH2:
            actual_length[ch] = length[ch] = constrain(length[ch] + direction, 2, 32);
            if (beats[ch] > length[ch])
                beats[ch] = length[ch];
            if (padding[ch] > 32 - length[ch])
                padding[ch] = 32 - length[ch];
            if (offset[ch] >= length[ch] + padding[ch])
                offset[ch] = length[ch] + padding[ch] - 1;
            break;
        case BEATS1:
        case BEATS2:
            actual_beats[ch] = beats[ch] = constrain(beats[ch] + direction, 0, length[ch]);
            break;
        case OFFSET1:
        case OFFSET2:
            actual_offset[ch] = offset[ch] = constrain(offset[ch] + direction, 0, length[ch] + padding[ch] - 1);
            break;
        case PADDING1:
        case PADDING2:
            padding[ch] = constrain(padding[ch] + direction, 0, 32 - length[ch]);
            if (offset[ch] >= length[ch] + padding[ch])
                offset[ch] = length[ch] + padding[ch] - 1;
            break;
        case CV_DEST1:
        case CV_DEST2:
            cv_dest[cursor - CV_DEST1] = (EuclidXParam) constrain(cv_dest[cursor - CV_DEST1] + direction, LENGTH1, PADDING2);
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        size_t idx = 0;
        ForEachChannel(ch) {
            Pack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}, length[ch] - 1);
            Pack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}, beats[ch]);
            Pack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}, offset[ch]);
            Pack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}, padding[ch]);
            Pack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}, cv_dest[ch]);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        size_t idx = 0;
        ForEachChannel(ch) {
            actual_length[ch] = length[ch] = Unpack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE}) + 1;
            actual_beats[ch]  = beats[ch]  = Unpack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE});
            actual_offset[ch] = offset[ch] = Unpack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE});
            actual_padding[ch] = padding[ch] = Unpack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE});
            cv_dest[ch] = (EuclidXParam) Unpack(data, PackLocation {idx++ * PARAM_SIZE, PARAM_SIZE});
        }
        step = 0; // reset
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "Clock A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER]  = "Len/Hits/Rot/CV";
        //                               "------------------" <-- Size Guide
    }

private:
    int step;
    int cursor = LENGTH1; // EuclidXParam 
    uint32_t pattern[2];

    // Settings
    uint8_t length[2];
    uint8_t beats[2];
    uint8_t offset[2];
    uint8_t padding[2];
    uint8_t actual_length[2];
    uint8_t actual_beats[2];
    uint8_t actual_offset[2];
    uint8_t actual_padding[2];

    EuclidXParam cv_dest[2] = {BEATS1, BEATS2}; // input modulation

    void DrawSteps() {
        gfxLine(0, 45, 63, 45);
        gfxLine(0, 62, 63, 62);
        gfxLine(0, 53, 63, 53);
        gfxLine(0, 54, 63, 54);
        ForEachChannel(ch) {
            for (int i = 0; i < 16; i++) {
                if ((pattern[ch] >> ((i + step) % (actual_length[ch]+actual_padding[ch]) )) & 0x1) {
                    gfxRect(4 * i + 1, 48 + 9 * ch, 3, 3);
                    //gfxLine(4 * i + 2, 47 + 9 * ch, 4 * i + 2, 47 + 9 * ch + 4);
                } else {
                    gfxPixel(4 * i + 2, 47 + 9 * ch + 2);
                }

                if ((i + step) % (actual_length[ch]+actual_padding[ch]) == 0) {
                    //gfxLine(4 * i, 46 + 9 * ch, 4 * i, 52 + 9 * ch);
                    gfxLine(4 * i, 46 + 9 * ch, 4 * i, 46 + 9 * ch + 1);
                    gfxLine(4 * i, 52 + 9 * ch - 1, 4 * i, 52 + 9 * ch);
                }
            }
        }
    }

    void DrawEditor() {
        const int spacing = 16;
        const int pad_left = 5;

        if (cursor < CV_DEST1) {
            gfxBitmap(pad_left + 0 * spacing, 15, 8, LENGTH_ICON);
        }
        gfxBitmap(pad_left + 1 * spacing, 15, 8, PULSES_ICON);
        gfxBitmap(pad_left + 2 * spacing, 15, 8, ROTATE_ICON);
        gfxIcon(pad_left + 3 * spacing, 15, OFFSET_ICON);

        int y = 15;
        ForEachChannel (ch) {
            y += 10;
            gfxPrint(0 * spacing + pad(10, actual_length[ch]), y, actual_length[ch]);
            gfxPrint(1 * spacing + pad(10, actual_beats[ch]), y, actual_beats[ch]);
            gfxPrint(2 * spacing + pad(10, actual_offset[ch]), y, actual_offset[ch]);
            gfxPrint(3 * spacing + pad(10, actual_padding[ch]), y, actual_padding[ch]);

            // CV assignment indicators
            ForEachChannel(ch_dest) {
                int ff = cv_dest[ch_dest] - LENGTH2*ch;
                if (ff >= 0 && ff < LENGTH2)
                    gfxBitmap(13 + ff * spacing, y, 3, ch_dest?SUB_TWO:SUP_ONE);
            }
        }

        int ch = cursor < LENGTH2 ? 0 : 1;
        int f = cursor - ch * LENGTH2;
        y = 33;
        switch (cursor) {
        case LENGTH2:
        case BEATS2:
        case OFFSET2:
        case PADDING2:
            y += 10;
        case LENGTH1:
        case BEATS1:
        case OFFSET1:
        case PADDING1:
            gfxCursor(f * spacing, y, 13);
            break;

        case CV_DEST1:
        case CV_DEST2:
            gfxBitmap(0, 13 + (cursor - CV_DEST1)*5, 8, CV_ICON);
            gfxBitmap(8, 15, 3, (cursor - CV_DEST1)? SUB_TWO : SUP_ONE);
            gfxCursor(0, 19 + (cursor - CV_DEST1)*5, 11, 7);
            break;
        }
    }
};
