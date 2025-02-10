// Copyright (c) 2018, Jason Justian
// Copyright (c) 2024, Nicholas J. Michalek
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

#pragma once

class ClockSetup : public HemisphereApplet {
public:
    static constexpr int SLIDEOUT_TIME = HS::MENU_ANIMATION_TIME;

    enum ClockSetupCursor {
        PLAY_STOP,
        TEMPO,
        SHUFFLE,
        EXT_PPQN,
        MULT1,
        MULT2,
        MULT3,
        MULT4,
        MULT5,
        MULT6,
        MULT7,
        MULT8,
        TRIG1,
        TRIG2,
        TRIG3,
        TRIG4,
        TRIG5,
        TRIG6,
        TRIG7,
        TRIG8,
        OUTSKIP1,
        OUTSKIP2,
        OUTSKIP3,
        OUTSKIP4,
        OUTSKIP5,
        OUTSKIP6,
        OUTSKIP7,
        OUTSKIP8,
        LAST_SETTING = OUTSKIP8
    };

    const char* applet_name() {
        return "ClockSet";
    }
    const uint8_t* applet_icon() { return CLOCK_ICON; }

    void Start() { }

    // The ClockSetup controller handles MIDI Clock and Transport Start/Stop
    void Controller() {
        bool clock_sync = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_1>();

        // MIDI Clock is filtered to 2 PPQN
        if (frame.MIDIState.clock_q) {
            frame.MIDIState.clock_q = 0;
            clock_sync = 1;
        }
        if (frame.MIDIState.start_q) {
            frame.MIDIState.start_q = 0;
            HS::clock_m.DisableMIDIOut();
            HS::clock_m.Start();
        }
        if (frame.MIDIState.stop_q) {
            frame.MIDIState.stop_q = 0;
            HS::clock_m.Stop();
            HS::clock_m.EnableMIDIOut();
        }

        // Paused means wait for clock-sync to start
        if (HS::clock_m.IsPaused() && clock_sync)
            HS::clock_m.Start();
        // TODO: automatically stop...

        // Advance internal clock, sync to external clock / reset
        if (HS::clock_m.IsRunning())
            HS::clock_m.SyncTrig( clock_sync );

        // ------------ //
        if (HS::clock_m.IsRunning() && HS::clock_m.MIDITock()) {
          usbMIDI.sendRealTime(usbMIDI.Clock);
          usbHostMIDI.sendRealTime(usbMIDI.Clock);
#ifdef ARDUINO_TEENSY41
          MIDI1.sendRealTime(midi::MidiType(usbMIDI.Clock));
#endif
        }

        // 8 internal clock flashers
        for (int i = 0; i < 8; ++i) {
            if (HS::clock_m.Tock(i))
                flash_ticker[i] = HEMISPHERE_PULSE_ANIMATION_TIME;
            else if (flash_ticker[i])
                --flash_ticker[i];
        }

        if (button_ticker) --button_ticker;
        if (slide_anim) --slide_anim;
    }

    void DrawIndicator(const bool centered = false) {
      const int x = centered? 27 : 4;
      // Clock indicator icons overlay
      if (HS::clock_m.IsRunning() || HS::clock_m.IsPaused()) {
        graphics.clearRect(59, x, 10, 10);

        if (HS::clock_m.IsPaused()) {
          gfxFrame(59, x, 10, 10);
          gfxIcon(60, x+1, PAUSE_ICON);
        } else {
          gfxIcon(60, x+1, HS::clock_m.Cycle() ? METRO_L_ICON : METRO_R_ICON);
        }
      }
    }
    void View() {
      if (OC::CORE::ticks - view_tick > 1000) {
        slide_anim = SLIDEOUT_TIME;
      }
      view_tick = OC::CORE::ticks;
      if (cursor >= OUTSKIP1) DrawIndicator();
      DrawInterface();
    }

    void OnButtonPress() {
        if (!EditMode()) { // special cases for toggle buttons
            if (cursor == PLAY_STOP) PlayStop();
            else CursorToggle();
        }
        else CursorToggle();

        if (cursor == TEMPO) {
            // Tap Tempo detection
            if (last_tap_tick) {
                tap_time[taps] = OC::CORE::ticks - last_tap_tick;

                if (tap_time[taps] > CLOCK_TICKS_MAX) {
                    taps = 0;
                    last_tap_tick = 0;
                }
                else if (++taps == NR_OF_TAPS)
                    HS::clock_m.SetTempoFromTaps(tap_time, taps);

                taps %= NR_OF_TAPS;
            }
            last_tap_tick = OC::CORE::ticks;
        }
    }

    void OnEncoderMove(int direction) {
        taps = 0;
        last_tap_tick = 0;
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((ClockSetupCursor)cursor) {
        case PLAY_STOP:
            PlayStop();
            break;

        case TRIG1:
        case TRIG2:
        case TRIG3:
        case TRIG4:
        case TRIG5:
        case TRIG6:
        case TRIG7:
        case TRIG8:
            HS::trigger_mapping[cursor-TRIG1] = constrain( HS::trigger_mapping[cursor-TRIG1] + direction, 0, TRIGMAP_MAX);
            break;

        /* the boops shall return in a hidden form
        case BOOP1:
        case BOOP2:
        case BOOP3:
        case BOOP4:
            HS::clock_m.Boop(cursor-BOOP1);
            button_ticker = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            break;
        */

        case OUTSKIP1:
        case OUTSKIP2:
        case OUTSKIP3:
        case OUTSKIP4:
        case OUTSKIP5:
        case OUTSKIP6:
        case OUTSKIP7:
        case OUTSKIP8:
            HS::frame.NudgeSkip(cursor-OUTSKIP1, direction);
            break;

        case EXT_PPQN:
            HS::clock_m.SetClockPPQN(HS::clock_m.GetClockPPQN() + direction);
            break;
        case TEMPO:
            HS::clock_m.SetTempoBPM(HS::clock_m.GetTempo() + direction);
            break;
        case SHUFFLE:
            HS::clock_m.SetShuffle(HS::clock_m.GetShuffle() + direction);
            break;

        case MULT1:
        case MULT2:
        case MULT3:
        case MULT4:
        case MULT5:
        case MULT6:
        case MULT7:
        case MULT8:
            HS::clock_m.SetMultiply(HS::clock_m.GetMultiply(cursor - MULT1) + direction, cursor - MULT1);
            break;

        default: break;
        }
    }
    void OnLeftEncoderMove(const int direction) {
      if (EditMode() && cursor >= MULT1 && cursor <= MULT8) {
        int mult = clock_m.GetMultiply(cursor - MULT1);

        if (0 == mult) mult += direction;
        else if (mult > 0) mult = (direction > 0) ? mult * 2 : mult / 2;
        else mult = ((direction > 0) ? (mult - 1) / 2 : (mult - 1) * 2) + 1;

        clock_m.SetMultiply(mult, cursor - MULT1);
      }
      else OnEncoderMove(direction);
    }

    // Same data blobs as T3 version, but different layout
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 9 }, HS::clock_m.GetTempo());
        Pack(data, PackLocation { 9, 7 }, HS::clock_m.GetShuffle());
        for (size_t i = 0; i < 8; ++i) {
            Pack(data, PackLocation { 16+i*6, 6 }, HS::clock_m.GetMultiply(i)+32);
        }

        return data;
    }
    void OnDataReceive(uint64_t data) {
        if (!HS::clock_m.IsRunning())
            HS::clock_m.SetTempoBPM(Unpack(data, PackLocation { 0, 9 }));
        HS::clock_m.SetShuffle(Unpack(data, PackLocation { 9, 7 }));
        for (size_t i = 0; i < 8; ++i) {
            HS::clock_m.SetMultiply(Unpack(data, PackLocation { 16+i*6, 6 })-32, i);
        }
    }

    uint64_t GetGlobals() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 1 }, HS::auto_save_enabled);
        Pack(data, PackLocation { 1, 1 }, HS::cursor_wrap);
        Pack(data, PackLocation { 2, 2 }, HS::screensaver_mode);
        Pack(data, PackLocation { 4, 7 }, HS::trig_length);
        Pack(data, PackLocation { 11, 5 }, HS::clock_m.GetClockPPQN());
        // 48 bits free
        return data;
    }
    void SetGlobals(const uint64_t &data) {
        HS::auto_save_enabled = Unpack(data, PackLocation { 0, 1 });
        HS::cursor_wrap = Unpack(data, PackLocation { 1, 1 });
        HS::screensaver_mode = Unpack(data, PackLocation { 2, 2 });
        HS::trig_length = constrain( Unpack(data, PackLocation { 4, 7 }), 1, 127);
        HS::clock_m.SetClockPPQN(Unpack(data, PackLocation { 11, 5 }));
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "";
        help[HEMISPHERE_HELP_CVS]      = "";
        help[HEMISPHERE_HELP_OUTS]     = "";
        help[HEMISPHERE_HELP_ENCODER]  = "";
        //                               "------------------" <-- Size Guide
    }

private:
    int cursor; // ClockSetupCursor
    int flash_ticker[8];
    int button_ticker;
    int slide_anim = 0;
    uint32_t view_tick = 0;

    static const int NR_OF_TAPS = 3;

    int taps = 0; // tap tempo
    uint32_t tap_time[NR_OF_TAPS]; // buffer of past tap tempo measurements
    uint32_t last_tap_tick = 0;

    void PlayStop() {
        if (HS::clock_m.IsRunning()) {
            HS::clock_m.Stop();
        } else {
            bool p = HS::clock_m.IsPaused();
            HS::clock_m.Start( !p ); // stop->pause->start
        }
    }

    // This applet is an overlay, drawn on top of the applet view.
    // Space must be cleared first, depending on the cursor position.
    void DrawInterface() {
      if (slide_anim) {
        if (cursor < OUTSKIP1) {
          const int height = 23 - (slide_anim * 23 / SLIDEOUT_TIME);
          graphics.clearRect(0, 0, 128, height+1);
          gfxDottedLine(0, height, 127, height);
          gfxLine(0, height+1, 127, height+1);
        } else {
          const int height = 23 - (slide_anim * 23 / SLIDEOUT_TIME);
          const int y = 63 - height;
          graphics.clearRect(0, y, 128, height+1);
          gfxLine(0, y, 127, y);
          gfxDottedLine(0, y+1, 127, y+1);
        }

        return;
      }
      /*
        graphics.setPrintPos(1, 2);
        graphics.print("Clocks/Triggers");
        gfxLine(0, 10, 127, 10);
      */
      if (cursor < OUTSKIP1) {
        // upper section
        graphics.clearRect(0, 0, 128, 24);

        gfxDottedLine(0, 21, 127, 21);
        gfxLine(0, 22, 127, 22);
      } else {
        // lower section
        graphics.clearRect(0, 41, 128, 23);

        gfxLine(0, 42, 127, 42);
        gfxDottedLine(0, 43, 127, 43);
      }

      if (cursor <= EXT_PPQN) {
        int y = 1;
        // Clock State
        if (clock_m.IsRunning()) {
            gfxIcon(1, y, clock_m.cycle ? METRO_R_ICON : METRO_L_ICON );
            gfxIcon(12, y, PLAY_ICON);
        } else {
            gfxIcon(1, y, CLOCK_ICON);
            gfxIcon(12, y, clock_m.IsPaused()? PAUSE_ICON : STOP_ICON);
        }

        // Tempo
        gfxPrint(22 + pad(100, clock_m.GetTempo()), y, clock_m.GetTempo());
        if (cursor != SHUFFLE)
            gfxPrint(" BPM");
        else {
            // Shuffle
            gfxIcon(44, y, METRO_R_ICON);
            gfxPrint(52 + pad(10, clock_m.GetShuffle()), y, clock_m.GetShuffle());
            gfxPrint("%");
        }

        // Input PPQN
        gfxPrint(79, y, "Sync=");
        gfxPrint(clock_m.GetClockPPQN());
      } else if (cursor <= MULT8) {
        int y = 1;
        for (int ch=0; ch<8; ++ch) {
            const int x = (ch % 4) * 32;
            if (ch == 4) y += 10;

            // Multipliers
            int mult = HS::clock_m.GetMultiply(ch);
            if (0 != mult || cursor == MULT1 + ch) { // hide if 0
                gfxPrint(1 + x, y, (mult >= 0) ? "x" : "/");
                gfxPrint( (mult >= 0) ? mult : 1 - mult );
            }
        }
      } else if (cursor <= TRIG8) {
        int y = 1;
        for (int ch=0; ch<8; ++ch) {
            const int x = (ch % 4) * 32;
            if (ch == 4) y += 10;

            // Physical trigger input mappings
            gfxPrint(1 + x, y, OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch] ] );

            // Trigger indicators
            gfxIcon(23 + x, y, DOWN_BTN_ICON);
            if (flash_ticker[ch]) gfxInvert(22 + x, y, 9, 8);
        }
      } else if (cursor <= OUTSKIP8) {
        int y = 45;
        for (int ch=0; ch<8; ++ch) {
            const int x = (ch % 4) * 32;
            if (ch == 4) y += 10;

            gfxPrint(1 + x, y, HS::frame.clockskip[ch] );
            gfxPrint(23 + x, y, "%");
        }
      }

        switch ((ClockSetupCursor)cursor) {
        case PLAY_STOP:
            gfxFrame(11, 0, 10, 10);
            break;
        case TEMPO:
            gfxCursor(22, 9, 19);
            break;
        case SHUFFLE:
            gfxCursor(52, 9, 13);
            break;
        case EXT_PPQN:
            gfxCursor(109,9, 13);
            break;

        case MULT1:
        case MULT2:
        case MULT3:
        case MULT4:
        case MULT5:
        case MULT6:
        case MULT7:
        case MULT8:
        {
          const int x_ = 8 + 32 * ((cursor-MULT1) % 4);
          const int y_ = 9 + ((cursor-MULT1) / 4 * 10);
          gfxCursor(x_, y_, 12);
          break;
        }

        case TRIG1:
        case TRIG2:
        case TRIG3:
        case TRIG4:
        case TRIG5:
        case TRIG6:
        case TRIG7:
        case TRIG8:
        {
          const int x_ = 1 + 32 * ((cursor-TRIG1) % 4);
          const int y_ = 9 + ((cursor-TRIG1) / 4 * 10);
          gfxCursor(x_, y_, 19);
          break;
        }

        case OUTSKIP1:
        case OUTSKIP2:
        case OUTSKIP3:
        case OUTSKIP4:
        case OUTSKIP5:
        case OUTSKIP6:
        case OUTSKIP7:
        case OUTSKIP8:
        {
          const int x_ = 1 + 32 * ((cursor-OUTSKIP1) % 4);
          const int y_ = 53 + ((cursor-OUTSKIP1) / 4 * 10);
          gfxCursor(x_, y_, 19);
          break;
        }

        /* the boops shall return in a hidden form
        case BOOP1:
        case BOOP2:
        case BOOP3:
        case BOOP4:
            if (0 == button_ticker)
                gfxIcon(12 + 32*(cursor-BOOP1), 49, LEFT_ICON);
            break;
        */

        default: break;
        }
    }
};

ClockSetup ClockSetup_instance;
