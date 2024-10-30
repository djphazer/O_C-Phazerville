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

/*
 * Turing Machine based on https://thonk.co.uk/documents/random%20sequencer%20documentation%20v2-1%20THONK%20KIT_LargeImages.pdf
 *
 * Thanks to Tom Whitwell for creating the concept, and for clarifying some things
 * Thanks to Jon Wheeler for the CV length and probability updates
 */

class ShiftReg : public HemisphereApplet {
public:

static constexpr int MAX_SCALE = OC::Scales::NUM_SCALES;
static constexpr int MIN_LENGTH = 2;
static constexpr int MAX_LENGTH = 16;

    const char* applet_name() {
        return "ShiftReg";
    }

    void Start() {
        reg = random(0, 65535);
        p = 0;
        length = 16;
        quant_range = 24;  //APD: Quantizer range
        cursor = 0;
        quantizer.Init();
        scale = OC::Scales::SCALE_SEMI;
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff); // Semi-tone
    }

    void Controller() {

        // CV 1 control over length
        int lengthCv = DetentedIn(0);
        if (lengthCv < 0) length = MIN_LENGTH;        
        if (lengthCv > 0) {
            length = constrain(ProportionCV(lengthCv, MAX_LENGTH + 1), MIN_LENGTH, MAX_LENGTH);
        }
      
        // CV 2 bi-polar modulation of probability
        int pCv = Proportion(DetentedIn(1), HEMISPHERE_MAX_CV, 100);
        bool clk = Clock(0);
        
        if (clk) {
            // If the cursor is not on the p value, and Digital 2 is not gated, the sequence remains the same
            int prob = (cursor == 1 || Gate(1)) ? p + pCv : 0;

            AdvanceRegister( constrain(prob, 0, 100) );
        }
 
        // Send 5-bit quantized CV
        // APD: Scale this to the range of notes allowed by quant_range: 32 should be all
        // This defies the faithful Turing Machine sim aspect of this code but gives a useful addition that the Disting adds to the concept
        int32_t note = Proportion(reg & 0x1f, 0x1f, quant_range);
        Out(0, quantizer.Lookup(note + 64));

        switch (out_b) {
        case 0:
          // Send 8-bit proportioned CV
          Out(1, Proportion(reg & 0x00ff, 255, HEMISPHERE_MAX_CV) );
          break;
        case 1:
          if (clk)
            ClockOut(1);
          break;
        case 2:
          // only trigger if 1st bit is high
          if (clk && (reg & 0x01) == 1)
            ClockOut(1);
          break;
        case 3: // duplicate of Out A
          Out(1, quantizer.Lookup(note + 64));
          break;
        case 4: // alternative 6-bit pitch
          note = Proportion( (reg >> 8 & 0x3f), 0x3f, quant_range);
          Out(1, quantizer.Lookup(note + 64));
          break;
        }
    }

    void View() {
      DrawSelector();
      DrawIndicator();
    }

    void OnButtonPress() {
      isEditing = !isEditing;
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 4);
            return;
        }

        switch (cursor) {
        case 0:
            length = constrain(length + direction, MIN_LENGTH, MAX_LENGTH);
            break;
        case 1:
            p = constrain(p + direction, 0, 100);
            break;
        case 2:
            scale += direction;
            if (scale >= MAX_SCALE) scale = 0;
            if (scale < 0) scale = MAX_SCALE - 1;
            quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
            break;
        case 3:
            quant_range = constrain(quant_range + direction, 1, 32);
            break;
        case 4:
            out_b = constrain(out_b + direction, 0, 4);
            break;
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,16}, reg);
        Pack(data, PackLocation {16,7}, p);
        Pack(data, PackLocation {23,4}, length - 1);
        Pack(data, PackLocation {27,5}, quant_range - 1);
        Pack(data, PackLocation {32,4}, out_b);
        Pack(data, PackLocation {36,8}, constrain(scale, 0, 255));

        return data;
    }

    void OnDataReceive(uint64_t data) {
        reg = Unpack(data, PackLocation {0,16});
        p = Unpack(data, PackLocation {16,7});
        length = Unpack(data, PackLocation {23,4}) + 1;
        quant_range = Unpack(data, PackLocation{27,5}) + 1;
        out_b = Unpack(data, PackLocation {32,4});
        scale = Unpack(data, PackLocation {36,8});
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "p Gate";
        help[HELP_CV1]      = "Length";
        help[HELP_CV2]      = "p Mod";
        help[HELP_OUT1]     = "5bits Q";
        help[HELP_OUT2]     = "8bits V";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                  "---------------------" <-- Extra text size guide
    }

private:
    int length; // Sequence length
    int cursor;  // 0 = length, 1 = p, 2 = scale
    braids::Quantizer quantizer;

    // Settings
    uint16_t reg; // 16-bit sequence register
    int p; // Probability of bit 15 changing on each cycle

    // Scale used for quantized output
    int scale;  // Logarhythm: hold larger values
    uint8_t quant_range;  // APD
    uint8_t out_b = 0; // 2nd output mode: 0=mod; 1=trig; 2=trig-on-msb; 3=duplicate of A; 4=alternate pitch

    void DrawSelector() {
        gfxBitmap(1, 14, 8, LOOP_ICON);
        gfxPrint(12 + pad(10, length), 15, length);
        gfxPrint(32, 15, "p=");
        if (cursor == 1 || Gate(1)) { // p unlocked
            int pCv = Proportion(DetentedIn(1), HEMISPHERE_MAX_CV, 100);
            int prob = constrain(p + pCv, 0, 100);
            gfxPrint(pad(100, prob), prob);
        } else { // p is disabled
            gfxBitmap(49, 14, 8, LOCK_ICON);
        }
        gfxBitmap(1, 24, 8, SCALE_ICON);
        gfxPrint(12, 25, OC::scale_names_short[scale]);
        gfxBitmap(41, 24, 8, NOTE4_ICON);
        gfxPrint(49, 25, quant_range); // APD
        gfxPrint(1, 35, OutputLabel(1));
        gfxPrint(":");
        switch (out_b) {
        case 0: // modulation output
            gfxBitmap(28, 35, 8, WAVEFORM_ICON);
            break;
        case 2: // clock out only on msb
            gfxPrint(36, 35, "1");
        case 1: // clock out icon
            gfxBitmap(28, 35, 8, CLOCK_ICON);
            break;
        case 3: // double output A
            gfxBitmap(28, 35, 8, LINK_ICON);
            break;
        case 4: // alternate 6-bit pitch
            gfxBitmap(28, 35, 8, CV_ICON);
            break;
        }

        switch (cursor) {
            case 0: gfxCursor(13, 23, 12); break; // Length Cursor
            case 1: gfxCursor(45, 23, 18); break; // Probability Cursor
            case 2: gfxCursor(12, 33, 25); break; // Scale Cursor
            case 3: gfxCursor(49, 33, 14); break; // Quant Range Cursor // APD
            case 4: gfxCursor(27, 43, (out_b == 2) ? 18 : 10); // out_b mode
        }
    }

    void DrawIndicator() {
        gfxLine(0, 45, 63, 45);
        gfxLine(0, 62, 63, 62);
        for (int b = 0; b < 16; b++)
        {
            int v = (reg >> b) & 0x01;
            if (v) gfxRect(60 - (4 * b), 47, 3, 14);
        }
    }

    void AdvanceRegister(int prob) {
        // Before shifting, determine the fate of the last bit
        int last = (reg >> (length - 1)) & 0x01;
        if (random(0, 99) < prob) last = 1 - last;

        // Shift left, then potentially add the bit from the other side
        reg = (reg << 1) + last;
    }

};
