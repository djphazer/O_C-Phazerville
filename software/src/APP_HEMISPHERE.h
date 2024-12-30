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

#pragma once
#ifndef _HEM_APP_HEMISPHERE_H_
#define _HEM_APP_HEMISPHERE_H_

#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_visualfx.h"
#include "OC_apps.h"
#include "OC_ui.h"

#include "OC_patterns.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

#include "HemisphereApplet.h"
#include "HSApplication.h"
#include "HSicons.h"
#include "HSMIDI.h"
#include "HSClockManager.h"

#ifdef ARDUINO_TEENSY41
#include "AudioSetup.h"
#endif

#include "hemisphere_config.h"

#ifdef ENABLE_APP_CALIBR8OR
// We depend on Calibr8or to save quantizer settings
#include "APP_CALIBR8OR.h"
#endif

// The settings specify the selected applets, and 64 bits of data for each applet,
// plus 64 bits of data for the ClockSetup applet (which includes some misc config).
// TRIGMAP and CVMAP are packed nibbles.
// This is the structure of a HemispherePreset in eeprom.
enum HEMISPHERE_SETTINGS {
    HEMISPHERE_SELECTED_LEFT_ID,
    HEMISPHERE_SELECTED_RIGHT_ID,
    HEMISPHERE_LEFT_DATA_B1,
    HEMISPHERE_LEFT_DATA_B2,
    HEMISPHERE_LEFT_DATA_B3,
    HEMISPHERE_LEFT_DATA_B4,
    HEMISPHERE_RIGHT_DATA_B1,
    HEMISPHERE_RIGHT_DATA_B2,
    HEMISPHERE_RIGHT_DATA_B3,
    HEMISPHERE_RIGHT_DATA_B4,
    HEMISPHERE_CLOCK_DATA1,
    HEMISPHERE_CLOCK_DATA2,
    HEMISPHERE_CLOCK_DATA3,
    HEMISPHERE_CLOCK_DATA4,
    HEMISPHERE_TRIGMAP,
    HEMISPHERE_CVMAP,
    HEMISPHERE_GLOBALS,
    HEMISPHERE_SETTING_LAST
};

#if defined(MOAR_PRESETS)
static constexpr int HEM_NR_OF_PRESETS = 16;
#elif defined(PEWPEWPEW)
static constexpr int HEM_NR_OF_PRESETS = 8;
#else
static constexpr int HEM_NR_OF_PRESETS = 8;
#endif

/* Hemisphere Preset
 * - conveniently store/recall multiple configurations
 */
class HemispherePreset : public SystemExclusiveHandler,
    public settings::SettingsBase<HemispherePreset, HEMISPHERE_SETTING_LAST> {
public:
    int GetAppletId(int h) {
        return (h == LEFT_HEMISPHERE) ? values_[HEMISPHERE_SELECTED_LEFT_ID]
                                      : values_[HEMISPHERE_SELECTED_RIGHT_ID];
    }
    HemisphereApplet* GetApplet(int h) {
      int idx = HS::get_applet_index_by_id( GetAppletId(h) );
      return HS::available_applets[idx].instance[h];
    }
    void SetAppletId(int h, int id) {
        apply_value(h, id);
    }
    bool is_valid() {
        return values_[HEMISPHERE_SELECTED_LEFT_ID] != 0;
    }

    uint64_t GetClockData() {
        return ( (uint64_t(values_[HEMISPHERE_CLOCK_DATA4]) << 48) |
                 (uint64_t(values_[HEMISPHERE_CLOCK_DATA3]) << 32) |
                 (uint64_t(values_[HEMISPHERE_CLOCK_DATA2]) << 16) |
                  uint64_t(values_[HEMISPHERE_CLOCK_DATA1]) );
    }
    void SetClockData(const uint64_t data) {
        apply_value(HEMISPHERE_CLOCK_DATA1, data & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA2, (data >> 16) & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA3, (data >> 32) & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA4, (data >> 48) & 0xffff);
    }

    // returns true if changed
    bool StoreInputMap() {
      uint16_t cvmap = 0;
      uint16_t trigmap = 0;
      for (size_t i = 0; i < 4; ++i) {
        trigmap |= (uint16_t(HS::trigger_mapping[i] + 1) & 0x0F) << (i*4);
        cvmap |= (uint16_t(HS::cvmapping[i] + 1) & 0x0F) << (i*4);
      }

      bool changed = (uint16_t(values_[HEMISPHERE_TRIGMAP]) != trigmap)
                   || (uint16_t(values_[HEMISPHERE_CVMAP]) != cvmap);
      apply_value(HEMISPHERE_TRIGMAP, trigmap);
      apply_value(HEMISPHERE_CVMAP, cvmap);

      return changed;
    }
    void LoadInputMap() {
      for (size_t i = 0; i < 4; ++i) {
        int val = (uint16_t(values_[HEMISPHERE_TRIGMAP]) >> (i*4)) & 0x0F;
        if (val != 0)
          HS::trigger_mapping[i] = constrain(val - 1, 0, TRIGMAP_MAX);

        val = (uint16_t(values_[HEMISPHERE_CVMAP]) >> (i*4)) & 0x0F;
        if (val != 0)
          HS::cvmapping[i] = constrain(val - 1, 0, CVMAP_MAX);
      }
    }

    uint64_t GetGlobals() {
      return ( uint64_t(values_[HEMISPHERE_GLOBALS]) & 0xffff );
    }
    void SetGlobals(const uint64_t &data) {
        apply_value(HEMISPHERE_GLOBALS, data & 0xffff);
    }

    // Manually get data for one side
    uint64_t GetData(const HEM_SIDE h) {
        return (uint64_t(values_[5 + h*4]) << 48) |
               (uint64_t(values_[4 + h*4]) << 32) |
               (uint64_t(values_[3 + h*4]) << 16) |
               (uint64_t(values_[2 + h*4]));
    }

    /* Manually store state data for one side */
    void SetData(const HEM_SIDE h, uint64_t &data) {
        apply_value(2 + h*4, data & 0xffff);
        apply_value(3 + h*4, (data >> 16) & 0xffff);
        apply_value(4 + h*4, (data >> 32) & 0xffff);
        apply_value(5 + h*4, (data >> 48) & 0xffff);
    }

    // TODO: I haven't updated the SysEx data structure here because I don't use it.
    // Clock data would probably be useful if it's not too big. -NJM
    void OnSendSysEx() {
        // Describe the data structure for the audience
        uint8_t V[18];
        V[0] = (uint8_t)values_[HEMISPHERE_SELECTED_LEFT_ID];
        V[1] = (uint8_t)values_[HEMISPHERE_SELECTED_RIGHT_ID];
        V[2] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B1] & 0xff);
        V[3] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B1] >> 8) & 0xff);
        V[4] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B1] & 0xff);
        V[5] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B1] >> 8) & 0xff);
        V[6] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B2] & 0xff);
        V[7] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B2] >> 8) & 0xff);
        V[8] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B2] & 0xff);
        V[9] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B2] >> 8) & 0xff);
        V[10] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B3] & 0xff);
        V[11] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B3] >> 8) & 0xff);
        V[12] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B3] & 0xff);
        V[13] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B3] >> 8) & 0xff);
        V[14] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B4] & 0xff);
        V[15] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B4] >> 8) & 0xff);
        V[16] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B4] & 0xff);
        V[17] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B4] >> 8) & 0xff);

        // Pack it up, ship it out
        UnpackedData unpacked;
        unpacked.set_data(18, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'H');
    }

    void OnReceiveSysEx() {
        uint8_t V[18];
        if (ExtractSysExData(V, 'H')) {
            values_[HEMISPHERE_SELECTED_LEFT_ID] = V[0];
            values_[HEMISPHERE_SELECTED_RIGHT_ID] = V[1];
            values_[HEMISPHERE_LEFT_DATA_B1] = ((uint16_t)V[3] << 8) + V[2];
            values_[HEMISPHERE_RIGHT_DATA_B1] = ((uint16_t)V[5] << 8) + V[4];
            values_[HEMISPHERE_LEFT_DATA_B2] = ((uint16_t)V[7] << 8) + V[6];
            values_[HEMISPHERE_RIGHT_DATA_B2] = ((uint16_t)V[9] << 8) + V[8];
            values_[HEMISPHERE_LEFT_DATA_B3] = ((uint16_t)V[11] << 8) + V[10];
            values_[HEMISPHERE_RIGHT_DATA_B3] = ((uint16_t)V[13] << 8) + V[12];
            values_[HEMISPHERE_LEFT_DATA_B4] = ((uint16_t)V[15] << 8) + V[14];
            values_[HEMISPHERE_RIGHT_DATA_B4] = ((uint16_t)V[17] << 8) + V[16];
        }
    }

};

// HemispherePreset hem_config; // special place for Clock data and Config data, 64 bits each

HemispherePreset hem_presets[HEM_NR_OF_PRESETS + 1];
HemispherePreset *hem_active_preset = 0;

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Manager
////////////////////////////////////////////////////////////////////////////////

using namespace HS;

void ReceiveManagerSysEx();
void BeatSyncProcess();

class HemisphereManager : public HSApplication {
public:
    void Start() {
        //select_mode = -1; // Not selecting

        help_hemisphere = -1;
        clock_setup = 0;

        showhide_cursor.Init(0, HEMISPHERE_AVAILABLE_APPLETS - 1);
        showhide_cursor.Scroll(0);

        for (int i = 0; i < 4; ++i) {
            quant_scale[i] = OC::Scales::SCALE_SEMI;
            quantizer[i].Init();
            quantizer[i].Configure(OC::Scales::GetScale(quant_scale[i]), 0xffff);
        }

        SetApplet(LEFT_HEMISPHERE, HS::get_applet_index_by_id(18)); // DualTM
        SetApplet(RIGHT_HEMISPHERE, HS::get_applet_index_by_id(15)); // EuclidX
    }

    void Resume() {
        if (!hem_active_preset)
            LoadFromPreset(0);
        // restore quantizer settings
        for (int i = 0; i < 4; ++i) {
            quantizer[i].Init();
            quantizer[i].Configure(OC::Scales::GetScale(quant_scale[i]), 0xffff);
        }
    }
    void Suspend() {
        if (hem_active_preset) {
            if (HS::auto_save_enabled || 0 == preset_id) StoreToPreset(preset_id, !HS::auto_save_enabled);
            hem_active_preset->OnSendSysEx();
        }
    }

    void StoreToPreset(HemispherePreset* preset, bool skip_eeprom = false) {
        bool doSave = (preset != hem_active_preset);

        hem_active_preset = preset;
        for (int h = 0; h < 2; h++)
        {
            int index = my_applet[h];
            if (hem_active_preset->GetAppletId(HEM_SIDE(h)) != HS::available_applets[index].id)
                doSave = 1;
            hem_active_preset->SetAppletId(HEM_SIDE(h), HS::available_applets[index].id);

            uint64_t data = HS::available_applets[index].instance[h]->OnDataRequest();
            if (data != applet_data[h]) doSave = 1;
            applet_data[h] = data;
            hem_active_preset->SetData(HEM_SIDE(h), data);
        }
        uint64_t data = ClockSetup_instance.OnDataRequest();
        if (data != clock_data) doSave = 1;
        clock_data = data;
        hem_active_preset->SetClockData(data);

        data = ClockSetup_instance.GetGlobals();
        if (data != global_data) doSave = 1;
        global_data = data;
        hem_active_preset->SetGlobals(data);

        if (hem_active_preset->StoreInputMap()) doSave = 1;

        if (doSave) {
        }

        // initiate actual EEPROM save - ONLY if necessary!
        if (doSave && !skip_eeprom) {
#ifdef ENABLE_APP_CALIBR8OR
          // call Calibr8or so it remembers quantizer settings
          // this also takes care of the EEPROM save
          Calibr8or_instance.SavePreset();
#else
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
#endif
        }

    }
    void StoreToPreset(int id, bool skip_eeprom = false) {
        StoreToPreset( (HemispherePreset*)(hem_presets + id), skip_eeprom );
        preset_id = id;
    }
    void LoadFromPreset(int id) {
        hem_active_preset = (HemispherePreset*)(hem_presets + id);
        if (hem_active_preset->is_valid()) {
            clock_data = hem_active_preset->GetClockData();
            ClockSetup_instance.OnDataReceive(clock_data);

            global_data = hem_active_preset->GetGlobals();
            ClockSetup_instance.SetGlobals(global_data);

            hem_active_preset->LoadInputMap();

            for (int h = 0; h < 2; h++)
            {
                int index = HS::get_applet_index_by_id( hem_active_preset->GetAppletId(h) );
                applet_data[h] = hem_active_preset->GetData(HEM_SIDE(h));
                SetApplet(HEM_SIDE(h), index);
                HS::available_applets[index].instance[h]->OnDataReceive(applet_data[h]);
            }


        }
        preset_id = id;
        PokePopup(PRESET_POPUP);
    }
    void ProcessQueue() {
      LoadFromPreset(queued_preset);
    }

    // does not modify the preset, only the manager
    void SetApplet(HEM_SIDE hemisphere, int index) {
        //if (my_applet[hemisphere]) // TODO: special case for first load?
        HS::available_applets[my_applet[hemisphere]].instance[hemisphere]->Unload();
        next_applet[hemisphere] = my_applet[hemisphere] = index;
        HS::available_applets[index].instance[hemisphere]->BaseStart(hemisphere);
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet[h], dir);
        next_applet[h] = index;
    }

    bool SelectModeEnabled() {
        return select_mode > -1;
    }

#if defined(__IMXRT1062__)
  #if defined(ARDUINO_TEENSY41)
    template <typename T1, typename T2, typename T3>
    void ProcessMIDI(T1 &device, T2 &next_device, T3 &dev3) {
  #else
    template <typename T1, typename T2>
    void ProcessMIDI(T1 &device, T2 &next_device) {
  #endif
#else
    template <typename T1>
    void ProcessMIDI(T1 &device) {
#endif
        HS::IOFrame &f = HS::frame;

        while (device.read()) {
            const int message = device.getType();
            const int data1 = device.getData1();
            const int data2 = device.getData2();

            if (message == usbMIDI.SystemExclusive) {
                ReceiveManagerSysEx();
                continue;
            }

            if (message == usbMIDI.ProgramChange) {
                int slot = device.getData1();
                if (slot < HEM_NR_OF_PRESETS) {
                  if (HS::clock_m.IsRunning()) {
                    queued_preset = slot;
                    HS::clock_m.BeatSync( &BeatSyncProcess );
                  }
                  else
                    LoadFromPreset(slot);
                }
                continue;
            }

            f.MIDIState.ProcessMIDIMsg(device.getChannel(), message, data1, data2);
#if defined(__IMXRT1062__)
            next_device.send(message, data1, data2, device.getChannel(), 0);
  #if defined(ARDUINO_TEENSY41)
            dev3.send((midi::MidiType)message, data1, data2, device.getChannel());
  #endif
#endif
        }
    }

    void Controller() {
        // top-level MIDI-to-CV handling - alters frame outputs
#if defined(__IMXRT1062__)
  #if defined(ARDUINO_TEENSY41)
        ProcessMIDI(usbMIDI, usbHostMIDI, MIDI1);
        thisUSB.Task();
        ProcessMIDI(usbHostMIDI, usbMIDI, MIDI1);
        ProcessMIDI(MIDI1, usbMIDI, usbHostMIDI);
  #else
        ProcessMIDI(usbMIDI, usbHostMIDI);
        thisUSB.Task();
        ProcessMIDI(usbHostMIDI, usbMIDI);
  #endif
#else
        ProcessMIDI(usbMIDI);
#endif

        // Clock Setup applet handles internal clock duties
        ClockSetup_instance.Controller();

        // execute Applets
        for (int h = 0; h < 2; h++)
        {
            if (my_applet[h] != next_applet[h]) {
              SetApplet(HEM_SIDE(h), next_applet[h]);
            }
            int index = my_applet[h];

            // MIDI signals mixed with inputs to applets
            if (HS::available_applets[index].id != 150) // not MIDI In
            {
                ForEachChannel(ch) {
                    int chan = h*2 + ch;
                    // mix CV inputs with applicable MIDI signals
                    switch (HS::frame.MIDIState.function[chan]) {
                    case HEM_MIDI_CC_OUT:
                    case HEM_MIDI_NOTE_OUT:
                    case HEM_MIDI_NOTE_POLY2_OUT:
                    case HEM_MIDI_NOTE_POLY3_OUT:
                    case HEM_MIDI_NOTE_POLY4_OUT:
                    case HEM_MIDI_NOTE_MIN_OUT:
                    case HEM_MIDI_NOTE_MAX_OUT:
                    case HEM_MIDI_NOTE_PEDAL_OUT:
                    case HEM_MIDI_NOTE_INV_OUT:
                    case HEM_MIDI_VEL_OUT:
                    case HEM_MIDI_VEL2_OUT:
                    case HEM_MIDI_VEL3_OUT:
                    case HEM_MIDI_VEL4_OUT:
                    case HEM_MIDI_AT_OUT:
                    case HEM_MIDI_PB_OUT:
                        HS::frame.inputs[chan] += HS::frame.MIDIState.outputs[chan];
                        break;
                    case HEM_MIDI_GATE_OUT:
                    case HEM_MIDI_GATE_INV_OUT:
                        HS::frame.gate_high[chan] |= (HS::frame.MIDIState.outputs[chan] > (12 << 7));
                        break;
                    case HEM_MIDI_TRIG_OUT:
                    case HEM_MIDI_TRIG_1ST_OUT:
                    case HEM_MIDI_TRIG_ALWAYS_OUT:
                    case HEM_MIDI_CLOCK_OUT:
                    case HEM_MIDI_START_OUT:
                        HS::frame.clocked[chan] |= HS::frame.MIDIState.trigout_q[chan];
                        HS::frame.MIDIState.trigout_q[chan] = 0;
                        break;
                    }
                }
            }
            if (HS::clock_m.auto_reset)
                HS::available_applets[index].instance[h]->Reset();

            HS::available_applets[index].instance[h]->BaseController();
        }
        HS::clock_m.auto_reset = false;

#ifdef ARDUINO_TEENSY41
        // auto-trigger outputs E..H
        for (int ch = 0; ch < 4; ++ch) {
          if (abs(HS::frame.output_diff[ch]) > HEMISPHERE_CHANGE_THRESHOLD)
            HS::frame.ClockOut(DAC_CHANNEL(ch + 4));
        }
#endif
    }

    void View() {
        bool draw_applets = true;

        if (preset_cursor) {
          DrawPresetSelector();
          draw_applets = false;
        }
        else if (view_state == CONFIG_MENU) {
          switch(config_page) {
          case LOADSAVE_POPUP:
            PokePopup(MENU_POPUP);
            // but still draw the applets
            // the popup will linger when moving onto the Config Dummy
            break;

          case INPUT_SETTINGS:
            DrawInputMappings();
            draw_applets = false;
            break;

          case QUANTIZER_SETTINGS:
            DrawQuantizerConfig();
            draw_applets = false;
            break;

          case CONFIG_SETTINGS:
            DrawConfigMenu();
            draw_applets = false;
            break;

          case SHOWHIDE_APPLETS:
            DrawAppletList();
            draw_applets = false;
            break;
          }

        }
#ifdef ARDUINO_TEENSY41
        if (view_state == AUDIO_SETUP) {
          gfxHeader("Audio DSP Setup");
          OC::AudioDSP::DrawAudioSetup();
          draw_applets = false;
        }
#endif

        if (HS::q_edit)
          PokePopup(QUANTIZER_POPUP);

        if (draw_applets) {
          if (help_hemisphere > -1) {
            int index = my_applet[help_hemisphere];
            HS::available_applets[index].instance[help_hemisphere]->BaseView(true);
            draw_applets = false;
          } else {
            for (int h = 0; h < 2; h++)
            {
                int index = my_applet[h];
                HS::available_applets[index].instance[h]->BaseView();
            }

            if (select_mode == LEFT_HEMISPHERE) graphics.drawFrame(0, 0, 64, 64);
            if (select_mode == RIGHT_HEMISPHERE) graphics.drawFrame(64, 0, 64, 64);

            // vertical separator
            graphics.drawLine(63, 0, 63, 63, 2);
          }

          // clock screen is an overlay
          if (clock_setup) {
            ClockSetup_instance.View();
          } else {
            ClockSetup_instance.DrawIndicator();
          }
        }

        // Overlay popup window last
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS * 2) {
          HS::DrawPopup(config_cursor, preset_id, CursorBlink());
        }
    }

    void DelegateEncoderPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (view_state == CONFIG_MENU) {
            // button release for config screen
            if (!down) ConfigButtonPush(h);
            return;
        }
#ifdef ARDUINO_TEENSY41
        if (view_state == AUDIO_SETUP) {
          if (!down) OC::AudioDSP::AudioSetupButtonAction(h);
          return;
        }
#endif

        // button down
        if (down) {
            // Clock Setup is more immediate for manual triggers
            if (clock_setup) ClockSetup_instance.OnButtonPress();
            // TODO: consider a new OnButtonDown handler for applets
            return;
        }

        // button release
        if (select_mode == h) {
            select_mode = -1; // Pushing a button for the selected side turns off select mode
        } else if (!clock_setup) {
            // regular applets get button release
            int index = my_applet[h];
            HS::available_applets[index].instance[h]->OnButtonPress();
        }
    }

#ifdef ARDUINO_TEENSY41
    void ExtraButtonPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        int h = (event.control == OC::CONTROL_BUTTON_UP2) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (down) {
          // dual press for Audio Setup
          if (event.mask == (OC::CONTROL_BUTTON_UP2 | OC::CONTROL_BUTTON_DOWN2) && h != first_click) {
              view_state = AUDIO_SETUP;
              OC::ui.SetButtonIgnoreMask(); // ignore button release
              return;
          }

          // mark this single click
          click_tick = OC::CORE::ticks;
          first_click = h;
          return;
        }

        // --- Button Release
        if (preset_cursor || view_state != APPLETS) {
            // cancel config screen, etc. on select button release
            preset_cursor = 0;
            view_state = APPLETS;
            HS::popup_tick = 0;
            return;
        }

        if (clock_setup) {
            clock_setup = 0; // Turn off clock setup with any single-click button release
            return;
        }

        if (event.control == OC::CONTROL_BUTTON_DOWN2)
            ToggleConfigMenu();

        if (event.control == OC::CONTROL_BUTTON_UP2)
            ShowPresetSelector();

    }
#endif

    void DelegateSelectButtonPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        const int hemisphere = (event.control == OC::CONTROL_BUTTON_A) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (!down && (preset_cursor || view_state != APPLETS)) {
            // cancel preset select, or config screen on select button release
            preset_cursor = 0;
            view_state = APPLETS;
            HS::popup_tick = 0;
            return;
        }

        if (clock_setup && !down) {
            clock_setup = 0; // Turn off clock setup with any single-click button release
            return;
        }

        // -- button down
        if (down) {
            // dual press for Clock Setup... check first_click, so we only process the 2nd button event
            if (event.mask == (OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B) && hemisphere != first_click) {
                clock_setup = 1;
                SetHelpScreen(-1);
                select_mode = -1;
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME) {
                // This is a double-click on one button. Activate corresponding help screen and deactivate select mode.
                if (hemisphere == first_click)
                    SetHelpScreen(hemisphere);

                // reset double-click timer either way
                click_tick = 0;
                return;
            }

            // -- Single click
            // If a help screen is already selected, and the button is for
            // the opposite one, go to the other help screen
            if (help_hemisphere > -1) {
                if (help_hemisphere != hemisphere) SetHelpScreen(hemisphere);
                else SetHelpScreen(-1); // Exit help screen if same button is clicked
                OC::ui.SetButtonIgnoreMask(); // ignore release
            }

            // mark this single click
            click_tick = OC::CORE::ticks;
            first_click = hemisphere;
            return;
        }

        // -- button release
        if (!clock_setup) {
          const int index = my_applet[hemisphere];
          HemisphereApplet* applet = HS::available_applets[index].instance[hemisphere];

          if (applet->EditMode()) {
            // select button becomes aux button while editing a param
            applet->AuxButton();
          } else {
            // Select Mode
            if (hemisphere == select_mode) select_mode = -1; // Exit Select Mode if same button is pressed
            else if (help_hemisphere < 0) // Otherwise, set Select Mode - UNLESS there's a help screen
              select_mode = hemisphere;
          }
        }
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (HS::q_edit) {
          HS::QEditEncoderMove(h, event.value);
          return;
        }

        if (view_state == CONFIG_MENU) {
          ConfigEncoderAction(h, event.value);
          return;
        }
#ifdef ARDUINO_TEENSY41
        if (view_state == AUDIO_SETUP) {
          OC::AudioDSP::AudioMenuAdjust(h, event.value);
          return;
        }
#endif

        if (clock_setup) {
          if (h == LEFT_HEMISPHERE)
            ClockSetup_instance.OnLeftEncoderMove(event.value);
          else
            ClockSetup_instance.OnEncoderMove(event.value);
        } else if (select_mode == h) {
            ChangeApplet(HEM_SIDE(h), event.value);
        } else {
            int index = my_applet[h];
            HS::available_applets[index].instance[h]->OnEncoderMove(event.value);
        }
    }

    void ToggleConfigMenu() {
      if (view_state != CONFIG_MENU) {
        view_state = CONFIG_MENU;
        //SetHelpScreen(-1);
      } else {
        view_state = APPLETS;
      }
    }
    void ShowPresetSelector() {
        view_state = CONFIG_MENU;
        config_cursor = LOAD_PRESET;
        preset_cursor = preset_id + 1;
    }

    void SetHelpScreen(int hemisphere) {
        help_hemisphere = hemisphere;
    }

    void HandleButtonEvent(const UI::Event &event) {
        switch (event.type) {
        case UI::EVENT_BUTTON_DOWN:
            if (event.control == OC::CONTROL_BUTTON_M) {
                ToggleClockRun();
                OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
                break;
            }
            if (HS::q_edit) {
              if (event.control == OC::CONTROL_BUTTON_A)
                HS::NudgeOctave(HS::qview, 1);
              else if (event.control == OC::CONTROL_BUTTON_B)
                HS::NudgeOctave(HS::qview, -1);
              else
                HS::q_edit = false;

              OC::ui.SetButtonIgnoreMask();
              break;
            }
            // most button-down events fall through here
        case UI::EVENT_BUTTON_PRESS:

            if (event.control == OC::CONTROL_BUTTON_A || event.control == OC::CONTROL_BUTTON_B) {
                DelegateSelectButtonPush(event);
            } else if (event.control == OC::CONTROL_BUTTON_L || event.control == OC::CONTROL_BUTTON_R) {
                DelegateEncoderPush(event);
            }
#ifdef ARDUINO_TEENSY41
            else // new buttons
                ExtraButtonPush(event);
#endif

            break;

        case UI::EVENT_BUTTON_LONG_PRESS:
            if (event.control == OC::CONTROL_BUTTON_B) ToggleConfigMenu();
            if (event.control == OC::CONTROL_BUTTON_L) ToggleClockRun();
            break;

        default: break;
        }
    }

private:
    int preset_id = 0;
    int queued_preset = 0;
    int preset_cursor = 0;
    int my_applet[2]; // Indexes to available_applets
    int next_applet[2]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[2]; // cache of applet data
    bool clock_setup;
    int config_cursor = 0;
    int config_page = 0;
    int dummy_count = 0;

    OC::menu::ScreenCursor<5> showhide_cursor;

    int select_mode = -1;
    int help_hemisphere; // Which of the hemispheres (if any) is in help mode, or -1 if none
    uint32_t click_tick; // Measure time between clicks for double-click
    int first_click; // The first button pushed of a double-click set, to see if the same one is pressed

    // State machine
    enum HEMView {
      APPLETS,
      APPLET_FULLSCREEN,
      CONFIG_MENU,
      PRESET_PICKER,
      CLOCK_SETUP,
#ifdef ARDUINO_TEENSY41
      AUDIO_SETUP,
#endif
    };
    HEMView view_state = APPLETS;

    enum HEMConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,
        AUTO_MIDI,

        // Global Quantizers: 4x(Scale, Root, Octave, Mask?)
        QUANT1, QUANT2, QUANT3, QUANT4,
        QUANT5, QUANT6, QUANT7, QUANT8,

        // Input Remapping
        TRIGMAP1, TRIGMAP2, TRIGMAP3, TRIGMAP4,
        CVMAP1, CVMAP2, CVMAP3, CVMAP4,

        // Applet visibility (dummy position)
        SHOWHIDELIST,

        MAX_CURSOR = CVMAP4
    };

    enum HEMConfigPage {
      LOADSAVE_POPUP,
      CONFIG_SETTINGS,
      QUANTIZER_SETTINGS,
      INPUT_SETTINGS,
      SHOWHIDE_APPLETS,

      LAST_PAGE = SHOWHIDE_APPLETS
    };

    void ConfigEncoderAction(const int h, const int dir) {
        if (!isEditing && !preset_cursor) {
          if (h == 0) { // change pages
            config_page += dir;
            config_page = constrain(config_page, 0, LAST_PAGE);

            const int cursorpos[] = { LOAD_PRESET, TRIG_LENGTH, QUANT1, TRIGMAP1, SHOWHIDELIST };
            config_cursor = cursorpos[config_page];
          } else if (config_page == SHOWHIDE_APPLETS) {
            showhide_cursor.Scroll(dir);
          } else { // move cursor
            config_cursor += dir;
            config_cursor = constrain(config_cursor, 0, MAX_CURSOR);

            if (config_cursor < CONFIG_DUMMY) config_page = LOADSAVE_POPUP;
            else if (config_cursor <= AUTO_MIDI) config_page = CONFIG_SETTINGS;
            else if (config_cursor < TRIGMAP1) config_page = QUANTIZER_SETTINGS;
            else if (config_cursor < SHOWHIDELIST) config_page = INPUT_SETTINGS;
            //else config_page = SHOWHIDE_APPLETS;

            ResetCursor();
          }
          return;
        }

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            HS::trigger_mapping[config_cursor-TRIGMAP1] = constrain(
                HS::trigger_mapping[config_cursor-TRIGMAP1] + dir,
                0, TRIGMAP_MAX);
            break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
            HS::cvmapping[config_cursor-CVMAP1] = constrain( HS::cvmapping[config_cursor-CVMAP1] + dir, 0, CVMAP_MAX);
            break;
        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        //case SCREENSAVER_MODE:
            // TODO?
            //break;
        case LOAD_PRESET:
        case SAVE_PRESET:
            if (h == 0) {
              config_cursor = constrain(config_cursor + dir, LOAD_PRESET, SAVE_PRESET);
            } else {
              preset_cursor = constrain(preset_cursor + dir, 1, HEM_NR_OF_PRESETS);
            }
            break;
        }
    }
    void ConfigButtonPush(int h) {
        if (preset_cursor) {
            // Save or Load on button push
            if (config_cursor == SAVE_PRESET)
                StoreToPreset(preset_cursor-1);
            else {
              if (HS::clock_m.IsRunning()) {
                queued_preset = preset_cursor - 1;
                HS::clock_m.BeatSync( &BeatSyncProcess );
              }
              else
                LoadFromPreset(preset_cursor-1);
            }

            preset_cursor = 0; // deactivate preset selection
            view_state = APPLETS;
            isEditing = false;
            return;
        }

        switch (config_cursor) {
        case CONFIG_DUMMY:
            ++dummy_count;
            break;

        case SAVE_PRESET:
        case LOAD_PRESET:
            preset_cursor = preset_id + 1;
            break;

        case AUTO_SAVE:
            HS::auto_save_enabled = !HS::auto_save_enabled;
            break;

        case QUANT1:
        case QUANT2:
        case QUANT3:
        case QUANT4:
        case QUANT5:
        case QUANT6:
        case QUANT7:
        case QUANT8:
            HS::QuantizerEdit(config_cursor - QUANT1);
            break;

        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
        case TRIG_LENGTH:
        default:
            isEditing = !isEditing;
            break;

        case SCREENSAVER_MODE:
            ++HS::screensaver_mode %= 4;
            break;

        case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
            break;

        case AUTO_MIDI:
            HS::frame.autoMIDIOut = !HS::frame.autoMIDIOut;
            break;

        case SHOWHIDELIST:
            if (h == 0) // left encoder inverts selection
            {
              HS::hidden_applets[0] = ~HS::hidden_applets[0];
              HS::hidden_applets[1] = ~HS::hidden_applets[1];
            }
            else // right encoder toggles current
              HS::showhide_applet(showhide_cursor.cursor_pos());
            break;
        }
    }

    void DrawInputMappings() {
        gfxHeader("<  Input Mapping  >");
        gfxIcon(25, 19, TR_ICON);
        gfxIcon(89, 19, TR_ICON);
        gfxIcon(25, 39, CV_ICON);
        gfxIcon(89, 39, CV_ICON);

        for (int ch=0; ch<4; ++ch) {
          // Physical trigger input mappings
          gfxPrint(4 + ch*32, 25, OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch] ] );

          // Physical CV input mappings
          gfxPrint(4 + ch*32, 45, OC::Strings::cv_input_names_none[ HS::cvmapping[ch] ] );
        }

        gfxLine(64, 11, 64, 63);

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
          gfxCursor(4 + 32*(config_cursor - TRIGMAP1), 33, 19);
          break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
          gfxCursor(4 + 32*(config_cursor - CVMAP1), 53, 19);
          break;
        }

    }

    void DrawQuantizerConfig() {
        gfxHeader("< Quantizer Setup >");

        for (int ch=0; ch<4; ++ch) {
          const int x = 8 + ch*32;

          // 1-4 on top
          gfxPrint(x, 15, "Q");
          gfxPrint(ch + 1);
          gfxLine(x, 23, x + 14, 23);
          gfxLine(x + 14, 13, x + 14, 23);

          const bool upper = config_cursor < QUANT5;
          const int ch_view = upper ? ch : ch + 4;

          gfxIcon(x + 3, upper? 25 : 45, upper? UP_BTN_ICON : DOWN_BTN_ICON);

          // Scale
          gfxPrint(x - 3, 30, OC::scale_names_short[ HS::quant_scale[ch_view] ]);

          // Root Note + Octave
          gfxPrint(x - 3, 40, OC::Strings::note_names[ HS::root_note[ch_view] ]);
          if (HS::q_octave[ch_view] >= 0) gfxPrint("+");
          gfxPrint(HS::q_octave[ch_view]);

          // (TODO: mask editor)

          // 5-8 on bottom
          gfxPrint(x, 55, "Q");
          gfxPrint(ch + 5);
          gfxLine(x, 53, x + 14, 53);
          gfxLine(x + 14, 53, x + 14, 63);
        }

        switch (config_cursor) {
        case QUANT1:
        case QUANT2:
        case QUANT3:
        case QUANT4:
          gfxIcon( 32*(config_cursor-QUANT1), 15, RIGHT_ICON);
          break;
        case QUANT5:
        case QUANT6:
        case QUANT7:
        case QUANT8:
          gfxIcon( 32*(config_cursor-QUANT5), 55, RIGHT_ICON);
          break;
        }
    }

    void DrawAppletList() {
      const size_t LineH = 12;

      int y = (64 - (5 * LineH)) / 2;

      for (int current = showhide_cursor.first_visible();
           current <= showhide_cursor.last_visible();
           ++current, y += LineH) {

        if (!HS::applet_is_hidden(current))
          gfxIcon(  12, y + 1, HS::available_applets[current].instance[0]->applet_icon());
        gfxPrint( 23, y + 2, HS::available_applets[current].instance[0]->applet_name());

        if (current == showhide_cursor.cursor_pos())
          gfxIcon(1, y + 1, RIGHT_ICON);
          //gfxInvert(0, y, 127, LineH - 1);
      }
    }
    void DrawConfigMenu() {
        // --- Config Selection
        gfxHeader("< General Settings >");

        gfxPrint(1, 15, "Trig Length: ");
        gfxPrint(HS::trig_length);
        gfxPrint("ms");

        const char * const ssmodes[4] = { "[blank]", "Meters", "Scope",
        #if defined(__IMXRT1062__)
        "Stars"
        #else
        "Zips"
        #endif
        };
        gfxPrint(1, 25, "Screensaver:  ");
        gfxPrint( ssmodes[HS::screensaver_mode] );

        const char * const cursor_mode_name[3] = { "modal", "modal+wrap" };
        gfxPrint(1, 35, "Cursor:  ");
        gfxPrint(cursor_mode_name[HS::cursor_wrap]);

        gfxPrint(1, 45, "Auto MIDI-Out:  ");
        gfxPrint( HS::frame.autoMIDIOut ? "On" : "Off" );

        switch (config_cursor) {
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
          gfxCursor(1 + 32*(config_cursor - CVMAP1), 63, 19);
          break;

        case TRIG_LENGTH:
            gfxCursor(80, 23, 24);
            break;
        case SCREENSAVER_MODE:
            gfxIcon(73, 25, RIGHT_ICON);
            break;
        case CURSOR_MODE:
            gfxIcon(43, 35, RIGHT_ICON);
            break;
        case AUTO_MIDI:
            gfxIcon(90, 45, RIGHT_ICON);
            break;
        case CONFIG_DUMMY:
            gfxIcon(2, 1, LEFT_ICON);
            break;
        }
    }

    void DrawPresetSelector() {
        gfxHeader((config_cursor == SAVE_PRESET) ? "Save" : "Load");
        gfxPrint(30, 1, "Preset");
        gfxDottedLine(16, 11, 16, 63);

        int y = 5 + constrain(preset_cursor,1,5)*10;
        gfxIcon(0, y, RIGHT_ICON);
        const int top = constrain(preset_cursor - 4, 1, HEM_NR_OF_PRESETS) - 1;
        y = 15;
        for (int i = top; i < HEM_NR_OF_PRESETS && i < top + 5; ++i)
        {
            if (i == preset_id)
              gfxIcon(8, y, ZAP_ICON);
            else
              gfxPrint(8, y, OC::Strings::capital_letters[i]);

            if (!hem_presets[i].is_valid())
                gfxPrint(18, y, "(empty)");
            else {
                gfxIcon(18, y, hem_presets[i].GetApplet(0)->applet_icon());
                gfxPrint(26, y, hem_presets[i].GetApplet(0)->applet_name());
                gfxPrint(", ");
                gfxPrint(hem_presets[i].GetApplet(1)->applet_name());
                gfxIcon(120, y, hem_presets[i].GetApplet(1)->applet_icon());
            }

            y += 10;
        }
    }

};

// TOTAL EEPROM SIZE: 8 presets * 32 bytes
SETTINGS_DECLARE(HemispherePreset, HEMISPHERE_SETTING_LAST) {
    {0, 0, 255, "Applet ID L", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Applet ID R", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 65535, "Data L block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Trig Input Map", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Misc Globals", NULL, settings::STORAGE_TYPE_U16}
};

HemisphereManager manager;

void ReceiveManagerSysEx() {
    if (hem_active_preset)
        hem_active_preset->OnReceiveSysEx();
}
void BeatSyncProcess() {
  manager.ProcessQueue();
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void HEMISPHERE_init() {
    manager.BaseStart();
}

static constexpr size_t HEMISPHERE_storageSize() {
    return HemispherePreset::storageSize() * (HEM_NR_OF_PRESETS + 1);
}

static size_t HEMISPHERE_save(void *storage) {
    // store hidden applet mask in secret preset
    hem_presets[HEM_NR_OF_PRESETS].SetData(HEM_SIDE(0), HS::hidden_applets[0]);
    hem_presets[HEM_NR_OF_PRESETS].SetData(HEM_SIDE(1), HS::hidden_applets[1]);

    size_t used = 0;
    for (int i = 0; i <= HEM_NR_OF_PRESETS; ++i) {
        used += hem_presets[i].Save(static_cast<char*>(storage) + used);
    }
    return used;
}

static size_t HEMISPHERE_restore(const void *storage) {
    size_t used = 0;
    for (int i = 0; i <= HEM_NR_OF_PRESETS; ++i) {
        used += hem_presets[i].Restore(static_cast<const char*>(storage) + used);
    }

    HS::hidden_applets[0] = hem_presets[HEM_NR_OF_PRESETS].GetData(HEM_SIDE(0));
    HS::hidden_applets[1] = hem_presets[HEM_NR_OF_PRESETS].GetData(HEM_SIDE(1));
    return used;
}

void FASTRUN HEMISPHERE_isr() {
    manager.BaseController();
}

void HEMISPHERE_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        manager.Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        manager.Suspend();
        break;

    default: break;
    }
}

void HEMISPHERE_loop() {} // Essentially deprecated in favor of ISR

void HEMISPHERE_menu() {
    manager.View();
}

void HEMISPHERE_screensaver() {
    switch (HS::screensaver_mode) {
    case 0x3: // Zips or Stars
        ZapScreensaver(true);
        break;
    case 0x2: // output scope
        //ZapScreensaver();
        OC::scope_render();
        break;
    case 0x1: // Meters
        manager.BaseScreensaver(true); // show note names
        break;
    default: break; // blank screen
    }
}

void HEMISPHERE_handleButtonEvent(const UI::Event &event) {
    manager.HandleButtonEvent(event);
}

void HEMISPHERE_handleEncoderEvent(const UI::Event &event) {
    manager.DelegateEncoderMovement(event);
}

#endif
