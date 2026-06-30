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
    HEMISPHERE_SETTINGS_COUNT
};

#ifdef __IMXRT1062__
// TODO: consider separate, smaller files - this could get slow
static constexpr int HEM_NR_OF_PRESETS = 50;
static const char* const PRESET_FILENAME = "HEM_PRESETS.DAT";
#elif defined(MOAR_PRESETS)
static constexpr int HEM_NR_OF_PRESETS = 16;
#elif defined(CUSTOM_BUILD) && !defined(PEWPEWPEW)
static constexpr int HEM_NR_OF_PRESETS = 4;
#else
static constexpr int HEM_NR_OF_PRESETS = 8;
#endif

/* Hemisphere Preset
 * - conveniently store/recall multiple configurations
 */
#ifdef __IMXRT1062__
#else
class HemispherePreset : public SystemExclusiveHandler,
    public settings::SettingsBase<HemispherePreset, HEMISPHERE_SETTINGS_COUNT> {
public:
    int GetAppletId(int h) {
        return (h == LEFT_HEMISPHERE) ? values_[HEMISPHERE_SELECTED_LEFT_ID]
                                      : values_[HEMISPHERE_SELECTED_RIGHT_ID];
    }
    HemisphereApplet* GetApplet(int h) {
      int idx = HS::get_applet_index_by_id( GetAppletId(h) );
      return HS::get_applet(idx, h);
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
      // TODO: this is likely broken in v2.0
      uint16_t cvmap = 0;
      uint16_t trigmap = 0;
      for (size_t i = 0; i < 4; ++i) {
        trigmap |= (uint16_t(HS::trigmap[i].source + 1) & 0x0F) << (i*4);
        cvmap |= (uint16_t(HS::cvmap[i].source + 1) & 0x0F) << (i*4);
      }

      bool changed = (uint16_t(values_[HEMISPHERE_TRIGMAP]) != trigmap)
                   || (uint16_t(values_[HEMISPHERE_CVMAP]) != cvmap);
      apply_value(HEMISPHERE_TRIGMAP, trigmap);
      apply_value(HEMISPHERE_CVMAP, cvmap);

      return changed;
    }
    void LoadInputMap() {
      // TODO: this is likely broken in v2.0
      for (size_t i = 0; i < 4; ++i) {
        int val = (uint16_t(values_[HEMISPHERE_TRIGMAP]) >> (i*4)) & 0x0F;
        if (val != 0)
          HS::trigmap[i].source = val - 1;

        val = (uint16_t(values_[HEMISPHERE_CVMAP]) >> (i*4)) & 0x0F;
        if (val != 0)
          HS::cvmap[i].source = val - 1;
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

  // TOTAL EEPROM SIZE: 8 presets * 32 bytes
  SETTINGS_ARRAY_DECLARE() {{
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
  }};
};
SETTINGS_ARRAY_DEFINE(HemispherePreset);

// 1 extra preset for global data... it's a dirty hack for T32.
HemispherePreset hem_presets[HEM_NR_OF_PRESETS + 1];
HemispherePreset *hem_active_preset = 0;
#endif

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Manager
////////////////////////////////////////////////////////////////////////////////

using namespace HS;

void ReceiveManagerSysEx();

OC_APP_CLASS(AppHemisphere, TWOCCS("HS"), "Hemisphere", "Applets"),
  public HSApplication {
public:
  OC_APP_INTERFACE_DECLARE(AppHemisphere,
#ifdef __IMXRT1062__
  0 // no EEPROM needed on T4
#else
  HemispherePreset::storageSize() * HEM_NR_OF_PRESETS + 1
#endif
  );

    void Start() {
        select_mode = -1; // Not selecting
        preset_id = -1;
        queued_preset = -1;
        preset_cursor = 0;
        my_applet[0] = next_applet[0] = -1;
        my_applet[1] = next_applet[1] = -1;

        config_cursor = 0;
        config_page = 0;
        dummy_count = 0;

        zoom_cursor = 0;
        click_tick = 0;
        first_click = -1;

        zoom_slot = -1;
        clock_setup = 0;

        SetApplet(LEFT_HEMISPHERE, HS::get_applet_index_by_id(18)); // DualTM
        SetApplet(RIGHT_HEMISPHERE, HS::get_applet_index_by_id(15)); // EuclidX
    }

    void Resume() {
#ifdef __IMXRT1062__
        // This loads only from LFS, assuming Hemisphere is only used on T40 without SD cards...
        if (PhzConfig::load_config(PRESET_FILENAME))
          LoadGlobals();

        if (preset_id < 0)
          LoadFromPreset(0);
#else
        if (!hem_active_preset)
            LoadFromPreset(0);
#endif
    }
    void Suspend() {
#ifdef __IMXRT1062__
        if (HS::auto_save_enabled)
            StoreToPreset(preset_id);
#else
        if (hem_active_preset) {
            if (HS::auto_save_enabled || 0 == preset_id) StoreToPreset(preset_id, !HS::auto_save_enabled);
            hem_active_preset->OnSendSysEx();
        }
#endif
    }

#if defined(__MK20DX256__)
    void StoreToPreset(HemispherePreset* preset, bool skip_eeprom = false) {
        bool doSave = (preset != hem_active_preset);

        hem_active_preset = preset;
        for (int h = 0; h < 2; h++)
        {
            int index = my_applet[h];
            if (hem_active_preset->GetAppletId(HEM_SIDE(h)) != appletIds[index])
                doSave = 1;
            hem_active_preset->SetAppletId(HEM_SIDE(h), appletIds[index]);

            uint64_t data = HS::get_applet(index, h)->OnDataRequest();
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

        // initiate actual EEPROM save - ONLY if necessary!
        if (doSave && !skip_eeprom) {
          // initiate actual EEPROM save
          OC::CORE::app_isr_enabled = false;
          //OC::draw_save_message(32);
          OC::save_app_data();
          //OC::draw_save_message(64);
          OC::CORE::app_isr_enabled = true;

          PokePopup(HS::MESSAGE_POPUP, HS::PRESET_SAVED);
        }
    }
#endif

    // lower 9 bits of PhzConfig KEY
    enum PresetDataKeys : uint16_t {
        // preset data, 0-99
        APPLET_METADATA_KEY = 0, // applet ids
        CLOCK_DATA_KEY = 1,
        GLOBALS_KEY = 2,
        OLD_INPUT_MAP_KEY = 3,

        OUTSKIP_KEY = 4,
        OLD_TRIGMAP_KEY = 5, // 4 x 16-bit DigitalInputMap
        CVMAP_KEY = 6, // 4 x 16-bit CVInputMap

        OUTSLEW_KEY = 7,

        OUTATTEN_KEY = 8,
        INSKIP_KEY = 9,

        APPLET_L_DATA_KEY = 10,
        APPLET_R_DATA_KEY = 11,

        TRIGMAP_KEY = 20, // 4 x 32-bit DigitalInputMap

        // globals, 100-500
        FILTERMASK1_KEY = 100,
        FILTERMASK2_KEY = 101,

        PC_CHANNEL_KEY = 110,
        PRESET_JUMP_KEY = 111,

        MIDI_MAPS_KEY  = 150, // + 0..32

        Q_ENGINE_KEY   = 200, // + slot number

        // 300-500 = Sequences (aka Patterns)
        SEQUENCES_KEY  = 300, // + blob index

        VERSION_KEY = 0xFFFF
    };

    void StoreToPreset(int id, bool skip_eeprom = false) {
#ifdef __IMXRT1062__
        uint16_t preset_key = id << 9;

        // clock data
        clock_data = ClockSetup_instance.OnDataRequest();
        PhzConfig::setValue(preset_key | CLOCK_DATA_KEY, clock_data);

        // vague globals
        global_data = ClockSetup_instance.GetGlobals();
        PhzConfig::setValue(preset_key | GLOBALS_KEY, global_data);

        uint64_t data = 0;
        // Input Mappings
        data = PackPackables(HS::trigmap[0], HS::trigmap[1]);
        PhzConfig::setValue(preset_key | TRIGMAP_KEY, data);
        data = PackPackables(HS::trigmap[2], HS::trigmap[3]);
        PhzConfig::setValue(preset_key | (TRIGMAP_KEY + 1), data);

        data = PackPackables(HS::cvmap[0], HS::cvmap[1], HS::cvmap[2], HS::cvmap[3]);
        PhzConfig::setValue(preset_key | CVMAP_KEY, data);

        data = 0;
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.clockinskip[i]);
        }
        PhzConfig::setValue(preset_key | INSKIP_KEY, data);
        data = 0;
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.clockoutskip[i]);
        }
        PhzConfig::setValue(preset_key | OUTSKIP_KEY, data);
        data = 0;
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.output_slew[i]);
        }
        PhzConfig::setValue(preset_key | OUTSLEW_KEY, data);
        data = 0;
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
          Pack(data, PackLocation{i*8, 8}, static_cast<uint8_t>(HS::frame.output_atten[i]));
        }
        PhzConfig::setValue(preset_key | OUTATTEN_KEY, data);

        data = 0;
        for (size_t h = 0; h < 2; h++)
        {
            int index = my_applet[h];
            Pack(data, PackLocation{h*8,8}, appletIds[index]);

            // applet data
            applet_data[h] = HS::get_applet(index, h)->OnDataRequest();
            PhzConfig::setValue(preset_key | (APPLET_L_DATA_KEY + h), applet_data[h]);
        }

        // applet ids, and maybe some other stuff?
        PhzConfig::setValue(preset_key | APPLET_METADATA_KEY, data);

        // -- Globals (per file) --
        PhzConfig::setValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::setValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        PhzConfig::setValue(PC_CHANNEL_KEY, HS::frame.MIDIState.pc_channel);

        data = PackPackables(jump_trig_);
        PhzConfig::setValue(PRESET_JUMP_KEY, data);

        // Global quantizer settings
        for (size_t qslot = 0; qslot < QUANT_CHANNEL_COUNT; ++qslot) {
          /* TODO
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

        if (PhzConfig::save_config(PRESET_FILENAME))
          PokePopup(HS::MESSAGE_POPUP, HS::PRESET_SAVED);
#else
        StoreToPreset( (HemispherePreset*)(hem_presets + id), skip_eeprom );
#endif
        preset_id = id;
    }
    void LoadFromPreset(int id) {
        preset_id = id;
#ifdef __IMXRT1062__
        // T4.x uses a LittleFS file via PhzConfig
        uint16_t preset_key = id << 9;
        uint64_t data;

        // applet ids + misc
        if (!PhzConfig::getValue(preset_key | APPLET_METADATA_KEY, data)) return;
        if (!data) return;

        for (size_t h = 0; h < 2; h++)
        {
            int index = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );

            // applet data
            PhzConfig::getValue(preset_key | (APPLET_L_DATA_KEY + h), applet_data[h]);
            SetApplet(HEM_SIDE(h), index);
            HS::get_applet(index, h)->OnDataReceive(applet_data[h]);
        }

        // clock data
        if (!PhzConfig::getValue(preset_key | CLOCK_DATA_KEY, clock_data)) return;
        ClockSetup_instance.OnDataReceive(clock_data);
        // if the first key exists, we are assuming the rest are present...

        // vague globals
        PhzConfig::getValue(preset_key | GLOBALS_KEY, global_data);
        ClockSetup_instance.SetGlobals(global_data);

        // Input Mappings

        if (PhzConfig::getValue(preset_key | TRIGMAP_KEY, data)) {
          UnpackPackables(data, HS::trigmap[0], HS::trigmap[1]);
          PhzConfig::getValue(preset_key | (TRIGMAP_KEY + 1), data);
          UnpackPackables(data, HS::trigmap[2], HS::trigmap[3]);
        } else if (PhzConfig::getValue(preset_key | OLD_TRIGMAP_KEY, data)) {
          // migrate from v1.x
          uint16_t mapdata[4];
          UnpackPackables(data, mapdata[0], mapdata[1], mapdata[2], mapdata[3]);
          HS::trigmap[0].Unpack(mapdata[0]);
          HS::trigmap[1].Unpack(mapdata[1]);
          HS::trigmap[2].Unpack(mapdata[2]);
          HS::trigmap[3].Unpack(mapdata[3]);
        }

        if (PhzConfig::getValue(preset_key | CVMAP_KEY, data)) {
          UnpackPackables(data, HS::cvmap[0], HS::cvmap[1], HS::cvmap[2], HS::cvmap[3]);

          PhzConfig::getValue(preset_key | OUTSKIP_KEY, data);
          for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i)
          {
            HS::frame.clockoutskip[i] = Unpack(data, PackLocation{i*8, 8});
          }
        }

        data = 0;
        PhzConfig::getValue(preset_key | INSKIP_KEY, data);
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i)
        {
          HS::frame.clockinskip[i] = Unpack(data, PackLocation{i*8, 8});
        }

        PhzConfig::getValue(preset_key | OUTSLEW_KEY, data);
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i)
        {
          HS::frame.output_slew[i] = Unpack(data, PackLocation{i*8, 8});
        }

        const bool has_output_atten = PhzConfig::getValue(preset_key | OUTATTEN_KEY, data);
        for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i)
        {
          HS::frame.output_atten[i] = has_output_atten ? Unpack(data, PackLocation{i*8, 8}) : 60;
        }

#else
        // T3.2 uses EEPROM interface
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
                HS::get_applet(index, h)->OnDataReceive(applet_data[h]);
            }
        }
#endif
        PokePopup(PRESET_POPUP);
    }

    void LoadGlobals() {
        // --- Global stuff ---
        // (per file, not per preset)
        PhzConfig::getValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::getValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        uint64_t data;
        if (PhzConfig::getValue(PC_CHANNEL_KEY, data)) HS::frame.MIDIState.pc_channel = (uint8_t) data;

        if (PhzConfig::getValue(PRESET_JUMP_KEY, data))
          UnpackPackables(data, jump_trig_);

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
    }

    void ProcessQueue() {
      if (queued_preset >= 0) {
        LoadFromPreset(queued_preset);
        queued_preset = -1;
      }
    }
    void QueuePresetLoad(int slot) {
      if (HS::clock_m.IsRunning()) {
        queued_preset = slot;
        HS::clock_m.BeatSync([this]() { ProcessQueue(); });
      } else
        LoadFromPreset(slot);
    }
    void JumpToNextPreset() {
      int next_id = preset_id + 1;
      while (!isValidPreset(next_id) && next_id != preset_id) {
        ++next_id %= HEM_NR_OF_PRESETS;
      }
      if (next_id != preset_id)
        QueuePresetLoad(next_id);
    }
#ifdef __IMXRT1062__
#else
    // T32 hacks for extra settings
    void StoreExtras() {
      // store hidden applet mask in secret preset
      hem_presets[HEM_NR_OF_PRESETS].SetData(HEM_SIDE(0), HS::hidden_applets[0]);
      hem_presets[HEM_NR_OF_PRESETS].SetData(HEM_SIDE(1), HS::hidden_applets[1]);

      hem_presets[HEM_NR_OF_PRESETS].SetGlobals(HS::frame.MIDIState.pc_channel);

      uint64_t data = PackPackables(jump_trig_);
      hem_presets[HEM_NR_OF_PRESETS].SetClockData(data);
    }
    void LoadExtras() {
      HS::hidden_applets[0] = hem_presets[HEM_NR_OF_PRESETS].GetData(HEM_SIDE(0));
      HS::hidden_applets[1] = hem_presets[HEM_NR_OF_PRESETS].GetData(HEM_SIDE(1));

      HS::frame.MIDIState.pc_channel =
        constrain((int)hem_presets[HEM_NR_OF_PRESETS].GetGlobals(), 0, 17);

      uint64_t data = hem_presets[HEM_NR_OF_PRESETS].GetClockData();
      UnpackPackables(data, jump_trig_);
    }
#endif

    // does not modify the preset, only the manager
    void SetApplet(HEM_SIDE hemisphere, int index) {
        if (index == my_applet[hemisphere]) return;
        /*noInterrupts();*/
        int oldidx = my_applet[hemisphere];
        HS::get_applet(index, hemisphere)->BaseStart(hemisphere);
        next_applet[hemisphere] = my_applet[hemisphere] = index;
        if (oldidx >= 0 && oldidx < HEMISPHERE_AVAILABLE_APPLETS)
          HS::get_applet(oldidx, hemisphere)->Unload();
        /*interrupts();*/
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet[h], dir);
        next_applet[h] = index;
    }

    template <typename T1>
    void ProcessMIDI(T1 &device) {
        HS::IOFrame &f = HS::frame;
        int load_slot = -1;

        while (timeout < 60 && device.read()) {
            const uint8_t message = device.getType();
            const uint8_t data1 = device.getData1();
            const uint8_t data2 = device.getData2();

            if (message == usbMIDI.SystemExclusive) {
                ReceiveManagerSysEx();
                continue;
            }

            if (message == usbMIDI.ProgramChange
            && (device.getChannel() == f.MIDIState.pc_channel || f.MIDIState.pc_channel == f.MIDIState.PC_OMNI)) {
                load_slot = device.getData1();
                //continue;
            }

            f.MIDIState.ProcessMIDIMsg({device.getChannel(), message, data1, data2});
        }
        if (load_slot >= 0 && load_slot < HEM_NR_OF_PRESETS) {
            QueuePresetLoad(load_slot);
        }
    }

    void mainloop() {
        timeout = 0;
        // top-level MIDI-to-CV handling - alters frame outputs
        ProcessMIDI(usbMIDI);
    }

    void Controller() {
        // Clock Setup applet handles internal clock duties
        ClockSetup_instance.Controller();
        // ^ this will process the queue and load presets

        // this might be triggered by the internal clock...
        if (jump_trig_.Clock() && !HS::clock_m.auto_reset) {
          JumpToNextPreset();
          // ^ this will sometimes queue a preset load

          // The paradox is we need to process the clock first, in case jump_trig needs it,
          // but then jump_trig will queue another preset load,
          // so we have to process the queue again.
          if (jump_trig_.is_clock()) ProcessQueue();
        }

        // execute Applets
        for (int h = 0; h < 2; h++)
        {
            int index = my_applet[h];

            if (HS::clock_m.auto_reset)
                HS::get_applet(index, h)->Reset();

            HS::get_applet(index, h)->Controller();
        }
        HS::clock_m.auto_reset = false;

        HemisphereApplet::ProcessCursors();
    }

    void DrawFullScreen() const {
      int index = my_applet[zoom_slot];

      if (select_mode == zoom_slot) {
        showhide_cursor.Scroll(next_applet[zoom_slot] - showhide_cursor.cursor_pos());
        DrawAppletList(CursorBlink());
        // dotted screen border during applet select
        gfxFrame(0, 0, 128, 64, true);
        return;
      }

      HS::get_applet(index, zoom_slot)->BaseView(true, zoom_cursor < 0);

      // draw cursor for editing applet select and input maps
      if (zoom_cursor < 0) {
        gfxIcon(64 - 8*zoom_slot, 1, DOWN_ICON, true);
      } else if (0 == zoom_cursor) {
        if (select_mode != zoom_slot && CursorBlink())
          gfxIcon(64 - 8*zoom_slot, 1, zoom_slot? RIGHT_ICON : LEFT_ICON, true);
      } else if (isEditing) {
        const int x = 64*((zoom_cursor-1)%2);
        const int y = 13 + 10*((zoom_cursor-1)/2);
        gfxInvert(x, y, 19, 9);
        gfxFrame(x, y, 19, 9, true);

        if (zoom_cursor <= 2) {
          // trigmap & clock multiplier
          const int io_chan = zoom_slot * 2 + zoom_cursor - 1;
          const int mult = HS::clock_m.GetMultiply(io_chan);

          graphics.clearRect(x, y - 9, 36, 8);
          graphics.drawBitmap8(x, y - 9, 8, RIGHT_ICON);
          graphics.drawBitmap8(x + 8, y - 9, 8, CLOCK_ICON);
          graphics.setPrintPos(x + 16, y - 9);
          graphics.print((mult >= 0) ? "x" : "/");
          graphics.print((mult >= 0) ? mult : 1 - mult);
        }
        if (zoom_cursor >= 5) {
          gfxIcon(x + 18, y + 1, DOWN_ICON, true);

          graphics.clearRect(0, y + 10, 127, 20);
          gfxPrint(x, y+10, "Slew=");
          gfxPrint(HS::frame.output_slew[zoom_slot*2 + zoom_cursor-5]);
          gfxPrint("%");

          const int att = Atten(HS::frame.output_atten[zoom_slot*2 + zoom_cursor-5]);
          gfxPrint(x, y+20, "Lvl=");
          if (att < 0) gfxPrint("-");
          graphics.printf("%d.%d%%", abs(att) / 10, abs(att) % 10);
        }
      } else {
        if (CursorBlink()) {
          const int x = 18 + 64*((zoom_cursor-1)%2);
          const int y = 14 + 10*((zoom_cursor-1)/2);
          gfxIcon(x, y, LEFT_ICON, true);
        }
      }

      gfxDisplayInputMapEditor();
    }

    void View() const {
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

          case MIDI_MAPS:
            DrawMidiMaps(config_cursor - MIDIMAP1);
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
        else if (HS::midi_edit)
          PokePopup(MIDI_POPUP);

        if (draw_applets) {
          if (zoom_slot > -1) {
            DrawFullScreen();
          } else {
            for (int h = 0; h < 2; h++)
            {
                int index = my_applet[h];
                HS::get_applet(index, h)->BaseView();
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
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS * 4) {
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

        // button down
        if (down) {
            // Clock Setup is more immediate for manual triggers
            if (clock_setup) ClockSetup_instance.OnButtonPress();
            // TODO: consider a new OnButtonDown handler for applets
            return;
        }

        // button release
        if (zoom_slot > -1) {
          switch (zoom_cursor) {
            case -1:
            {
              int index = my_applet[zoom_slot];
              HS::get_applet(index, zoom_slot)->OnButtonPress();
              break;
            }

            case 0:
              if (zoom_slot == select_mode) {
                SetApplet(HEM_SIDE(zoom_slot), next_applet[zoom_slot]);
                SetFullScreen(-1);
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
              if (h == RIGHT_HEMISPHERE && CheckEditInputMapPress(
                    zoom_cursor,
                    IndexedInput(1, trigmap[zoom_slot*2]),
                    IndexedInput(2, trigmap[zoom_slot*2+1])
                  ))
                break;
            case 5:
            case 6:
            default:
              isEditing = !isEditing;
              break;
          }
          return;
        }
        if (select_mode == h) {
            select_mode = -1; // Pushing a button for the selected side turns off select mode
        } else if (!clock_setup) {
            // regular applets get button release
            int index = my_applet[h];
            HS::get_applet(index, h)->OnButtonPress();
        }
    }

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
                SetFullScreen(-1);
                select_mode = -1;
                ClearEditInputMap();
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME) {
                // This is a double-click on one button. Activate corresponding help screen and deactivate select mode.
                if (hemisphere == first_click)
                    SetFullScreen(hemisphere);

                // reset double-click timer either way
                click_tick = 0;
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            // -- Single click
            // If a help screen is already selected, and the button is for
            // the opposite one, go to the other help screen
            if (zoom_slot > -1) {
                if (zoom_slot != hemisphere) SetFullScreen(hemisphere);
                else SetFullScreen(-1); // Exit help screen if same button is clicked
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
          HemisphereApplet* applet = HS::get_applet(index, hemisphere);

          if (applet->EditMode()) {
            // select button becomes aux button while editing a param
            applet->AuxButton();
            click_tick = 0;
          } else {
            if (hemisphere == select_mode) select_mode = -1; // Exit Select Mode if same button is pressed
            else select_mode = hemisphere;
          }
        }
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        HEM_SIDE h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        int increment = event.value;
        if (event.mask & (OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R)) {
          // push-and-turn for coarse adjustments
          OC::ui.SetButtonIgnoreMask();
          increment *= 10;
        }

        if (HS::q_edit) {
          HS::QEditEncoderMove(h, increment);
          return;
        }
        if (HS::midi_edit) {
          HS::MEditEncoderMove(h, increment);
          return;
        }

        if (view_state == CONFIG_MENU) {
          ConfigEncoderAction(h, increment);
          return;
        }

        if (clock_setup) {
          if (h == LEFT_HEMISPHERE)
            ClockSetup_instance.OnLeftEncoderMove(increment);
          else
            ClockSetup_instance.OnEncoderMove(increment);

          return;
        }

        // Fullscreen cursor stuff
        if (zoom_slot > -1) {
          if (select_mode == zoom_slot) ChangeApplet(HEM_SIDE(zoom_slot), increment);
          else if (!isEditing && LEFT_HEMISPHERE == h) // left enc jumps between applet or config
            zoom_cursor = (event.value > 0)? 0 : -1;
          else if (zoom_cursor < 0) { // right enc is normal applet behavior
            int index = my_applet[zoom_slot];
            HS::get_applet(index, zoom_slot)->OnEncoderMove(increment);
          } else if (isEditing) { // either enc changes config value
            switch (zoom_cursor)
            {
              case 1:
              case 2:
              {
                int chan = zoom_slot*2 + zoom_cursor - 1;
                if (h == LEFT_HEMISPHERE) {
                  clock_m.SetMultiply(clock_m.GetMultiply(chan) + increment, chan);
                } else if (!EditSelectedInputMap(increment))
                  HS::trigmap[zoom_slot*2 + zoom_cursor - 1].ChangeSource(increment);
                break;
              }
              case 3:
              case 4:
                if (!EditSelectedInputMap(increment))
                  HS::cvmap[zoom_slot*2 + zoom_cursor - 3].ChangeSource(increment);
                break;
              case 5:
              case 6:
                if (h == LEFT_HEMISPHERE)
                  HS::frame.NudgeAtten(zoom_slot*2 + zoom_cursor - 5, increment);
                else
                  HS::frame.NudgeSlew(zoom_slot*2 + zoom_cursor - 5, increment);
                break;
              default:
                isEditing = false;
                break;
            }
          } else { // right enc moves cursor
            zoom_cursor = constrain(zoom_cursor + increment, 0, 6);
            ResetCursor();
          }
        } else if (select_mode == h) {
          // old style select mode
          ChangeApplet(h, increment);
          SetApplet(h, next_applet[h]);
        } else {
            int index = my_applet[h];
            HS::get_applet(index, h)->OnEncoderMove(increment);
        }
    }

    void ToggleConfigMenu() {
      if (view_state != CONFIG_MENU) {
        view_state = CONFIG_MENU;
        //SetFullScreen(-1);
      } else {
        view_state = APPLETS;
      }
    }
    void ShowPresetSelector() {
        view_state = CONFIG_MENU;
        config_cursor = LOAD_PRESET;
        preset_cursor = preset_id + 1;
    }

    void SetFullScreen(int hemisphere) {
        zoom_slot = hemisphere;
        select_mode = -1;
        isEditing = false;
        ClearEditInputMap();
    }

private:
    int preset_id = -1;
    int queued_preset = -1;
    int preset_cursor = 0;
    int my_applet[2]; // Indexes to applets
    int next_applet[2]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[2]; // cache of applet data
    bool clock_setup;
    int config_cursor = 0;
    int config_page = 0;
    int dummy_count = 0;

    int select_mode = -1;
    int zoom_slot; // Which of the hemispheres (if any) is in fullscreen/help mode, -1 if none
    int zoom_cursor; // 0=select; 1,2=trigmap; 3,4=cvmap; 5,6=outmode
    uint32_t click_tick; // Measure time between clicks for double-click
    int first_click; // The first button pushed of a double-click set, to see if the same one is pressed

    elapsedMicros timeout = 0;

    // State machine
    enum HEMView {
      APPLETS,
      //APPLET_FULLSCREEN,
      CONFIG_MENU,
      PRESET_PICKER,
      CLOCK_SETUP,
    };
    HEMView view_state = APPLETS;

    enum HEMConfigCursor {
        DELETE_PRESET,
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        RANDOMIZER, // past this point goes full screen

        // General Settings
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,
        PRESET_JUMP_TRIG,
        MIDI_BEND_RANGE,
        MIDI_PC_CHANNEL,
        AUTO_MIDI,
        MIDI_POLY_MODE,
        // MIDI_THRU_TOGGLE,

        // Input Remapping
        TRIGMAP1, TRIGMAP2, TRIGMAP3, TRIGMAP4,
        CVMAP1, CVMAP2, CVMAP3, CVMAP4,

        // Global Quantizers: 4x(Scale, Root, Octave, Mask?)
        QUANT1, QUANT2, QUANT3, QUANT4,
        QUANT5, QUANT6, QUANT7, QUANT8,

        // MIDI Mappings
        MIDIMAP1, MIDIMAP2, MIDIMAP3, MIDIMAP4,
        MIDIMAP5, MIDIMAP6, MIDIMAP7, MIDIMAP8,
        MIDIMAP9, MIDIMAP10, MIDIMAP11, MIDIMAP12,
        MIDIMAP13, MIDIMAP14, MIDIMAP15, MIDIMAP16,
        MIDIMAP17, MIDIMAP18, MIDIMAP19, MIDIMAP20,
        MIDIMAP21, MIDIMAP22, MIDIMAP23, MIDIMAP24,
        MIDIMAP25, MIDIMAP26, MIDIMAP27, MIDIMAP28,
        MIDIMAP29, MIDIMAP30, MIDIMAP31, MIDIMAP32,

        // Applet visibility (dummy position)
        SHOWHIDELIST,

        MAX_CURSOR = MIDIMAP32
    };

    enum HEMConfigPage {
      LOADSAVE_POPUP,
      CONFIG_SETTINGS,
      INPUT_SETTINGS,
      QUANTIZER_SETTINGS,
      MIDI_MAPS,
      SHOWHIDE_APPLETS,

      LAST_PAGE = SHOWHIDE_APPLETS
    };

    void ConfigEncoderAction(const int h, const int dir) {
        if (!isEditing && !preset_cursor) {
          if (h == 0) { // change pages
            config_page = constrain(config_page + dir, 0, LAST_PAGE);

            const int cursorpos[] = { LOAD_PRESET, TRIG_LENGTH, TRIGMAP1, QUANT1, MIDIMAP1, SHOWHIDELIST };
            config_cursor = cursorpos[config_page];
          } else if (config_page == SHOWHIDE_APPLETS) {
            showhide_cursor.Scroll(dir);
          } else { // move cursor
            config_cursor = constrain(config_cursor + dir, LOAD_PRESET, MAX_CURSOR);

            if (config_cursor <= RANDOMIZER) config_page = LOADSAVE_POPUP;
            else if (config_cursor < TRIGMAP1) config_page = CONFIG_SETTINGS;
            else if (config_cursor < QUANT1) config_page = INPUT_SETTINGS;
            else if (config_cursor < MIDIMAP1) config_page = QUANTIZER_SETTINGS;
            else if (config_cursor < SHOWHIDELIST) config_page = MIDI_MAPS;
            //else config_page = SHOWHIDE_APPLETS;

            ResetCursor();
          }
          if (config_cursor > RANDOMIZER) HS::popup_tick = 0;
          return;
        }

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            if (!EditSelectedInputMap(dir))
              HS::trigmap[config_cursor-TRIGMAP1].ChangeSource(dir);
            break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
            if (!EditSelectedInputMap(dir))
              HS::cvmap[config_cursor-CVMAP1].ChangeSource(dir);
            break;
        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        case PRESET_JUMP_TRIG:
            if (!EditSelectedInputMap(dir))
              jump_trig_.ChangeSource(dir);
            break;
        case MIDI_BEND_RANGE:
            HS::frame.MIDIState.bend_range =
              constrain(HS::frame.MIDIState.bend_range + dir, 0, 36);
            break;
        case MIDI_PC_CHANNEL:
            HS::frame.MIDIState.pc_channel =
              constrain(HS::frame.MIDIState.pc_channel + dir, 0, 17);
            break;
        case MIDI_POLY_MODE:
            HS::frame.MIDIState.poly_mode =
              constrain(HS::frame.MIDIState.poly_mode + dir, 0, HS::MIDIPolyMode::POLY_LAST);
            break;
        case SCREENSAVER_MODE:
            HS::screensaver_mode = constrain(HS::screensaver_mode + dir, 0, SCREENSAVER_MODE_COUNT - 1);
            break;
        case DELETE_PRESET:
        case LOAD_PRESET:
        case SAVE_PRESET:
            if (h == 0) {
              config_cursor = constrain(config_cursor + dir, DELETE_PRESET, SAVE_PRESET);
            } else {
              preset_cursor = constrain(preset_cursor + dir, 1, HEM_NR_OF_PRESETS);
            }
            break;
        }
    }
    void DeletePreset(int id) {
#ifdef __IMXRT1062__
      uint16_t preset_key = id << 9;
      // non-global values are all 0-99 in the enum
      for (int i = 0; i < 100; ++i) {
        PhzConfig::deleteKey(preset_key | i);
      }
#else
      hem_presets[id].SetAppletId(0, 0);
#endif
    }
    void ConfigButtonPush(int h) {
        if (preset_cursor) {
          // Save or Load on button push
          switch (config_cursor) {
            case DELETE_PRESET:
              DeletePreset(preset_cursor - 1);
              break;
            case SAVE_PRESET:
              StoreToPreset(preset_cursor - 1);
              break;
            case LOAD_PRESET:
              QueuePresetLoad(preset_cursor - 1);
              break;
          }

          preset_cursor = 0; // deactivate preset selection
          view_state = APPLETS;
          isEditing = false;
          return;
        }

        switch (config_cursor) {
        case RANDOMIZER:
            ++dummy_count;
            // reset input mappings to defaults
            HS::ResetMappings();
            // randomize both applets
            for (int ch = 0; ch < 2; ++ch) {
              SetApplet(HEM_SIDE(ch), random(HEMISPHERE_AVAILABLE_APPLETS));
            }
            break;

        case DELETE_PRESET:
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
          if (CheckEditInputMapPress(
                config_cursor,
                IndexedInput(CVMAP1, cvmap[0]),
                IndexedInput(CVMAP2, cvmap[1]),
                IndexedInput(CVMAP3, cvmap[2]),
                IndexedInput(CVMAP4, cvmap[3])
              ))
            break;
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            if (CheckEditInputMapPress(
                  config_cursor,
                  IndexedInput(TRIGMAP1, trigmap[0]),
                  IndexedInput(TRIGMAP2, trigmap[1]),
                  IndexedInput(TRIGMAP3, trigmap[2]),
                  IndexedInput(TRIGMAP4, trigmap[3])
                ))
              break;
        case TRIG_LENGTH:
        case MIDI_BEND_RANGE:
        case MIDI_PC_CHANNEL:
        case MIDI_POLY_MODE:
        case SCREENSAVER_MODE:
            isEditing = !isEditing;
            break;

        case PRESET_JUMP_TRIG:
          if (!CheckEditInputMapPress(
                config_cursor, IndexedInput(PRESET_JUMP_TRIG, jump_trig_)
              ))
            isEditing ^= 1;
          break;

        case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
            break;

        case AUTO_MIDI:
            HS::frame.autoMIDIOut = !HS::frame.autoMIDIOut;
            break;

            /*
        case MIDI_THRU_TOGGLE:
            HS::midi_thru_enabled = !HS::midi_thru_enabled;
            break;
            */

        case SHOWHIDELIST:
            if (h == 0) // left encoder inverts selection
            {
              HS::hidden_applets[0] = ~HS::hidden_applets[0];
              HS::hidden_applets[1] = ~HS::hidden_applets[1];
            }
            else // right encoder toggles current
              HS::showhide_applet(showhide_cursor.cursor_pos());
            break;
        default: {
            // I'm not going to paste 32 different MIDI cursor positions so it's just the default :P
            int midx = constrain(config_cursor - MIDIMAP1, 0, 31);
            HS::MidiMapEdit(midx);
            break;
          }
        }
    }

    void DrawInputMappings() const {
        gfxHeader("<  Input Mapping  >");
        gfxIcon(25, 19, TR_ICON);
        gfxIcon(89, 19, TR_ICON);
        gfxIcon(25, 39, CV_ICON);
        gfxIcon(89, 39, CV_ICON);

        for (int ch=0; ch<4; ++ch) {
          // Physical trigger input mappings
          gfxPrint(4 + ch*32, 25, HS::trigmap[ch].InputName() );

          // Physical CV input mappings
          gfxPrint(4 + ch*32, 45, HS::cvmap[ch].InputName() );
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

        gfxDisplayInputMapEditor();
    }

    void DrawQuantizerConfig() const {
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

    void DrawConfigMenu() const {
        // --- Config Selection
        gfxHeader("< General Settings >");

        const int NUM_ROWS = TRIGMAP1 - TRIG_LENGTH; // lol weird
        const int SHOW_ROWS = 6;
        const int ROW_HEIGHT = 8;

        int scroll_top = config_cursor - TRIG_LENGTH - 2;
        CONSTRAIN(scroll_top, 0, NUM_ROWS - SHOW_ROWS);

        // Draw 6 visible rows from scroll_top (scroll_top is a row index)
        for (int i = 0; i < SHOW_ROWS; ++i) {
            int row = scroll_top + i;
            if (row >= NUM_ROWS) break;
            HS::DrawConfigRow(
              row,
              15 + i * ROW_HEIGHT,
              (config_cursor - TRIG_LENGTH) == row,
              EditMode()
            );
        }

        // Scroll arrows
        if (scroll_top > 0)
            gfxIcon(121, 14, UP_ICON);
        if (scroll_top + SHOW_ROWS < NUM_ROWS)
            gfxIcon(121, 56, DOWN_ICON);

        gfxDisplayInputMapEditor();
    }

    bool isValidPreset(int id) {
#ifdef __IMXRT1062__
      uint64_t data;
      return PhzConfig::getValue(id << 9 | APPLET_METADATA_KEY, data);
#else
      return hem_presets[id].is_valid();
#endif
    }

    HemisphereApplet* GetApplet(int id, size_t h) const {
        // TODO: names and icons should be static... then again,
        //       using get_applet() will make sure applets are available
        //       as soon as presets are enumerated
#ifdef __IMXRT1062__
        uint64_t data = 0;
        PhzConfig::getValue(id << 9 | APPLET_METADATA_KEY, data);
        int idx = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );
        return HS::get_applet(idx, h);
#else
        return hem_presets[id].GetApplet(h);
#endif
    }
    void DrawPresetSelector() const {
        const char * const hdrtxt[] = { "DEL!", "Load", "Save", "???" };
        gfxHeader(hdrtxt[config_cursor]);
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

            if (!isValidPreset(i))
                gfxPrint(18, y, "(empty)");
            else {
                gfxIcon(18, y, GetApplet(i, 0)->applet_icon());
                gfxPrint(26, y, GetApplet(i, 0)->applet_name());
                gfxPrint(", ");
                gfxPrint(GetApplet(i, 1)->applet_name());
                gfxIcon(120, y, GetApplet(i, 1)->applet_icon(), true);
            }

            y += 10;
        }
    }

};

void ReceiveManagerSysEx() {
#ifdef __IMXRT1062__
    // TODO: reimplement SysEx backup
#else
    if (hem_active_preset)
        hem_active_preset->OnReceiveSysEx();
#endif
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void AppHemisphere::Init() {
  BaseStart();
}

#ifdef __IMXRT1062__
size_t AppHemisphere::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  return 0;
}
size_t AppHemisphere::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  return 0;
}
#else
size_t AppHemisphere::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  StoreExtras();

  for (int i = 0; i < HEM_NR_OF_PRESETS + 1; ++i) {
    hem_presets[i].Save(stream_buffer);
  }
  return stream_buffer.written();
}

size_t AppHemisphere::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  for (int i = 0; i < HEM_NR_OF_PRESETS + 1; ++i) {
    hem_presets[i].Restore(stream_buffer);
  }

  // using secret hidden preset for extra data
  LoadExtras();

  return stream_buffer.read();
}
#endif

void AppHemisphere::Process(OC::IOFrame *ioframe) {
  BaseController(ioframe);
}
void AppHemisphere::GetIOConfig(OC::IOConfig &ioconfig) const
{
  using namespace OC;
  ioconfig.digital_inputs[DIGITAL_INPUT_1].set("TR1");
  ioconfig.digital_inputs[DIGITAL_INPUT_2].set("TR2");
  ioconfig.digital_inputs[DIGITAL_INPUT_3].set("TR3");
  ioconfig.digital_inputs[DIGITAL_INPUT_4].set("TR4");

  ioconfig.cv[0].set("C 1");
  ioconfig.cv[1].set("C 2");
  ioconfig.cv[2].set("C 3");
  ioconfig.cv[3].set("C 4");

  ioconfig.outputs[0].set("Left A", OUTPUT_MODE_PITCH);
  ioconfig.outputs[1].set("Left B", OUTPUT_MODE_PITCH);
  ioconfig.outputs[2].set("Right C", OUTPUT_MODE_PITCH);
  ioconfig.outputs[3].set("Right D", OUTPUT_MODE_PITCH);
}

void AppHemisphere::HandleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        Suspend();
        break;

    default: break;
    }
}

void AppHemisphere::Loop() {
    mainloop();
}

void AppHemisphere::DrawMenu() const {
    View();
}

void AppHemisphere::DrawScreensaver() const {
    switch (HS::screensaver_mode) {
    case SCREEN_ZIPS:
    case SCREEN_STARS:
    case SCREEN_ZAPS:
        ZapScreensaver(screensaver_mode - SCREEN_ZAPS);
        break;
    case SCREEN_SCOPE:
        OC::scope_render();
        break;
    case SCREEN_METERS:
        BaseScreensaver(true); // show note names
        break;
    case SCREEN_BEATS:
        BeatCounterScreensaver();
        break;
    default: break; // blank screen
    }
}
void AppHemisphere::DrawDebugInfo() const {
  // TODO:
}

void AppHemisphere::HandleButtonEvent(const UI::Event &event) {
    switch (event.type) {
      case UI::EVENT_BUTTON_DOWN:
        if (event.control == OC::CONTROL_BUTTON_M) {
            ToggleClockRun();
            OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
            break;
        }
        if (HS::q_edit) {
          if (event.control == OC::CONTROL_BUTTON_B)
            HS::NudgeOctave(HS::qview, 1);
          else if (event.control == OC::CONTROL_BUTTON_A)
            HS::NudgeOctave(HS::qview, -1);
          else {
            HS::q_edit = 0;
            HS::popup_tick = 0;
            select_mode = -1;
          }

          OC::ui.SetButtonIgnoreMask();
          break;
        }

        if (HS::midi_edit) {
          if (event.control == OC::CONTROL_BUTTON_A) {
            mview = constrain(mview - 1, 0, MIDIMAP_MAX-1);
            config_cursor = MIDIMAP1 + mview;
          } else if (event.control == OC::CONTROL_BUTTON_B) {
            mview = constrain(mview + 1, 0, MIDIMAP_MAX-1);
            config_cursor = MIDIMAP1 + mview;
          } else {
            // TODO: auto-learn from Z button
            HS::midi_edit = 0;
            HS::popup_tick = 0;
            select_mode = -1;
          }
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

        break;

      case UI::EVENT_BUTTON_LONG_PRESS:
        if (event.control == OC::CONTROL_BUTTON_B) ToggleConfigMenu();
            break;

        case UI::EVENT_BUTTON_LONG_RELEASE:
            if (event.control == OC::CONTROL_BUTTON_L) ToggleClockRun();
            if (event.control == OC::CONTROL_BUTTON_R) OC::ui.JumpToMenu();
            break;

      default: break;
    }
}

void AppHemisphere::HandleEncoderEvent(const UI::Event &event) {
    DelegateEncoderMovement(event);
}
