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

// Logarhythm: Added triplets output (3 triggers per 4 input clocks) to the unused 2nd output

class Shuffle : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Shuffle";
    }
    const uint8_t* applet_icon() { return PhzIcons::shuffle; }

    void Start() {
        delay[0] = 0;
        delay[1] = 0;
        which = 0;
        cursor = 1;
        last_tick = 0;
        
        triplet_which = 0;  // Triplets
        next_trip_trigger = 0;
        triplet_time = 0;
    }

    void Reset() {
        which = 0; // Reset (next trigger will be even clock)
        last_tick = OC::CORE::ticks;
        triplet_which = 0;  // Triplets reset to down beat
    }

    void Controller() {
        const uint32_t tick = OC::CORE::ticks;
        if (Clock(1)) Reset();

        // continuously update CV modulated delay values, for display
        ForEachChannel(ch)
        {
            _delay[ch] = delay[ch];
            Modulate(_delay[ch], ch, 0, 100);
        }

        if (Clock(0))
        {
            // Triplets: Track what triplet timing should be to span 4 normal clocks
            triplet_time = (ClockCycleTicks(0) * 4) / 3;
            if(triplet_which == 0)
            {
              next_trip_trigger = tick;  // Trigger right now (downbeat)
            }
            
            if(++triplet_which > 3)
            {
              triplet_which = 0;        
            }
            
            // Swing
            which = 1 - which;
            if (last_tick) {
                tempo = tick - last_tick;
                uint32_t delay_ticks = Proportion(_delay[which], 100, tempo);
                next_trigger = tick + delay_ticks;
            }
            last_tick = tick;
        }

        // Shuffle output
        if (tick == next_trigger) ClockOut(0);

        // Logarhythm: Triplets output
        if(tick == next_trip_trigger)
        {
          next_trip_trigger = tick + triplet_time;  // Schedule the next triplet output
          triplet_time = 0; // Ensure that triplets will stop being scheduled if clocks aren't received
          ClockOut(1);
        }

    }

    void View() {
        DrawSelector();
        DrawIndicator();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1);
            return;
        }
        delay[cursor] += direction;
        delay[cursor] = constrain(delay[cursor], 0, 99);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, delay[0]);
        Pack(data, PackLocation {7,7}, delay[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        delay[0] = Unpack(data, PackLocation {0,7});
        delay[1] = Unpack(data, PackLocation {7,7});
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Odd Mod";
        help[HELP_CV2]      = "Even";
        help[HELP_OUT1]     = "Shuffle";
        help[HELP_OUT2]     = "Triplets";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                  "---------------------" <-- Extra text size guide
    }
    
private:
    int cursor;
    bool which; // The current clock state, 0=even, 1=odd
    uint32_t last_tick; // For calculating tempo
    uint32_t next_trigger; // The tick of the next scheduled trigger
    uint32_t tempo; // Calculated time between ticks

    // Logarhythm: Triplets (output on out B)
    uint32_t triplet_which; // The current 4 count of clocks used to determine triplet reset
    uint32_t next_trip_trigger; // The tick of the next scheduled triplet trigger
    uint32_t triplet_time;  // Number of ticks between triplet output pulses
    
    // Settings
    int16_t delay[2]; // Percentage delay for even (0) and odd (1) clock
    int16_t _delay[2]; // after CV modulation

    void DrawSelector() {
        ForEachChannel(i)
        {
            gfxPrint(32 + pad(10, _delay[i]), 15 + (i * 10), _delay[i]);
            gfxPrint("%");
            if (cursor == i) gfxCursor(32, 23 + (i * 10), 18);
        }

        // Lines to the first parameter
        int x = Proportion(delay[0], 100, 20) + 8;
        gfxDottedLine(x, 41, x, 19, 3);
        gfxDottedLine(x, 19, 30, 19, 3);

        // Line to the second parameter
        gfxDottedLine(Proportion(delay[1], 100, 20) + 28, 45, 41, 33, 3);
    }

    void DrawIndicator() {
        // Draw some cool-looking barlines
        gfxLine(1, 40, 1, 62);
        gfxLine(57, 40, 57, 62);
        gfxLine(60, 40, 60, 62);
        gfxLine(61, 40, 61, 62);
        gfxCircle(53, 47, 1);
        gfxCircle(53, 55, 1);

        ForEachChannel(n)
        {
            int x = Proportion(_delay[n], 100, 20) + (n * 20) + 4;
            gfxBitmap(x, 48 - (which == n ? 3 : 0), 8, which == n ? NOTE_ICON : X_NOTE_ICON);
        }

        int lx = Proportion(OC::CORE::ticks - last_tick, tempo, 20) + (which * 20) + 4;
        lx = constrain(lx, 1, 54);
        gfxDottedLine(lx, 42, lx, 60, 2);
    }
};
