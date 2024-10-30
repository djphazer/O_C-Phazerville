// Copyright (c) 2023, Nicholas J. Michalek
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

/*
 * Based on a design spec from Chris Meyer / Alias Zone / Learning Modular
 */

#pragma once
#ifdef ENABLE_APP_CALIBR8OR

#include "HSApplication.h"
#include "HSMIDI.h"
#include "HSClockManager.h"
#include "util/util_settings.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"
#include "OC_autotuner.h"
#include "SegmentDisplay.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "HemisphereApplet.h"
#ifdef ARDUINO_TEENSY41
#include "applets/ClockSetupT4.h"
#else
#include "applets/ClockSetup.h"
#endif

static constexpr int CAL8_MAX_TRANSPOSE = 60;
static constexpr int CAL8OR_PRECISION = 10000;

static const int SCALE_SIZE(const int scale) {
  return OC::Scales::GetScale(scale).num_notes;
}
// channel configs
struct Cal8ChannelConfig {
    int last_note; // for S&H mode

    uint8_t clocked_mode;
    int8_t offset; // fine-tuning offset
    int16_t scale_factor; // precision of 0.01% as an offset from 100%
    int8_t transpose; // in semitones
    int8_t transpose_active; // held value while waiting for trigger

    DAC_CHANNEL chan_;
    DAC_CHANNEL get_channel() { return chan_; }
    void ExitAutotune() {
        FreqMeasure.end();
        OC::DigitalInputs::reInit();
    }
};

// Preset storage spec
enum Cal8Settings {
    CAL8_DATA_VALID, // 1 bit

    CAL8_SCALE_A, // 12 bits
    CAL8_SCALEFACTOR_A, // 10 bits
    CAL8_OFFSET_A, // 8 bits
    CAL8_TRANSPOSE_A, // 8 bits
    CAL8_ROOTKEY_AND_CLOCKMODE_A, // 4 + 2 bits
    CAL8_SCALEMASK_A, // 16 bits

    CAL8_SCALE_B,
    CAL8_SCALEFACTOR_B,
    CAL8_OFFSET_B,
    CAL8_TRANSPOSE_B,
    CAL8_ROOTKEY_AND_CLOCKMODE_B,
    CAL8_SCALEMASK_B,

    CAL8_SCALE_C,
    CAL8_SCALEFACTOR_C,
    CAL8_OFFSET_C,
    CAL8_TRANSPOSE_C,
    CAL8_ROOTKEY_AND_CLOCKMODE_C,
    CAL8_SCALEMASK_C,

    CAL8_SCALE_D,
    CAL8_SCALEFACTOR_D,
    CAL8_OFFSET_D,
    CAL8_TRANSPOSE_D,
    CAL8_ROOTKEY_AND_CLOCKMODE_D,
    CAL8_SCALEMASK_D,

#ifdef ARDUINO_TEENSY41
    CAL8_SCALE_E,
    CAL8_SCALEFACTOR_E,
    CAL8_OFFSET_E,
    CAL8_TRANSPOSE_E,
    CAL8_ROOTKEY_AND_CLOCKMODE_E,
    CAL8_SCALEMASK_E,

    CAL8_SCALE_F,
    CAL8_SCALEFACTOR_F,
    CAL8_OFFSET_F,
    CAL8_TRANSPOSE_F,
    CAL8_ROOTKEY_AND_CLOCKMODE_F,
    CAL8_SCALEMASK_F,

    CAL8_SCALE_G,
    CAL8_SCALEFACTOR_G,
    CAL8_OFFSET_G,
    CAL8_TRANSPOSE_G,
    CAL8_ROOTKEY_AND_CLOCKMODE_G,
    CAL8_SCALEMASK_G,

    CAL8_SCALE_H,
    CAL8_SCALEFACTOR_H,
    CAL8_OFFSET_H,
    CAL8_TRANSPOSE_H,
    CAL8_ROOTKEY_AND_CLOCKMODE_H,
    CAL8_SCALEMASK_H,
#endif

    CAL8_SETTING_LAST
};
enum Cal8Presets {
    CAL8_PRESET_A,
    CAL8_PRESET_B,
    CAL8_PRESET_C,
    CAL8_PRESET_D,

    NR_OF_PRESETS
};

enum Cal8ClockMode {
    CONTINUOUS,
    TRIG_TRANS,
    SAMPLE_AND_HOLD,

    NR_OF_CLOCKMODES
};

class Calibr8orPreset : public settings::SettingsBase<Calibr8orPreset, CAL8_SETTING_LAST> {
public:
    bool is_valid() {
        return values_[CAL8_DATA_VALID];
    }
    bool load_preset(Cal8ChannelConfig *channel) {
        if (!is_valid()) return false; // don't try to load a blank

        int ix = 1; // skip validity flag

        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            HS::QuantizerConfigure(ch, values_[ix++]);
            const int ssize_ = SCALE_SIZE(HS::GetScale(ch));

            channel[ch].scale_factor = values_[ix++] - 500;
            channel[ch].offset = values_[ix++] - 63;
            channel[ch].transpose = values_[ix++] - CAL8_MAX_TRANSPOSE;
            const int overflow = channel[ch].transpose / ssize_;
            if (overflow != 0) {
              HS::q_octave[ch] = constrain(overflow, -5, 5);
              channel[ch].transpose %= ssize_;
            }

            uint32_t root_and_mode = uint32_t(values_[ix++]);
            channel[ch].clocked_mode = ((root_and_mode >> 4) & 0x03) % NR_OF_CLOCKMODES;
            HS::SetRootNote(ch, int(root_and_mode & 0x0f) );

            HS::q_mask[ch] = values_[ix++];
        }

        return true;
    }
    void save_preset(Cal8ChannelConfig *channel) {
        int ix = 0;

        values_[ix++] = 1; // validity flag

        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
          const int scale = HS::GetScale(ch);
            values_[ix++] = scale;
            values_[ix++] = channel[ch].scale_factor + 500;
            values_[ix++] = channel[ch].offset + 63;
            values_[ix++] = channel[ch].transpose + HS::q_octave[ch] * SCALE_SIZE(scale) + CAL8_MAX_TRANSPOSE;
            values_[ix++] = ((channel[ch].clocked_mode & 0x03) << 4) | (HS::GetRootNote(ch) & 0x0f);
            values_[ix++] = HS::q_mask[ch];
        }
    }

};

Calibr8orPreset cal8_presets[NR_OF_PRESETS];

class Calibr8or : public HSApplication {
public:
    Calibr8or() {
        for (int i = DAC_CHANNEL_A; i < DAC_CHANNEL_LAST; ++i) {
            channel[i].chan_ = DAC_CHANNEL(i);
        }
    }

  OC::Autotuner<Cal8ChannelConfig> autotuner;


    void Start() {
        segment.Init(SegmentSize::BIG_SEGMENTS);

        // make sure to turn this off, just in case
        FreqMeasure.end();
        OC::DigitalInputs::reInit();

        // This initializes the global HS Quantizers
        ClearPreset();

        autotuner.Init();
    }
	
    void ClearPreset() {
        for (int ch = 0; ch < QUANT_CHANNEL_COUNT; ++ch) {
            HS::quantizer[ch].Init();
#ifdef ARDUINO_TEENSY41
            HS::QuantizerConfigure(ch, OC::Scales::SCALE_SEMI, 0xffff);
#else
            // Q1..Q4 default to Semitones
            // Q5..Q8 get initialized as USR1..USR4
            HS::QuantizerConfigure(ch, (ch<4) ? OC::Scales::SCALE_SEMI : ch - 4, 0xffff);
#endif
        }

        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            channel[ch].scale_factor = 0;
            channel[ch].offset = 0;
            channel[ch].transpose = 0;
            channel[ch].clocked_mode = 0;
            channel[ch].last_note = 0;
        }
    }
    void LoadPreset() {
        bool success = cal8_presets[index].load_preset(channel);
        if (success) {
            Resume();
            preset_modified = 0;
        }
        else
            ClearPreset();
    }
    void SavePreset() {
        cal8_presets[index].save_preset(channel);
        preset_modified = 0;

        // initiate actual EEPROM save
        OC::CORE::app_isr_enabled = false;
        OC::draw_save_message(16);
        delay(1);
        OC::draw_save_message(32);
        OC::save_app_data();
        OC::draw_save_message(64);

        const uint32_t timeout = 100;
        uint32_t start = millis();
        while(millis() < start + timeout) {
          GRAPHICS_BEGIN_FRAME(true);
          graphics.setPrintPos(13, 18);
          graphics.print("Settings saved");
          graphics.setPrintPos(31, 27);
          graphics.print("to EEPROM!");
          GRAPHICS_END_FRAME();
        }

        OC::CORE::app_isr_enabled = true;
    }

    void Resume() {
        // restore quantizer settings
        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            HS::quantizer[ch].Requantize();
        }
    }

    void ProcessMIDI() {
        HS::IOFrame &f = HS::frame;
        bool dothething = false;

        while (usbMIDI.read()) {
            const int message = usbMIDI.getType();
            const int data1 = usbMIDI.getData1();
            const int data2 = usbMIDI.getData2();

            if (message == usbMIDI.SystemExclusive) {
                // TODO: consider implementing SysEx import/export for Calibr8or
                continue;
            }

            f.MIDIState.ProcessMIDIMsg(usbMIDI.getChannel(), message, data1, data2);

            if (message == usbMIDI.NoteOn || message == usbMIDI.NoteOff) {
              dothething = true;
            }
        }

#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
        thisUSB.Task();
        while (usbHostMIDI.read()) {
            const int message = usbHostMIDI.getType();
            const int data1 = usbHostMIDI.getData1();
            const int data2 = usbHostMIDI.getData2();

            if (message == usbMIDI.SystemExclusive) {
                // TODO: consider implementing SysEx import/export for Calibr8or
                continue;
            }

            f.MIDIState.ProcessMIDIMsg(usbHostMIDI.getChannel(), message, data1, data2);

            if (message == usbMIDI.NoteOn || message == usbMIDI.NoteOff) {
              dothething = true;
            }
        }
        while (MIDI1.read()) {
            const int message = MIDI1.getType();
            const int data1 = MIDI1.getData1();
            const int data2 = MIDI1.getData2();

            if (message == usbMIDI.SystemExclusive) {
                // TODO: consider implementing SysEx import/export for Calibr8or
                continue;
            }

            f.MIDIState.ProcessMIDIMsg(MIDI1.getChannel(), message, data1, data2);

            if (message == usbMIDI.NoteOn || message == usbMIDI.NoteOff) {
              dothething = true;
            }
        }
#endif

        if (dothething) {
          // reconfigure with MIDI-derived masks
          for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            uint16_t mask_ = HS::frame.MIDIState.semitone_mask[ch];

            if (mask_) // manually override global config
              HS::quantizer[ch].Configure(OC::Scales::GetScale(OC::Scales::SCALE_SEMI), mask_);
            else // restore global config
              HS::quantizer[ch].Configure(OC::Scales::GetScale(HS::quant_scale[ch]), 0xffff);

            HS::quantizer[ch].Requantize();
          }
        }
    }

    void Controller() {
        ProcessMIDI();

        // ClockSetup applet handles internal clock duties
        ClockSetup_instance.Controller();

        // -- core processing --
        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            bool clocked = Clock(ch);
            Cal8ChannelConfig &cfg = channel[ch];

            // clocked transpose
            if (CONTINUOUS == cfg.clocked_mode || clocked) {
                cfg.transpose_active = cfg.transpose;
            }
            if (HS::frame.MIDIState.semitone_mask[ch] != 0)
              cfg.transpose_active = 0;

            // respect S&H mode
            if (cfg.clocked_mode != SAMPLE_AND_HOLD || clocked) {
                // CV value
                int pitch = In(ch);
                int quantized = HS::Quantize(ch, pitch, 0, cfg.transpose_active);
                cfg.last_note = quantized;
            }

            int output_cv = cfg.last_note;
            if ( OC::DAC::calibration_data_used( DAC_CHANNEL(sel_chan) ) != 0x01 ) // not autotuned
                output_cv = output_cv * (CAL8OR_PRECISION + cfg.scale_factor) / CAL8OR_PRECISION;
            output_cv += cfg.offset;

            Out(ch, output_cv);

            // for UI flashers
            if (clocked) trigger_flash[ch] = HEMISPHERE_PULSE_ANIMATION_TIME;
            else if (trigger_flash[ch]) --trigger_flash[ch];
        }
    }

    void View() {
        if (clock_setup) {
            ClockSetup_instance.View();
            return;
        }

        if (autotuner.active()) {
            autotuner.Draw();
            return;
        }

        gfxHeader("Calibr8or");

        // Metronome icon
        if (HS::clock_m.IsRunning()) {
            gfxIcon(56, 1, HS::clock_m.Cycle() ? METRO_L_ICON : METRO_R_ICON);
        } else if (HS::clock_m.IsPaused()) {
            gfxIcon(56, 1, PAUSE_ICON);
        }

        if (preset_select) {
            gfxPrint(70, 1, "- Presets");
            DrawPresetSelector();
        } else {
            gfxPos(110, 1);
            if (preset_modified) gfxPrint("*");
            if (cal8_presets[index].is_valid()) gfxPrint(OC::Strings::capital_letters[index]);

            DrawInterface();
        }

        if (HS::q_edit)
          PokePopup(QUANTIZER_POPUP);

        // Overlay popup window last
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS) {
          HS::DrawPopup();
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        // handled on button down
        if (clock_setup) return;

        // Toggle between Transpose mode and Tracking Compensation
        // also doubles as Load or Save for preset select
        edit_mode = !edit_mode;

        // prevent saving to the (clear) slot
        if (edit_mode && preset_select == 5) preset_select = 4;
    }

    void OnLeftButtonLongPress() {
        if (preset_select) return;

        if (edit_mode) {
            FreqMeasure.begin();
            autotuner.Open(&channel[sel_chan]);
            return;
        }

        // Toggle triggered transpose mode
        ++channel[sel_chan].clocked_mode %= NR_OF_CLOCKMODES;
        preset_modified = 1;
    }

    void OnRightButtonPress() {
        // handled on button down
        if (clock_setup) return;

        if (preset_select) {
            // special case to clear values
            if (!edit_mode && preset_select == NR_OF_PRESETS + 1) {
                ClearPreset();
                preset_modified = 1;
            }
            else {
                index = preset_select - 1;
                if (edit_mode) SavePreset();
                else LoadPreset();
            }

            preset_select = 0;
            return;
        }

        // Scale selection
        HS::qview = sel_chan;
        HS::q_edit = 1;
    }

    void OnButtonDown(const UI::Event &event) {
        // check for clock setup secret combo (dual press)
        const bool dual = (event.mask == (OC::CONTROL_BUTTON_UP | OC::CONTROL_BUTTON_DOWN));
        const bool hemisphere = (event.control == OC::CONTROL_BUTTON_UP) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        switch (event.control) {
          case OC::CONTROL_BUTTON_DOWN:
          case OC::CONTROL_BUTTON_UP:
            if ( dual ) {
              clock_setup = 1;
              click_tick = 0;
            } else {
              first_click = hemisphere;
              click_tick = OC::CORE::ticks;
            }
            break;
          case OC::CONTROL_BUTTON_L:
          case OC::CONTROL_BUTTON_R:
            if (clock_setup) // pass button down to Clock Setup
              ClockSetup_instance.OnButtonPress();
            break;
        }
    }

    // fires on button release
    void SwitchChannel(bool up) {
        if (!clock_setup && !preset_select) {
            sel_chan += (up? -1 : 1) + DAC_CHANNEL_LAST;
            sel_chan %= DAC_CHANNEL_LAST;
        }

        if (click_tick) {
            // always cancel clock setup and preset select on single click
            clock_setup = 0;
            preset_select = 0;
        }
    }

    void OnDownButtonLongPress() {
        // show preset screen, select last loaded
        preset_select = 1 + index;
    }

    // Left encoder: Octave or VScaling + Scale Select
    void OnLeftEncoderMove(int direction) {
        if (clock_setup) {
            ClockSetup_instance.OnEncoderMove(direction);
            return;
        }
        if (preset_select) {
            edit_mode = (direction>0);
            // prevent saving to the (clear) slot
            if (edit_mode && preset_select == 5) preset_select = 4;
            return;
        }

        preset_modified = 1;
        if (HS::q_edit) {
            // Scale Select
            HS::NudgeScale(sel_chan, direction);
            HS::quantizer[sel_chan].Requantize();
            return;
        }

        if (!edit_mode) { // Octave jump
          HS::q_octave[sel_chan] += direction;
          CONSTRAIN(HS::q_octave[sel_chan], -5, 5);
        }
        else if ( OC::DAC::calibration_data_used( DAC_CHANNEL(sel_chan) ) != 0x01 ) // not autotuned
        {
            // Tracking compensation
            channel[sel_chan].scale_factor = constrain(channel[sel_chan].scale_factor + direction, -500, 500);
        }
    }

    // Right encoder: Semitones or Bias Offset + Root Note
    void OnRightEncoderMove(int direction) {
        if (clock_setup) {
            ClockSetup_instance.OnEncoderMove(direction);
            return;
        }
        if (preset_select) {
            preset_select = constrain(preset_select + direction, 1, NR_OF_PRESETS + (1-edit_mode));
            return;
        }

        preset_modified = 1;
        if (HS::q_edit) {
            // Root Note
            HS::SetRootNote(sel_chan, HS::GetRootNote(sel_chan) + direction);
            HS::quantizer[sel_chan].Requantize();
            return;
        }

        if (!edit_mode) {
            SetTranspose(sel_chan, channel[sel_chan].transpose + direction);
        }
        else {
            channel[sel_chan].offset = constrain(channel[sel_chan].offset + direction, -63, 64);
        }
    }

    void SetTranspose(const int chan, int val) {
      const int ssize_ = SCALE_SIZE(HS::GetScale(chan));
      CONSTRAIN(val, -CAL8_MAX_TRANSPOSE, CAL8_MAX_TRANSPOSE);
      const int overflow = val / ssize_;
      if (overflow != 0) {
        HS::q_octave[chan] += overflow;
        CONSTRAIN(HS::q_octave[chan], -5, 5);
        val %= ssize_;
      }
      channel[chan].transpose = val;
    }

    int index = 0;

    int sel_chan = 0;
    bool edit_mode = 0;
    int preset_select = 0; // both a flag and an index
    bool preset_modified = 0;

    uint32_t click_tick = 0;
    bool first_click = 0;
    bool clock_setup = 0;

    int trigger_flash[DAC_CHANNEL_LAST];

    SegmentDisplay segment;
    Cal8ChannelConfig channel[DAC_CHANNEL_LAST];

    void DrawPresetSelector() {
        // index is the currently loaded preset (0-3)
        // preset_select is current selection (1-4, 5=clear)
        int y = 5 + 10*preset_select;
        gfxPrint(25, y, edit_mode ? "Save" : "Load");
        gfxIcon(50, y, RIGHT_ICON);

        for (int i = 0; i < NR_OF_PRESETS; ++i) {
            gfxPrint(60, 15 + i*10, OC::Strings::capital_letters[i]);
            if (!cal8_presets[i].is_valid())
                gfxPrint(" (empty)");
            else if (i == index)
                gfxPrint(" *");
        }
        if (!edit_mode)
            gfxPrint(60, 55, "[CLEAR]");
    }

    void DrawTabs() {
        // Draw channel tabs
        const size_t w = 128 / DAC_CHANNEL_LAST;
        for (int i = 0; i < DAC_CHANNEL_LAST; ++i) {
          const size_t x = i * w;
            gfxLine(x, 12, x, 22); // vertical line on left

            const size_t center_x = x + w/2 - 3;
            switch (channel[i].clocked_mode) {
              case CONTINUOUS:
                gfxPrint(center_x, 14, i+1);
                break;
              case TRIG_TRANS:
                gfxIcon(center_x, 14, CLOCK_ICON);
                break;
              case SAMPLE_AND_HOLD:
                gfxIcon(center_x, 14, STAIRS_ICON);
                break;
            }

            if (i == sel_chan)
                gfxInvert(1 + x, 12, w - 1, 11);
        }
        gfxLine(127, 12, 127, 22); // vertical line
        gfxLine(0, 23, 127, 23);
    }
    void DrawInterface() {
        DrawTabs();

        // Draw parameters for selected channel
        int y = 32;

        // Transpose
        gfxIcon(9, y, BEND_ICON);

        // -- LCD Display Section --
        const int s = SCALE_SIZE(HS::GetScale(sel_chan));
        int degrees = channel[sel_chan].transpose + HS::q_octave[sel_chan] * s;
        const bool positive = degrees >= 0;
        const int octave = degrees / s;
        degrees %= s;

        gfxFrame(20, y-3, 64, 18);
        gfxIcon(23, y+2, positive? PLUS_ICON : MINUS_ICON);

        segment.PrintWhole(33, y, abs(octave), 10);
        gfxPrint(53, y+5, ".");
        segment.PrintWhole(61, y, abs(degrees), 10);

        // Scale
        gfxIcon(89, y, SCALE_ICON);
        gfxPrint(99, y, OC::scale_names_short[HS::GetScale(sel_chan)]);
        if (HS::q_edit) {
            gfxInvert(98, y-1, 29, 9);
            gfxIcon(100, y+10, RIGHT_ICON);
        }
        // Root Note
        gfxPrint(110, y+10, OC::Strings::note_names_unpadded[HS::GetRootNote(sel_chan)]);

        // Tracking Compensation
        y += 22;
        gfxIcon(9, y, ZAP_ICON);

        if ( OC::DAC::calibration_data_used( DAC_CHANNEL(sel_chan) ) == 0x01 ) {
            gfxPrint(20, y, "(auto) ");
        } else {
            int whole = (channel[sel_chan].scale_factor + CAL8OR_PRECISION) / 100;
            int decimal = (channel[sel_chan].scale_factor + CAL8OR_PRECISION) % 100;
            gfxPrint(20 + pad(100, whole), y, whole);
            gfxPrint(".");
            if (decimal < 10) gfxPrint("0");
            gfxPrint(decimal);
            gfxPrint("% ");
        }
        if (channel[sel_chan].offset >= 0) gfxPrint("+");
        gfxPrint(channel[sel_chan].offset);

        if (HS::frame.MIDIState.semitone_mask[sel_chan] != 0)
          gfxIcon(100, y, MIDI_ICON);

        // mode indicator
        if (!HS::q_edit)
            gfxIcon(0, 32 + edit_mode*22, RIGHT_ICON);
    }
};

// TOTAL EEPROM SIZE: 4 * 29 bytes
SETTINGS_DECLARE(Calibr8orPreset, CAL8_SETTING_LAST) {
    {0, 0, 1, "validity flag", NULL, settings::STORAGE_TYPE_U8},

    {0, 0, 65535, "Scale A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor A", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode A", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask A", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor B", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode B", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask B", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor C", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode C", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask C", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor D", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode D", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask D", NULL, settings::STORAGE_TYPE_U16},

#ifdef ARDUINO_TEENSY41
    {0, 0, 65535, "Scale E", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor E", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias E", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose E", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode E", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask E", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale F", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor F", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias F", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose F", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode F", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask F", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale G", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor G", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias G", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose G", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode G", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask G", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scale H", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Scaling Factor H", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 255, "Offset Bias H", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Transpose H", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Root Key + Mode H", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 0xffff, "Scale Mask H", NULL, settings::STORAGE_TYPE_U16},
#endif
};


Calibr8or Calibr8or_instance;

// App stubs
void Calibr8or_init() { Calibr8or_instance.BaseStart(); }

static constexpr size_t Calibr8or_storageSize() {
    return Calibr8orPreset::storageSize() * NR_OF_PRESETS;
}

static size_t Calibr8or_save(void *storage) {
    size_t used = 0;
    for (int i = 0; i < NR_OF_PRESETS; ++i) {
        used += cal8_presets[i].Save(static_cast<char*>(storage) + used);
    }
    return used;
}

static size_t Calibr8or_restore(const void *storage) {
    size_t used = 0;
    for (int i = 0; i < NR_OF_PRESETS; ++i) {
        used += cal8_presets[i].Restore(static_cast<const char*>(storage) + used);
    }
    Calibr8or_instance.LoadPreset();
    return used;
}

void Calibr8or_isr() {
    if (Calibr8or_instance.autotuner.active()) {
      Calibr8or_instance.autotuner.ISR();
      return;
    }
    Calibr8or_instance.BaseController();
}

void Calibr8or_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        Calibr8or_instance.Resume();
        break;

    // The idea is to auto-save when the screen times out...
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
        break;

    default: break;
    }
}

void Calibr8or_loop() {} // Deprecated

void Calibr8or_menu() { Calibr8or_instance.BaseView(); }

void Calibr8or_screensaver() {
    Calibr8or_instance.BaseScreensaver(true);
}

void Calibr8or_handleButtonEvent(const UI::Event &event) {
  if (Calibr8or_instance.autotuner.active()) {
    Calibr8or_instance.autotuner.HandleButtonEvent(event);
    return;
  }
  
    // For left encoder, handle press and long press
    // For right encoder, only handle press (long press is reserved)
    // For up button, handle only press (long press is reserved)
    // For down button, handle press and long press
    switch (event.type) {
    case UI::EVENT_BUTTON_DOWN:
        // Quantizer popup editor intercepts everything on-press
        if (HS::q_edit) {
          if (event.control == OC::CONTROL_BUTTON_UP)
            HS::NudgeOctave(HS::qview, 1);
          else if (event.control == OC::CONTROL_BUTTON_DOWN)
            HS::NudgeOctave(HS::qview, -1);
          else {
            HS::q_edit = false;
          }

          OC::ui.SetButtonIgnoreMask();
          break;
        }

        if (event.control == OC::CONTROL_BUTTON_M) {
            HS::ToggleClockRun();
            OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
            break;
        }
        Calibr8or_instance.OnButtonDown(event);
        break;

    case UI::EVENT_BUTTON_PRESS: {
        switch (event.control) {
        case OC::CONTROL_BUTTON_L:
            Calibr8or_instance.OnLeftButtonPress();
            break;
        case OC::CONTROL_BUTTON_R:
            Calibr8or_instance.OnRightButtonPress();
            break;
        case OC::CONTROL_BUTTON_DOWN:
        case OC::CONTROL_BUTTON_UP:
            Calibr8or_instance.SwitchChannel(event.control == OC::CONTROL_BUTTON_UP);
            break;
        default: break;
        }
    } break;
    case UI::EVENT_BUTTON_LONG_PRESS:
        if (event.control == OC::CONTROL_BUTTON_L) {
            Calibr8or_instance.OnLeftButtonLongPress();
        }
        if (event.control == OC::CONTROL_BUTTON_DOWN) {
            Calibr8or_instance.OnDownButtonLongPress();
        }
        break;

    default: break;
    }
}

void Calibr8or_handleEncoderEvent(const UI::Event &event) {
  if (Calibr8or_instance.autotuner.active()) {
    Calibr8or_instance.autotuner.HandleEncoderEvent(event);
    return;
  }

    // Q-editor popup takes precedence
    if (HS::q_edit) {
      HS::QEditEncoderMove(event.control == OC::CONTROL_ENCODER_R, event.value);
      return;
    }

    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) Calibr8or_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) Calibr8or_instance.OnRightEncoderMove(event.value);
}

#endif // ENABLE_APP_CALIBR8OR
