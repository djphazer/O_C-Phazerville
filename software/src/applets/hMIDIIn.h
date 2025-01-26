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

    enum hMIDIIn_Page_Cursor {
        hMIDIIn_CHAN_A = 0,
        hMIDIIn_CHAN_B,
        hMIDIIn_GLOBAL,
        hMIDIIn_LOG_VIEW,

        hMIDIIn_PAGE_LAST = hMIDIIn_LOG_VIEW
    };

    enum hMIDIIn_Channel_Cursor {
        MIDI_CHANNEL = 1,
        OUTPUT_MODE,
        POLY_VOICE,

        hMIDIIn_CHAN_LAST = POLY_VOICE
    };

    enum hMIDIIn_Global_Cursor {
        POLY_MODE = 1,
        // POLY_CHANNEL_FILTER,

        // hMIDIIN_GLOB_LAST = POLY_CHANNEL_FILTER
        hMIDIIN_GLOB_LAST = POLY_MODE
    };

    const char* applet_name() {
        return "MIDIIn";
    }
    const uint8_t* applet_icon() { return PhzIcons::midiIn; }

    void Start() {
        // int v = 2 * hemisphere;
        ForEachChannel(ch) {
            int ch_ = ch + io_offset;
            frame.MIDIState.channel[ch_] = 0; // Default channel 1
            frame.MIDIState.function[ch_] = (ch_ % 2) ? HEM_MIDI_GATE_POLY_OUT : HEM_MIDI_NOTE_POLY_OUT;
            frame.MIDIState.outputs[ch_] = 0;
            frame.MIDIState.dac_polyvoice[ch_] = hemisphere;
            Out(ch, 0);
        }

        frame.MIDIState.log_index = 0;
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
            int ch_ = ch + io_offset;
            switch (frame.MIDIState.function[ch_]) {
            case HEM_MIDI_NOOP:
                break;
            case HEM_MIDI_CLOCK_OUT:
            case HEM_MIDI_START_OUT:
            case HEM_MIDI_TRIG_OUT:
            case HEM_MIDI_TRIG_1ST_OUT:
            case HEM_MIDI_TRIG_ALWAYS_OUT:
                if (frame.MIDIState.trigout_q[ch_]) {
                    frame.MIDIState.trigout_q[ch_] = 0;
                    ClockOut(ch);
                }
                break;
            case HEM_MIDI_RUN_OUT:
                GateOut(ch, frame.MIDIState.clock_run);
                break;
            default:
                Out(ch, frame.MIDIState.outputs[ch_]);
                break;
            }
        }
    }

    void View() {
        switch (page) {
            case hMIDIIn_CHAN_A:
            case hMIDIIn_CHAN_B:
                DrawChannelPage();
                DrawMonitor();
                break;
            case hMIDIIn_GLOBAL:
                DrawGlobalPage();
                break;
            case hMIDIIn_LOG_VIEW:
            default:
                DrawLog();
                break;
        }
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        int ch = io_offset + page;
        switch (page) {
            case hMIDIIn_CHAN_A:
            case hMIDIIn_CHAN_B:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, hMIDIIn_CHAN_LAST);
                    return;
                }
                switch (cursor) {
                    case 0:
                        page = constrain(page + direction, 0, hMIDIIn_PAGE_LAST);
                        break;
                    case MIDI_CHANNEL:
                        frame.MIDIState.channel[ch] = constrain(frame.MIDIState.channel[ch] + direction, 0, 16); // 16 = omni
                        frame.MIDIState.UpdateMidiChannelFilter();
                        break;
                    case OUTPUT_MODE:
                        frame.MIDIState.function[ch] = constrain(frame.MIDIState.function[ch] + direction, 0, HEM_MIDI_MAX_FUNCTION);
                        frame.MIDIState.function_cc[ch] = -1; // auto-learn MIDI CC
                        frame.MIDIState.clock_count = 0;
                        break;
                    case POLY_VOICE:
                        frame.MIDIState.dac_polyvoice[ch] = constrain(frame.MIDIState.dac_polyvoice[ch] + direction, 0, DAC_CHANNEL_LAST-1);
                        frame.MIDIState.UpdateMaxPolyphony();
                    default: break;
                } break;
            case hMIDIIn_GLOBAL:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, hMIDIIN_GLOB_LAST);
                    return;
                }
                switch (cursor) {
                    case 0:
                        page = constrain(page + direction, 0, hMIDIIn_PAGE_LAST);
                        break;
                    case POLY_MODE:
                        frame.MIDIState.poly_mode = constrain(frame.MIDIState.poly_mode + direction, 0, POLY_LAST);
                        break;
                    default: break;
                } break;
            case hMIDIIn_LOG_VIEW:
            default:
                if (!EditMode()) {
                    MoveCursor(cursor, direction, 1);
                    return;
                }
                if (cursor == 0) page = constrain(page + direction, 0, hMIDIIn_PAGE_LAST);
                else return;
                break;
        }
        ResetCursor();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, frame.MIDIState.channel[io_offset + 0]);
        Pack(data, PackLocation {4,4}, frame.MIDIState.channel[io_offset + 1]);
        // 6 bits empty here
        Pack(data, PackLocation {14,7}, frame.MIDIState.function_cc[io_offset + 0] + 1);
        Pack(data, PackLocation {21,7}, frame.MIDIState.function_cc[io_offset + 1] + 1);

        Pack(data, PackLocation {28,5}, frame.MIDIState.function[io_offset + 0]);
        Pack(data, PackLocation {33,5}, frame.MIDIState.function[io_offset + 1]);

        Pack(data, PackLocation {38,3}, frame.MIDIState.dac_polyvoice[io_offset + 0]);
        Pack(data, PackLocation {41,3}, frame.MIDIState.dac_polyvoice[io_offset + 1]);

        Pack(data, PackLocation {44,4}, frame.MIDIState.poly_mode);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        frame.MIDIState.channel[io_offset + 0] = constrain(Unpack(data, PackLocation {0,4}), 0, 15);
        frame.MIDIState.channel[io_offset + 1] = constrain(Unpack(data, PackLocation {4,4}), 0, 15);
        frame.MIDIState.function[io_offset + 0] = constrain(Unpack(data, PackLocation {28,5}), 0, HEM_MIDI_MAX_FUNCTION);
        frame.MIDIState.function[io_offset + 1] = constrain(Unpack(data, PackLocation {33,5}), 0, HEM_MIDI_MAX_FUNCTION);
        frame.MIDIState.function_cc[io_offset + 0] = constrain(Unpack(data, PackLocation {14,7}) - 1, -1, 127);
        frame.MIDIState.function_cc[io_offset + 1] = constrain(Unpack(data, PackLocation {21,7}) - 1, -1, 127);
        frame.MIDIState.dac_polyvoice[io_offset + 0] = constrain(Unpack(data, PackLocation {38,3}), 0, DAC_CHANNEL_LAST-1);
        frame.MIDIState.dac_polyvoice[io_offset + 1] = constrain(Unpack(data, PackLocation {41,3}), 0, DAC_CHANNEL_LAST-1);
        frame.MIDIState.poly_mode = constrain(Unpack(data, PackLocation {44,4}), 0, POLY_LAST);
        frame.MIDIState.UpdateMidiChannelFilter();
        frame.MIDIState.UpdateMaxPolyphony();
    }

protected:
    void SetHelp() {
        //                      "-------" <-- Label size guide
        //help[HELP_DIGITAL1] = "";
        //help[HELP_DIGITAL2] = "";
        //help[HELP_CV1]      = "";
        //help[HELP_CV2]      = "";
        help[HELP_OUT1]       = midi_fn_name[frame.MIDIState.function[io_offset + 0]];
        help[HELP_OUT2]       = midi_fn_name[frame.MIDIState.function[io_offset + 1]];
        //help[HELP_EXTRA1]   = "";
        //help[HELP_EXTRA2]   = "";
        //                      "---------------------" <-- Extra text size guide
    }

private:
    // Housekeeping
    int cursor;
    int page = hMIDIIn_CHAN_A;
    int last_icon_ticks[2];

    void DrawMonitor() {
        if ((OC::CORE::ticks - frame.MIDIState.last_msg_tick) < 100) {
            // reset icon display timers
            if (frame.MIDIState.channel[io_offset + 0] == frame.MIDIState.last_midi_channel)
                last_icon_ticks[0] = OC::CORE::ticks;
            if (frame.MIDIState.channel[io_offset + 1] == frame.MIDIState.last_midi_channel)
                last_icon_ticks[1] = OC::CORE::ticks;
        }

        if (OC::CORE::ticks - last_icon_ticks[page] < 4000) // Ch midi activity
            gfxBitmap(54, 13, 8, MIDI_ICON);
    }

    void DrawChannelPage() {
        char out_label[] = {(char)('A' + io_offset + page), '\0' };
        gfxPrint(1, 13, "DAC "); gfxPrint(out_label);
        gfxLine(1, 22, 63, 22);

        gfxPrint(1, 25, "MIDICh: ");
        if (frame.MIDIState.channel[io_offset + page] > 15) gfxPrint("Om");
        else gfxPrint(frame.MIDIState.channel[io_offset + page] + 1);

        gfxBitmap(2, 34, 8, MIDI_ICON); gfxPrint(13, 35, midi_fn_name[frame.MIDIState.function[io_offset + page]]);
        if (frame.MIDIState.function[io_offset + page] == HEM_MIDI_CC_OUT)
            gfxPrint(frame.MIDIState.function_cc[io_offset + page]);

        gfxPrint(1, 45, "Voice:  "); gfxPrint(frame.MIDIState.dac_polyvoice[io_offset + page] + 1);

        // Cursor
        switch (cursor) {
            case 0:
                gfxCursor(1, 21, 52);
                break;
            case OUTPUT_MODE:
                gfxCursor(12, 23 + (cursor * 10), 51);
                break;
            case MIDI_CHANNEL:
            case POLY_VOICE:
                gfxCursor(49, 23 + (cursor * 10), 14);
            default: break;
        }

        // Last log entry
        if (frame.MIDIState.log_index > 0) {
            PrintLogEntry(56, frame.MIDIState.log_index - 1);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawGlobalPage() {
        gfxPrint(1, 13, "Poly Cfg");
        gfxLine(1, 22, 63, 22);

        gfxPrint(1, 25, "Md: "); gfxPrint(midi_poly_mode_name[frame.MIDIState.poly_mode]);

        gfxPrint(1, 35, "ChFilt:");
        for (int i = 0; i <= 16; ++i) {
            if (frame.MIDIState.CheckMidiChannelFilter(i)) gfxRect(1 + (i*4), 45, 2, 7);
            else gfxRect(1 + (i*4), 51, 2, 1);
        }

        // Cursor
        switch (cursor) {
            case 0:
                gfxCursor(1, 21, 52);
                break;
            case POLY_MODE:
                gfxCursor(19, 23 + (cursor * 10), 45);
                break;
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
                PrintLogEntry(15 + (i * 8), i);
            }
        }
    }

    void PrintLogEntry(int y, int index) {
        MIDILogEntry &log_entry_ = frame.MIDIState.log[index];

        switch ( log_entry_.message ) {
        case HEM_MIDI_NOTE_ON:
            gfxBitmap(1, y, 8, NOTE_ICON);
            gfxPrint(10, y, midi_note_numbers[log_entry_.data1]);
            gfxPrint(40, y, log_entry_.data2);
            break;

        case HEM_MIDI_NOTE_OFF:
            gfxPrint(1, y, "-");
            gfxPrint(10, y, midi_note_numbers[log_entry_.data1]);
            break;

        case HEM_MIDI_CC:
            gfxBitmap(1, y, 8, MOD_ICON);
            gfxPrint(10, y, log_entry_.data2);
            break;

        case HEM_MIDI_AFTERTOUCH_CHANNEL:
            gfxBitmap(1, y, 8, AFTERTOUCH_ICON);
            gfxPrint(10, y, log_entry_.data1);
            break;

        case HEM_MIDI_AFTERTOUCH_POLY:
            gfxBitmap(1, y, 8, AFTERTOUCH_ICON);
            gfxPrint(10, y, log_entry_.data2);
            break;

        case HEM_MIDI_PITCHBEND: {
            int data = (log_entry_.data2 << 7) + log_entry_.data1 - 8192;
            gfxBitmap(1, y, 8, BEND_ICON);
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
