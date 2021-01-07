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

#ifndef _HEM_ASR_H_
#define _HEM_ASR_H_

#include <Arduino.h>
#include "OC_core.h"
#include "OC_scales.h"
#include "HemisphereApplet.h"
#include "HSRingBufferManager.h" // Singleton Ring Buffer manager

class ASR : public HemisphereApplet {
public:

    const char* applet_name() {
        return "\"A\"SR";
    }

    void Start() {
        scale = OC::Scales::SCALE_SEMI;
        buffer_m.SetIndex(1);
        ForEachChannel(ch) {
            QuantizerConfigure(ch, scale);
        }
    }

    void Controller() {
        buffer_m.Register(hemisphere);
        bool secondary = buffer_m.IsLinked() && hemisphere == RIGHT_HEMISPHERE;

        if (Clock(0) && !secondary) {
            StartADCLag();
        }

        if (EndOfADCLag()) {
            // advance the buffer first, then write the new value
            buffer_m.Advance();

            if (!Gate(1)) {
                int cv = In(0);
                buffer_m.WriteValue(cv);
            }
        }

        index_mod = Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, 32);
        ForEachChannel(ch)
        {
            int cv = buffer_m.ReadValue(ch + secondary*2, index_mod);
            int quantized = Quantize(ch, cv, 0, 0);
            Out(ch, quantized);
        }
    }

    void View() {
        DrawInterface();
        DrawData();
    }

    void OnButtonPress() {
        CursorAction(cursor, 1);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }

        if (cursor == 0) { // Index
            uint8_t ix = buffer_m.GetIndex();
            buffer_m.SetIndex(ix + direction);
        }
        if (cursor == 1) { // Scale selection
            scale += direction;
            if (scale >= OC::Scales::NUM_SCALES) scale = 0;
            if (scale < 0) scale = OC::Scales::NUM_SCALES - 1;
            ForEachChannel(ch)
                QuantizerConfigure(ch, scale);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        uint8_t ix = buffer_m.GetIndex();
        Pack(data, PackLocation {0,8}, ix);
        Pack(data, PackLocation {8,8}, scale);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        buffer_m.SetIndex(Unpack(data, PackLocation {0,8}));
        scale = Unpack(data, PackLocation {8,8});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Freeze";
        help[HEMISPHERE_HELP_CVS]      = "1=CV 2=Mod index";
        help[HEMISPHERE_HELP_OUTS]     = "Outputs";
        help[HEMISPHERE_HELP_ENCODER]  = "Index/Scale";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor;
    int scale;
    int index_mod; // Effect of modulation
    
    void DrawInterface() {
        // Show Link icon if linked with another ASR
        if (buffer_m.IsLinked() && hemisphere == RIGHT_HEMISPHERE) gfxIcon(56, 1, LINK_ICON);

        // Index (shared between all instances of ASR)
        uint8_t ix = buffer_m.GetIndex() + index_mod;
        gfxPrint(1, 15, "Index: ");
        gfxPrint(pad(100, ix), ix);
        if (index_mod != 0) gfxIcon(54, 26, CV_ICON);

        // Scale
        gfxBitmap(1, 24, 8, SCALE_ICON);
        gfxPrint(12, 25, OC::scale_names_short[scale]);

        // Cursor
        if (cursor == 0) gfxCursor(43, 23, 18); // Index Cursor
        if (cursor == 1) gfxCursor(13, 33, 30); // Scale Cursor
    }

    void DrawData() {
        for (uint8_t x = 0; x < 64; ++x)
        {
            int y = buffer_m.GetYAt(x, hemisphere) + 40;
            gfxPixel(x, y);
        }
    }

};
#endif
