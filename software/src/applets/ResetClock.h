// Copyright (c) 2020, Peter Kyme
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

#define RC_MIN_SPACING 6
#define RC_TICKS_PER_MS 17

class ResetClock : public HemisphereApplet {
public:

    const char* applet_name() {
        return "ResetClk";
    }
    const uint8_t* applet_icon() { return PhzIcons::resetClk; }

    void Start() {
        cursor = 0;
        spacing = 6;
        ticks_since_clock = 0;
        pending_clocks = 0;
        length = 8;
        position = 0;
        offset = 0;
    }

    void Reset() {
        pending_clocks += (length - position + offset_mod - 1) % length;
    }

    void Controller() {
        // CV1 modulates offset
        int cv1_mod = offset;
        Modulate(cv1_mod, 0, 0, length-1);

        if (cv1_mod != offset_mod) // changed
            UpdateOffset(offset_mod, cv1_mod);

        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) pending_clocks++;

        if (pending_clocks && (ticks_since_clock > spacing * RC_TICKS_PER_MS)) {
            ClockOut(0, (spacing * RC_TICKS_PER_MS)/2);
            if (pending_clocks==1) {
                ClockOut(1, (spacing * RC_TICKS_PER_MS)/2);
            }
            ticks_since_clock = 0;
            position = (position + 1) % length;
            pending_clocks--;
        } else {
            ticks_since_clock++;
        } 
    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, 3);
        return;
      }

      switch (cursor) {
      case 0: // length
        length = constrain(length + direction, 1, 32);
        position %= length;
        offset %= length;
        break;
      case 1: // offset
        offset = constrain(offset + direction, 0, length - 1);
        break;
      case 2: // spacing
        spacing = constrain(spacing + direction, RC_MIN_SPACING, 100);
        break;
      case 3: // position
        position = ( position + direction + length ) % length;
        break;
      }
    }
    void UpdateOffset(int &o, int new_offset) {
      int prev_offset = o;
      o = constrain(new_offset, 0, length-1);
      pending_clocks += o - prev_offset;
      if (pending_clocks < 0) pending_clocks += length;
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,5}, length-1);
        Pack(data, PackLocation {5,5}, offset);
        Pack(data, PackLocation {10,7}, spacing);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        length = Unpack(data, PackLocation {0,5}) + 1;
        offset = Unpack(data, PackLocation {5,5});
        spacing = Unpack(data, PackLocation {10,7});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Reset";
    help[HELP_CV1]      = "Offset";
    help[HELP_CV2]      = "";
    help[HELP_OUT1]     = "Advance";
    help[HELP_OUT2]     = "Trigger";
    help[HELP_EXTRA1] = " 2nd trigger fires";
    help[HELP_EXTRA2] = "when seq step settles";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor;
    int ticks_since_clock;
    int pending_clocks;
    
    // Settings
    int length;
    int position;
    int offset, offset_mod;
    int spacing;

    void DrawInterface() {
        // Length
        gfxIcon(1, 14, LOOP_ICON);
        gfxPrint(12 + pad(10, length), 15, length);

        // Offset
        gfxIcon(32, 15, ROTATE_R_ICON);
        gfxPrint(40 + pad(10, offset_mod), 15, offset_mod);
    
        // Spacing
        gfxIcon(1, 25, CLOCK_ICON);
        gfxPrint(12 + pad(100, spacing), 25, spacing);
        gfxPrint("ms");

        DrawIndicator();
    
        switch (cursor) {
        case 0: gfxCursor(13, 23, 12); break; // length
        case 1: gfxCursor(41, 23, 12); break; // offset
        case 2: gfxCursor(13, 33, 18); break; // spacing
        case 3: gfxCursor(0, 62, 64, 17); break; // position
        }
    }

    void DrawIndicator() {
        gfxLine(0, 45, 63, 45);
        if (cursor != 3) gfxLine(0, 62, 63, 62);
        for(int i = 0; i < length; i++)
        {
            int x0 = (i*64)/length;
            int width = ((i+1)*64)/length - x0;
            if (position == i)
            {
                gfxRect(x0, 46, width, 16);
            }
            else
            {
                gfxLine(x0, 46, x0, 61);
            }  
        }
        gfxLine(63, 46, 63, 61);
    }

};
