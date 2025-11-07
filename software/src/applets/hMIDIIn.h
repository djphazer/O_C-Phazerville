// Copyright (c) 2018, Jason Justian and 2025, Beau Sterling
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

#ifndef _HEM_H_MIDI_IN_H_
#define _HEM_H_MIDI_IN_H_

class hMIDIIn : public HemisphereApplet {
public:

    enum hMIDIIn_Cursor {
        MAP_INDEX_A,
        hMIDIIn_A_MIDI_CHANNEL,
        hMIDIIn_A_OUTPUT_MODE,
        hMIDIIn_A_POLY_VOICE,
        MAP_A_RANGELOW, MAP_A_RANGEHIGH,

        MAP_INDEX_B,
        hMIDIIn_B_MIDI_CHANNEL,
        hMIDIIn_B_OUTPUT_MODE,
        hMIDIIn_B_POLY_VOICE,
        MAP_B_RANGELOW, MAP_B_RANGEHIGH,

        hMIDIIn_GLOBAL_POLY_MODE,
        hMIDIIn_GLOBAL_PROG_CHANGE_CHANNEL,
        hMIDIIn_LOG_VIEW,

        hMIDIIn_CURSOR_LAST = hMIDIIn_LOG_VIEW
    };

    const char* applet_name() {
        return "MIDIIn";
    }
    const uint8_t* applet_icon() { return PhzIcons::midiIn; }

    void Start() {
        // int v = 2 * hemisphere;
        map_index[0] = io_offset;
        map_index[1] = io_offset + 1;
        ForEachChannel(ch) {
            MIDIMapping &map = frame.MIDIState.mapping[map_index[ch]];
            map.output = 0;
            Out(ch, 0);
        }
        frame.MIDIState.log_index = 0;
    }

    void Reset() {
        frame.MIDIState.clock_count = 0;

        frame.MIDIState.ClearMonoBuffer();
        frame.MIDIState.ClearPolyBuffer();
        frame.MIDIState.UpdateMidiChannelFilter();
        frame.MIDIState.UpdateMaxPolyphony();
    }

    void Controller() {
        // MIDI input is processed at a higher level
        // here, we just pass the MIDI signals on to physical outputs
        ForEachChannel(ch) {
            int ch_ = map_index[ch];
            MIDIMapping &map = frame.MIDIState.mapping[ch_];
            switch (map.function) {
                case HEM_MIDI_NOOP:
                    break;
                case HEM_MIDI_RUN_OUT:
                    GateOut(ch, frame.MIDIState.clock_run);
                    break;
                default:
                    Out(ch, map.output);
                    break;
            }
        }
    }

    void View() {
        switch (cursor) {
            default:
            case hMIDIIn_A_MIDI_CHANNEL:
            case hMIDIIn_A_OUTPUT_MODE:
            case hMIDIIn_A_POLY_VOICE:
            case hMIDIIn_B_MIDI_CHANNEL:
            case hMIDIIn_B_OUTPUT_MODE:
            case hMIDIIn_B_POLY_VOICE:
            case MAP_A_RANGELOW:
            case MAP_A_RANGEHIGH:
            case MAP_B_RANGELOW:
            case MAP_B_RANGEHIGH:
                DrawChannelPage();
                DrawMonitor();
                break;
            case hMIDIIn_GLOBAL_POLY_MODE:
            case hMIDIIn_GLOBAL_PROG_CHANGE_CHANNEL:
                DrawGlobalPage();
                break;
            case hMIDIIn_LOG_VIEW:
                DrawLog();
                break;
        }
    }

    void AuxButton() override {
      switch (cursor) {
        case hMIDIIn_A_OUTPUT_MODE:
        case hMIDIIn_B_OUTPUT_MODE:
          {
            int ch = map_index[io_page];
            MIDIMapping &map = frame.MIDIState.mapping[ch];
            map.channel = 16; // omni
            map.function = HEM_MIDI_LEARN;
            map.function_cc = -1; // auto-learn MIDI CC or precise NoteOn
            frame.MIDIState.clock_count = 0;
            frame.MIDIState.UpdateMidiChannelFilter();
            break;
          }
        default: break;
      }
      CancelEdit();
    }
    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, hMIDIIn_CURSOR_LAST);
            io_page = (cursor >= MAP_INDEX_B);
            return;
        }
        int ch = map_index[io_page];
        MIDIMapping &map = frame.MIDIState.mapping[ch];
        switch (cursor) {
            case MAP_INDEX_A:
            case MAP_INDEX_B:
                map_index[cursor > MAP_INDEX_A] =
                  constrain(map_index[cursor > MAP_INDEX_A] + direction, 0, HS::MIDIMAP_MAX - 1);
                break;
            case hMIDIIn_A_MIDI_CHANNEL:
            case hMIDIIn_B_MIDI_CHANNEL:
                map.channel = constrain(map.channel + direction, 0, 16); // 16 = omni
                frame.MIDIState.UpdateMidiChannelFilter();
                break;
            case hMIDIIn_A_OUTPUT_MODE:
            case hMIDIIn_B_OUTPUT_MODE:
                map.function = constrain(map.function + direction, 0, HEM_MIDI_MAX_FUNCTION);
                if (map.function == HEM_MIDI_CC_OUT)
                  map.function_cc = -1; // auto-learn MIDI CC
                frame.MIDIState.UpdateMidiChannelFilter();
                break;
            case hMIDIIn_A_POLY_VOICE:
            case hMIDIIn_B_POLY_VOICE:
                map.dac_polyvoice = constrain(map.dac_polyvoice + direction, 0, DAC_CHANNEL_LAST - 1);
                frame.MIDIState.UpdateMaxPolyphony();
                break;
            case MAP_A_RANGELOW:
            case MAP_B_RANGELOW:
                map.AdjustRangeLow(direction);
                break;
            case MAP_A_RANGEHIGH:
            case MAP_B_RANGEHIGH:
                map.AdjustRangeHigh(direction);
                break;

            case hMIDIIn_GLOBAL_POLY_MODE:
                frame.MIDIState.poly_mode = constrain(frame.MIDIState.poly_mode + direction, 0, POLY_LAST);
                break;
            case hMIDIIn_GLOBAL_PROG_CHANGE_CHANNEL:
                frame.MIDIState.pc_channel = constrain(frame.MIDIState.pc_channel + direction, 0, 17); // 0 = omni, 17 = off
                break;
            case hMIDIIn_LOG_VIEW:
            default:
                break;
        }
        ResetCursor();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation{0, 5}, map_index[0]);
        Pack(data, PackLocation{8, 5}, map_index[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        map_index[0] = Unpack(data, PackLocation {0,5});
        map_index[1] = Unpack(data, PackLocation {8,5});
    }

protected:
    void SetHelp() {
        //                      "-------" <-- Label size guide
        //help[HELP_DIGITAL1] = "";
        //help[HELP_DIGITAL2] = "";
        //help[HELP_CV1]      = "";
        //help[HELP_CV2]      = "";
        help[HELP_OUT1]       = midi_fn_name[frame.MIDIState.mapping[map_index[0]].function];
        help[HELP_OUT2]       = midi_fn_name[frame.MIDIState.mapping[map_index[1]].function];
        //help[HELP_EXTRA1]   = "";
        //help[HELP_EXTRA2]   = "";
        //                      "---------------------" <-- Extra text size guide
    }

private:
    // Housekeeping
    int cursor;
    int map_index[2] = {0, 1};
    int io_page = 0;
    int last_icon_ticks[2];

    void DrawMonitor() {
        if ((OC::CORE::ticks - frame.MIDIState.last_msg_tick) < 100) {
            // reset icon display timers
            if (frame.MIDIState.mapping[map_index[0]].channel == frame.MIDIState.last_midi_channel)
                last_icon_ticks[0] = OC::CORE::ticks;
            if (frame.MIDIState.mapping[map_index[1]].channel == frame.MIDIState.last_midi_channel)
                last_icon_ticks[1] = OC::CORE::ticks;
        }

        if (OC::CORE::ticks - last_icon_ticks[io_page] < 4000) // Ch midi activity
            gfxIcon(54, 13, MIDI_ICON);
    }

    void DrawChannelPage() {
        char out_label[] = {(char)('A' + io_offset + io_page), '\0' };
        gfxPrint(1, 13, out_label);
        gfxPrint(": ");
        gfxPrint(OC::Strings::cv_input_names_none[ADC_CHANNEL_LAST + DAC_CHANNEL_LAST + map_index[io_page] + 1]);

        // ------------------ //
        gfxLine(1, 22, 63, 22);

        MIDIMapping &map = frame.MIDIState.mapping[map_index[io_page]];
        uint8_t m_ch = map.channel;
        gfxPrint(1, 25, "MIDICh:");
        if (m_ch > 15) graphics.printf("%3s", "Om");
        else graphics.printf("%3d", m_ch + 1);

        gfxIcon(2, 34, MIDI_ICON);
        if (map.function < HEM_MIDI_FN_COUNT)
          gfxPrint(13, 35, midi_fn_name[map.function]);
        if (map.function == HEM_MIDI_CC_OUT)
            gfxPrint(map.function_cc);

        if (cursor >= (MAP_A_RANGELOW + io_page*(MAP_A_RANGEHIGH+1))) {
          gfxPrint(1, 45, "<"); gfxPrint(HS::midi_note_numbers[map.range_low]);
          gfxPrint(34, 45, HS::midi_note_numbers[map.range_high]); gfxPrint(">");
        } else {
          gfxPrint(1, 45, "Voice:"); gfxPrint(55, 45, map.dac_polyvoice + 1);
        }

        // Cursor
        switch (cursor) {
            case MAP_INDEX_A:
            case MAP_INDEX_B:
                gfxCursor(19, 21, 19);
                break;
            case hMIDIIn_A_MIDI_CHANNEL:
            case hMIDIIn_B_MIDI_CHANNEL:
                gfxCursor(42, 33, 21);
                break;
            case hMIDIIn_A_OUTPUT_MODE:
            case hMIDIIn_B_OUTPUT_MODE:
                gfxCursor(12, 43, 51);
                break;
            case hMIDIIn_A_POLY_VOICE:
            case hMIDIIn_B_POLY_VOICE:
                gfxCursor(42, 53, 21);
                break;
            case MAP_A_RANGELOW:
            case MAP_B_RANGELOW:
                gfxCursor(7, 53, 19);
                break;
            case MAP_A_RANGEHIGH:
            case MAP_B_RANGEHIGH:
                gfxCursor(34, 53, 19);
                break;
            default: break;
        }

        // Last log entry
        if (frame.MIDIState.log_index > 0) {
            PrintLogEntry(56, frame.MIDIState.log_index - 1);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawGlobalPage() {
        gfxPrint(1, 13, "Global");
        gfxLine(1, 22, 63, 22);

        gfxBitmap(2, 25, 16, PhzIcons::keyboard);
        gfxPos(26, 25); graphics.printf("%6s", midi_poly_mode_name[frame.MIDIState.poly_mode]);

        const uint8_t pc_ch = frame.MIDIState.pc_channel;
        gfxPrint(1, 35, "PC"); gfxIcon(14, 35, PhzIcons::filter);

        gfxPos(43, 35);
        if (pc_ch == 0) graphics.printf("%3s", "Om");
        else if (pc_ch <= 16) graphics.printf("%3d", pc_ch);
        else graphics.printf("%3s", "Off");

        for (int i = 0; i < 16; ++i) {
            if (frame.MIDIState.CheckMidiChannelFilter(i)) gfxRect(1 + i * 4, 45, 2, 7);
            else gfxRect(1 + i * 4, 51, 2, 1);
        }

        // Cursor
        switch (cursor) {
            case hMIDIIn_GLOBAL_POLY_MODE:
                gfxCursor(25, 33, 38);
                break;
            case hMIDIIn_GLOBAL_PROG_CHANGE_CHANNEL:
                gfxCursor(42, 43, 21);
            default: break;
        }

        // Last log entry
        if (frame.MIDIState.log_index > 0) {
            PrintLogEntry(56, frame.MIDIState.log_index - 1);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawLog() {
        if (frame.MIDIState.log_index) {
            for (int i = 0; i < frame.MIDIState.log_index; i++) {
                PrintLogEntry(15 + i * 8, i);
            }
        }
    }

    void PrintLogEntry(int y, int index) {
        MIDILogEntry &log_entry_ = frame.MIDIState.log[index];

        switch (log_entry_.message) {
            case HEM_MIDI_NOTE_ON:
                gfxIcon(1, y, NOTE_ICON);
                gfxPrint(10, y, midi_note_numbers[log_entry_.data1]);
                gfxPrint(40, y, log_entry_.data2);
                break;

            case HEM_MIDI_NOTE_OFF:
                gfxPrint(1, y, "-");
                gfxPrint(10, y, midi_note_numbers[log_entry_.data1]);
                break;

            case HEM_MIDI_CC:
                gfxIcon(1, y, MOD_ICON);
                gfxPrint(10, y, log_entry_.data2);
                break;

            case HEM_MIDI_AFTERTOUCH_CHANNEL:
                gfxIcon(1, y, AFTERTOUCH_ICON);
                gfxPrint(10, y, log_entry_.data1);
                break;

            case HEM_MIDI_AFTERTOUCH_POLY:
                gfxIcon(1, y, AFTERTOUCH_ICON);
                gfxPrint(10, y, log_entry_.data2);
                break;

            case HEM_MIDI_PITCHBEND: {
                int data = (log_entry_.data2 << 7) + log_entry_.data1 - 8192;
                gfxIcon(1, y, BEND_ICON);
                gfxPrint(10, y, data);
                break;
                }

            default:
                gfxPrint(1, y, "?");
                gfxPrint(10, y, log_entry_.data1);
                gfxPrint(" ");
                gfxPrint(log_entry_.data2);
                break;
        }
    }
};
#endif
