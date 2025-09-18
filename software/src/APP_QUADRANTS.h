// Copyright (c) 2018, Jason Justian
// Copyright (c) 2024, Nicholas J. Michalek
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

#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_visualfx.h"
#include "OC_apps.h"
#include "OC_ui.h"

#include "OC_patterns.h"
#include "UI/ui_events.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

#include "HemisphereApplet.h"
#include "HSApplication.h"
#include "HSicons.h"
#include "HSMIDI.h"
#include "HSClockManager.h"

#include "PackingUtils.h"
#include "hemisphere_config.h"
#include "hemisphere_audio_config.h"

#include "PhzConfig.h"

// per bank file
static constexpr int QUAD_PRESET_COUNT = 32;
static constexpr int PRESET_FILE_REVISION = 0;

using namespace HS;

void QuadrantSysExHandler();
void QuadrantBeatSync();

class QuadAppletManager : public HSApplication {
public:
    void Start() {
        audio_app.Init();

        /* Hemisphere already does this...
         * assuming Hemisphere is always included next to Quadrants...
        for (int i = 0; i < QUANT_CHANNEL_COUNT; ++i) {
            q_engine[i].Configure( (i<4)? OC::Scales::SCALE_SEMI : i-4, 0xffff);
        }
        */

        SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(18)); // DualTM
        SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(15)); // EuclidX
        SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(68)); // DivSeq
        SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(71)); // Pigeons
    }

    void Resume() {
        SetBank(bank_num);

        if (preset_id < 0)
          LoadFromPreset(0);
    }
    void Suspend() {
        if (preset_id >= 0) {
            if (HS::auto_save_enabled)
              StoreToPreset(preset_id);
            // TODO
            //OnSendSysEx();
        }
    }
    void SetBank(uint8_t id) {
      bank_filename[5] = '0' + char(id / 100);
      bank_filename[6] = '0' + char(id / 10 % 10);
      bank_filename[7] = '0' + char(id % 10);

      bool success = false;
      if (SDcard_Ready) // load from SD card
        success = PhzConfig::load_config(bank_filename, SD);

      if (!success) // fallback load from LFS
        PhzConfig::load_config(bank_filename);

      // Version flag - for future reconfigurations...
      //  - This can be used to migrate old data after breaking changes to the schema
      //  - Right now, its existence indicates v1.10.1+
      uint64_t data = 0;
      bool version_key_present = PhzConfig::getValue(VERSION_KEY, data);
      //int preset_file_revision = version_key_present? data : 0;

      // Data migration to protect against a bug in v1.10
      // This can be removed later, when we're sure no one is still using broken preset files...
      if (!version_key_present) {
        for (size_t i = 0; i < QUAD_PRESET_COUNT; ++i) {
          PhzConfig::deleteKey((i << 11) | (CVMAP_KEY + 1));
          PhzConfig::deleteKey((i << 11) | (TRIGMAP_KEY + 1));
        }
      }

      // update version key after all data migrations
      PhzConfig::setValue(VERSION_KEY, PRESET_FILE_REVISION);
    }

    // lower 11 bits of PhzConfig KEY
    enum PresetDataKeys : uint16_t {
        APPLET_METADATA_KEY = 0, // applet ids
        CLOCK_DATA_KEY = 1,
        GLOBALS_KEY = 2,
        OLD_TRIGMAP_KEY = 3, // from v1.9
        OLD_CVMAP_KEY = 4, // from v1.9
        OUTSKIP_KEY = 5,

        APPLET_L1_DATA_KEY = 10,
        APPLET_R1_DATA_KEY = 11,
        APPLET_L2_DATA_KEY = 12,
        APPLET_R2_DATA_KEY = 13,

        CVMAP_KEY = 20, // 16-bit CVInputMap, multiple pages

        TRIGMAP_KEY = 30, // 16-bit DigitalInputMap

        // 100s = Globals
        FILTERMASK1_KEY = 100,
        FILTERMASK2_KEY = 101,

        PC_CHANNEL_KEY  = 110,

        MIDI_MAPS_KEY   = 150, // + 0..32

        // 200s = Quantizers
        Q_ENGINE_KEY    = 200, // + slot number

        // 300-500 = Sequences (aka Patterns)
        SEQUENCES_KEY   = 300, // + blob index

        VERSION_KEY = 0xFFFF
    };

    void StoreToPreset(int id) {
        // preset id is upper 5 bits - 32 presets per bank
        uint16_t preset_key = id << 11;

        // clock data
        clock_data = ClockSetup_instance.OnDataRequest();
        PhzConfig::setValue(preset_key | CLOCK_DATA_KEY, clock_data);

        // vague globals
        global_data = ClockSetup_instance.GetGlobals();
        PhzConfig::setValue(preset_key | GLOBALS_KEY, global_data);

        uint64_t data = 0;
        // Input Mappings
        for (size_t i = 0; i < ADC_CHANNEL_LAST/4; ++i) {
          data = PackPackables(HS::trigmap[i*4], HS::trigmap[i*4+1], HS::trigmap[i*4+2], HS::trigmap[i*4+3]);
          PhzConfig::setValue(preset_key | (TRIGMAP_KEY + i), data);

          data = PackPackables(HS::cvmap[i*4], HS::cvmap[i*4+1], HS::cvmap[i*4+2], HS::cvmap[i*4+3]);
          PhzConfig::setValue(preset_key | (CVMAP_KEY + i), data);
        }

        data = 0;
        for (size_t i = 0; i < 8; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.clockskip[i]);
        }
        PhzConfig::setValue(preset_key | OUTSKIP_KEY, data);

        data = 0;
        for (size_t h = 0; h < APPLET_SLOTS; h++)
        {
            int index = active_applet_index[h];
            Pack(data, PackLocation{h*8,8}, HS::available_applets[index].id);

            // applet data
            applet_data[h] = HS::available_applets[index].instance[h]->OnDataRequest();
            PhzConfig::setValue(preset_key | (APPLET_L1_DATA_KEY + h), applet_data[h]);
        }

        // applet ids, and maybe some other stuff?
        PhzConfig::setValue(preset_key | APPLET_METADATA_KEY, data);

        // applet filtering is actually just global
        PhzConfig::setValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::setValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        PhzConfig::setValue(PC_CHANNEL_KEY, HS::frame.MIDIState.pc_channel);

        // Global quantizer settings
        for (size_t qslot = 0; qslot < QUANT_CHANNEL_COUNT; ++qslot) {
          /*
            // XXX: fine-tuning stuff from Calibr8or that should also be global
            int8_t offset;
            int16_t scale_factor; // precision of 0.01% as an offset from 100%
            int8_t transpose; // in semitones
          */
          auto &q = q_engine[qslot];
          data = PackPackables(
              q.scale,
              q.octave,
              q.root_note,
              q.mask
              );
          PhzConfig::setValue(Q_ENGINE_KEY + qslot, data);
        }

        // Global MIDI Maps
        for (size_t midx = 0; midx < MIDIMAP_MAX; ++midx) {
          data = PackPackables(frame.MIDIState.mapping[midx]);
          PhzConfig::setValue(MIDI_MAPS_KEY + midx, data);
        }

        // User Patterns aka Sequences
        for (size_t i = 0; i < OC::Patterns::PATTERN_USER_COUNT; ++i) {
          data = 0;
          for (size_t step = 0; step < ARRAY_SIZE(OC::Pattern::notes); ++step) {
            Pack(data, PackLocation{(step & 0x3)*16, 16}, (uint16_t)OC::user_patterns[i].notes[step]);
            if ((step & 0x3) == 0x3) {
              PhzConfig::setValue(SEQUENCES_KEY + ((i << 2) | (step >> 2)), data);
              data = 0;
            }
          }
        }

        audio_app.SavePreset(id);

        bool success = false;
        if (SDcard_Ready)
          success = PhzConfig::save_config(bank_filename, SD);
        else
          success = PhzConfig::save_config(bank_filename);

        if (success)
          PokePopup(HS::MESSAGE_POPUP, HS::PRESET_SAVED);

        preset_id = id;
    }
    void LoadFromPreset(int id) {
        preset_id = id;

        uint16_t preset_key = id << 11;
        uint64_t data;

        // applet ids + misc
        if (!PhzConfig::getValue(preset_key | APPLET_METADATA_KEY, data)) return;
        if (!data) return;

        for (size_t h = 0; h < APPLET_SLOTS; h++)
        {
            int index = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );

            // applet data
            PhzConfig::getValue(preset_key | (APPLET_L1_DATA_KEY + h), applet_data[h]);
            SetApplet(HEM_SIDE(h), index);
            HS::available_applets[index].instance[h]->OnDataReceive(applet_data[h]);
        }

        // clock data
        if (!PhzConfig::getValue(preset_key | CLOCK_DATA_KEY, clock_data)) return;
        ClockSetup_instance.OnDataReceive(clock_data);
        // if the first key exists, we are assuming the rest are present...

        // vague globals
        PhzConfig::getValue(preset_key | GLOBALS_KEY, global_data);
        ClockSetup_instance.SetGlobals(global_data);

        // Input Mappings
        if (!PhzConfig::getValue(preset_key | TRIGMAP_KEY, data)) {
          PhzConfig::getValue(preset_key | OLD_TRIGMAP_KEY, data);
          const size_t bitsize = 5;
          for (size_t i = 0; i < 8; ++i) {
            const int val = Unpack(data, PackLocation{i*bitsize, bitsize});
            if (val != 0) HS::trigmap[i].source = constrain(val - 1, 0, TRIGMAP_MAX);
          }
        } else {
          for (size_t i = 0; i < ADC_CHANNEL_LAST/4; ++i) {
            UnpackPackables(data, HS::trigmap[i*4], HS::trigmap[i*4+1], HS::trigmap[i*4+2], HS::trigmap[i*4+3]);
            if (!PhzConfig::getValue(preset_key | (TRIGMAP_KEY + i+1), data)) break;
          }
        }

        if (!PhzConfig::getValue(preset_key | CVMAP_KEY, data)) {
          PhzConfig::getValue(preset_key | OLD_CVMAP_KEY, data);
          const size_t bitsize = 5;
          for (size_t i = 0; i < 8; ++i) {
            const int val = Unpack(data, PackLocation{i*bitsize, bitsize});
            if (val != 0) HS::cvmap[i].source = constrain(val - 1, 0, CVMAP_MAX);
          }
        } else {
          for (size_t i = 0; i < ADC_CHANNEL_LAST/4; ++i) {
            UnpackPackables(data, HS::cvmap[i*4], HS::cvmap[i*4+1], HS::cvmap[i*4+2], HS::cvmap[i*4+3]);
            if (!PhzConfig::getValue(preset_key | (CVMAP_KEY + i+1), data)) break;
          }
        }

        PhzConfig::getValue(preset_key | OUTSKIP_KEY, data);
        for (size_t i = 0; i < 8; ++i)
        {
          HS::frame.clockskip[i] = Unpack(data, PackLocation{i*8, 8});
        }

        // applet filtering is actually just global
        PhzConfig::getValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::getValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        if (PhzConfig::getValue(PC_CHANNEL_KEY, data)) HS::frame.MIDIState.pc_channel = (uint8_t) data;

        // Global quantizer settings
        for (size_t qslot = 0; qslot < QUANT_CHANNEL_COUNT; ++qslot) {
          if (!PhzConfig::getValue(Q_ENGINE_KEY + qslot, data))
              break;
          auto &q = q_engine[qslot];
          UnpackPackables(data,
              q.scale,
              q.octave,
              q.root_note,
              q.mask);
          q.Reconfig();
        }

        // Global MIDI Maps
        for (size_t midx = 0; midx < MIDIMAP_MAX; ++midx) {
          if (!PhzConfig::getValue(MIDI_MAPS_KEY + midx, data))
              break;
          UnpackPackables(data, frame.MIDIState.mapping[midx]);
        }
        frame.MIDIState.UpdateMidiChannelFilter();
        frame.MIDIState.UpdateMaxPolyphony();

        // User Patterns aka Sequences
        for (size_t i = 0; i < OC::Patterns::PATTERN_USER_COUNT; ++i) {
          for (size_t step = 0; step < ARRAY_SIZE(OC::Pattern::notes); ++step) {
            if ((step & 0x3) == 0x0) {
              data = 0;
              if (!PhzConfig::getValue(SEQUENCES_KEY + ((i << 2) | (step >> 2)), data))
                break;
            }
            OC::user_patterns[i].notes[step] = Unpack(data, PackLocation{(step & 0x3)*16, 16});
          }
        }

        audio_app.LoadPreset(id);
        PokePopup(PRESET_POPUP);
    }
    void ProcessQueue() {
      LoadFromPreset(queued_preset);
    }
    void QueuePresetLoad(int id) {
      if (HS::clock_m.IsRunning()) {
        queued_preset = id;
        HS::clock_m.BeatSync( &QuadrantBeatSync );
      }
      else
        LoadFromPreset(id);
    }

    // does not modify the preset, only the quad_manager
    void SetApplet(HEM_SIDE hemisphere, int index) {
        if (active_applet[hemisphere])
          active_applet[hemisphere]->Unload();

        next_applet_index[hemisphere] = active_applet_index[hemisphere] = index;
        active_applet[hemisphere] = HS::available_applets[index].instance[hemisphere];
        active_applet[hemisphere]->BaseStart(hemisphere);
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet_index[h], dir);
        next_applet_index[h] = index;
    }

    template <typename T1, typename T2, typename T3>
    void ProcessMIDI(T1 &device, T2 &next_device, T3 &dev3) {
        HS::IOFrame &f = HS::frame;

        while (device.read()) {
            const uint8_t message = device.getType();
            const uint8_t data1 = device.getData1();
            const uint8_t data2 = device.getData2();

            if (message == usbMIDI.SystemExclusive) {
                QuadrantSysExHandler();
                continue;
            }

            if (message == usbMIDI.ProgramChange
            && (device.getChannel() == f.MIDIState.pc_channel || f.MIDIState.pc_channel == f.MIDIState.PC_OMNI)) {
                uint8_t slot = device.getData1();
                if (slot < QUAD_PRESET_COUNT) {
                    QueuePresetLoad(slot);
                }
                //continue;
            }

            f.MIDIState.ProcessMIDIMsg({device.getChannel(), message, data1, data2});
            next_device.send(message, data1, data2, device.getChannel(), 0);
            dev3.send((midi::MidiType)message, data1, data2, device.getChannel());
        }
    }

    void Controller() {
        // top-level MIDI-to-CV handling - alters frame outputs
        ProcessMIDI(usbMIDI, usbHostMIDI, MIDI1);
        thisUSB.Task();
        ProcessMIDI(usbHostMIDI, usbMIDI, MIDI1);
        ProcessMIDI(MIDI1, usbMIDI, usbHostMIDI);

        // Clock Setup applet handles internal clock duties
        ClockSetup_instance.Controller();

        // execute Applets
        for (int h = 0; h < APPLET_SLOTS; h++)
        {
            if (HS::clock_m.auto_reset)
                active_applet[h]->Reset();

            active_applet[h]->Controller();
        }
        HS::clock_m.auto_reset = false;
        audio_app.Controller();
        HemisphereApplet::ProcessCursors();
    }

    void DrawFullScreen() {
      if (select_mode == zoom_slot) {
        showhide_cursor.Scroll(next_applet_index[zoom_slot] - showhide_cursor.cursor_pos());
        DrawAppletList(CursorBlink());
        // dotted screen border during applet select
        gfxFrame(0, 0, 128, 64, true);
      } else {
        active_applet[zoom_slot]->BaseView(true, zoom_cursor < 0);
        // Applets 3 and 4 get inverted titles
        if (zoom_slot > 1) gfxInvert(1 + (zoom_slot%2)*64, 1, 63, 10);

        gfxDisplayInputMapEditor();
      }

      // draw cursor for editing applet select and input maps
      if (zoom_cursor < 0) {
        gfxIcon(64 - 8*(zoom_slot & 1), 1, DOWN_ICON, true);
      } else if (0 == zoom_cursor) {
        if (select_mode != zoom_slot && CursorBlink())
          gfxIcon(64 - 8*(zoom_slot & 1), 1, (zoom_slot & 1)? RIGHT_ICON : LEFT_ICON, true);
      } else if (isEditing) {
        const int x = ((zoom_cursor-1)%2)*64;
        const int y = 13 + 10*((zoom_cursor-1)/2);
        gfxInvert(x, y, 19, 9);
        gfxFrame(x, y, 19, 9, true);
      } else {
        if (CursorBlink()) {
          const int x = 18 + 64*((zoom_cursor-1)%2);
          const int y = 14 + 10*((zoom_cursor-1)/2);
          gfxIcon(x, y, LEFT_ICON, true);
        }
      }
    }

    void DrawOverview() {
      active_applet[0]->gfxHeader(0);
      active_applet[1]->gfxHeader(0);
      active_applet[2]->gfxHeader(54);
      active_applet[3]->gfxHeader(54);

      gfxDottedLine(63, 0, 63, 63); // vert
      gfxDottedLine(0, 32, 127, 32); // horiz

      ForAllChannels(applet) {
        ForEachChannel(ch) {
            int length;
            int max_length = 62;
            int in_bar_y = 13 + (applet>>1)*22 + (ch * 10);
            int out_bar_y = 17 + (applet>>1)*22 + (ch * 10);

            // positive values extend bars from left side of screen to the right
            // negative values go from right side to left
            length = ProportionCV(abs(DetentedIn(applet*2 + ch)), max_length);
            if (DetentedIn(applet*2 + ch) < 0)
                active_applet[applet]->gfxFrame(max_length - length, in_bar_y, length, 3);
            else
                active_applet[applet]->gfxFrame(0, in_bar_y, length, 3);

            length = ProportionCV(abs(ViewOut(applet*2 + ch)), max_length);
            if (ViewOut(applet*2 + ch) < 0)
                active_applet[applet]->gfxRect(max_length - length, out_bar_y, length, 3);
            else
                active_applet[applet]->gfxRect(0, out_bar_y, length, 3);
        }
      }
    }

    void View() {
        bool draw_applets = true;

        if (preset_cursor) {
          DrawPresetSelector();
          draw_applets = false;
        }
        else if (config_page > HIDE_CONFIG) {
          switch(config_page) {
          default:
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
        if (HS::q_edit)
          PokePopup(QUANTIZER_POPUP);

        if (draw_applets) {
          if (view_state == AUDIO_SETUP) {
            audio_app.View();
            ClockSetup_instance.DrawIndicator();

            // gfxHeader("Audio DSP Setup");
            // OC::AudioDSP::DrawAudioSetup();
            draw_applets = false;
          }
        }

        if (draw_applets) {
          if (view_state == APPLET_FULLSCREEN) {
            DrawFullScreen();
          } else if (view_state == OVERVIEW) {
            DrawOverview();
          } else {
            // only two applets visible at a time
            for (int h = 0; h < 2; h++)
            {
                HEM_SIDE slot = HEM_SIDE(h + view_slot[h]*2);
                active_applet[slot]->BaseView();

                // Applets 3 and 4 get inverted titles
                if (slot > 1) gfxInvert(1 + h*64, 1, 63, 10);
            }

            // vertical separator
            graphics.drawLine(63, 0, 63, 63, 2);
          }

          // Clock setup is an overlay
          if (view_state == CLOCK_SETUP) {
            ClockSetup_instance.View();
          } else {
            ClockSetup_instance.DrawIndicator(view_state == OVERVIEW);
          }
        }

        // Overlay popup window last
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS * 4) {
          HS::DrawPopup(config_cursor, preset_id, CursorBlink());
        }
    }

    // always act-on-press for encoder
    void DelegateEncoderPush(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);

        if (config_page > HIDE_CONFIG || preset_cursor) {
          ConfigButtonPush(h);
          return;
        }
        if (view_state == AUDIO_SETUP) {
          // OC::AudioDSP::AudioSetupButtonAction(h);
          // audio_app.HandleButtonEvent(event);
          return;
        }
        if (view_state == APPLET_FULLSCREEN) {
          switch (zoom_cursor) {
            case -1:
              active_applet[zoom_slot]->OnButtonPress();
              break;

            case 0:
              if (zoom_slot == select_mode) {
                SetApplet(zoom_slot, next_applet_index[zoom_slot]);
                ExitFullScreen();
              } else
                select_mode = zoom_slot;
              break;
            //// 0=select; 1,2=trigmap; 3,4=cvmap; 5,6=outmode
            case 3:
            case 4:
              if (CheckEditInputMapPress(
                    zoom_cursor,
                    IndexedInput(3, cvmap[zoom_slot*2]),
                    IndexedInput(4, cvmap[zoom_slot*2+1])
                  ))
                break;
            case 1:
            case 2:
            case 5:
            case 6:
            default:
              isEditing = !isEditing;
              break;
          }
          return;
        }

        if (view_state == CLOCK_SETUP) {
          ClockSetup_instance.OnButtonPress();
          return;
        }

        active_applet[slot]->OnButtonPress();
    }

    const HEM_SIDE ButtonToSlot(const UI::Event &event) {
      if (event.control == OC::CONTROL_BUTTON_X)
        return LEFT2_HEMISPHERE;
      if (event.control == OC::CONTROL_BUTTON_B)
        return RIGHT_HEMISPHERE;
      if (event.control == OC::CONTROL_BUTTON_Y)
        return RIGHT2_HEMISPHERE;

      //if (event.control == OC::CONTROL_BUTTON_A)
      return LEFT_HEMISPHERE; // default
    }

    // returns true if combo detected and button release should be ignored
    bool CheckButtonCombos(const UI::Event &event) {
        HEM_SIDE slot = ButtonToSlot(event);

        // dual press A+B for Clock Setup
        if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) {
            view_state = CLOCK_SETUP;
            return true;
        }
        // dual press X+Y for Audio Setup
        if (CheckButtonCombo(OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            view_state = AUDIO_SETUP;
            return true;
        }
        // dual press A+X for Load Preset
        if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_X)) {
            ShowPresetSelector();
            return true;
        }

        // dual press B+Y for Input Mapping
        if (CheckButtonCombo(OC::CONTROL_BUTTON_B | OC::CONTROL_BUTTON_Y)) {
            config_page = INPUT_SETTINGS;
            config_cursor = TRIGMAP1;
            return true;
        }

        // cancel preset select or config screens
        if (config_page || preset_cursor) {
          if (isEditing && config_cursor == PRESET_BANK_NUM) SetBank(bank_num);
          preset_cursor = 0;
          config_page = HIDE_CONFIG;
          HS::popup_tick = 0;
          return true;
        }

        // cancel other view layers
        if (view_state != APPLETS && view_state != APPLET_FULLSCREEN) {
          view_state = APPLETS;
          return true;
        }

        // A/B/X/Y buttons becomes aux button while editing a param
        if (SlotIsVisible(slot) && active_applet[slot]->EditMode()) {
          active_applet[slot]->AuxButton();
          return true;
        }

        return false;
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);
        if (HS::q_edit) {
          HS::QEditEncoderMove(h, event.value);
          return;
        }

        if (config_page > HIDE_CONFIG || preset_cursor) {
            ConfigEncoderAction(h, event.value);
            return;
        }
        if (view_state == AUDIO_SETUP) {
          // OC::AudioDSP::AudioMenuAdjust(h, event.value);
          audio_app.HandleEncoderEvent(event);
          return;
        }

        if (view_state == CLOCK_SETUP) {
          if (h == LEFT_HEMISPHERE)
            ClockSetup_instance.OnLeftEncoderMove(event.value);
          else
            ClockSetup_instance.OnEncoderMove(event.value);
          return;
        }

        if (view_state == APPLET_FULLSCREEN) {
            if (select_mode == zoom_slot)
              ChangeApplet(zoom_slot, event.value);
            else if (h == LEFT_HEMISPHERE && !isEditing)
              zoom_cursor = (event.value > 0)? 0 : -1;
            else if (zoom_cursor < 0)
              active_applet[zoom_slot]->OnEncoderMove(event.value);
            else if (isEditing) { // enc changes value
              switch (zoom_cursor)
              {
                case 1:
                case 2:
                {
                  const int chan = zoom_slot*2 + zoom_cursor - 1;
                  if (clock_m.IsRunning()) // && clock_m.GetMultiply(chan))
                  {
                    clock_m.SetMultiply(clock_m.GetMultiply(chan) + event.value, chan);
                  } else
                    HS::trigmap[chan].ChangeSource(event.value);
                  break;
                }
                case 3:
                case 4:
                  if (!EditSelectedInputMap(event.value))
                    HS::cvmap[zoom_slot*2 + zoom_cursor - 3].ChangeSource(event.value);
                  break;
                case 5:
                case 6:
                  // TODO: per applet?
                default:
                  isEditing = false;
                  break;
              }
            } else { // enc moves cursor
              zoom_cursor = constrain(zoom_cursor + event.value, 0, 4);
              ResetCursor();
            }
        } else if (event.mask & (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            // hold down X or Y to change applet with encoder
            ChangeApplet(slot, event.value);
            SetApplet(slot, next_applet_index[slot]);
        } else {
            active_applet[slot]->OnEncoderMove(event.value);
        }
    }

    void SetConfigPageFromCursor() {
      if (config_cursor <= CONFIG_DUMMY) config_page = LOADSAVE_POPUP;
      else if (config_cursor < TRIGMAP1) config_page = CONFIG_SETTINGS;
      else if (config_cursor < QUANT1) config_page = INPUT_SETTINGS;
      else if (config_cursor < SHOWHIDELIST) config_page = QUANTIZER_SETTINGS;
      else config_page = LAST_PAGE;
    }
    void ToggleConfigMenu() {
      if (config_page) {
        config_page = HIDE_CONFIG;
      } else {
        SetConfigPageFromCursor();
      }
    }
    void ShowPresetSelector() {
      config_cursor = LOAD_PRESET;
      preset_cursor = preset_id + 1;
    }

    // this toggles the view on a given side
    void SwapViewSlot(int h) {
      // h should be 0 or 1 (left or right)
      //h %= 2;

      view_slot[h] = 1 - view_slot[h];
      // also switch fullscreen to corresponding side/slot
      zoom_slot = HEM_SIDE(view_slot[h]*2 + h);
    }

    bool SlotIsVisible(HEM_SIDE h) {
      if (view_state == APPLET_FULLSCREEN)
        return zoom_slot == h;

      return (view_slot[h % 2] == h / 2);
    }
    // this brings a specific applet into view on the appropriate side
    void SwitchToSlot(HEM_SIDE h) {
      view_slot[h % 2] = h / 2;
      zoom_slot = h;
    }

    void ExitFullScreen() {
      view_state = APPLETS;
      isEditing = false;
      select_mode = -1;
    }
    void SetFullScreen(HEM_SIDE hemisphere) {
      SwitchToSlot(hemisphere);
      view_state = APPLET_FULLSCREEN;
      isEditing = false;
      select_mode = -1;
    }
    void ToggleFullScreen() {
      view_state = (view_state == APPLET_FULLSCREEN) ? APPLETS : APPLET_FULLSCREEN;
    }

    void HandleButtonEvent(const UI::Event &event) {
        last_mask = mask;
        mask = event.mask;
        SERIAL_PRINTLN(
          "mask=%d type=%d value=%d control=%d last_mask=%d",
          event.mask,
          event.type,
          event.value,
          event.control,
          last_mask
        );

        if (AUDIO_SETUP == view_state) {
          if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) {
            view_state = APPLETS;
            return;
          }
          if ((event.control == OC::CONTROL_BUTTON_L
               || event.control == OC::CONTROL_BUTTON_R)) {
            audio_app.HandleEncoderButtonEvent(event);
            return;
          }
          if (event.control == OC::CONTROL_BUTTON_X
              || event.control == OC::CONTROL_BUTTON_Y
              || event.control == OC::CONTROL_BUTTON_A
              || event.control == OC::CONTROL_BUTTON_B) {
            if (audio_app.HandleButtonEvent(event))
              view_state = APPLETS;
            return;
          }
        }

        if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_Y) ||
            CheckButtonCombo(OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_B)) {
          view_state = OVERVIEW;
          return;
        }

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
              select_mode = -1;
            }

            OC::ui.SetButtonIgnoreMask();
            break;
          }

          if (event.control == OC::CONTROL_BUTTON_Z)
          {
              // Z-button - Zap the CLOCK!!
              ToggleClockRun();
              OC::ui.SetButtonIgnoreMask();
          } else if (
            event.control == OC::CONTROL_BUTTON_L ||
            event.control == OC::CONTROL_BUTTON_R)
          {
              DelegateEncoderPush(event);
              // ignore long-press to prevent Main Menu >:)
              //OC::ui.SetButtonIgnoreMask();
          } else if (
            event.control == OC::CONTROL_BUTTON_A ||
            event.control == OC::CONTROL_BUTTON_B ||
            event.control == OC::CONTROL_BUTTON_X ||
            event.control == OC::CONTROL_BUTTON_Y)
          {
              if (CheckButtonCombos(event)) {
                select_mode = -1;
                isEditing = false;
                OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
              } else {
                HEM_SIDE slot = ButtonToSlot(event);
                if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME
                    && (slot == first_click))
                {
                    // This is a double-click on one button.
                    SetFullScreen(slot);
                    click_tick = 0;
                    OC::ui.SetButtonIgnoreMask(); // ignore button release
                    return;
                }

                // -- Single click
                // If a help screen is already selected, and the button is for
                // the opposite one, go to the other help screen
                if (view_state == APPLET_FULLSCREEN) {
                    if (zoom_slot != slot) SetFullScreen(slot);
                    else ExitFullScreen(); // Exit help screen if same button is clicked
                    OC::ui.SetButtonIgnoreMask(); // ignore release
                }

                // mark this single click
                click_tick = OC::CORE::ticks;
                first_click = slot;
              }
          }

          break;

        case UI::EVENT_BUTTON_PRESS:
          // A/B/X/Y switch to corresponding applet on release
          if (event.control == OC::CONTROL_BUTTON_A ||
              event.control == OC::CONTROL_BUTTON_B ||
              event.control == OC::CONTROL_BUTTON_X ||
              event.control == OC::CONTROL_BUTTON_Y)
          {
            HEM_SIDE slot = ButtonToSlot(event);
            if (view_state == APPLET_FULLSCREEN && slot == zoom_slot)
              view_state = APPLETS;

            SwitchToSlot(slot);
          }
          // ignore all other button release events
          break;

        case UI::EVENT_BUTTON_LONG_PRESS:
          if (event.control == OC::CONTROL_BUTTON_B) ToggleConfigMenu();
          break;

        default: break;
        }
    }

protected:
    bool CheckButtonCombo(uint16_t combo) {
        return mask == combo && mask != last_mask;
    }

private:
    char bank_filename[16] = "BANK_000.DAT";
    uint8_t bank_num = 0;
    int preset_id = -1;
    int queued_preset = 0;
    int preset_cursor = 0;
    HemisphereApplet *active_applet[4]; // Pointers to actual applets
    int active_applet_index[4]; // Indexes to available_applets
                      // Left side: 0,2
                      // Right side: 1,3
    int next_applet_index[4]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[4]; // cache of applet data
    bool view_slot[2] = {0, 0}; // Two applets on each side, only one visible at a time
    int config_cursor = 0;

    int select_mode = -1;
    HEM_SIDE zoom_slot; // Which of the hemispheres (if any) is in fullscreen/help mode
    int zoom_cursor; // 0=select; 1,2=trigmap; 3,4=cvmap; 5,6=outmode
    uint32_t click_tick; // Measure time between clicks for double-click
    int first_click; // The first button pushed of a double-click set, to see if the same one is pressed

    // Button combos can cause multiple triggers if the buttons are pressed
    // close enough together. Each press will have its own event with both
    // button marked in the mask. So, we track mask history to ensure button
    // state has actually changed between events to register a combo.
    uint16_t mask = 0;
    uint16_t last_mask = 0;

    // State machine
    enum QuadrantsView {
      APPLETS,
      APPLET_FULLSCREEN,
      OVERVIEW,
      //CONFIG_MENU,
      //PRESET_PICKER,
      CLOCK_SETUP,
      AUDIO_SETUP,
    };
    QuadrantsView view_state = APPLETS;

    enum QuadrantsConfigPage {
      HIDE_CONFIG,
      LOADSAVE_POPUP,
      CONFIG_SETTINGS,
      INPUT_SETTINGS,
      QUANTIZER_SETTINGS,
      SHOWHIDE_APPLETS,

      LAST_PAGE = SHOWHIDE_APPLETS
    };
    int config_page = HIDE_CONFIG;

    enum QuadrantsConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,
        PRESET_BANK_NUM,
        MIDI_PC_CHANNEL,

        // Input Remapping
        TRIGMAP1, TRIGMAP2, TRIGMAP3, TRIGMAP4,
        CVMAP1, CVMAP2, CVMAP3, CVMAP4,
        TRIGMAP5, TRIGMAP6, TRIGMAP7, TRIGMAP8,
        CVMAP5, CVMAP6, CVMAP7, CVMAP8,

        // Global Quantizers: 4x(Scale, Root, Octave, Mask?)
        QUANT1, QUANT2, QUANT3, QUANT4,
        QUANT5, QUANT6, QUANT7, QUANT8,

        // Applet visibility (dummy position)
        SHOWHIDELIST,

        MAX_CURSOR = QUANT8
    };

    void ConfigEncoderAction(int h, int dir) {
        if (!isEditing && !preset_cursor) {
          if (h == 0) { // change pages
            config_page += dir;
            config_page = constrain(config_page, LOADSAVE_POPUP, LAST_PAGE);

            const int cursorpos[] = { 0, LOAD_PRESET, TRIG_LENGTH, TRIGMAP1, QUANT1, SHOWHIDELIST };
            config_cursor = cursorpos[config_page];
          } else if (config_page == SHOWHIDE_APPLETS) {
            showhide_cursor.Scroll(dir);
          } else { // move cursor
            config_cursor += dir;
            config_cursor = constrain(config_cursor, 0, MAX_CURSOR);

            SetConfigPageFromCursor();
          }
          ResetCursor();
          if (config_cursor > CONFIG_DUMMY) HS::popup_tick = 0;
          return;
        }

        if (preset_cursor && isEditing) {
          // bank select
          bank_num = constrain(bank_num + dir, 0, 99);
          return;
        }

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            HS::trigmap[config_cursor-TRIGMAP1].ChangeSource(dir);
            break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
            if (!EditSelectedInputMap(dir))
              HS::cvmap[config_cursor-CVMAP1].ChangeSource(dir);
            break;
        case TRIGMAP5:
        case TRIGMAP6:
        case TRIGMAP7:
        case TRIGMAP8:
            HS::trigmap[config_cursor-TRIGMAP5 + 4].ChangeSource(dir);
            break;
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
            if (!EditSelectedInputMap(dir))
              HS::cvmap[config_cursor-CVMAP5 + 4].ChangeSource(dir);
            break;
        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        case PRESET_BANK_NUM:
            bank_num = constrain(bank_num + dir, 0, 99);
            break;
        case MIDI_PC_CHANNEL:
            HS::frame.MIDIState.pc_channel =
              constrain(HS::frame.MIDIState.pc_channel + dir, 0, 17);
            break;
        //case SCREENSAVER_MODE:
            // TODO?
            //break;
        case LOAD_PRESET:
        case SAVE_PRESET:
            if (h == 0) {
              config_cursor = constrain(config_cursor + dir, LOAD_PRESET, SAVE_PRESET);
            } else {
              preset_cursor = constrain(preset_cursor + dir, 1, QUAD_PRESET_COUNT);
            }
            break;
        }
    }
    void ConfigButtonPush(int h) {
      static uint8_t old_bank_num = 0xff;
        if (preset_cursor) {
            // left enc toggles bank select
            if (0 == h || isEditing) {
              isEditing = !isEditing;
              if (!isEditing) {
                if (old_bank_num != bank_num) SetBank(bank_num);
              } else
                old_bank_num = bank_num;

              return;
            }

            // Save or Load on button push
            if (config_cursor == SAVE_PRESET)
                StoreToPreset(preset_cursor-1);
            else {
                QueuePresetLoad(preset_cursor - 1);
            }

            preset_cursor = 0; // deactivate preset selection
            config_page = HIDE_CONFIG;
            view_state = APPLETS;
            isEditing = false;
            return;
        }

        switch (config_cursor) {
        case CONFIG_DUMMY:
            // reset input mappings to defaults
            HS::Init();
            // randomize all applets
            for (int ch = 0; ch < APPLET_SLOTS; ++ch) {
              size_t index = random(HEMISPHERE_AVAILABLE_APPLETS);
              SetApplet(HEM_SIDE(ch), index);
#ifdef PEWPEWPEW
              // load random data !!!
              // this will expose critical bugs in data validation ;)
              HS::available_applets[index].instance[ch]->OnDataReceive(uint64_t(random()) << 32 | (uint64_t)random());
#endif
            }
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

        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
          if (CheckEditInputMapPress(
                config_cursor,
                IndexedInput(CVMAP1, cvmap[0]),
                IndexedInput(CVMAP2, cvmap[1]),
                IndexedInput(CVMAP3, cvmap[2]),
                IndexedInput(CVMAP4, cvmap[3]),
                IndexedInput(CVMAP5, cvmap[4]),
                IndexedInput(CVMAP6, cvmap[5]),
                IndexedInput(CVMAP7, cvmap[6]),
                IndexedInput(CVMAP8, cvmap[7])
              ))
            break;
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
        case TRIGMAP5:
        case TRIGMAP6:
        case TRIGMAP7:
        case TRIGMAP8:
        case TRIG_LENGTH:
        case MIDI_PC_CHANNEL:
            isEditing = !isEditing;
            break;

        case PRESET_BANK_NUM:
            isEditing = !isEditing;
            if (!isEditing) SetBank(bank_num);
            break;

        case SCREENSAVER_MODE:
            ++HS::screensaver_mode %= SCREENSAVER_MODE_COUNT;
            break;

        case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
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
        default: break;
        }
    }

    void DrawInputMappings() {
        gfxHeader("<  Input Mapping  >");
        gfxIcon(25, 13, TR_ICON); gfxIcon(89, 13, TR_ICON);
        gfxIcon(25, 26, CV_ICON); gfxIcon(89, 26, CV_ICON);
        gfxIcon(25, 39, TR_ICON); gfxIcon(89, 39, TR_ICON);
        gfxIcon(25, 52, CV_ICON); gfxIcon(89, 52, CV_ICON);

        for (int ch=0; ch<4; ++ch) {
          // Physical trigger input mappings
          // Physical CV input mappings
          // Top 2 applets
          gfxPrint(4 + ch*32, 15, HS::trigmap[ch].InputName() );
          gfxPrint(4 + ch*32, 28, HS::cvmap[ch].InputName() );

          // Bottom 2 applets
          gfxPrint(4 + ch*32, 41, HS::trigmap[ch + 4].InputName() );
          gfxPrint(4 + ch*32, 54, HS::cvmap[ch + 4].InputName() );
        }

        gfxDottedLine(63, 11, 63, 63); // vert
        gfxDottedLine(0, 38, 127, 38); // horiz

        // Cursor location is within a 4x4 grid
        const int cur_x = (config_cursor-TRIGMAP1) % 4;
        const int cur_y = (config_cursor-TRIGMAP1) / 4;

        gfxCursor(4 + 32*cur_x, 23 + 13*cur_y, 19);

        gfxDisplayInputMapEditor();
    }
    void DrawQuantizerConfig() {
        gfxHeader("<  Quantizer Setup");

        for (int ch=0; ch<4; ++ch) {
          const int x = 8 + ch*32;

          // 1-4 on top
          gfxPrint(x, 15, "Q");
          gfxPrint(ch + 1);
          gfxLine(x, 23, x + 14, 23);
          gfxLine(x + 14, 13, x + 14, 23);

          const bool upper = config_cursor < QUANT5;
          const int ch_view = upper ? ch : ch + 4;
          auto &q = q_engine[ch_view];

          gfxIcon(x + 3, upper? 25 : 45, upper? UP_BTN_ICON : DOWN_BTN_ICON);

          // Scale
          gfxPrint(x - 3, 30, OC::scale_names_short[ q.scale ]);

          // Root Note + Octave
          gfxPrint(x - 3, 40, OC::Strings::note_names[ q.root_note ]);
          if (q.octave >= 0) gfxPrint("+");
          gfxPrint(q.octave);

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

    void DrawConfigMenu() {
        // --- Config Selection
        gfxHeader("< General Settings  >");

        gfxPrint(1, 15, "Trig Length: ");
        gfxPrint(HS::trig_length);
        gfxPrint("ms");

        gfxPrint(1, 25, "Screensaver:  ");
        gfxPrint( ssmodes[HS::screensaver_mode] );

        gfxPrint(1, 35, "Cursor wrap:  ");
        gfxPrint(OC::Strings::off_on[HS::cursor_wrap]);

        gfxPrint(1, 45, "Preset Bank# ");
        gfxPrint(bank_num);

        const uint8_t pc_ch = HS::frame.MIDIState.pc_channel;
        gfxPrint(1, 55, "MIDI-PC Ch:  ");
        if (pc_ch == 0) graphics.printf("%4s", "Omni");
        else if (pc_ch <= 16) graphics.printf("%4d", pc_ch);
        else graphics.printf("%4s", "Off");

        switch (config_cursor) {
        case TRIG_LENGTH:
            gfxCursor(80, 23, 24);
            break;
        case SCREENSAVER_MODE:
            gfxIcon(73, 25, RIGHT_ICON);
            break;
        case CURSOR_MODE:
            gfxIcon(73, 35, RIGHT_ICON);
            break;
        case PRESET_BANK_NUM:
            gfxCursor(78, 53, 19);
            break;
        case MIDI_PC_CHANNEL:
            gfxCursor(78, 63, 25);
            break;
        case CONFIG_DUMMY:
            gfxIcon(2, 1, LEFT_ICON);
            break;
        }
    }

    bool isValidPreset(int id) {
      uint64_t data;
      return PhzConfig::getValue(id << 11 | APPLET_METADATA_KEY, data);
    }
    HemisphereApplet* GetApplet(int id, size_t h) {
        uint64_t data = 0;
        PhzConfig::getValue(id << 11 | APPLET_METADATA_KEY, data);
        int idx = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );
        return HS::available_applets[idx].instance[h];
    }
    void DrawPresetSelector() {
        gfxHeader((config_cursor == SAVE_PRESET) ? "Save" : "Load");
        gfxPrint(30, 1, "Preset: Bank# ");
        gfxPrint(bank_num);
        if (!SDcard_Ready) gfxInvert(78, 0, 30, 9);

        // bank select cursor
        if (isEditing) {
          gfxIcon(114, 11, UP_ICON);
          return;
        }

        // Draw preset list
        gfxDottedLine(16, 11, 16, 63);
        int y = 5 + constrain(preset_cursor,1,5)*10;
        gfxIcon(0, y, RIGHT_ICON);
        const int top = constrain(preset_cursor - 4, 1, QUAD_PRESET_COUNT) - 1;
        y = 15;
        for (int i = top; i < QUAD_PRESET_COUNT && i < top + 5; ++i)
        {
            if (i == preset_id)
              gfxIcon(8, y, ZAP_ICON);
            else
              gfxPrint(8, y, OC::Strings::capital_letters[i]);

            if (!isValidPreset(i))
                gfxPrint(18, y, "(empty)");
            else {
                gfxIcon(18, y, GetApplet(i, LEFT_HEMISPHERE)->applet_icon());
                gfxPrint(26, y, GetApplet(i, LEFT_HEMISPHERE)->applet_name());
                gfxPrint(", ");
                gfxPrint(GetApplet(i, RIGHT_HEMISPHERE)->applet_name());
                gfxIcon(120, y, GetApplet(i, RIGHT_HEMISPHERE)->applet_icon(), true);
            }

            y += 10;
        }
    }
};

QuadAppletManager quad_manager;

void QuadrantSysExHandler() {
  // TODO
}
void QuadrantBeatSync() {
  quad_manager.ProcessQueue();
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void QUADRANTS_init() {
    quad_manager.BaseStart();
}

static constexpr size_t QUADRANTS_storageSize() {
    return 0;
}

static size_t QUADRANTS_save(void *storage) {
    size_t used = 0;
    return used;
}

static size_t QUADRANTS_restore(const void *storage) {
    size_t used = 0;
    return used;
}

void FASTRUN QUADRANTS_isr() {
    quad_manager.BaseController();
}

void QUADRANTS_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        quad_manager.Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        quad_manager.Suspend();
        break;

    default: break;
    }
}

void QUADRANTS_loop() {
  audio_app.mainloop();
} // Essentially deprecated in favor of ISR

void QUADRANTS_menu() {
    quad_manager.View();
}

void QUADRANTS_screensaver() {
    switch (HS::screensaver_mode) {
    case SCREEN_ZIPS:
    case SCREEN_STARS:
    case SCREEN_ZAPS:
        ZapScreensaver(screensaver_mode - SCREEN_ZAPS);
        break;
    case SCREEN_SCOPE: // output scope
        OC::scope_render();
        break;
    case SCREEN_METERS: // Meters
        quad_manager.BaseScreensaver(true); // show note names
        break;
    default: break; // blank screen
    }
}

void QUADRANTS_handleButtonEvent(const UI::Event &event) {
    quad_manager.HandleButtonEvent(event);
}

void QUADRANTS_handleEncoderEvent(const UI::Event &event) {
    quad_manager.DelegateEncoderMovement(event);
}
