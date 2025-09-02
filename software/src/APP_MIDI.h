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
//
// Adapted for T4.x and 8-channel hardware by djphazer

#ifdef ENABLE_APP_MIDI

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include "OC_config.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "src/drivers/display.h"
#include "util/util_settings.h"
#include "HSApplication.h"
#include "HSMIDI.h"

static constexpr int MIDI_SETUP_COUNT = 4;
static constexpr int MIDI_PARAMETER_COUNT = 40;
static constexpr int MIDI_CURRENT_SETUP = (MIDI_PARAMETER_COUNT * MIDI_SETUP_COUNT);
static constexpr int MIDI_SETTING_COUNT = (MIDI_CURRENT_SETUP + 1);
static constexpr int MIDI_LOG_MAX_SIZE = 101;

enum MIDI_IN_FUNCTION : uint8_t {
  // T32 only, deprecated
    MIDI_IN_OFF,
    MIDI_IN_NOTE = HEM_MIDI_NOTE_OUT,
    MIDI_IN_GATE = HEM_MIDI_GATE_OUT,
    MIDI_IN_TRIGGER = HEM_MIDI_TRIG_OUT,
    MIDI_IN_VELOCITY = HEM_MIDI_VEL_OUT,
    MIDI_IN_MOD = HEM_MIDI_CC_OUT,
    MIDI_IN_AFTERTOUCH = HEM_MIDI_AT_CHAN_OUT,
    MIDI_IN_PITCHBEND = HEM_MIDI_PB_OUT,
    /*
    if (in_fn == MIDI_IN_EXPRESSION) cc = 11;
    if (in_fn == MIDI_IN_PAN) cc = 10;
    if (in_fn == MIDI_IN_HOLD) cc = 64;
    if (in_fn == MIDI_IN_BREATH) cc = 2;
    if (in_fn == MIDI_IN_Y_AXIS) cc = 74;
    */
    MIDI_IN_EXPRESSION,
    MIDI_IN_PAN,
    MIDI_IN_HOLD,
    MIDI_IN_BREATH,
    MIDI_IN_Y_AXIS,

    // clock divisions
    MIDI_IN_CLOCK_4TH = HEM_MIDI_CLOCK_OUT,
    MIDI_IN_CLOCK_8TH = HEM_MIDI_CLOCK_8_OUT,
    MIDI_IN_CLOCK_16TH = HEM_MIDI_CLOCK_16_OUT,
    MIDI_IN_CLOCK_24PPQN = HEM_MIDI_CLOCK_24_OUT,
};

enum MIDI_OUT_FUNCTION : uint8_t {
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

    MIDI_OUT_FUNCTION_COUNT
};

const char* const midi_in_functions[17] = {
    "--", "Note", "Gate", "Trig", "Veloc", "Mod", "Aft", "Bend",  "Expr", "Pan", "Hold", "Brth", "yAxis", "Qtr", "8th", "16th", "24ppq"
};
const char* const midi_out_functions[MIDI_OUT_FUNCTION_COUNT] = {
    "--", "Note", "Leg.", "Veloc", "Mod", "Aft", "Bend", "Expr", "Pan", "Hold", "Brth", "yAxis"
};

const char* const midi2cv_label[] = {
  "MIDI > A", "MIDI > B", "MIDI > C", "MIDI > D",
  "MIDI > E", "MIDI > F", "MIDI > G", "MIDI > H",
};
const char* const cv2midi_label[] = {
  "1 > MIDI", "2 > MIDI", "3 > MIDI", "4 > MIDI",
  "5 > MIDI", "6 > MIDI", "7 > MIDI", "8 > MIDI",
};

#define MIDI_SETUP_PARAMETER_LIST \
{ 0, 0, 16, midi2cv_label[0], midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[1], midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[2], midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[3], midi_in_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, cv2midi_label[0], midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, cv2midi_label[1], midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, cv2midi_label[2], midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 11, cv2midi_label[3], midi_out_functions, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[0], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[1], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[2], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, midi2cv_label[3], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, cv2midi_label[0], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, cv2midi_label[1], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, cv2midi_label[2], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 16, cv2midi_label[3], midi_channels, settings::STORAGE_TYPE_U8 },\
{ 0, -24, 24, midi2cv_label[0], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, midi2cv_label[1], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, midi2cv_label[2], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, midi2cv_label[3], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, cv2midi_label[0], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, cv2midi_label[1], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, cv2midi_label[2], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, -24, 24, cv2midi_label[3], NULL, settings::STORAGE_TYPE_I8 },\
{ 0, 0, 127, midi2cv_label[0], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[1], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[2], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[3], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, cv2midi_label[0], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, cv2midi_label[1], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, cv2midi_label[2], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, cv2midi_label[3], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[0], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[1], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[2], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 0, 0, 127, midi2cv_label[3], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, cv2midi_label[0], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, cv2midi_label[1], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, cv2midi_label[2], midi_note_numbers, settings::STORAGE_TYPE_U8 },\
{ 127, 0, 127, cv2midi_label[3], midi_note_numbers, settings::STORAGE_TYPE_U8 },

#ifdef __IMXRT1062__
// per channel
const settings::value_attr CaptainSettings[] = {
  // Assigned function
  // MIDI-to-CV
  { 0, 0, HEM_MIDI_MAX_FUNCTION, "", midi_fn_name, settings::STORAGE_TYPE_U8 },
  // CV-to-MIDI
  { 0, 0, MIDI_OUT_FUNCTION_COUNT-1, "", midi_out_functions, settings::STORAGE_TYPE_U8 },

  // Channel
  { 0, 0, 16, "", midi_channels, settings::STORAGE_TYPE_U8 },
  // Transpose
  { 0, -24, 24, "", NULL, settings::STORAGE_TYPE_I8 },
  // Range Low
  { 0, 0, 127, "", midi_note_numbers, settings::STORAGE_TYPE_U8 },
  // Range High
  { 0, 0, 127, "", midi_note_numbers, settings::STORAGE_TYPE_U8 },
};
#endif

enum CaptainsKeys : uint16_t {
  SETUP_KEY = 0,

  // upper 7 bits of mapping key
  INPUT_MAP_KEY = 1 << 9,
  OUTPUT_MAP_KEY = 2 << 9,
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

class CaptainMIDI : public HSApplication, public SystemExclusiveHandler
#ifndef __IMXRT1062__
  , public settings::SettingsBase<CaptainMIDI, MIDI_SETTING_COUNT>
#endif
{
public:
    static constexpr int MIDI_INDICATOR_COUNTDOWN = 2000;

    OC::menu::ScreenCursor<OC::menu::kScreenLines> cursor;

    void Start() {
        screen = 0;
        display = 0;
        cursor.Init(0, ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT - 1);
        log_index = 0;
        log_view = 0;
        Reset();

        // default everything off
        for (int i = 0; i < MIDIMAP_MAX; ++i) {
          frame.MIDIState.mapping[i].function = HEM_MIDI_NOOP;
        }
#ifdef __IMXRT1062__
#else
        // Go through all the Setups and change the default high ranges to G9
        for (int s = 0; s < MIDI_SETUP_COUNT; s++)
        {
            for (int p = 0; p < 8; p++)
                if (values_[s * MIDI_PARAMETER_COUNT + 32 + p] == 0) values_[s * MIDI_PARAMETER_COUNT + 32 + p] = 127;
        }
#endif
    }

    void Suspend() {
        StoreData();
        OnSendSysEx();
    }
    void StoreData() {
#ifdef __IMXRT1062__
        PhzConfig::setValue(SETUP_KEY, active_setup);
        StoreSetup();
        PhzConfig::save_config("CAPTAIN.DAT");
#endif
    }
    void Resume() {
#ifdef __IMXRT1062__
        PhzConfig::load_config("CAPTAIN.DAT");
        uint64_t data = 0;
        PhzConfig::getValue(SETUP_KEY, data);
        active_setup = data;
#endif
        SelectSetup(get_setup_number(), 0);
    }

    void Controller() {
        // Process incoming MIDI traffic
        process_midi_in(usbMIDI);
#ifdef ARDUINO_TEENSY41
        process_midi_in(usbHostMIDI);
        process_midi_in(MIDI1);
        thisUSB.Task();
#endif

        // Convert CV inputs to outgoing MIDI messages
        process_midi_out();

        // set CV outputs from MIDI mappings
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ch++)
        {
            // Handle clock timing
            if (indicator_in[ch] > 0) --indicator_in[ch];
            if (indicator_out[ch] > 0) --indicator_out[ch];

            MIDIMapping &map = frame.MIDIState.mapping[ch];

            Out(ch, map.output);
        }
    }

    void View() {
        if (copy_mode) DrawCopyScreen();
        else if (display == 0) DrawSetupScreens();
        else DrawLogScreen();
    }

    void EncoderEdit(int dir) {
#ifdef __IMXRT1062__
      int pos = cursor.cursor_pos();
      bool input = pos < DAC_CHANNEL_COUNT;
      MIDIMapping &m = input ?
          frame.MIDIState.mapping[pos] :
          frame.MIDIState.outmap[pos - DAC_CHANNEL_COUNT];

        switch (screen) {
          case 0:
            m.function = constrain(m.function + dir, 0,
                input? HEM_MIDI_MAX_FUNCTION : MIDI_OUT_FUNCTION_COUNT-1);
            if (input && m.function == HEM_MIDI_CC_OUT)
              m.function_cc = -1; // learn
            break;
          case 1:
            m.channel = constrain(m.channel + dir, 0, 16);
            break;
          case 2:
            m.AdjustTranspose(dir);
            break;
          case 3:
            m.AdjustRangeLow(dir);
            break;
          case 4:
            m.AdjustRangeHigh(dir);
            break;
          default: break;
        }
#else
        change_value(cursor.cursor_pos(), dir);
        ConstrainRangeValue(cursor.cursor_pos());
#endif
    }
    void MoveCursor(int dir) {
        cursor.Scroll(dir);
    }

#ifdef __IMXRT1062__
    void StoreSetup() {
        for (int i = 0; i < MIDIMAP_MAX; ++i) {
          PhzConfig::setValue(INPUT_MAP_KEY + i + active_setup*MIDIMAP_MAX,
              PackPackables(frame.MIDIState.mapping[i]));
        }
        for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
          PhzConfig::setValue(OUTPUT_MAP_KEY + i + active_setup*ADC_CHANNEL_COUNT,
              PackPackables(frame.MIDIState.outmap[i]));
        }
    }
#endif
    void SelectSetup(int setup_number, int new_screen = -1) {
        // moving to another setup?
        if (setup_number != get_setup_number()) {
#ifdef __IMXRT1062__
          // store current settings
          StoreSetup();

          // Load from config file
          active_setup = setup_number;
          uint64_t data = 0;
          for (int i = 0; i < MIDIMAP_MAX; ++i) {
            if (!PhzConfig::getValue(INPUT_MAP_KEY + i + active_setup*MIDIMAP_MAX, data)) {
              frame.MIDIState.Init();
              break;
            }
            UnpackPackables(data, frame.MIDIState.mapping[i]);
          }
          for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
            if (!PhzConfig::getValue(OUTPUT_MAP_KEY + i + active_setup*ADC_CHANNEL_COUNT, data)) {
              frame.MIDIState.Init();
              break;
            }
            UnpackPackables(data, frame.MIDIState.outmap[i]);
          }
#endif
          Reset();
        }

        // Screen switching, default to same
        if (new_screen == -1) new_screen = screen;

#ifdef __IMXRT1062__
#else
        // Find the cursor position, and new start and end menu items
        int prev_cursor = cursor.cursor_pos() - ((screen * 8) + (get_setup_number() * MIDI_PARAMETER_COUNT));
        int start = (new_screen * 8) + (setup_number * MIDI_PARAMETER_COUNT);
        int end = (new_screen * 8) + (setup_number * MIDI_PARAMETER_COUNT) + 7;

        // And go to there
        cursor.Init(start, end);
        cursor.Scroll(prev_cursor);

        values_[MIDI_CURRENT_SETUP] = setup_number;
#endif
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
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ch++)
        {
            note_in[ch] = -1;
            note_out[ch] = -1;
            indicator_in[ch] = 0;
            indicator_out[ch] = 0;
            Out(ch, 0);
        }
        frame.MIDIState.clock_count = 0;
    }

    void Panic() {
        Reset();
        auto &hMIDI = HS::frame.MIDIState;

        // Send all notes off on every channel
        for (int note = 0; note < 128; note++)
        {
            for (int channel = 0; channel < 16; channel++)
            {
                hMIDI.SendNoteOff(channel, note);
            }
        }
    }

    /* When the app is suspended, it sends out a system exclusive dump, generated here */
    void OnSendSysEx() {
        // Teensy will receive 60-byte sysex files, so there's room for one and only one
        // Setup. The currently-selected Setup will be the one we're sending. That's 40
        // bytes.
        uint8_t V[MIDI_PARAMETER_COUNT];
        for (int i = 0; i < MIDI_PARAMETER_COUNT; i++)
        {
#ifdef __IMXRT1062__
        // TODO
            int p = 0;
#else
            uint8_t offset = MIDI_PARAMETER_COUNT * get_setup_number();
            int p = values_[i + offset];
#endif
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
            for (int i = 0; i < MIDI_PARAMETER_COUNT; i++)
            {
                int p = (int)V[i];
                if (i > 15 && i < 24) p -= 24; // Restore the sign removed in OnSendSysEx()
#ifdef __IMXRT1062__
                // TODO
#else
                uint8_t offset = MIDI_PARAMETER_COUNT * get_setup_number();
                apply_value(i + offset, p);
#endif
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
#ifdef __IMXRT1062__
          uint64_t data = 0;
          for (int i = 0; i < MIDIMAP_MAX; ++i) {
            if (!PhzConfig::getValue(INPUT_MAP_KEY + i + source*MIDIMAP_MAX, data))
              break;
            PhzConfig::setValue(INPUT_MAP_KEY + i + target*MIDIMAP_MAX, data);
          }
          for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
            if (!PhzConfig::getValue(OUTPUT_MAP_KEY + i + source*ADC_CHANNEL_COUNT, data))
              break;
            PhzConfig::setValue(OUTPUT_MAP_KEY + i + target*ADC_CHANNEL_COUNT, data);
          }
#else
           int source_offset = MIDI_PARAMETER_COUNT * source;
           int target_offset = MIDI_PARAMETER_COUNT * target;
           for (int c = 0; c < MIDI_PARAMETER_COUNT; c++)
           {
               values_[target_offset + c] = values_[source_offset + c];
           }
#endif
           SelectSetup(target);
           Resume();
       }
       copy_mode = 0;
   }

#ifdef __IMXRT1062__
        // TODO?
#else
   /* If the changed value is a high or low range, make sure that the high range doesn't go
    * below the low range, or that the low range doesn't go above the high range
    */
   void ConstrainRangeValue(int ix) {
       int page = ix / 8; // Page within a Setup
       if (page == 4 && values_[ix] < values_[ix - 8]) values_[ix] = values_[ix - 8];
       if (page == 3 && values_[ix] > values_[ix + 8]) values_[ix] = values_[ix + 8];
   }
#endif

private:
    // Housekeeping
    int screen; // 0=Assign, 1=Channel, 2=Transpose, 3=Range Low, 4=Range High
    bool display; // 0=Setup Edit 1=Log
    bool copy_mode; // Copy mode on/off
    int active_setup; // index of current setup
    int copy_setup_source; // Which setup is being copied?
    int copy_setup_target; // Which setup is being copied to?

    CaptainMIDILog log[MIDI_LOG_MAX_SIZE];
    int log_index; // Index of log for writing
    int log_view; // Current index for viewing

    // MIDI In
    // TODO: replace with data from MIDIFrame
    int note_in[DAC_CHANNEL_COUNT]; // track active note per DAC channel
    uint16_t indicator_in[DAC_CHANNEL_COUNT]; // A MIDI indicator will display next to MIDI In assignment

    // MIDI Out
    bool gated[ADC_CHANNEL_COUNT]; // Current gated status of each input
    int note_out[ADC_CHANNEL_COUNT]; // Most recent note from this input channel
    int last_channel[ADC_CHANNEL_COUNT]; // Keep track of the actual send channel, in case it's changed while the note is on
    int legato_on[ADC_CHANNEL_COUNT]; // The note handler may currently respond to legato note changes
    uint16_t indicator_out[ADC_CHANNEL_COUNT]; // A MIDI indicator will display next to MIDI Out assignment

    void DrawSetupScreens() {
        // Create the header, showing the current Setup and Screen name
        gfxHeader("");
        switch (screen) {
          case 0: graphics.print("MIDI Assign"); break;
          case 1: graphics.print("MIDI Channel"); break;
          case 2: graphics.print("Transpose"); break;
          case 3: graphics.print("Range Low"); break;
          case 4: graphics.print("Range High"); break;
          default: break;
        }
        gfxPrint(128 - 42, 1, "Setup ");
        gfxPrint(get_setup_number() + 1);

        // Iterate through the current range of settings
        OC::menu::SettingsList<OC::menu::kScreenLines, 0, OC::menu::kDefaultValueX - 1> settings_list(cursor);
        OC::menu::SettingsListItem list_item;
        while (settings_list.available())
        {
            bool suppress = 0; // Don't show the setting if it's not relevant
            const int current = settings_list.Next(list_item);
            int p = current % (DAC_CHANNEL_COUNT+ADC_CHANNEL_COUNT);

            // MIDI In and Out indicators for all screens
            if (p < DAC_CHANNEL_COUNT) { // It's a MIDI In assignment
                int in_fn = get_in_assign(p);
                if (in_fn == MIDI_IN_OFF && screen > 0) suppress = 1;

                if (indicator_in[p] > 0 || note_in[p] > -1) {
                    if (in_fn == MIDI_IN_NOTE) {
                        if (note_in[p] > -1) {
                            graphics.setPrintPos(70, list_item.y + 2);
                            graphics.print(midi_note_numbers[note_in[p]]);
                        }
                    } else graphics.drawBitmap8(70, list_item.y + 2, 8, MIDI_ICON);
                }

                // Indicate if the assignment is a note type
                if (in_fn == MIDI_IN_NOTE)
                    graphics.drawBitmap8(56, list_item.y + 1, 8, MIDI_note_icon);
                else if (screen > 1) suppress = 1;

                // Indicate if the assignment is a clock
                if (in_fn >= MIDI_IN_CLOCK_4TH) {
                    uint8_t o_x = (frame.MIDIState.clock_count < 12) ? 2 : 0;
                    graphics.drawBitmap8(80 + o_x, list_item.y + 1, 8, MIDI_clock_icon);
                    if (screen > 0) suppress = 1;
                }

            } else { // It's a MIDI Out assignment
                p -= DAC_CHANNEL_COUNT;
                int out_fn = get_out_assign(p);

                if (out_fn == MIDI_OUT_OFF && screen > 0) suppress = 1;

                if (indicator_out[p] > 0 || note_out[p] > -1) {
                    if (out_fn == MIDI_OUT_NOTE || out_fn == MIDI_OUT_LEGATO) {
                        if (note_out[p] > -1) {
                            graphics.setPrintPos(70, list_item.y + 2);
                            graphics.print(midi_note_numbers[note_out[p]]);
                        }
                    } else graphics.drawBitmap8(70, list_item.y + 2, 8, MIDI_ICON);
                }

                // Indicate if the assignment is a note type
                if (out_fn == MIDI_OUT_NOTE || out_fn == MIDI_OUT_LEGATO)
                    graphics.drawBitmap8(56, list_item.y + 1, 8, MIDI_note_icon);
                else if (screen > 1) suppress = 1;
            }

            // Draw the item last so that if it's selected, the icons are reversed, too
            if (!suppress) {
#ifdef __IMXRT1062__
              const int idx = (current < DAC_CHANNEL_COUNT && screen == 0) ? 0 : screen + 1;
              list_item.SetPrintPos();
              graphics.print(current < DAC_CHANNEL_COUNT ?
                  midi2cv_label[current] :
                  cv2midi_label[current - DAC_CHANNEL_COUNT]);
              list_item.DrawDefault(GetValue(current), CaptainSettings[idx]);
#else
              list_item.DrawDefault(get_value(current), CaptainMIDI::value_attr(current));
#endif
            } else {
                list_item.SetPrintPos();
                graphics.print("                   --");
                list_item.DrawCustom();
            }
        }
    }

    // The value of the parameter at the given cursor position
    int GetValue(int pos) {
      MIDIMapping &m = (pos < DAC_CHANNEL_COUNT) ?
          frame.MIDIState.mapping[pos] :
          frame.MIDIState.outmap[pos - DAC_CHANNEL_COUNT];
      switch (screen) {
        case 0: return m.function;
        case 1: return m.channel;
        case 2: return m.transpose;
        case 3: return m.range_low;
        case 4: return m.range_high;
        default: return 0;
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
#ifdef __IMXRT1062__
        return active_setup;
#else
        return values_[MIDI_CURRENT_SETUP];
#endif
    }

    // CV inputs translated to MIDI messages
    void process_midi_out() {
        auto &hMIDI = HS::frame.MIDIState;
        for (int ch = 0; ch < ADC_CHANNEL_COUNT; ch++)
        {
            int out_fn = get_out_assign(ch);
            if (out_fn == MIDI_OUT_OFF) continue;

            int out_ch = get_out_channel(ch);
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
                        hMIDI.SendNoteOff(last_channel[ch], note_out[ch]);
                        UpdateLog(0, ch, 1, last_channel[ch], note_out[ch], 0);
                        note_out[ch] = -1;
                        indicator = 1;
                        note_on = 1;
                    }

                    if (!in_out_range(ch, midi_note)) note_on = 0; // Don't play if out of range

                    if (note_on) {
                        int velocity = 0x64;
                        // Look for an input assigned to velocity on the same channel and, if found, use it
                        for (int vch = 0; vch < DAC_CHANNEL_COUNT; vch++)
                        {
                            if (get_out_assign(vch) == MIDI_OUT_VELOCITY && get_out_channel(vch) == out_ch) {
                                velocity = Proportion(In(vch), HSAPPLICATION_5V, 127);
                            }
                        }
                        CONSTRAIN(velocity, 0, 127);
                        hMIDI.SendNoteOn(out_ch, midi_note, velocity);
                        UpdateLog(0, ch, 0, out_ch, midi_note, velocity);
                        indicator = 1;
                        note_out[ch] = midi_note;
                        last_channel[ch] = out_ch;
                        if (legato) legato_on[ch] = 1;
                    }
                }

                if (!read_gate && gated[ch]) { // A note off message should be sent
                    hMIDI.SendNoteOff(last_channel[ch], note_out[ch], 0);
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
                    CONSTRAIN(value, 0, 127);
                    if (cc == 64) value = (value >= 60) ? 127 : 0; // On or off for sustain pedal

                    hMIDI.SendCC(out_ch, cc, value);
                    UpdateLog(0, ch, 2, out_ch, cc, value);
                    indicator = 1;
                }

                // Aftertouch
                if (out_fn == MIDI_OUT_AFTERTOUCH) {
                    int value = Proportion(In(ch), HSAPPLICATION_5V, 127);
                    CONSTRAIN(value, 0, 127);
                    hMIDI.SendAfterTouch(out_ch, value);
                    UpdateLog(0, ch, 3, out_ch, 0, value);
                    indicator = 1;
                }

                // Pitch Bend
                if (out_fn == MIDI_OUT_PITCHBEND) {
                    int16_t bend = Proportion(In(ch) + HSAPPLICATION_3V, HSAPPLICATION_3V * 2, 16383);
                    CONSTRAIN(bend, 0, 16383);
                    hMIDI.SendPitchBend(out_ch, bend);
                    UpdateLog(0, ch, 4, out_ch, 0, bend - 8192);
                    indicator = 1;
                }
            }

            if (indicator) indicator_out[ch] = MIDI_INDICATOR_COUNTDOWN;
        }
    }

    template <typename T1>
    void process_midi_in(T1 &device) {
        if (device.read()) {
            uint8_t message = device.getType();
            uint8_t channel = device.getChannel();
            uint8_t data1 = device.getData1();
            uint8_t data2 = device.getData2();

            // Handle system exclusive dump for Setup data
            if (message == HEM_MIDI_SYSEX) OnReceiveSysEx();

            HS::frame.MIDIState.ProcessMIDIMsg({channel, message, data1, data2});
            //old_process_midi_in(message, channel, data1, data2);
        }
    }

    [[ deprecated ]] // TODO: verify all this is handled in HS::MIDIState, MIDIMapping
    void old_process_midi_in(int message, int channel, int data1, int data2) {

        bool note_captured = 0; // A note or gate should only be captured by
        bool gate_captured = 0; // one assignment, to allow polyphony in the interface

        // A MIDI message has been received; go through each channel to see if it
        // needs to be routed to any of the CV outputs
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ch++)
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

            if (message == HEM_MIDI_AFTERTOUCH_CHANNEL && in_fn == MIDI_IN_AFTERTOUCH && in_ch == channel) {
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

            #ifdef MIDI_DIAGNOSTIC
            if (message > 0) {
                UpdateLog(1, ch, 6, message, data1, data2);
            }
            #endif

            if (indicator) indicator_in[ch] = MIDI_INDICATOR_COUNTDOWN;
        }
    }

#ifdef __IMXRT1062__
    uint8_t get_in_assign(int ch) {
      return frame.MIDIState.get_in_assign(ch);
    }

    uint8_t get_in_channel(int ch) {
      return frame.MIDIState.get_in_channel(ch);
    }

    int8_t get_in_transpose(int ch) {
      return frame.MIDIState.get_in_transpose(ch);
    }

    bool in_in_range(int ch, int note) {
      return frame.MIDIState.in_in_range(ch, note);
    }

    uint8_t get_out_assign(int ch) {
      return frame.MIDIState.get_out_assign(ch);
    }

    uint8_t get_out_channel(int ch) {
      return frame.MIDIState.get_out_channel(ch);
    }

    int8_t get_out_transpose(int ch) {
      return frame.MIDIState.get_out_transpose(ch);
    }

    bool in_out_range(int ch, int note) {
      return frame.MIDIState.in_out_range(ch, note);
    }
#else
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
#endif

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

#ifdef __IMXRT1062__
#else
// TOTAL EEPROM SIZE: 40*4 + 1 == 161 bytes
SETTINGS_DECLARE(CaptainMIDI, MIDI_SETTING_COUNT) {
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    MIDI_SETUP_PARAMETER_LIST
    { 0, 0, 1, "Setup", NULL, settings::STORAGE_TYPE_U8 }
};
#endif

CaptainMIDI captain_midi_instance;

////////////////////////////////////////////////////////////////////////////////
//// App Functions
////////////////////////////////////////////////////////////////////////////////
void MIDI_init() {
    captain_midi_instance.Start();
}

#ifdef __IMXRT1062__
static constexpr size_t MIDI_storageSize() { return 0; }
static size_t MIDI_save(void *storage) { return 0; }
static size_t MIDI_restore(const void *storage) { return 0; }
#else
static constexpr size_t MIDI_storageSize() {
    return CaptainMIDI::storageSize();
}
static size_t MIDI_save(void *storage) {
    return captain_midi_instance.Save(storage);
}
static size_t MIDI_restore(const void *storage) {
    size_t s = captain_midi_instance.Restore(storage);
    captain_midi_instance.Resume();
    return s;
}
#endif

void MIDI_isr() {
	return captain_midi_instance.BaseController();
}

void MIDI_handleAppEvent(OC::AppEvent event) {
    if (event == OC::APP_EVENT_SUSPEND) {
      captain_midi_instance.Suspend();
    }
    if (event == OC::APP_EVENT_RESUME) {
      captain_midi_instance.Resume();
    }
}

void MIDI_loop() {}

void MIDI_menu() {
    captain_midi_instance.BaseView();
}

void MIDI_screensaver() {
    captain_midi_instance.BaseScreensaver();
}

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
            captain_midi_instance.EncoderEdit(event.value);
        } else {
            captain_midi_instance.MoveCursor(event.value);
        }
    }
    if (event.control == OC::CONTROL_ENCODER_L) {
        captain_midi_instance.SwitchScreenOrLogView(event.value);
    }
}


#endif
