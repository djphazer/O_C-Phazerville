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

#ifndef _HEM_H_MIDI_IN_H_
#define _HEM_H_MIDI_IN_H_

class hMIDIIn : public HemisphereApplet {
public:

    enum hMIDIInCursor {
        MIDI_CHANNEL_A,
        MIDI_CHANNEL_B,
        OUTPUT_MODE_A,
        OUTPUT_MODE_B,
        LOG_VIEW,

        MIDI_CURSOR_LAST = LOG_VIEW
    };

    const char* applet_name() {
        return "MIDIIn";
    }
    const uint8_t* applet_icon() { return PhzIcons::midiIn; }

    void Start() {
        ForEachChannel(ch)
        {
            int ch_ = ch + io_offset;
            frame.MIDIState.channel[ch_] = 0; // Default channel 1
            frame.MIDIState.function[ch_] = HEM_MIDI_NOOP;
            frame.MIDIState.outputs[ch_] = 0;
            Out(ch, 0);
        }

        frame.MIDIState.log_index = 0;
        frame.MIDIState.clock_count = 0;
        frame.MIDIState.NoteStackClear();
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
        DrawMonitor();
        if (cursor == LOG_VIEW) DrawLog();
        else DrawSelector();
    }

    //void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, MIDI_CURSOR_LAST);
            return;
        }

        // Log view
        if (cursor == LOG_VIEW) return;

        if (cursor == MIDI_CHANNEL_A || cursor == MIDI_CHANNEL_B) {
            int ch = io_offset + cursor - MIDI_CHANNEL_A;
            frame.MIDIState.channel[ch] = constrain(frame.MIDIState.channel[ch] + direction, 0, 15);
        }
        else {
            int ch = io_offset + cursor - OUTPUT_MODE_A;
            frame.MIDIState.function[ch] = constrain(frame.MIDIState.function[ch] + direction, 0, HEM_MIDI_MAX_FUNCTION);
            frame.MIDIState.function_cc[ch] = -1; // auto-learn MIDI CC
            frame.MIDIState.clock_count = 0;
        }
        ResetCursor();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, frame.MIDIState.channel[io_offset + 0]);
        Pack(data, PackLocation {4,4}, frame.MIDIState.channel[io_offset + 1]);
        Pack(data, PackLocation {8,3}, frame.MIDIState.function[io_offset + 0]);
        Pack(data, PackLocation {11,3}, frame.MIDIState.function[io_offset + 1]);
        Pack(data, PackLocation {14,7}, frame.MIDIState.function_cc[io_offset + 0] + 1);
        Pack(data, PackLocation {21,7}, frame.MIDIState.function_cc[io_offset + 1] + 1);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        frame.MIDIState.channel[io_offset + 0] = Unpack(data, PackLocation {0,4});
        frame.MIDIState.channel[io_offset + 1] = Unpack(data, PackLocation {4,4});
        frame.MIDIState.function[io_offset + 0] = Unpack(data, PackLocation {8,3});
        frame.MIDIState.function[io_offset + 1] = Unpack(data, PackLocation {11,3});
        frame.MIDIState.function_cc[io_offset + 0] = Unpack(data, PackLocation {14,7}) - 1;
        frame.MIDIState.function_cc[io_offset + 1] = Unpack(data, PackLocation {21,7}) - 1;
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    //help[HELP_DIGITAL1] = "";
    //help[HELP_DIGITAL2] = "";
    //help[HELP_CV1]      = "";
    //help[HELP_CV2]      = "";
    help[HELP_OUT1]     = midi_fn_name[frame.MIDIState.function[io_offset + 0]];
    help[HELP_OUT2]     = midi_fn_name[frame.MIDIState.function[io_offset + 1]];
    //help[HELP_EXTRA1]  = "";
    //help[HELP_EXTRA2]  = "";
    //                   "---------------------" <-- Extra text size guide
  }

private:
    // Housekeeping
    int cursor; // 0=MIDI channel, 1=A/C function, 2=B/D function

    void DrawMonitor() {
        if (OC::CORE::ticks - frame.MIDIState.last_msg_tick < 4000) {
            if (hemisphere & 1)
                gfxBitmap( 9, 1, 8, MIDI_ICON);
            else
                gfxBitmap(46, 1, 8, MIDI_ICON);
        }
    }

    void DrawSelector() {
        // MIDI Channels

        char out_label[] = { 'C', 'h', (char)('A' + io_offset), ':', '\0'  };
        gfxPrint(1, 15, out_label);
        gfxPrint(24, 15, frame.MIDIState.channel[io_offset + 0] + 1);
        ++out_label[2];
        gfxPrint(1, 25, out_label);
        gfxPrint(24, 25, frame.MIDIState.channel[io_offset + 1] + 1);

        // Output 1 function
        char out_label_fn[] = { (char)('A' + io_offset), ' ', ':', '\0'  };
        gfxPrint(1, 35, out_label_fn);
        gfxPrint(24, 35, midi_fn_name[frame.MIDIState.function[io_offset + 0]]);
        if (frame.MIDIState.function[io_offset + 0] == HEM_MIDI_CC_OUT)
            gfxPrint(frame.MIDIState.function_cc[io_offset + 0]);

        // Output 2 function
        ++out_label_fn[0];
        gfxPrint(1, 45, out_label_fn);
        gfxPrint(24, 45, midi_fn_name[frame.MIDIState.function[io_offset + 1]]);
        if (frame.MIDIState.function[io_offset + 1] == HEM_MIDI_CC_OUT)
            gfxPrint(frame.MIDIState.function_cc[io_offset + 1]);

        // Cursor
        gfxCursor(24, 23 + (cursor * 10), 39);

        // Last log entry
        if (frame.MIDIState.log_index > 0) {
            PrintLogEntry(56, frame.MIDIState.log_index - 1);
        }
        gfxInvert(0, 55, 63, 9);
    }

    void DrawLog() {
        if (frame.MIDIState.log_index) {
            for (int i = 0; i < frame.MIDIState.log_index; i++)
            {
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

        case HEM_MIDI_AFTERTOUCH:
            gfxBitmap(1, y, 8, AFTERTOUCH_ICON);
            gfxPrint(10, y, log_entry_.data1);
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
