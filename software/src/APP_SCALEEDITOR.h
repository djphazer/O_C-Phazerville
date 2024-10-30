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

#include <Arduino.h>
#include "OC_apps.h"
#include "OC_ui.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"
#include "HSApplication.h"
#include "HSMIDI.h"
#include "SegmentDisplay.h"

class ScaleEditor : public HSApplication, public SystemExclusiveHandler {
public:
	void Start() {
	    current_scale = 0;
	    current_note = 0;
        current_import_scale = 5;
        undo_value = OC::user_scales[current_scale].notes[0];
        octave = 1;
        segment.Init(SegmentSize::BIG_SEGMENTS);
        tinynumbers.Init(SegmentSize::TINY_SEGMENTS);
	}

	void Resume() {
        HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
        QuantizeCurrent();
	}

    void Controller() {
        ListenForSysEx();

        // Scale monitor
        int32_t pitch = In(0);
        int32_t quantized = HS::quantizer[0].Process(pitch, 0, 0);
        Out(0, quantized);

        // Current note monitor
        Out(1, current_quantized);
    }

    void View() {
        gfxHeader("Scale Editor    ");
        gfxPrint(OC::scale_names_short[current_scale]);
        if (import_mode) DrawImportScreen();
        else DrawInterface();
    }

    /* Send SysEx on app suspend and when the left encoder is pressed */
    void OnSendSysEx() { // Left Enc Push
        uint8_t V[35];
        int ix = 0;

        // Encode span
        uint16_t span = static_cast<uint16_t>(OC::user_scales[current_scale].span);
        V[ix++] = span & 0xff; // Low byte
        V[ix++] = (span >> 8) & 0xff; // High byte

        // Encode length
        V[ix++] = static_cast<uint8_t>(OC::user_scales[current_scale].num_notes);

        // Encode values
        for (int i = 0; i < 16; i++)
        {
            uint16_t note = static_cast<uint16_t>(OC::user_scales[current_scale].notes[i]);
            V[ix++] = note & 0xff; // Low
            V[ix++] = (note >> 8) & 0xff; // High
        }

        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'E');
    }

    /* Send SysEx on app suspend and when the left encoder is pressed */
    void OnReceiveSysEx() {
        uint8_t V[35];
        if (ExtractSysExData(V, 'E')) {
            int ix = 0;

            // Decode span
            uint8_t low = V[ix++];
            uint8_t high = V[ix++];
            OC::user_scales[current_scale].span = (int16_t)(high << 8) | low;

            // Decode length
            uint8_t num_notes = V[ix++];
            OC::user_scales[current_scale].num_notes = constrain(num_notes, 4, 16);

            // Decode values
            for (int i = 0; i < 16; i++)
            {
                uint8_t low = V[ix++];
                uint8_t high = V[ix++];
                OC::user_scales[current_scale].notes[i] = (uint16_t)(high << 8) | low;
            }

            // Reset
            current_note = 0;
            undo_value = OC::user_scales[current_scale].notes[current_note];
            // Configure and force requantize for real-time monitoring purposes
            HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
            QuantizeCurrent();
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        if (import_mode) import_mode = 0;
        else Undo();
    }

    void OnLeftButtonLongPress() {
        if (import_mode) ToggleImportMode();
        else OnSendSysEx();
    }

    void OnRightButtonPress() {
        if (import_mode) ImportScale();
        else ToggleLengthSet();
    }

    void OnUpButtonPress() {
        SwitchScale(1);
    }

    void OnDownButtonPress() {
        SwitchScale(-1);
    }

    void OnDownButtonLongPress() {
        ToggleImportMode();
    }

    void OnLeftEncoderMove(int direction) {
        if (!import_mode && !length_set_mode) ChangeValue(direction);
    }

    void OnRightEncoderMove(int direction) {
        if (import_mode) ChangeImport(direction);
        else if (length_set_mode) ChangeLength(direction);
        else ChangeNote(direction);
    }


private:
    int8_t cursor;
    int8_t current_scale;
    int8_t current_note;
    int8_t current_import_scale;
    int undo_value;
    bool length_set_mode;
    bool import_mode;
    int current_quantized;
    int octave;
    SegmentDisplay segment;
    SegmentDisplay tinynumbers;

    void DrawInterface() {
        // The interface is a spreadsheet-like 4x4 grid, with each
        // cell being 32x9 pixels. At the bottom of grid is an editing
        // area with larger number with the current value, or the
        // scale size. First draw the grid:
        for (uint8_t cx = 0; cx < 4; cx++)
        {
            for (uint8_t cy = 0; cy < 4; cy++)
            {
                uint8_t ix = (cy * 4) + cx; // index within the scale
                uint8_t x = cx * 32;
                uint8_t y = cy * 9 + 15;
                if (ix < OC::user_scales[current_scale].num_notes) {

                    uint32_t note_value = OC::user_scales[current_scale].notes[ix];
                    uint32_t cents = (note_value * 100) >> 7;
                    uint32_t frac_cents = ((note_value * 100000) >> 7) - cents * 1000;

                    tinynumbers.PrintWhole(x + 2, y + 2, cents);
                    tinynumbers.PrintDecimal(frac_cents, 1, 1000);

                    // If this is the current note, highlight it
                    if (ix == current_note && !length_set_mode) gfxInvert(x, y, 32, 9);
                    if (length_set_mode && ix == OC::user_scales[current_scale].num_notes - 1)
                        gfxCursor(x, y + 9, 32);
                }
            }
        }

        // Now the editing area:
        if (length_set_mode) {
            segment.PrintWhole(22, 52, OC::user_scales[current_scale].num_notes);
        } else {
            // Show the value
            uint32_t note_value = OC::user_scales[current_scale].notes[current_note];
            uint32_t cents = (note_value * 100) >> 7;
            uint32_t frac_cents = ((note_value * 100000) >> 7) - cents * 1000;

            segment.PrintWhole(12, 52, cents);
            segment.PrintDecimal(frac_cents, 3, 1000);
        }
    }

    void DrawImportScreen() {
        gfxPrint(0, 15, "Import");
        gfxPrint(0, 35, OC::scale_names[current_import_scale]);
        gfxCursor(0, 43, 127);

        gfxPrint(0, 55, "[CANCEL]");
        gfxPrint(78, 55, "[IMPORT]");
    }

    /////////////////////////////////////////////////////////////////
    // Control handler activities
    /////////////////////////////////////////////////////////////////
    void SwitchScale(int direction) { // Up/Down buttons
        current_scale = constrain(current_scale + direction, 0, OC::Scales::SCALE_USER_LAST - 1);

        // Configure and force requantize for real-time monitoring purposes
        HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
        QuantizeCurrent();

        uint8_t length = static_cast<uint8_t>(OC::user_scales[current_scale].num_notes);
        if (current_note > length - 1) current_note = OC::user_scales[current_scale].num_notes - 1;
        undo_value = OC::user_scales[current_scale].notes[current_note];
    }

    /* Switch length set mode on and off when the right encoder is pressed */
    void ToggleLengthSet() { // Right Push
        length_set_mode = 1 - length_set_mode;
        ResetCursor();
    }

    void ToggleImportMode() {
        import_mode = 1 - import_mode;
    }

    void ChangeNote(int direction) {
        int length = OC::user_scales[current_scale].num_notes;
        current_note += direction;
        if (current_note < 0) {
            if (octave > -1) current_note = length - 1;
            else current_note += 1;
            octave = constrain(octave - 1, -1, 4);
        }
        if (current_note == length) {
            if (octave < 4) current_note = 0;
            else current_note -= 1;
            octave = constrain(octave + 1, -1, 4);
        }
        undo_value = OC::user_scales[current_scale].notes[current_note];
        QuantizeCurrent();
    }

    void ChangeImport(int direction) {
        current_import_scale += direction;
        if (current_import_scale == OC::Scales::NUM_SCALES) current_import_scale = 0;
        if (current_import_scale < 0) current_import_scale = OC::Scales::NUM_SCALES - 1;
    }

    void ChangeLength(int direction) {
        int length = OC::user_scales[current_scale].num_notes;
        if (direction > 0 && OC::user_scales[current_scale].notes[length - 1] + 1 >= (12 <<7)) {
            // Cannot add a new note because the current top can't be exceeded
            return;
        }

        length = constrain(length + direction, 4, 16);
        OC::user_scales[current_scale].num_notes = length;
        if (current_note > (length - 1)) current_note = length - 1;

        // Make sure added notes follow the range rules
        if (direction > 0 && OC::user_scales[current_scale].notes[length - 1] < OC::user_scales[current_scale].notes[length - 2]) {
            OC::user_scales[current_scale].notes[length - 1] = OC::user_scales[current_scale].notes[length - 2] + 1;
        }

        // Configure and force requantize for real-time monitoring purposes
        HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
        QuantizeCurrent();

        ResetCursor();
    }

    void ChangeValue(int direction) {
        int16_t new_value = OC::user_scales[current_scale].notes[current_note] + direction;

        // Is this constraint necessary? What if you want the scale to go in reverse, or want it to be
        // otherwise out-of-order? Heck with this, I say, let the user decide.
        //
        // But, yeah, this is necessary, it turns out. The note stops working right if the one above it is
        // at a higher pitch. I haven't looked into why, but it's probably in the Braids quantizer code somewhere
        int length = OC::user_scales[current_scale].num_notes;
        const int32_t min = current_note > 0 ? OC::user_scales[current_scale].notes[current_note - 1] : 0;
        const int32_t max = current_note < length - 1 ? OC::user_scales[current_scale].notes[current_note + 1] : OC::user_scales[current_scale].span;
        if (new_value <= min) new_value = current_note > 0 ? min + 1 : 0;
        if (new_value >= max) new_value = max - 1;
        OC::user_scales[current_scale].notes[current_note] = new_value;

        // Configure and force requantize for real-time monitoring purposes
        HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
        QuantizeCurrent();
    }

    void ImportScale() {
        OC::Scale source = OC::Scales::GetScale(current_import_scale);
        memcpy(&OC::user_scales[current_scale], &source, sizeof(source));
        HS::quantizer[0].Configure(OC::Scales::GetScale(current_scale), 0xffff);
        QuantizeCurrent();
        import_mode = 0;
        undo_value = OC::user_scales[current_scale].notes[current_note];
    }

    void Undo() {
        OC::user_scales[current_scale].notes[current_note] = static_cast<int16_t>(undo_value);
        QuantizeCurrent();
    }

    void QuantizeCurrent() {
        int transpose = OC::user_scales[current_scale].span * octave;
        HS::quantizer[0].Requantize();
        current_quantized = HS::quantizer[0].Process(OC::user_scales[current_scale].notes[current_note] + transpose, 0, 0);
        HS::quantizer[0].Requantize(); // This is for the next one in the Controller
    }
};

ScaleEditor scale_editor_instance;

// App stubs
void SCALEEDITOR_init() {
    scale_editor_instance.BaseStart();
}

// Not using O_C Storage
static constexpr size_t SCALEEDITOR_storageSize() {return 0;}
static size_t SCALEEDITOR_save(void *storage) {return 0;}
static size_t SCALEEDITOR_restore(const void *storage) {return 0;}

void SCALEEDITOR_isr() {
	return scale_editor_instance.BaseController();
}

void SCALEEDITOR_handleAppEvent(OC::AppEvent event) {
    if (event == OC::APP_EVENT_SUSPEND) {
        scale_editor_instance.OnSendSysEx();
    }
    if (event == OC::APP_EVENT_RESUME) {
        scale_editor_instance.Resume();
    }
}

void SCALEEDITOR_loop() {}

void SCALEEDITOR_menu() {
    scale_editor_instance.BaseView();
}

void SCALEEDITOR_screensaver() {}

void SCALEEDITOR_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_PRESS) scale_editor_instance.OnLeftButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) scale_editor_instance.OnLeftButtonLongPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) scale_editor_instance.OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP && event.type == UI::EVENT_BUTTON_PRESS) scale_editor_instance.OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) scale_editor_instance.OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) scale_editor_instance.OnDownButtonLongPress();
    }
}

void SCALEEDITOR_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) scale_editor_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) scale_editor_instance.OnRightEncoderMove(event.value);
}
