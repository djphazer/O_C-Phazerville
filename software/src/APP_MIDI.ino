// Copyright (c) 2018, Jason Justian
//
// Menu & screen cursor Copyright (c) 2016 Patrick Dowling
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

#ifdef ENABLE_APP_MIDI

#include "HSApplication.h"
#include "HSMIDI.h"

#define MIDI_INDICATOR_COUNTDOWN 2000
#define MIDI_PARAMETER_COUNT 40
#define MIDI_CURRENT_SETUP (MIDI_PARAMETER_COUNT * 4)
#define MIDI_SETTING_LAST (MIDI_CURRENT_SETUP + 1)
#define MIDI_LOG_MAX_SIZE 101

// Icons that are used next to the menu items
const uint8_t MIDI_midi_icon[8] = {0x3c, 0x42, 0x91, 0x45, 0x45, 0x91, 0x42, 0x3c};
const uint8_t MIDI_note_icon[8] = {0xc0, 0xe0, 0xe0, 0xe0, 0x7f, 0x02, 0x14, 0x08};
const uint8_t MIDI_clock_icon[8] = {0x9c, 0xa2, 0xc1, 0xcf, 0xc9, 0xa2, 0x9c, 0x00};

const char* const midi_in_functions[17] = {
    "--", "Note", "Gate", "Trig", "Veloc", "Mod", "Aft", "Bend",  "Expr", "Pan", "Hold", "Brth", "yAxis", "Qtr", "8th", "16th", "24ppq"
};
const char* const midi_out_functions[12] = {
    "--", "Note", "Leg.", "Veloc", "Mod", "Aft", "Bend", "Expr", "Pan", "Hold", "Brth", "yAxis"
};

#define MIDI_SETUP_PARAMETER_LIST \
{ 0, 0, 16, "MIDI > A", midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > B", midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > C", midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > D", midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, "1 > MIDI", midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, "2 > MIDI", midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, "3 > MIDI", midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, "4 > MIDI", midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > A", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > B", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > C", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "MIDI > D", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "1 > MIDI", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "2 > MIDI", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "3 > MIDI", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, "4 > MIDI", midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, -24, 24, "MIDI > A", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "MIDI > B", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "MIDI > C", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "MIDI > D", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "1 > MIDI", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "2 > MIDI", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "3 > MIDI", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, "4 > MIDI", NULL, settings::STORAGE_TYPE_I8 },\
{ 0, 0, 127, "MIDI > A", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > B", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > C", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > D", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "1 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "2 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "3 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "4 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > A", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > B", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > C", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, "MIDI > D", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, "1 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, "2 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, "3 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, "4 > MIDI", midi_note_numbers, settings::STORAGE_TYPE_U8 },

enum MIDI_IN_FUNCTION {
    MIDI_IN_OFF,
    MIDI_IN_NOTE,
    MIDI_IN_GATE,
    MIDI_IN_TRIGGER,
    MIDI_IN_VELOCITY,
    MIDI_IN_MOD,
    MIDI_IN_AFTERTOUCH,
    MIDI_IN_PITCHBEND,
    MIDI_IN_EXPRESSION,
    MIDI_IN_PAN,
    MIDI_IN_HOLD,
    MIDI_IN_BREATH,
    MIDI_IN_Y_AXIS,
    MIDI_IN_CLOCK_4TH,
    MIDI_IN_CLOCK_8TH,
    MIDI_IN_CLOCK_16TH,
    MIDI_IN_CLOCK_24PPQN,
};

enum MIDI_OUT_FUNCTION {
    MIDI_OUT_OFF,
    MIDI_OUT_NOTE,
    MIDI_OUT_LEGATO,
    MIDI_OUT_VELOCITY,
    MIDI_OUT_MOD,
    MIDI_OUT_AFTERTOUCH,
    MIDI_OUT_PITCHBEND,
    MIDI_OUT_EXPRESSION,
    MIDI_OUT_PAN,
    MIDI_OUT_HOLD,
    MIDI_OUT_BREATH,
    MIDI_OUT_Y_AXIS,
};

const char* const midi_messages[7] = {
    "Note", "Off", "CC#", "Aft", "Bend", "SysEx", "Diag"
};
//#define MIDI_DIAGNOSTIC
struct CaptainMIDILog {
    bool midi_in; // 0 = out, 1 = in
    char io; // 1, 2, 3, 4, A, B, C, D
    uint8_t message; // 0 = Note On, 1 = Note Off, 2 = CC, 3 = Aftertouch, 4 = Bend, 5 = SysEx, 6 = Diagnostic
    uint8_t channel; // MIDI channel
    int16_t data1;
    int16_t data2;

    void DrawAt(int y) {
        if (message == 5) {
            int app_code = static_cast<char>(data1);
            if (app_code > 0) {
                graphics.setPrintPos(1, y);
                graphics.print("SysEx: ");
                if (app_code == 'M') graphics.print("Captain MIDI");
                if (app_code == 'H') graphics.print("Hemisphere");
                if (app_code == 'D') graphics.print("D. Timeline");
                if (app_code == 'E') graphics.print("Scale Ed");
                if (app_code == 'T') graphics.print("Enigma");
                if (app_code == 'W') graphics.print("Waveform Ed");
                if (app_code == '_') graphics.print("O_C EEPROM");
                if (app_code == 'B') graphics.print("Backup");
                if (app_code == 'N') graphics.print("Neural Net");
            }
        } else {
            graphics.setPrintPos(1, y);
            if (midi_in) graphics.print(">");
            graphics.print(io);
            if (!midi_in) graphics.print(">");
            graphics.print(" ");
            graphics.print(midi_channels[channel]);
            graphics.setPrintPos(37, y);

            graphics.print(midi_messages[message]);
            graphics.setPrintPos(73, y);

            uint8_t x_offset = (data2 < 100) ? 6 : 0;
            x_offset += (data2 < 10) ? 6 : 0;

            if (message == 0 || message == 1) {
                graphics.print(midi_note_numbers[data1]);
                graphics.setPrintPos(91 + x_offset, y);
                graphics.print(data2); // Velocity
            }

            if (message == 2 || message == 3) {
                if (message == 2) graphics.print(data1); // Controller number
                graphics.setPrintPos(91 + x_offset, y);
                graphics.print(data2); // Value
            }

            if (message == 4) {
                if (data2 > 0) graphics.print("+");
                graphics.print(data2); // Aftertouch or bend value
            }

            if (message == 6) {
                graphics.print(data1);
                graphics.print("/");
                graphics.print(data2);
            }
        }
    }
};

class CaptainMIDI : public SystemExclusiveHandler, public HSApplication,
    public settings::SettingsBase<CaptainMIDI, MIDI_SETTING_LAST> {
public:
    menu::ScreenCursor<menu::kScreenLines> cursor;

    void Start() {
        screen = 0;
        display = 0;
        cursor.Init(0, 7);
        log_index = 0;
        log_view = 0;
        Reset();

        // Go through all the Setups and change the default high ranges to G9
        for (int s = 0; s < 4; s++)
        {
            for (int p = 0; p < 8; p++)
                if (values_[s * MIDI_PARAMETER_COUNT + 32 + p] == 0) values_[s * MIDI_PARAMETER_COUNT + 32 + p] = 127;
        }
	}

    void Resume() {
        SelectSetup(get_setup_number(), 0);
    }

    void Controller() {
        midi_in();
        midi_out();

        // Handle clock timing
        for (int ch = 0; ch < 4; ch++)
        {
            if (indicator_in[ch] > 0) --indicator_in[ch];
            if (indicator_out[ch] > 0) --indicator_out[ch];
        }
    }

    void View() {
        if (copy_mode) DrawCopyScreen();
        else if (display == 0) DrawSetupScreens();
        else DrawLogScreen();
    }

    void SelectSetup(int setup_number, int new_screen = -1) {
        // Stay the same if not provided
        if (new_screen == -1) new_screen = screen;

        // Reset if moving to another setup
        if (setup_number != get_setup_number()) Reset();

        // Find the cursor position, and new start and end menu items
        int prev_cursor = cursor.cursor_pos() - ((screen * 8) + (get_setup_number() * MIDI_PARAMETER_COUNT));
        int start = (new_screen * 8) + (setup_number * MIDI_PARAMETER_COUNT);
        int end = (new_screen * 8) + (setup_number * MIDI_PARAMETER_COUNT) + 7;

        // And go to there
        cursor.Init(start, end);
        cursor.Scroll(prev_cursor);
        values_[MIDI_CURRENT_SETUP] = setup_number;
        screen = new_screen;
    }

    void SwitchScreenOrLogView(int dir) {
        if (display == 0) {
            // Switch screen
            int new_screen = constrain(screen + dir, 0, 4);
            SelectSetup(get_setup_number(), new_screen);
        } else {
            // Scroll Log view
            if (log_index > 6) log_view = constrain(log_view + dir, 0, log_index - 6);
        }
    }

    void SwitchSetup(int dir) {
        if (copy_mode) {
            copy_setup_target = constrain(copy_setup_target + dir, 0, 3);
        } else {
            int new_setup = constrain(get_setup_number() + dir, 0, 3);
            SelectSetup(new_setup);
        }
    }

    void ToggleDisplay() {
        if (copy_mode) copy_mode = 0;
        else display = 1 - display;
    }

    void Reset() {
        // Reset the interface states
        for (int ch = 0; ch < 4; ch++)
        {
            note_in[ch] = -1;
            note_out[ch] = -1;
            indicator_in[ch] = 0;
            indicator_out[ch] = 0;
            Out(ch, 0);
        }
        clock_count = 0;
    }

    void Panic() {
        Reset();

        // Send all notes off on every channel
        for (int note = 0; note < 128; note++)
        {
            for (int channel = 1; channel <= 16; channel++)
            {
                usbMIDI.sendNoteOff(note, 0, channel);
            }
        }
    }

    /* When the app is suspended, it sends out a system exclusive dump, generated here */
    void OnSendSysEx() {
        // Teensy will receive 60-byte sysex files, so there's room for one and only one
        // Setup. The currently-selected Setup will be the one we're sending. That's 40
        // bytes.
        uint8_t V[MIDI_PARAMETER_COUNT];
        uint8_t offset = MIDI_PARAMETER_COUNT * get_setup_number();
        for (int i = 0; i < MIDI_PARAMETER_COUNT; i++)
        {
            int p = values_[i + offset];
            if (i > 15 && i < 24) p += 24; // These are signed, so they need to be converted
            V[i] = static_cast<uint8_t>(p);
        }

        // Pack the data and send it out
        UnpackedData unpacked;
        unpacked.set_data(40, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'M');
    }

    void OnReceiveSysEx() {
        // Since only one Setup is coming, use the currently-selected setup to determine
        // where to stash it.
        uint8_t V[MIDI_PARAMETER_COUNT];
        if (ExtractSysExData(V, 'M')) {
            uint8_t offset = MIDI_PARAMETER_COUNT * get_setup_number();
            for (int i = 0; i < MIDI_PARAMETER_COUNT; i++)
            {
                int p = (int)V[i];
                if (i > 15 && i < 24) p -= 24; // Restore the sign removed in OnSendSysEx()
                apply_value(i + offset, p);
            }
            UpdateLog(1, 0, 5, 0, 'M', 0);
        } else {
            char app_code = LastSysExApplicationCode();
            UpdateLog(1, 0, 5, 0, app_code, 0);
        }
        Resume();
    }

   void ToggleCopyMode() {
       copy_mode = 1 - copy_mode;
       copy_setup_source = get_setup_number();
       copy_setup_target = copy_setup_source + 1;
       if (copy_setup_target > 3) copy_setup_target = 0;
   }

   void ToggleCursor() {
       if (copy_mode) CopySetup(copy_setup_target, copy_setup_source);
       else cursor.toggle_editing();
   }

   /* Perform a copy or sysex dump */
   void CopySetup(int target, int source) {
       if (source == target) {
           OnSendSysEx();
       } else {
           int source_offset = MIDI_PARAMETER_COUNT * source;
           int target_offset = MIDI_PARAMETER_COUNT * target;
           for (int c = 0; c < MIDI_PARAMETER_COUNT; c++)
           {
               values_[target_offset + c] = values_[source_offset + c];
           }
           SelectSetup(target);
           Resume();
       }
       copy_mode = 0;
   }

   /* If the changed value is a high or low range, make sure that the high range doesn't go
    * below the low range, or that the low range doesn't go above the high range
    */
   void ConstrainRangeValue(int ix) {
       int page = ix / 8; // Page within a Setup
       if (page == 4 && values_[ix] < values_[ix - 8]) values_[ix] = values_[ix - 8];
       if (page == 3 && values_[ix] > values_[ix + 8]) values_[ix] = values_[ix + 8];
   }

private:
    // Housekeeping
    int screen; // 0=Assign 2=Channel 3=Transpose
    bool display; // 0=Setup Edit 1=Log
    bool copy_mode; // Copy mode on/off
    int copy_setup_source; // Which setup is being copied?
    int copy_setup_target; // Which setup is being copied to?

    CaptainMIDILog log[MIDI_LOG_MAX_SIZE];
    int log_index; // Index of log for writing
    int log_view; // Current index for viewing

    // MIDI In
    int note_in[4]; // Up to four notes at a time are kept track of with MIDI In
    uint16_t indicator_in[4]; // A MIDI indicator will display next to MIDI In assignment
    uint8_t clock_count; // MIDI clock counter (24ppqn)

    // MIDI Out
    bool gated[4]; // Current gated status of each input
    int note_out[4]; // Most recent note from this input channel
    int last_channel[4]; // Keep track of the actual send channel, in case it's changed while the note is on
    int legato_on[4]; // The note handler may currently respond to legato note changes
    uint16_t indicator_out[4]; // A MIDI indicator will display next to MIDI Out assignment

    void DrawSetupScreens() {
        // Create the header, showing the current Setup and Screen name
        gfxHeader("");
        if (screen == 0) graphics.print("MIDI Assign");
        if (screen == 1) graphics.print("MIDI Channel");
        if (screen == 2) graphics.print("Transpose");
        if (screen == 3) graphics.print("Range Low");
        if (screen == 4) graphics.print("Range High");
        gfxPrint(128 - 42, 1, "Setup ");
        gfxPrint(get_setup_number() + 1);

        // Iterate through the current range of settings
        menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX - 1> settings_list(cursor);
        menu::SettingsListItem list_item;
        while (settings_list.available())
        {
            bool suppress = 0; // Don't show the setting if it's not relevant
            const int current = settings_list.Next(list_item);
            const int value = get_value(current);
            int p = current % 8; // Menu position from 0-7

            // MIDI In and Out indicators for all screens
            if (p < 4) { // It's a MIDI In assignment
                if (indicator_in[p] > 0 || note_in[p] > -1) {
                    if (get_in_assign(p) == MIDI_IN_NOTE) {
                        if (note_in[p] > -1) {
                            graphics.setPrintPos(70, list_item.y + 2);
                            graphics.print(midi_note_numbers[note_in[p]]);
                        }
                    } else graphics.drawBitmap8(70, list_item.y + 2, 8, MIDI_midi_icon);
                }

                // Indicate if the assignment is a note type
                if (get_in_channel(p) > 0 && get_in_assign(p) == MIDI_IN_NOTE)
                    graphics.drawBitmap8(56, list_item.y + 1, 8, MIDI_note_icon);
                else if (screen > 1) suppress = 1;

                // Indicate if the assignment is a clock
                if (get_in_assign(p) >= MIDI_IN_CLOCK_4TH) {
                    uint8_t o_x = (clock_count < 12) ? 2 : 0;
                    graphics.drawBitmap8(80 + o_x, list_item.y + 1, 8, MIDI_clock_icon);
                    if (screen > 0) suppress = 1;
                }

            } else { // It's a MIDI Out assignment
                p -= 4;
                if (indicator_out[p] > 0 || note_out[p] > -1) {
                    if ((get_out_assign(p) == MIDI_OUT_NOTE || get_out_assign(p) == MIDI_OUT_LEGATO)) {
                        if (note_out[p] > -1) {
                            graphics.setPrintPos(70, list_item.y + 2);
                            graphics.print(midi_note_numbers[note_out[p]]);
                        }
                    } else graphics.drawBitmap8(70, list_item.y + 2, 8, MIDI_midi_icon);
                }

                // Indicate if the assignment is a note type
                if (get_out_channel(p) > 0 && (get_out_assign(p) == MIDI_OUT_NOTE || get_out_assign(p) == MIDI_OUT_LEGATO))
                    graphics.drawBitmap8(56, list_item.y + 1, 8, MIDI_note_icon);
                else if (screen > 1) suppress = 1;
            }

            // Draw the item last so that if it's selected, the icons are reversed, too
            if (!suppress) list_item.DrawDefault(value, CaptainMIDI::value_attr(current));
            else {
                list_item.SetPrintPos();
                graphics.print("                   --");
                list_item.DrawCustom();
            }
        }
    }

    void DrawLogScreen() {
        gfxHeader("IO Ch Type  Values");
        if (log_index) {
            for (int l = 0; l < 6; l++)
            {
                int ix = l + log_view; // Log index
                if (ix < log_index) {
                    log[ix].DrawAt(l * 8 + 15);
                }
            }

            // Draw scroll
            if (log_index > 6) {
                graphics.drawFrame(122, 14, 6, 48);
                int y = Proportion(log_view, log_index - 6, 38);
                y = constrain(y, 0, 38);
                graphics.drawRect(124, 16 + y, 2, 6);
            }
        }
    }

    void DrawCopyScreen() {
        gfxHeader("Copy");

        graphics.setPrintPos(8, 28);
        graphics.print("Setup ");
        graphics.print(copy_setup_source + 1);
        graphics.print(" -");
        graphics.setPrintPos(58, 28);
        graphics.print("> ");
        if (copy_setup_source == copy_setup_target) graphics.print("SysEx");
        else {
            graphics.print("Setup ");
            graphics.print(copy_setup_target + 1);
        }

        graphics.setPrintPos(0, 55);
        graphics.print("[CANCEL]");

        graphics.setPrintPos(90, 55);
        graphics.print(copy_setup_source == copy_setup_target ? "[DUMP]" : "[COPY]");
    }

    int get_setup_number() {
        return values_[MIDI_CURRENT_SETUP];
    }

    void midi_out() {
        for (int ch = 0; ch < 4; ch++)
        {
            int out_fn = get_out_assign(ch);
            int out_ch = get_out_channel(ch);
            if (out_ch == 0) continue;
            bool indicator = 0;

            if (out_fn == MIDI_OUT_NOTE || out_fn == MIDI_OUT_LEGATO) {
                bool read_gate = Gate(ch);
                bool legato = out_fn == MIDI_OUT_LEGATO;

                // Prepare to read pitch and send gate in the near future; there's a slight
                // lag between when a gate is read and when the CV can be read.
                if (read_gate && !gated[ch]) StartADCLag(ch);
                bool note_on = EndOfADCLag(ch); // If the ADC lag has ended, a note will always be sent

                if (note_on || legato_on[ch]) {
                    // Get a new reading when gated, or when checking for legato changes
                    uint8_t midi_note = MIDIQuantizer::NoteNumber(In(ch), get_out_transpose(ch));

                    if (legato_on[ch] && midi_note != note_out[ch]) {
                        // Send note off if the note has changed
                        usbMIDI.sendNoteOff(note_out[ch], 0, last_channel[ch]);
                        UpdateLog(0, ch, 1, last_channel[ch], note_out[ch], 0);
                        note_out[ch] = -1;
                        indicator = 1;
                        note_on = 1;
                    }

                    if (!in_out_range(ch, midi_note)) note_on = 0; // Don't play if out of range

                    if (note_on) {
                        int velocity = 0x64;
                        // Look for an input assigned to velocity on the same channel and, if found, use it
                        for (int vch = 0; vch < 4; vch++)
                        {
                            if (get_out_assign(vch) == MIDI_OUT_VELOCITY && get_out_channel(vch) == out_ch) {
                                velocity = Proportion(In(vch), HSAPPLICATION_5V, 127);
                            }
                        }
                        velocity = constrain(velocity, 0, 127);
                        usbMIDI.sendNoteOn(midi_note, velocity, out_ch);
                        UpdateLog(0, ch, 0, out_ch, midi_note, velocity);
                        indicator = 1;
                        note_out[ch] = midi_note;
                        last_channel[ch] = out_ch;
                        if (legato) legato_on[ch] = 1;
                    }
                }

                if (!read_gate && gated[ch]) { // A note off message should be sent
                    usbMIDI.sendNoteOff(note_out[ch], 0, last_channel[ch]);
                    UpdateLog(0, ch, 1, last_channel[ch], note_out[ch], 0);
                    note_out[ch] = -1;
                    indicator = 1;
                }

                gated[ch] = read_gate;
                if (!gated[ch]) legato_on[ch] = 0;
            }

            // Handle other messages
            if (Changed(ch)) {
                // Modulation wheel
                if (out_fn == MIDI_OUT_MOD || out_fn >= MIDI_OUT_EXPRESSION) {
                    int cc = 1; // Modulation wheel
                    if (out_fn == MIDI_OUT_EXPRESSION) cc = 11;
                    if (out_fn == MIDI_OUT_PAN) cc = 10;
                    if (out_fn == MIDI_OUT_HOLD) cc = 64;
                    if (out_fn == MIDI_OUT_BREATH) cc = 2;
                    if (out_fn == MIDI_OUT_Y_AXIS) cc = 74;

                    int value = Proportion(In(ch), HSAPPLICATION_5V, 127);
                    value = constrain(value, 0, 127);
                    if (cc == 64) value = (value >= 60) ? 127 : 0; // On or off for sustain pedal

                    usbMIDI.sendControlChange(cc, value, out_ch);
                    UpdateLog(0, ch, 2, out_ch, cc, value);
                    indicator = 1;
                }

                // Aftertouch
                if (out_fn == MIDI_OUT_AFTERTOUCH) {
                    int value = Proportion(In(ch), HSAPPLICATION_5V, 127);
                    value = constrain(value, 0, 127);
                    usbMIDI.sendAfterTouch(value, out_ch);
                    UpdateLog(0, ch, 3, out_ch, 0, value);
                    indicator = 1;
                }

                // Pitch Bend
                if (out_fn == MIDI_OUT_PITCHBEND) {
                    int16_t bend = Proportion(In(ch) + HSAPPLICATION_3V, HSAPPLICATION_3V * 2, 16383);
                    bend = constrain(bend, 0, 16383);
                    usbMIDI.sendPitchBend(bend, out_ch);
                    UpdateLog(0, ch, 4, out_ch, 0, bend - 8192);
                    indicator = 1;
                }
            }

            if (indicator) indicator_out[ch] = MIDI_INDICATOR_COUNTDOWN;
        }
    }

    void midi_in() {
        if (usbMIDI.read()) {
            int message = usbMIDI.getType();
            int channel = usbMIDI.getChannel();
            int data1 = usbMIDI.getData1();
            int data2 = usbMIDI.getData2();

            // Handle system exclusive dump for Setup data
            if (message == HEM_MIDI_SYSEX) OnReceiveSysEx();

            // Listen for incoming clock
            if (message == HEM_MIDI_CLOCK) {
                if (++clock_count >= 24) clock_count = 0;
            }

            bool note_captured = 0; // A note or gate should only be captured by
            bool gate_captured = 0; // one assignment, to allow polyphony in the interface

            // A MIDI message has been received; go through each channel to see if it
            // needs to be routed to any of the CV outputs
            for (int ch = 0; ch < 4; ch++)
            {
                int in_fn = get_in_assign(ch);
                int in_ch = get_in_channel(ch);
                bool indicator = 0;
                if (message == HEM_MIDI_NOTE_ON && in_ch == channel) {
                    if (note_in[ch] == -1) { // If this channel isn't already occupied with another note, handle Note On
                        if (in_fn == MIDI_IN_NOTE && !note_captured) {
                            // Send quantized pitch CV. Isolate transposition to quantizer so that it notes off aren't
                            // misinterpreted if transposition is changed during the note.
                            int note = data1 + get_in_transpose(ch);
                            note = constrain(note, 0, 127);
                            if (in_in_range(ch, note)) {
                                Out(ch, MIDIQuantizer::CV(note));
                                UpdateLog(1, ch, 0, in_ch, note, data2);
                                indicator = 1;
                                note_captured = 1;
                                note_in[ch] = data1;
                            } else note_in[ch] = -1;
                        }

                        if (in_fn == MIDI_IN_GATE && !gate_captured) {
                            // Send a gate at Note On
                            GateOut(ch, 1);
                            indicator = 1;
                            gate_captured = 1;
                            note_in[ch] = data1;
                        }

                        if (in_fn == MIDI_IN_TRIGGER) {
                            // Send a trigger pulse to CV
                            ClockOut(ch);
                            indicator = 1;
                            gate_captured = 1;
                        }

                        if (in_fn == MIDI_IN_VELOCITY) {
                            // Send velocity data to CV
                            Out(ch, Proportion(data2, 127, HSAPPLICATION_5V));
                            indicator = 1;
                        }
                    }
                }

                if (message == HEM_MIDI_NOTE_OFF && in_ch == channel) {
                    if (note_in[ch] == data1) { // If the note off matches the note on assingned to this output
                        note_in[ch] = -1;
                        if (in_fn == MIDI_IN_GATE) {
                            // Turn off gate on Note Off
                            GateOut(ch, 0);
                            indicator = 1;
                        } else if (in_fn == MIDI_IN_NOTE) {
                            // Log Note Off on the note assignment
                            UpdateLog(1, ch, 1, in_ch, data1, 0);
                        } else if (in_fn == MIDI_IN_VELOCITY) {
                            Out(ch, 0);
                        }
                    }
                }

                bool cc = (in_fn == MIDI_IN_MOD || in_fn >= MIDI_IN_EXPRESSION);
                if (cc && message == HEM_MIDI_CC && in_ch == channel) {
                    uint8_t cc = 1; // Modulation wheel
                    if (in_fn == MIDI_IN_EXPRESSION) cc = 11;
                    if (in_fn == MIDI_IN_PAN) cc = 10;
                    if (in_fn == MIDI_IN_HOLD) cc = 64;
                    if (in_fn == MIDI_IN_BREATH) cc = 2;
                    if (in_fn == MIDI_IN_Y_AXIS) cc = 74;

                    // Send CC wheel to CV
                    if (data1 == cc) {
                        if (in_fn == MIDI_IN_HOLD && data2 > 0) data2 = 127;
                        Out(ch, Proportion(data2, 127, HSAPPLICATION_5V));
                        UpdateLog(1, ch, 2, in_ch, data1, data2);
                        indicator = 1;
                    }
                }

                if (message == HEM_MIDI_AFTERTOUCH && in_fn == MIDI_IN_AFTERTOUCH && in_ch == channel) {
                    // Send aftertouch to CV
                    Out(ch, Proportion(data1, 127, HSAPPLICATION_5V));
                    UpdateLog(1, ch, 3, in_ch, data1, data2);
                    indicator = 1;
                }

                if (message == HEM_MIDI_PITCHBEND && in_fn == MIDI_IN_PITCHBEND && in_ch == channel) {
                    // Send pitch bend to CV
                    int data = (data2 << 7) + data1 - 8192;
                    Out(ch, Proportion(data, 0x7fff, HSAPPLICATION_3V));
                    UpdateLog(1, ch, 4, in_ch, 0, data);
                    indicator = 1;
                }

                if (in_fn >= MIDI_IN_CLOCK_4TH) {
                    // Clock is unlogged because there can be a lot of it
                    uint8_t mod = get_clock_mod(in_fn);
                    if (clock_count % mod == 0) ClockOut(ch);
                }

                #ifdef MIDI_DIAGNOTIC
                if (message > 0) {
                    UpdateLog(1, ch, 6, message, data1, data2);
                }
                #endif

                if (indicator) indicator_in[ch] = MIDI_INDICATOR_COUNTDOWN;
            }
        }
    }

    uint8_t get_clock_mod(int fn) {
        uint8_t mod = 1;
        if (fn == MIDI_IN_CLOCK_4TH) mod = 24;
        if (fn == MIDI_IN_CLOCK_8TH) mod = 12;
        if (fn == MIDI_IN_CLOCK_16TH) mod = 6;
        return mod;
    }

    int get_in_assign(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[ch + setup_offset];
    }

    int get_in_channel(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[8 + ch + setup_offset];
    }

    int get_in_transpose(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[16 + ch + setup_offset];
    }

    bool in_in_range(int ch, int note) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        int range_low = values_[24 + ch + setup_offset];
        int range_high = values_[32 + ch + setup_offset];
        return (note >= range_low && note <= range_high);
    }

    int get_out_assign(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[4 + ch + setup_offset];
    }

    int get_out_channel(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[12 + ch + setup_offset];
    }

    int get_out_transpose(int ch) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        return values_[20 + ch + setup_offset];
    }

    bool in_out_range(int ch, int note) {
        int setup_offset = get_setup_number() * MIDI_PARAMETER_COUNT;
        int range_low = values_[28 + ch + setup_offset];
        int range_high = values_[36 + ch + setup_offset];
        return (note >= range_low && note <= range_high);
    }

    void UpdateLog(bool midi_in, int ch, uint8_t message, uint8_t channel, int16_t data1, int16_t data2) {
        // Don't log SysEx unless the user is on the log display screen
        if (message == 5 && display == 0) return;

        char io = midi_in ? ('A' + ch) : ('1' + ch);
        log[log_index++] = {midi_in, io, message, channel, data1, data2};
        if (log_index == MIDI_LOG_MAX_SIZE) {
            for (int i = 0; i < MIDI_LOG_MAX_SIZE - 1; i++)
            {
                memcpy(&log[i], &log[i+1], sizeof(log[i + 1]));
            }
            log_index--;
        }
        log_view = log_index - 6;
        if (log_view < 0) log_view = 0;
    }
};

// TOTAL EEPROM SIZE: 40*4 + 1 == 161 bytes
SETTINGS_DECLARE(CaptainMIDI, MIDI_SETTING_LAST) {
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    { 0, 0, 1, "Setup", NULL, settings::STORAGE_TYPE_U8 }
};

CaptainMIDI captain_midi_instance;

////////////////////////////////////////////////////////////////////////////////
//// App Functions
////////////////////////////////////////////////////////////////////////////////
void MIDI_init() {
    captain_midi_instance.Start();
}

constexpr size_t MIDI_storageSize() {
    return CaptainMIDI::storageSize();
}

size_t MIDI_save(void *storage) {
    return captain_midi_instance.Save(storage);
}

size_t MIDI_restore(const void *storage) {
    size_t s = captain_midi_instance.Restore(storage);
    captain_midi_instance.Resume();
    return s;
}

void MIDI_isr() {
	return captain_midi_instance.BaseController();
}

void MIDI_handleAppEvent(OC::AppEvent event) {
    if (event == OC::APP_EVENT_SUSPEND) {
        captain_midi_instance.OnSendSysEx();
    }
}

void MIDI_loop() {}

void MIDI_menu() {
    captain_midi_instance.BaseView();
}

void MIDI_screensaver() {}

void MIDI_handleButtonEvent(const UI::Event &event) {
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS)
        captain_midi_instance.ToggleCursor();
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) captain_midi_instance.Panic();
        if (event.type == UI::EVENT_BUTTON_PRESS) captain_midi_instance.ToggleDisplay();
    }

    if (event.control == OC::CONTROL_BUTTON_UP && event.type == UI::EVENT_BUTTON_PRESS)
        captain_midi_instance.SwitchSetup(1);
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) captain_midi_instance.SwitchSetup(-1);
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) captain_midi_instance.ToggleCopyMode();
    }
}

void MIDI_handleEncoderEvent(const UI::Event &event) {
    if (event.control == OC::CONTROL_ENCODER_R) {
        if (captain_midi_instance.cursor.editing()) {
            captain_midi_instance.change_value(captain_midi_instance.cursor.cursor_pos(), event.value);
            captain_midi_instance.ConstrainRangeValue(captain_midi_instance.cursor.cursor_pos());
        } else {
            captain_midi_instance.cursor.Scroll(event.value);
        }
    }
    if (event.control == OC::CONTROL_ENCODER_L) {
        captain_midi_instance.SwitchScreenOrLogView(event.value);
    }
}


#endif
