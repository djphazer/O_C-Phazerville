// The Darkest Timeline v2.0
//
// Copyright (c) 2018, Jason Justian
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

#ifdef ENABLE_APP_DARKEST_TIMELINE

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include "OC_core.h"
#include "OC_config.h"
#include "OC_apps.h"
#include "OC_patterns.h"
#include "OC_strings.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "src/drivers/display.h"

#include "HemisphereApplet.h"
#include "util/util_settings.h"
#include "OC_DAC.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"
#include "HSApplication.h"
#include "HSMIDI.h"

#define DT_CV_TIMELINE 0
#define DT_PROBABILITY_TIMELINE 1
#define DT_SETUP_SCREEN_TIMEOUT 166667

enum {
    DT_LENGTH,
    DT_INDEX,
    DT_SCALE,
    DT_ROOT,
    DT_MIDI_CHANNEL,
    DT_MIDI_CHANNEL_ALT,
    DT_MIDI_CHANNEL_IN,
    DT_GATE_TIME,
    DT_SETTING_LAST
};

class TheDarkestTimeline : public HSApplication, public SystemExclusiveHandler,
    public settings::SettingsBase<TheDarkestTimeline, DT_SETTING_LAST> {
public:
	void Start() {
        Resume();
	}

	void Resume() {
        setup_screen = 0;
        last_midi_note[0] = -1;
        last_midi_note[1] = -1;
        if (!length()) {
            values_[DT_LENGTH] = 16;
            values_[DT_SCALE] = 5;
            values_[DT_MIDI_CHANNEL] = 11;
            values_[DT_MIDI_CHANNEL_ALT] = 12;
        }
        HS::quantizer[0].Init();
        HS::quantizer[0].Configure(OC::Scales::GetScale(scale()), 0xffff);
	}

    void Controller() {
        // Listen for MIDI In
        bool note_on = 0;
        uint8_t in_note_number = 0;
        uint8_t in_velocity = 0;
        if (usbMIDI.read()) {
            int message = usbMIDI.getType();
            int channel = usbMIDI.getChannel();
            int data1 = usbMIDI.getData1();
            int data2 = usbMIDI.getData2();

            // Handle system exclusive dump for Setup data
            if (message == HEM_MIDI_SYSEX) OnReceiveSysEx();

            if (message == HEM_MIDI_NOTE_ON && channel == midi_channel_in()) {
                note_on = 1;
                in_note_number = data1;
                in_velocity = data2;
            }
        }

        // Step forward with Digital 1
        int gate_ticks = HEMISPHERE_CLOCK_TICKS;
        if (Clock(0)) {
            last_tempo = OC::CORE::ticks - last_clock_event;
            last_clock_event = OC::CORE::ticks;
            if (gate_time() > 0) {
                gate_ticks = Proportion(gate_time(), 100, last_tempo);
            }

            // Reverse direction with gate at Digital 2
            move_cursor(Gate(1) ? -1 : 1);
            clocked = 1;
        }

        // Reset sequencer with Digital 3
        if (Clock(2)) {
            last_tempo = OC::CORE::ticks - last_clock_event;
            last_clock_event = OC::CORE::ticks;
            cursor = 0;
            clocked = 1;
        }

        // Handle CV setting of the index from CV 3, if it's not being set from the panel
        if (!index_edit_enabled) {
            int cv = DetentedIn(2);
            if (cv > 0) {
                int cv_index = cv / (HSAPPLICATION_5V / 32);
                apply_value(DT_INDEX, cv_index);
            }
        }

        uint8_t idx = data_index_at_position(0);

        // Handle record and playback for two timelines from CV 1 and 2
        uint8_t vel = 0; // Velocity for MIDI Out
        int cv = 0; // CV for current i/o
        for (uint8_t tl = 0; tl < 2; tl++)
        {
            if (record[tl]) {
                if (midi_channel_in()) {
                    // If a MIDI channel is set, record using MIDI note
                    if (note_on) {
                        int write_cv;
                        if (tl == 0) { // Write to CV Timeline based on note number
                            write_cv = MIDIQuantizer::CV(in_note_number);
                        } else { // Write to Probability Timeline based on velocity
                            write_cv = Proportion(in_velocity, 127, HSAPPLICATION_5V);
                        }
                        write_data_at(idx, tl, write_cv);
                    }
                } else {
                    cv = In(tl);
                    cv = constrain(cv, 0, HSAPPLICATION_5V);
                    write_data_at(idx, tl, cv);
                }
            }

            // Playback
            cv = get_data_at(idx, tl);

            // Get transpose value from CV 4 over a range of 1 octave
            int transpose_cv = DetentedIn(3);
            int transpose = transpose_cv / 128;

            if (tl == 0) {
                // This is a CV Timeline, so output the normal universe note
                int32_t pitch = HS::quantizer[0].Process(cv, root() << 7, transpose);
                Out(0, pitch);

                // and then output the alternate universe note
                uint8_t alt_idx = (idx + length()) % 32;
                int alt_cv = get_data_at(alt_idx, tl);
                pitch = HS::quantizer[0].Process(alt_cv, root() << 7, transpose);
                Out(1, pitch);
            } else if (clocked) {
                // This is the Probability Timeline, and it's only calculated when
                // the system is clocked. The "clocked" state can arise in several ways:
                // movement of the cursor via one of the Digital inputs, or using the
                // knobs to change the cursor or index.
                //
                // The "clocked" state applies to trigger states as well as MIDI Note Out
                // generation.
                clocked = 0; // Reset the clock

                // Calculate normal probability for Output 3
                int prob = random(0, HSAPPLICATION_5V);
                if (prob < cv || Gate(3)) { // Gate at digital 4 makes all probabilities certainties
                    ClockOut(2, gate_ticks);

                    // Send the MIDI Note On
                    if (last_midi_note[0] > -1) usbMIDI.sendNoteOff(last_midi_note[0], 0, last_midi_channel[0]);
                    if (midi_channel()) {
                        last_midi_channel[0] = midi_channel();
                        last_midi_note[0] = MIDIQuantizer::NoteNumber(get_data_at(idx, DT_CV_TIMELINE), transpose);
                        vel = Proportion(cv, HSAPPLICATION_5V, 127);
                        usbMIDI.sendNoteOn(last_midi_note[0], vel, last_midi_channel[0]);
                        last_length[0] = OC::CORE::ticks - last_clock[0];
                        last_clock[0] = OC::CORE::ticks;
                    }
                }

                // Calculate complementary probability for Output 4
                prob = random(0, HSAPPLICATION_5V);
                if (prob < (HSAPPLICATION_5V - cv) || Gate(3)) {
                    ClockOut(3, gate_ticks);

                    // Send the MIDI Note On for Alternate Universe
                    if (last_midi_note[1] > -1) usbMIDI.sendNoteOff(last_midi_note[1], 0, last_midi_channel[1]);
                    if (midi_channel_alt()) {
                        last_midi_channel[1] = midi_channel_alt();
                        uint8_t alt_idx = (idx + length()) % 32;
                        last_midi_note[1] = MIDIQuantizer::NoteNumber(get_data_at(alt_idx, DT_CV_TIMELINE));
                        vel = Proportion(get_data_at(alt_idx, DT_PROBABILITY_TIMELINE), HSAPPLICATION_5V, 127);
                        usbMIDI.sendNoteOn(last_midi_note[1], vel, last_midi_channel[1]);
                        last_length[1] = OC::CORE::ticks - last_clock[1];
                        last_clock[1] = OC::CORE::ticks;
                    }
                }
            } // Thus ends the processing of the Probability Timeline
        }

        // Turn off notes that have been on for too long
        for (uint8_t ch = 0; ch < 2; ch++)
        {
            if (last_midi_note[ch] > -1 && (OC::CORE::ticks - last_clock[ch]) > (last_length[ch]) * 2) {
                usbMIDI.sendNoteOff(last_midi_note[ch], 0, last_midi_channel[ch]);
                last_midi_note[ch] = -1;
            }
        }

        // Timeout for setup screen
        if (--setup_screen_timeout_countdown < 0) {}

        // Advance to next step if a MIDI note was received and recorded
        if (!Clock(0) && note_on && (record[0] || record[1])) move_cursor(1);
    }

    void View() {
        gfxHeader("Darkest Timeline");
        DrawInterface();
    }

    /* When the app is suspended, it sends out a system exclusive dump, generated here */
    void OnSendSysEx() {
        uint8_t V[35];
        // Limit is 48 packed bytes, and there are 130 bytes that need packing. So wrap this
        // app's data up into four smaller sysex "pages" of 32 bytes each.
        for (int page = 0; page < 4; page++)
        {
            int ix = 0;
            V[ix++] = (char)page;
            V[ix++] = (uint8_t)values_[DT_LENGTH]; // Store length and index with each page (legacy 1.3 support)
            V[ix++] = (uint8_t)values_[DT_INDEX];
            for (int b = 0; b < 16; b++)
            {
                uint16_t cv = OC::user_patterns[page].notes[b];
                V[ix++] = cv & 0xff; // Low byte
                V[ix++] = (cv >> 8) & 0xff; // High byte
            }
            UnpackedData unpacked;
            unpacked.set_data(ix, V);
            PackedData packed = unpacked.pack();
            SendSysEx(packed, 'D');
        }

        // Send a fifth page containing metadata
        int ix = 0;
        V[ix++] = static_cast<char>(4); // Page 4
        V[ix++] = static_cast<char>(length());
        V[ix++] = static_cast<char>(index());
        V[ix++] = static_cast<char>(scale());
        V[ix++] = static_cast<char>(root());
        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'D');
    }

    void OnReceiveSysEx() {
        uint8_t V[35];
        if (ExtractSysExData(V, 'D')) {
            int ix = 0;
            int page = V[ix++];
            if (page < 4) {
                // CV data pages 0-3
                values_[DT_LENGTH] = V[ix++]; // V1.3 legacy support
                values_[DT_INDEX] = V[ix++];
                for (int b = 0; b < 16; b++)
                {
                    uint8_t low = V[ix++];
                    uint8_t high = V[ix++];
                    uint16_t cv = (uint16_t)(high << 8) | low;
                    OC::user_patterns[page].notes[b] = constrain(cv, 0, HSAPPLICATION_5V);
                }
            } else if (page == 4) {
                // Metadata page 4
                values_[DT_LENGTH] = V[ix++];
                values_[DT_INDEX] = V[ix++];
                values_[DT_SCALE] = V[ix++];
                values_[DT_ROOT] = V[ix++];
            }
        }
    }

    uint8_t length() {return values_[DT_LENGTH];}
    int8_t index() {return values_[DT_INDEX];}
    uint8_t scale() {return values_[DT_SCALE];}
    uint8_t root() {return values_[DT_ROOT];}
    uint8_t midi_channel() {return values_[DT_MIDI_CHANNEL];}
    uint8_t midi_channel_alt() {return values_[DT_MIDI_CHANNEL_ALT];}
    uint8_t midi_channel_in() {return values_[DT_MIDI_CHANNEL_IN];}
    uint8_t gate_time() {return values_[DT_GATE_TIME];}

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        // Advance through the setup screen
        if (++setup_screen > (DT_SETTING_LAST - 2)) setup_screen = 0;
        if (setup_screen > 0) setup_screen_timeout_countdown = DT_SETUP_SCREEN_TIMEOUT;
        ResetCursor();
    }

    void OnLeftButtonLongPress() {
        // Randomize the timelines
        for (uint8_t tl = 0; tl < 2; tl++)
        {
            for (uint8_t s = 0; s < 32; s++)
            {
                write_data_at(s, tl, random(0, HSAPPLICATION_5V));
            }
        }
    }

    void OnRightButtonPress() {
        // Toggle between index edit and cursor edit
        index_edit_enabled = 1 - index_edit_enabled;
    }

    void OnUpButtonPress() {
        record[DT_CV_TIMELINE] = 1 - record[DT_CV_TIMELINE];
    }

    void OnDownButtonPress() {
        record[DT_PROBABILITY_TIMELINE] = 1 - record[DT_PROBABILITY_TIMELINE];
    }

    void OnDownButtonLongPress() {
        // Clear the timelines
        for (uint8_t tl = 0; tl < 2; tl++)
        {
            for (uint8_t s = 0; s < 32; s++)
            {
                write_data_at(s, tl, 0);
            }
        }
    }

    void OnLeftEncoderMove(int direction) {
        if (setup_screen == 0) change_value(DT_LENGTH, -direction);
        else change_value(setup_screen + 1, direction);

        HS::quantizer[0].Configure(OC::Scales::GetScale(scale()), 0xffff);
        if (setup_screen > 0) setup_screen_timeout_countdown = DT_SETUP_SCREEN_TIMEOUT;
    }

    void OnRightEncoderMove(int direction) {
        if (index_edit_enabled) move_index(direction);
        else move_cursor(direction);

        // If a clock pulse seems to be ongoing, don't interfere with that pulse. Otherwise,
        // set the clocked flag to force a Probability Timeline calculation
        if (OC::CORE::ticks - last_clock_event > last_tempo) clocked = 1;
    }

private:
    // Internal States
    int8_t cursor; // The play/record point within the sequence
    bool record[2]; // 0 = CV Timeline, 1 = Proability Timeline
    bool index_edit_enabled; // The index is being edited via the panel
    uint8_t setup_screen; // Setup screen state
    int setup_screen_timeout_countdown;
    bool clocked; // Sequencer has been clocked, and a probability trigger needs to be determined
    uint32_t last_clock_event; // The last clock event from a Digital input
    uint32_t last_tempo; // Time between the last two clock events

    // MIDI
    int8_t last_midi_note[2];
    uint8_t last_midi_channel[2];
    uint32_t last_clock[2];
    uint32_t last_length[2];

    void DrawInterface() {
        gfxPrint(116 + (length() < 10 ? 6 : 0), 2, length());

        // Show record source icon
        if (record[0] || record[1]) gfxBitmap(104, 1, 8, midi_channel_in() ? MIDI_ICON : CV_ICON);

        if (setup_screen == 0) {
            DrawTimeline(DT_CV_TIMELINE);
            DrawTimeline(DT_PROBABILITY_TIMELINE);
        } else DrawSetupScreen();
    }

    void DrawTimeline(bool timeline) {
        int y_offset = 15 + (timeline * 24);
        for (int pos = 0; pos < length(); pos++)
        {
            int idx = data_index_at_position(pos);
            int cv = get_data_at(idx, timeline);
            bool is_index = (idx == index()) ? 1 : 0;
            DrawColumn(pos, y_offset, cv, record[timeline], is_index);
        }
    }

    void DrawColumn(int pos, int y_offset, int cv, bool is_recording, bool is_index) {
        uint8_t height = Proportion(cv, HSAPPLICATION_5V, 22);
        uint8_t width = (128 / length()) / 2;
        uint8_t x_pos = (128 / length()) * pos;
        uint8_t x_offset = width / 2;
        uint8_t y_pos = 24 - height + y_offset; // Find the top
        if (pos == 0) { // The cursor position is at the leftmost column
            gfxRect(x_pos + x_offset, y_pos, width, height);
            if (is_recording && CursorBlink()) {
                // If in record mode, draw a full-sized blinking frame around the cursor
                gfxInvert(x_pos, y_offset, width * 2, 24);
            }
        } else {
            gfxFrame(x_pos + x_offset, y_pos, width, height);
        }

        if (is_index) {
            int start_y = 15;

            if (index_edit_enabled) {
                // Draw a solid blinking line if the index edit is enabled
                if (!CursorBlink()) graphics.drawLine(x_pos, start_y, x_pos, 63);
            } else {
                // Draw a dashed line showing the index point
                for (int y = start_y; y < 63; y += 4)
                {
                    if (y + 2 < 63) gfxLine(x_pos, y, x_pos, y + 1);
                }
            }
        }
    }

    void DrawSetupScreen() {
        gfxPrint(1, 15, "Scale   : ");
        gfxPrint(OC::scale_names_short[scale()]);
        gfxPrint(102, 15, OC::Strings::note_names_unpadded[root()]);

        gfxPrint(1, 25, "MIDI Out: ");
        gfxPrint(midi_channels[midi_channel()]);

        gfxPrint(1, 35, "     Alt: ");
        gfxPrint(midi_channels[midi_channel_alt()]);

        gfxPrint(1, 45, "     In : ");
        gfxPrint(midi_channels[midi_channel_in()]);

        gfxPrint(1, 55, "Trg/Gate: ");
        if (gate_time() > 0) {
            gfxPrint(gate_time());
            gfxPrint("%");
        } else {
            gfxPrint("Trg");
        }

        if (setup_screen == 1) gfxCursor(60, 23, 30);
        if (setup_screen == 2) gfxCursor(102, 23, 18);
        if (setup_screen == 3) gfxCursor(60, 33, 30);
        if (setup_screen == 4) gfxCursor(60, 43, 30);
        if (setup_screen == 5) gfxCursor(60, 53, 30);
        if (setup_screen == 6) gfxCursor(60, 63, 30);
    }

    /* Geez, don't even ask me to explain this. It's 11PM. (position + cursor) % length constrains the
     * pattern to length steps. Then the index is added. Then the % DT_TIMELINE_SIZE constrains
     * the pattern to the entire indexed pattern.
     */
    int data_index_at_position(int position) {
        int idx = (((position + cursor) % length()) + index()) % 32;
        return idx;
    }

    int move_cursor(int direction) {
        cursor += direction;
        if (cursor < 0) cursor = length() - 1;
        if (cursor >= length()) cursor = 0;
        return cursor;
    }

    int move_index(int direction) {
        values_[DT_INDEX] += direction;
        if (index() < 0) values_[DT_INDEX] = 31;
        if (index() >= 32) values_[DT_INDEX] = 0;
        return values_[DT_INDEX];
    }

    uint16_t get_data_at(int step, bool timeline) {
        if (timeline) step += 32;
        uint16_t note = 0;
        if (step >= 0 && step <= 64) {
            int pattern = step / 16;
            int num = step % 16;
            note = OC::user_patterns[pattern].notes[num];
        }
        return note;
    }

    void write_data_at(int step, bool timeline, uint16_t note) {
        if (timeline) step += 32;
        if (step >= 0 && step <= 64) {
            int pattern = step / 16;
            int num = step % 16;
            OC::user_patterns[pattern].notes[num] = note;
        }
    }
};

// MIDI channels are U8 instead of U4 because the channel number is not zero-indexed; 0 means "off"
// TOTAL EEPROM SIZE: 8 bytes
SETTINGS_DECLARE(TheDarkestTimeline, DT_SETTING_LAST) {
    {16, 1, 32, "Length", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 31, "Index", NULL, settings::STORAGE_TYPE_U8},
    {5, 0, OC::Scales::NUM_SCALES - 1, "Scale", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 11, "Root", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 16, "MIDI Channel", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 16, "MIDI Channel Alt", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 16, "MIDI Channel In", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 100, "Trigger Pct", NULL, settings::STORAGE_TYPE_U8},
};

TheDarkestTimeline TheDarkestTimeline_instance;

// App stubs
void TheDarkestTimeline_init() {
    TheDarkestTimeline_instance.BaseStart();
}

static constexpr size_t TheDarkestTimeline_storageSize() {
    return TheDarkestTimeline::storageSize();
}

static size_t TheDarkestTimeline_save(void *storage) {
    return TheDarkestTimeline_instance.Save(storage);
}

static size_t TheDarkestTimeline_restore(const void *storage) {
    return TheDarkestTimeline_instance.Restore(storage);
}

void TheDarkestTimeline_isr() {
	return TheDarkestTimeline_instance.BaseController();
}

void TheDarkestTimeline_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        TheDarkestTimeline_instance.Resume();
    }
    if (event == OC::APP_EVENT_SUSPEND) {
        TheDarkestTimeline_instance.OnSendSysEx();
    }
}

void TheDarkestTimeline_loop() {} // Deprecated

void TheDarkestTimeline_menu() {
    TheDarkestTimeline_instance.BaseView();
}

void TheDarkestTimeline_screensaver() {}

void TheDarkestTimeline_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) TheDarkestTimeline_instance.OnLeftButtonLongPress();
        if (event.type == UI::EVENT_BUTTON_PRESS) TheDarkestTimeline_instance.OnLeftButtonPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) TheDarkestTimeline_instance.OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP && event.type == UI::EVENT_BUTTON_PRESS) TheDarkestTimeline_instance.OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) TheDarkestTimeline_instance.OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) TheDarkestTimeline_instance.OnDownButtonLongPress();
    }
}

void TheDarkestTimeline_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) TheDarkestTimeline_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) TheDarkestTimeline_instance.OnRightEncoderMove(event.value);
}

#endif
