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

// See https://www.pjrc.com/teensy/td_midi.html

// The functions available for each output
class hMIDIOut : public HemisphereApplet {
public:
  enum MIDIOutCursor {
    CHANNEL, TRANSPOSE, CV2_FUNC, LEGATO,
    LOG_VIEW,

    MAX_CURSOR = LOG_VIEW
  };
  enum MIDIOutMode {
    HEM_MIDI_CC_IN,
    HEM_MIDI_AT_IN,
    HEM_MIDI_PB_IN,
    HEM_MIDI_VEL_IN,
  };

    const char* applet_name() { // Maximum 10 characters
        return "MIDIOut";
    }

    void Start() {
        channel = 0; // Default channel 1
        last_channel = 0;
        function = 0;
        gated = 0;
        transpose = 0;
        legato = 1;
        log_index = 0;

        // defaults for auto MIDI out
        HS::frame.MIDIState.outfn[io_offset + 0] = HEM_MIDI_NOTE_OUT;
        HS::frame.MIDIState.outfn[io_offset + 1] = HEM_MIDI_GATE_OUT;
    }

    void Controller() {
        bool read_gate = Gate(0);
        auto &hMIDI = HS::frame.MIDIState;

        // Handle MIDI notes

        // Prepare to read pitch and send gate in the near future; there's a slight
        // lag between when a gate is read and when the CV can be read.
        if (read_gate && !gated) StartADCLag();

        bool note_on = EndOfADCLag(); // If the ADC lag has ended, a note will always be sent
        if (note_on || legato_on) {
            // Get a new reading when gated, or when checking for legato changes
            uint8_t midi_note = MIDIQuantizer::NoteNumber(In(0), transpose);

            if (legato_on && midi_note != last_note) {
                // Send note off if the note has changed
                hMIDI.SendNoteOff(last_channel, last_note, 0);
                UpdateLog(HEM_MIDI_NOTE_OFF, midi_note, 0);
                note_on = 1;
            }

            if (note_on) {
                int velocity = 0x64;
                if (function == HEM_MIDI_VEL_IN) {
                    velocity = ProportionCV(In(1), 127);
                }
                last_velocity = velocity;

                hMIDI.SendNoteOn(channel, midi_note, velocity);
                last_note = midi_note;
                last_channel = channel;
                last_tick = OC::CORE::ticks;
                if (legato) legato_on = 1;

                UpdateLog(HEM_MIDI_NOTE_ON, midi_note, velocity);
            }
        }

        if (!read_gate && gated) { // A note off message should be sent
            hMIDI.SendNoteOff(last_channel, last_note, 0);
            UpdateLog(HEM_MIDI_NOTE_OFF, last_note, 0);
            last_tick = OC::CORE::ticks;
        }

        gated = read_gate;
        if (!gated) legato_on = 0;

        // Handle other messages
        if (function != HEM_MIDI_VEL_IN) {
            if (Changed(1)) {
                // Modulation wheel
                if (function == HEM_MIDI_CC_IN) {
                    int value = ProportionCV(In(1), 127);
                    if (value != last_cc) {
                      hMIDI.SendCC(channel, 1, value);
                      last_cc = value;
                      UpdateLog(HEM_MIDI_CC, value, 0);
                      last_tick = OC::CORE::ticks;
                    }
                }

                // Aftertouch
                if (function == HEM_MIDI_AT_IN) {
                    int value = ProportionCV(In(1), 127);
                    if (value != last_at) {
                      hMIDI.SendAfterTouch(channel, value);
                      last_at = value;
                      UpdateLog(HEM_MIDI_AFTERTOUCH, value, 0);
                      last_tick = OC::CORE::ticks;
                    }
                }

                // Pitch Bend
                if (function == HEM_MIDI_PB_IN) {
                    uint16_t bend = Proportion(In(1) + HEMISPHERE_3V_CV, HEMISPHERE_3V_CV * 2, 16383);
                    bend = constrain(bend, 0, 16383);
                    if (bend != last_bend) {
                      hMIDI.SendPitchBend(channel, bend);
                      last_bend = bend;
                      UpdateLog(HEM_MIDI_PITCHBEND, bend - 8192, 0);
                      last_tick = OC::CORE::ticks;
                    }
                }
            }
        }
    }

    void View() {
        DrawMonitor();
        if (cursor == LOG_VIEW) DrawLog();
        else DrawSelector();
    }

    void OnButtonPress() {
        if (cursor == LEGATO && !EditMode()) { // special case to toggle legato
            legato = 1 - legato;
            ResetCursor();
            return;
        }

        CursorToggle();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, MAX_CURSOR);
            return;
        }

        switch (cursor) {
        case CHANNEL:
          channel = constrain(channel + direction, 0, 15);
          HS::frame.MIDIState.outchan[io_offset + 0] = channel;
          HS::frame.MIDIState.outchan[io_offset + 1] = channel;
          break;
        case TRANSPOSE: transpose = constrain(transpose + direction, -24, 24);
                break;
        case CV2_FUNC:
          function = constrain(function + direction, 0, 3);
          HS::frame.MIDIState.outfn[io_offset + 0] = HEM_MIDI_NOTE_OUT;
          if (function == HEM_MIDI_CC_IN)
            HS::frame.MIDIState.outfn[io_offset + 1] = HEM_MIDI_CC_OUT;
          else
            HS::frame.MIDIState.outfn[io_offset + 1] = HEM_MIDI_GATE_OUT;
          break;
        case LEGATO: legato = direction > 0 ? 1 : 0;
                break;
        }
        ResetCursor();
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, channel);
        Pack(data, PackLocation {4,3}, function);
        Pack(data, PackLocation {7,1}, legato);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        channel = Unpack(data, PackLocation {0,4});
        function = Unpack(data, PackLocation {4,3});
        legato = Unpack(data, PackLocation {7,1});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Gate";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "Pitch";
    help[HELP_CV2]      = fn_name[function];
    help[HELP_OUT1]     = "";
    help[HELP_OUT2]     = "";
    help[HELP_EXTRA1]  = "";
    help[HELP_EXTRA2]  = "";
    //                   "---------------------" <-- Extra text size guide
  }

private:
    // Settings
    int channel; // MIDI Out channel
    int function; // Function of B/D output
    int transpose; // Semitones of transposition
    int legato; // New notes are sent when note is changed

    // Housekeeping
    int cursor; // 0=MIDI channel, 1=Transpose, 2=CV 2 function, 3=Legato
    int last_note; // Last MIDI note number awaiting not off
    int last_velocity;
    int last_channel; // The last Note On channel, just in case the channel is changed before release
    int last_cc; // Last modulation wheel sent
    int last_at; // Last aftertouch sent
    int last_bend; // Last pitch bend sent
    bool gated; // The most recent gate status
    bool legato_on; // The note handler may currently respond to legato note changes
    int last_tick; // Most recent MIDI message sent
    int adc_lag_countdown;
    const char* const fn_name[4] = {"Mod", "Aft", "Bend", "Veloc"};

    // Logging
    MIDILogEntry log[7];
    int log_index;

    void UpdateLog(int message, int data1, int data2) {
        log[log_index++] = {message, data1, data2};
        if (log_index == 7) {
            for (int i = 0; i < 6; i++)
            {
                memcpy(&log[i], &log[i+1], sizeof(log[i+1]));
            }
            log_index--;
        }
    }

    void DrawMonitor() {
        if (OC::CORE::ticks - last_tick < 4000) {
            gfxBitmap(46, 1, 8, MIDI_ICON);
        }
    }

    void DrawSelector() {
        // MIDI Channel
        gfxPrint(1, 15, "Ch:");
        gfxPrint(24, 15, channel + 1);

        // Transpose
        gfxPrint(1, 25, "Tr:");
        if (transpose > -1) gfxPrint(24, 25, "+");
        gfxPrint(30, 25, transpose);

        // Input 2 function
        gfxPrint(1, 35, "i2:");
        gfxPrint(24, 35, fn_name[function]);

        // Legato
        gfxPrint(1, 45, "Legato ");
        if (cursor != LEGATO || CursorBlink()) gfxIcon(54, 45, legato ? CHECK_ON_ICON : CHECK_OFF_ICON);

        // Cursor
        if (cursor < LEGATO) gfxCursor(24, 23 + (cursor * 10), 39);

        // Last note log
        if (last_velocity) {
            gfxBitmap(1, 56, 8, NOTE_ICON);
            gfxPrint(10, 56, midi_note_numbers[last_note]);
            gfxPrint(40, 56, last_velocity);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawLog() {
        if (log_index) {
            for (int i = 0; i < log_index; i++)
            {
                log_entry(15 + (i * 8), i);
            }
        }
    }

    void log_entry(int y, int index) {
        if (log[index].message == HEM_MIDI_NOTE_ON) {
            gfxBitmap(1, y, 8, NOTE_ICON);
            gfxPrint(10, y, midi_note_numbers[log[index].data1]);
            gfxPrint(40, y, log[index].data2);
        }

        if (log[index].message == HEM_MIDI_NOTE_OFF) {
            gfxPrint(1, y, "-");
            gfxPrint(10, y, midi_note_numbers[log[index].data1]);
        }

        if (log[index].message == HEM_MIDI_CC) {
            gfxBitmap(1, y, 8, MOD_ICON);
            gfxPrint(10, y, log[index].data1);
        }

        if (log[index].message == HEM_MIDI_AFTERTOUCH) {
            gfxBitmap(1, y, 8, AFTERTOUCH_ICON);
            gfxPrint(10, y, log[index].data1);
        }

        if (log[index].message == HEM_MIDI_PITCHBEND) {
            int data = log[index].data1;
            gfxBitmap(1, y, 8, BEND_ICON);
            gfxPrint(10, y, data);
        }
    }
};

