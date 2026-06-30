// Copyright (c) 2018, Jason Justian
// Copyright (c) 2025, Nicholas J. Michalek
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
    enum MIDIOutCursor : uint8_t {
        CHANNEL, TRANSPOSE, CV2_FUNC, LEGATO,
        LOG_VIEW,

        MAX_CURSOR = LOG_VIEW
    };
    enum MIDIOutMode : uint8_t {
        NOTE,
        AFTERTOUCH,
        PITCHBEND,
        VELOCITY,
        CC_CONTROL,
        // values beyond here represent CC#

        MAX_MODE = CC_CONTROL + 127
    };
    const char* const fn_name[5] = {"Note", "Aft", "Bend", "Veloc", "CC#"};

    const char* applet_name() { // Maximum 10 characters
        return "MIDIOut";
    }
    const uint8_t* applet_icon() { return PhzIcons::midiOut; }

    void Start() {
        channel = 0; // Default channel 1
        gated = 0;
        transpose = 0;
        legato = 1;
        log_index = 0;

        ReloadSettings();
        last_channel = channel;
    }

    void Controller() {
        bool read_gate = Gate(0);
        auto &hMIDI = HS::frame.MIDIState;

        // TODO: literally all of this could be handled globally in the IOFrame/MIDIFrame instead
        // ADC lag might be tricky tho...

        // Handle MIDI notes

        // Prepare to read pitch and send gate in the near future; there's a slight
        // lag between when a gate is read and when the CV can be read.
        if (read_gate && !gated) StartADCLag();

        bool note_on = EndOfADCLag(); // If the ADC lag has ended, a note will always be sent
        if (functionA == NOTE) {
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
                if (functionB == VELOCITY) {
                    velocity = ProportionCV(In(1), 127, HEMISPHERE_MAX_INPUT_CV);
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
        } else if (Changed(0)) {
          // CC from CV1
          int value = ProportionCV(In(0), 127, HEMISPHERE_MAX_INPUT_CV);
          if (value != last_cc[0]) {
            hMIDI.SendCC(channel, functionA - CC_CONTROL, value);
            last_cc[0] = value;
            UpdateLog(HEM_MIDI_CC, functionA - CC_CONTROL, value);
            last_tick = OC::CORE::ticks;
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
        if (functionB != VELOCITY && Changed(1)) {
            // CC# such as Mod wheel
            if (functionB >= CC_CONTROL) {
                int value = ProportionCV(In(1), 127, HEMISPHERE_MAX_INPUT_CV);
                if (value != last_cc[1]) {
                    hMIDI.SendCC(channel, functionB - CC_CONTROL, value);
                    last_cc[1] = value;
                    UpdateLog(HEM_MIDI_CC, functionB - CC_CONTROL, value);
                    last_tick = OC::CORE::ticks;
                }
            }

            // Aftertouch
            if (functionB == AFTERTOUCH) {
                int value = ProportionCV(In(1), 127, HEMISPHERE_MAX_INPUT_CV);
                if (value != last_at) {
                    hMIDI.SendAfterTouch(channel, value);
                    last_at = value;
                    UpdateLog(HEM_MIDI_AFTERTOUCH_CHANNEL, value, 0);
                    last_tick = OC::CORE::ticks;
                }
            }

            // Pitch Bend
            if (functionB == PITCHBEND) {
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

    void View() {
        DrawMonitor();
        if (cursor == LOG_VIEW) DrawLog();
        else DrawSelector();
    }

    void AuxButton() {
      if (cursor == TRANSPOSE) {
        if (functionA == NOTE)
          functionA = CC_CONTROL;
        else
          functionA = NOTE;

        CommitSettings();
      }
    }
    void OnButtonPress() {
        if (cursor == LEGATO && !EditMode()) { // special case to toggle legato
            legato = !legato;
            ResetCursor();
            return;
        }

        CursorToggle();
    }

    // from applet to global MIDIFrame
    void CommitSettings() {
      auto& hMIDI = HS::frame.MIDIState;
      HS::MIDIMapping &mapA = hMIDI.outmap[io_offset + 0];
      HS::MIDIMapping &mapB = hMIDI.outmap[io_offset + 1];

      mapA.SetChannel(channel);
      if (functionA == NOTE) {
        mapA.SetPitch(0);
        mapA.SetTranspose(transpose);
      } else {
        mapA.SetCC(functionA - CC_CONTROL);
      }

      mapB.SetChannel(channel);
      if (functionB >= CC_CONTROL) {
        mapB.SetCC(functionB - CC_CONTROL);
      } else
        mapB.SetGate(0);
    }
    // from MIDIFrame to applet settings
    void ReloadSettings() {
      auto &hMIDI = HS::frame.MIDIState;
      HS::MIDIMapping &map = hMIDI.outmap[io_offset + 0];
      HS::MIDIMapping &mapB = hMIDI.outmap[io_offset + 1];

      channel = map.get_channel();
      transpose = map.get_transpose();
      if (map.IsPitch()) {
        functionA = NOTE;
      } else
        functionA = CC_CONTROL + map.get_subtype();

      if (mapB.IsCC())
        functionB = CC_CONTROL + mapB.get_subtype();
      if (mapB.get_type() == HS::MIDIMapSettings::MODULATOR) {
        switch (mapB.get_subtype()) {
          case HS::MIDIMapSettings::MOD_VEL_MONO:
            functionB = VELOCITY;
            break;
          case HS::MIDIMapSettings::MOD_AT_CHAN:
            functionB = AFTERTOUCH;
            break;
          default:
            functionB = PITCHBEND;
        }
      }
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR);
        return;
      }

      switch (cursor) {
        case CHANNEL:
          channel = constrain(channel + direction, 0, 15);
          break;

        case TRANSPOSE:
          if (functionA == NOTE)
            transpose = constrain(transpose + direction, -48, 48);
          else {
            functionA = constrain(functionA + direction, CC_CONTROL, MAX_MODE);
          }
          break;

        case CV2_FUNC:
          functionB = constrain(functionB + direction, 0, MAX_MODE);
          break;

        case LEGATO:
          legato = direction > 0 ? 1 : 0;
          break;
      }

      CommitSettings();
      ResetCursor();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, channel);
        //Pack(data, PackLocation {4,3}, functionB);
        Pack(data, PackLocation {7,1}, legato);

        // new params
        Pack(data, PackLocation { 8,8}, functionA);
        Pack(data, PackLocation {16,8}, functionB);
        Pack(data, PackLocation {24,8}, static_cast<uint8_t>(transpose));

        return data;
    }

    void OnDataReceive(uint64_t data) {
        channel = Unpack(data, PackLocation {0,4});
        //functionB = Unpack(data, PackLocation {4,3});
        legato = Unpack(data, PackLocation {7,1});

        functionA = Unpack(data, PackLocation { 8,8});
        functionB = Unpack(data, PackLocation {16,8});
        transpose = Unpack(data, PackLocation {24,8});
        CONSTRAIN(transpose, -48, 48);
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Gate";
        help[HELP_DIGITAL2] = "";
        help[HELP_CV1]      = "Pitch";
        help[HELP_CV2]      = fn_name[min(functionB, CC_CONTROL)];
        help[HELP_OUT1]     = "";
        help[HELP_OUT2]     = "";
        help[HELP_EXTRA1]   = "";
        help[HELP_EXTRA2]   = "";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    int cursor; // MIDIOutCursor

    // Settings
    uint8_t channel; // MIDI Out channel
    uint8_t functionA = NOTE;
    uint8_t functionB = CC_CONTROL + 1; // Function of second CV input
    int8_t transpose; // Semitones of transposition
    bool legato; // New notes are sent when note is changed

    // Housekeeping
    uint8_t last_note; // Last MIDI note number awaiting note off
    uint8_t last_velocity;
    uint8_t last_channel; // The last Note On channel, just in case the channel is changed before release
    uint8_t last_cc[2]; // Last CC value sent
    uint8_t last_at; // Last aftertouch sent
    uint16_t last_bend; // Last pitch bend sent
    bool gated; // The most recent gate status
    bool legato_on; // The note handler may currently respond to legato note changes
    uint32_t last_tick; // Most recent MIDI message sent

    // Logging
    MIDIMessage log[7];
    int log_index;

    void UpdateLog(uint8_t message, uint8_t data1, uint8_t data2) {
        log[log_index++] = {channel, message, data1, data2};
        if (log_index == 7) {
            for (int i = 0; i < 6; i++) {
                memcpy(&log[i], &log[i+1], sizeof(log[i+1]));
            }
            log_index--;
        }
    }

    void DrawMonitor() {
        if (OC::CORE::ticks - last_tick < 4000) {
            if (hemisphere & 1)
                gfxIcon( 9, 1, MIDI_ICON);
            else
                gfxIcon(46, 1, MIDI_ICON);
        }
    }

    void DrawSelector() {
        // MIDI Channel
        gfxPrint(1, 15, "Ch:");
        gfxPrint(24, 15, channel + 1);

        if (functionA == NOTE) {
          // Transpose
          gfxPrint(1, 25, "Tr:");
          if (transpose > -1) gfxPrint(24, 25, "+");
          gfxPrint(30, 25, transpose);
        } else {
          gfxPrint(1, 25, "i1: CC#");
          gfxPrint(functionA - CC_CONTROL);
        }

        // Input 2 function
        gfxPrint(1, 35, "i2:");
        if (functionB >= CC_CONTROL) {
          gfxPrint(24, 35, fn_name[CC_CONTROL]);
          gfxPrint(functionB - CC_CONTROL);
        } else
          gfxPrint(24, 35, fn_name[functionB]);

        // Legato
        gfxPrint(1, 45, "Legato");
        if (cursor != LEGATO || CursorBlink()) gfxIcon(54, 45, legato ? CHECK_ON_ICON : CHECK_OFF_ICON);

        // Cursor
        switch (cursor) {
          default:
            gfxCursor(24, 23 + (cursor * 10), 33);
            break;
          case TRANSPOSE:
            gfxSpicyCursor(24, 23 + (cursor * 10), 33);
            break;
          case LEGATO:
            gfxIcon(46, 45, RIGHT_ICON);
            break;
          case LOG_VIEW:
            break;
        }

        // Last note log
        if (last_velocity) {
            gfxIcon(1, 56, NOTE_ICON);
            gfxPrint(10, 56, midi_note_numbers[last_note]);
            gfxPrint(40, 56, last_velocity);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawLog() {
        if (log_index) {
            for (int i = 0; i < log_index; i++) {
                log_entry(15 + (i * 8), i);
            }
        }
    }

    void log_entry(int y, int index) {
      switch (log[index].message) {
        case HEM_MIDI_NOTE_ON:
          gfxIcon(1, y, NOTE_ICON);
          gfxPrint(10, y, midi_note_numbers[log[index].data1]);
          gfxPrint(40, y, log[index].data2);
          break;

        case HEM_MIDI_NOTE_OFF:
          gfxPrint(1, y, "-");
          gfxPrint(10, y, midi_note_numbers[log[index].data1]);
          break;

        case HEM_MIDI_CC:
          gfxIcon(1, y, MOD_ICON);
          gfxPrint(10, y, log[index].data1);
          gfxPrint(40, y, log[index].data2);
          break;

        case HEM_MIDI_AFTERTOUCH_CHANNEL:
          gfxIcon(1, y, AFTERTOUCH_ICON);
          gfxPrint(10, y, log[index].data1);
          break;

        case HEM_MIDI_PITCHBEND:
          gfxIcon(1, y, BEND_ICON);
          gfxPrint(10, y, log[index].data1);
          break;
      }
    }
};

