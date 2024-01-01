// Copyright (c) 2023, Jakob Zerbian
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

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define MS_QUANT_SCALES_COUNT 4

class MultiScale : public HemisphereApplet {
public:

    const char* applet_name() {
        return "MultiScale";
    }

    void Start() {
        // init all scales no all notes
        for (uint8_t i = 0; i < MS_QUANT_SCALES_COUNT; i++) {
            scale_mask[i] = 0x0001;
        }
        QuantizerConfigure(0, 5, scale_mask[0]);
    }

    void Controller() {

        // user selected scale from input 2
        int scale = DetentedIn(1);
        if (scale < 0) scale = 0;        
        if (scale > 0) {
            scale = constrain(ProportionCV(scale, MS_QUANT_SCALES_COUNT + 1), 0, MS_QUANT_SCALES_COUNT - 1);
        }
        if (scale != current_scale) {
            current_scale = scale;
            QuantizerConfigure(0, 5, scale_mask[current_scale]);
            ClockOut(1); // send clock at second output
        }

        // quantize notes from input 1, can be clocked
        if (Clock(0)) {
            continuous = false;
            StartADCLag(0);
        }

        // Unclock
        if (Gate(1)) {
            continuous = true;
        }

        if (continuous || EndOfADCLag(0)) {
            int32_t quantized = Quantize(0, In(0), 0, 0);
            Out(0, quantized);
        }
    }

    void View() {
        DrawKeyboard();
        DrawIndicators();
    }

    void ToggleBit(const uint8_t bit) {
        scale_mask[scale_page] ^= (0x01 << bit); // togle bit at position
        if (scale_page == current_scale) {
            QuantizerConfigure(0, 5, scale_mask[current_scale]);
        }
    }
    void OnButtonPress() {
        if (cursor == 0) { // scale page selection mode
            CursorAction(cursor, 12);
        } else { // scale note edit mode
            const uint8_t bit = cursor - 1;
            ToggleBit(bit);
        }

    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            // move cursor along the "keyboard"
            MoveCursor(cursor, direction, 12);
            return;
        }

        // select page
        scale_page = constrain(scale_page + direction, 0, MS_QUANT_SCALES_COUNT - 1);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 12}, scale_mask[0]);
        Pack(data, PackLocation {12, 12}, scale_mask[1]);
        Pack(data, PackLocation {24, 12}, scale_mask[2]);
        Pack(data, PackLocation {36, 12}, scale_mask[3]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        scale_mask[0] = Unpack(data, PackLocation { 0, 12});
        scale_mask[1] = Unpack(data, PackLocation {12, 12});
        scale_mask[2] = Unpack(data, PackLocation {24, 12});
        scale_mask[3] = Unpack(data, PackLocation {36, 12});
        QuantizerConfigure(0, 5, scale_mask[0]);
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Unclock";
        help[HEMISPHERE_HELP_CVS]      = "1=CV     2=Scale";
        help[HEMISPHERE_HELP_OUTS]     = "A=Pitch  B=Gate";
        help[HEMISPHERE_HELP_ENCODER]  = "Edit Scales";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor = 0;
    bool continuous = true;
    uint16_t scale_mask[MS_QUANT_SCALES_COUNT];

    int8_t current_scale = 0;
    int8_t scale_page = 0;

    void DrawKeyboard() {
        gfxFrame(4, 27, 56, 32);

        // white keys
        for (uint8_t x = 0; x < 7; x++) {
            gfxLine(x * 8 + 4, 27, x*8 + 4, 58);
        }

        // black keys
        for (uint8_t i = 0; i < 6; i++) {
            if (i != 2) {
                uint8_t x = (i * 8) + 10;
                gfxRect(x, 27, 5, 16);
            }
        }

        gfxBitmap(1, 14, 8, PLAY_ICON);
        gfxPrint(12, 15, current_scale + 1);

        gfxBitmap(32, 14, 8, EDIT_ICON);
        gfxPrint(43, 15, scale_page + 1);
        
    }

    void DrawIndicators() {
        if (cursor == 0) {
            // draw cursor at scale page text
            gfxCursor(31, 23, 20);
        }
        uint8_t x[12] = {2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50};
        uint8_t p[12] = {0, 1,  0,  1,  0,  0,  1,  0,  1,  0,  1,  0};
        for (uint8_t i = 0; i < 12; i++) {
            if ((scale_mask[scale_page] >> i) & 0x01) gfxInvert(x[i] + 4, (p[i] ? 37 : 51), 4 - p[i], 4 - p[i]);
            
            if (i == (cursor - 1)) gfxCursor(x[i] + 3, p[i] ? 25 : 60, 6);
        }
    }
};
