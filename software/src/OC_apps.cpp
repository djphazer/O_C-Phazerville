// Copyright (c) 2016-2019 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
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

#include "OC_core.h"
#include "OC_gpio.h"
#include "OC_scales.h"
#include "OC_ui.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_config.h"
#include "OC_digital_inputs.h"
#include "OC_storage.h"
#include "OC_app_switcher.h"
#include "OC_global_settings.h"
#include "util/util_misc.h"
#include "util/util_settings.h"
#include "util/util_integer_sequences.h"
#include "util/util_trigger_delay.h"
#include "util/util_logistic_map.h"
#include "util/util_arp.h"
#include "util/util_grid.h"
#include "util/util_ringbuffer.h"
#include "util/util_settings.h"
#include "util/util_semitone_quantizer.h"
#include "util/util_sync.h"

#include "OC_input_maps.h"
#include "OC_pitch_utils.h"
#include "OC_euclidean_mask_draw.h"
#include "OC_trigger_delays.h"

#include "OC_calibration.h"
#include "OC_patterns.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_pagestorage.h"
#include "src/drivers/EEPROMStorage.h"
#include "PhzConfig.h"
#include "VBiasManager.h"
#include "HSClockManager.h"

#ifndef NO_HEMISPHERE
// applets
#include "applets/_config.h"
#ifdef ARDUINO_TEENSY41
#include "audio_applets/_config.h"
#endif
#endif

#include "HSApplication.h"
#include "OC_scale_edit.h"
#include "OC_visualfx.h"

// actual apps are included and instantiated here
#include "apps/_config.h"

namespace OC {

// NOTE These are slightly wasteful, in that the PageStorage implementation and
// the local data both retain a copy of the data. Removing this would in theory
// reclaim some memory, although RAM isn't currently an issue.
/*extern*/ DMAMEM GlobalSettings global_settings;
/*extern*/ AppSwitcher app_switcher;
static DMAMEM AppData app_data;
#ifndef __IMXRT1062__
static GlobalSettingsStorage global_settings_storage;
#endif
static DMAMEM AppDataStorage app_data_storage;

#ifdef __IMXRT1062__
enum GlobalSettingsDataKeys : uint16_t {
  // upper 8 bits of key, non-zero
  METADATA_KEY        = 1 << 8, // selected app id, etc.
  USER_SCALES_KEY     = 2 << 8,
  SEQUENCES_KEY       = 3 << 8,
  CHORDS_KEY          = 4 << 8,
  TURING_MACHINES_KEY = 5 << 8,
  WAVEFORMS_KEY       = 6 << 8,
  AUTOCAL_KEY         = 7 << 8,

  // lower 8 bits of key
  SCALE_METADATA = 0xff,
  SCALE_NOTEDATA = 0,
};
#endif

FLASHMEM
static void SaveGlobalSettings() {
  APPS_SERIAL_PRINTLN("Save global settings");

#ifdef __IMXRT1062__
  //PhzConfig::clear_config();
  PhzConfig::load_config(); // use default config file

  // Metadata
  uint64_t data = 0;
  // TODO:
  //global_settings.DAC_scaling = OC::DAC::store_scaling();
  Pack(data, PackLocation{0, 16}, global_settings.current_app_id);
  Pack(data, PackLocation{16, 1}, global_settings.encoders_enable_acceleration);
  Pack(data, PackLocation{17, 1}, 1); // v2.0 first-run validation
  PhzConfig::setValue(METADATA_KEY, data);

  // User Scales
  char filename[] = "000.SCL";
  for (size_t i = 0; i < Scales::SCALE_USER_COUNT; ++i) {
    PhzConfig::setValue(USER_SCALES_KEY | (i << 4) | SCALE_METADATA, uint64_t(user_scales[i].span) << 16 | user_scales[i].num_notes);
    data = 0;
    for (size_t nn = 0; nn < user_scales[i].num_notes; ++nn) {
      Pack(data, PackLocation{(nn & 0x3)*16, 16}, (uint16_t)user_scales[i].notes[nn]);

      // after every 4th value (64 bits), store and reset
      if ((nn & 0x3) == 0x3) {
        PhzConfig::setValue(USER_SCALES_KEY | (i << 4) | (SCALE_NOTEDATA + (nn >> 2)), data);
        data = 0;
      }
    }

    if (SDcard_Ready) {
      filename[2] = char('0' + i);
      SD.remove(filename);
      File file = SD.open(filename, FILE_WRITE_BEGIN);
      if (file) {
        Scales::SaveToScala(user_scales[i], file);
      }
      file.close();
    }
  }

  // User Patterns aka Sequences
  for (size_t i = 0; i < Patterns::PATTERN_USER_COUNT; ++i) {
    data = 0;
    for (size_t step = 0; step < ARRAY_SIZE(Pattern::notes); ++step) {
      Pack(data, PackLocation{(step & 0x3)*16, 16}, (uint16_t)user_patterns[i].notes[step]);
      if ((step & 0x3) == 0x3) {
        PhzConfig::setValue(SEQUENCES_KEY | (i << 3) | (step >> 2), data);
        data = 0;
      }
    }
  }

  // User Chords (progression sequences from Acid Curds)
  for (size_t i = 0; i < Chords::CHORDS_USER_COUNT; ++i) {
    data = 0;
    Pack(data, PackLocation{0, 8}, (uint8_t)user_chords[i].quality);
    Pack(data, PackLocation{8, 8}, (uint8_t)user_chords[i].inversion);
    Pack(data, PackLocation{16,8}, (uint8_t)user_chords[i].voicing);
    Pack(data, PackLocation{24,8}, (uint8_t)user_chords[i].base_note);
    Pack(data, PackLocation{32,8}, (uint8_t)user_chords[i].octave);
    PhzConfig::setValue(CHORDS_KEY | i, data);
  }

  // User Turing Machines (for Enigma and friends)
  for (size_t i = 0; i < HS::TURING_MACHINE_COUNT; ++i) {
    data = 0;
    Pack(data, PackLocation{0, 16}, HS::user_turing_machines[i].reg);
    Pack(data, PackLocation{16, 8}, HS::user_turing_machines[i].len);
    Pack(data, PackLocation{24, 1}, HS::user_turing_machines[i].favorite);
    PhzConfig::setValue(TURING_MACHINES_KEY | i, data);
  }

#ifndef NO_HEMISPHERE
  data = 0;
  // User Waveform (custom VectorOsc shapes)
  for (size_t i = 0; i < HS::VO_SEGMENT_COUNT; ++i) {
    Pack(data, PackLocation{(i & 0x3) * 16, 16}, uint16_t(HS::user_waveforms[i].level) << 8 | HS::user_waveforms[i].time);

    if ((i & 0x3) == 0x3) {
      PhzConfig::setValue(WAVEFORMS_KEY | (i >> 2), data);
      data = 0;
    }
  }
#endif

  // Auto Calibration Data
  /*
  for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
    data = 0;
    PhzConfig::setValue(AUTOCAL_KEY | (0xff - i), auto_calibration_data[i].use_auto_calibration_);

    for (size_t oct = 0; oct < OCTAVES + 1; ++oct) {
      Pack(data, PackLocation{(oct & 0x3) * 16, 16}, auto_calibration_data[i].auto_calibrated_octaves[oct]);
      if ((oct & 0x3) == 0x3) {
        PhzConfig::setValue(AUTOCAL_KEY | (i << 4) | (oct >> 2), data);
        data = 0;
      }
    }
  }
  */

  PhzConfig::save_config(); // save to default config file
#else // --- Teensy 3.2
  memcpy(global_settings.user_scales, OC::user_scales, sizeof(OC::user_scales));
  memcpy(global_settings.user_patterns, OC::user_patterns, sizeof(OC::user_patterns));
#ifdef ENABLE_APP_CHORDS
  memcpy(global_settings.user_chords, OC::user_chords, sizeof(OC::user_chords));
#else
  memcpy(global_settings.user_turing_machines, HS::user_turing_machines, sizeof(HS::user_turing_machines));
#endif
#ifndef NO_HEMISPHERE
  memcpy(global_settings.user_waveforms, HS::user_waveforms, sizeof(HS::user_waveforms));
#endif

  for (int i = 0; i < QUANT_CHANNEL_COUNT; ++i) {
    global_settings.q_engines[i].scale = HS::q_engine[i].scale;
    global_settings.q_engines[i].mask = HS::q_engine[i].mask;
    global_settings.q_engines[i].octave = HS::q_engine[i].octave;
    global_settings.q_engines[i].root_note = HS::q_engine[i].root_note;
  }
  for (int i = 0; i < MIDIMAP_MAX; ++i) {
    global_settings.midi_maps[i].channel       = HS::frame.MIDIState.mapping[i].channel      ;
    global_settings.midi_maps[i].dac_polyvoice = HS::frame.MIDIState.mapping[i].dac_polyvoice;
    global_settings.midi_maps[i].function      = HS::frame.MIDIState.mapping[i].function     ;
    global_settings.midi_maps[i].function_cc   = HS::frame.MIDIState.mapping[i].function_cc  ;
    global_settings.midi_maps[i].transpose     = HS::frame.MIDIState.mapping[i].transpose    ;
    global_settings.midi_maps[i].range_low     = HS::frame.MIDIState.mapping[i].range_low    ;
    global_settings.midi_maps[i].range_high    = HS::frame.MIDIState.mapping[i].range_high   ;
  }

  global_settings_storage.Save(global_settings);
  APPS_SERIAL_PRINTLN("Saved global settings: page_index %d", global_settings_storage.page_index());
#endif
}

/* old eeprom space checking logic
static constexpr size_t total_storage_size() {
    size_t used = 0;
    for (size_t i = 0; i < app_container.num_apps(); ++i) {
        used += app_container[i]->storage_size() + sizeof(AppChunkHeader);
        if (used & 1) ++used; // align on 2-byte boundaries
    }
    return used;
}
static constexpr size_t totalsize = total_storage_size();
static_assert(totalsize < OC::AppData::kAppDataSize, "EEPROM Allocation Exceeded");
*/

FLASHMEM void SaveAppData() {
  SaveGlobalSettings(); // yeah, why not
  APPS_SERIAL_PRINTLN("Save app data... (%u bytes available)", OC::AppData::kAppDataSize);

  app_data.used = 0;
  uint8_t *data = app_data.data;
  uint8_t *data_end = data + OC::AppData::kAppDataSize;

  size_t start_app = random(app_container.num_apps());
  for (size_t i = 0; i < app_container.num_apps(); ++i) {
    const AppBase* app = (AppBase*)app_container[(start_app + i) % app_container.num_apps()].instance;
    if (!app) continue;
    size_t storage_size = app->storage_size() + sizeof(AppChunkHeader);
    if (storage_size & 1) ++storage_size; // Align chunks on 2-byte boundaries
    if (storage_size > sizeof(AppChunkHeader)) {
      if (data + storage_size > data_end) {
        APPS_SERIAL_PRINTLN("%s: ERROR: %u BYTES NEEDED, %u BYTES AVAILABLE OF %u BYTES TOTAL", app->name(), storage_size, data_end - data, AppData::kAppDataSize);
        continue;
      }

      AppChunkHeader *chunk = reinterpret_cast<AppChunkHeader *>(data);
      chunk->id = app->id();
      chunk->length = storage_size;

      util::StreamBufferWriter stream_buffer{chunk + 1, chunk->length};
      auto result = app->Save(stream_buffer);
      if (stream_buffer.overflow()) {
        APPS_SERIAL_PRINTLN("* %s (%02x) : Save overflowed, result=%u, skipping app...", 
                            app->name(), app->id(), result);
      } else {
        APPS_SERIAL_PRINTLN("* %s (%02x) : Saved %u bytes... (%u)",
                            app->name(), app->id(), result, storage_size);
        app_data.used += chunk->length;
        data += chunk->length;
      }
      (void)result;
    }
  }
  APPS_SERIAL_PRINTLN("App settings used: %u/%u", app_data.used, EEPROM_APPDATA_BINARY_SIZE);
  app_data_storage.Save(app_data);
  APPS_SERIAL_PRINTLN("Saved app settings in page_index %d", app_data_storage.page_index());
}

FLASHMEM
static void RestoreAppData() {
  APPS_SERIAL_PRINTLN("Restoring app data from page_index %d, used=%u", app_data_storage.page_index(), app_data.used);

  const uint8_t *data = app_data.data;
  const uint8_t *data_end = data + app_data.used;
  size_t restored_bytes = 0;

  while (data < data_end) {
    const AppChunkHeader *chunk = reinterpret_cast<const AppChunkHeader *>(data);
    if (data + chunk->length > data_end) {
      APPS_SERIAL_PRINTLN("App chunk length %u exceeds available space (%u)", chunk->length, data_end - data);
      break;
    }
    if (!chunk->id || !chunk->length) {
      APPS_SERIAL_PRINTLN("Invalid app chunk id=%02x, length=%d, stopping restore", chunk->id, chunk->length);
      break;
    }

    auto app = app_container.FindAppByID(chunk->id);
    if (!app) {
      APPS_SERIAL_PRINTLN("App %02x not found, ignoring chunk... skipping %u", chunk->id, chunk->length);
      if (!chunk->length)
        break;
      data += chunk->length;
      continue;
    }
    size_t expected_length = app->storage_size() + sizeof(AppChunkHeader);
    if (expected_length & 0x1) ++expected_length;
    if (chunk->length != expected_length) {
      APPS_SERIAL_PRINTLN("* %s (%02x): chunk length %u != %u (storage_size=%u), skipping...", app->name(), chunk->id, chunk->length, expected_length, app->storage_size());
      data += chunk->length;
      continue;
    }

    util::StreamBufferReader stream_buffer{chunk + 1, chunk->length};
    auto result = app->Restore(stream_buffer);
    if (stream_buffer.underflow()) {
      APPS_SERIAL_PRINTLN("* %s (%02x): Restore underflow, result=%u, re-init",
                          app->name(), chunk->id, result);
      app->InitDefaults();
    } else {
      APPS_SERIAL_PRINTLN("* %s (%02x): Restored %u from %u (chunk length %u)...",
                          app->name(), chunk->id, result, chunk->length - sizeof(AppChunkHeader), chunk->length);
      restored_bytes += chunk->length;
    }
    (void)result;

    data += chunk->length;
  }

  APPS_SERIAL_PRINTLN("App data restored: %u, expected %u", restored_bytes, app_data.used);
}

FLASHMEM
void AppSwitcher::set_current_app(size_t index)
{
  current_app_ = app_container[index];
  global_settings.current_app_id = current_app_.id();
  #ifdef VOR
  VBiasManager *vbias_m = vbias_m->get();
  vbias_m->SetStateForApp(current_app_);
  #endif
}

FLASHMEM
void AppSwitcher::Init(bool reset_settings) {

  APPS_SERIAL_PRINTLN("Init");
  app_container.for_each([](RuntimeSlot app) {
    APPS_SERIAL_PRINTLN("> %s", app->name());
    app.InitDefaults(app.instance);
  });

  current_app_ = app_container[DEFAULT_APP_INDEX];

  SERIAL_PRINTLN("[App Initializations]");

  Scales::Init();
  HS::Init();
#ifndef NO_HEMISPHERE
  HS::showhide_cursor.Init(0, HEMISPHERE_AVAILABLE_APPLETS - 1);
#endif
  HS::frame.Init();

  global_settings.Init();
  global_settings.encoders_enable_acceleration = OC_ENCODERS_ENABLE_ACCELERATION_DEFAULT;
  global_settings.reserved0 = false;
  global_settings.reserved1 = false;
  global_settings.reserved2 = 0U;
  global_settings.current_app_id = DEFAULT_APP_ID;
  memset(HS::user_turing_machines, 0, sizeof(HS::user_turing_machines));

  uint64_t data = 0;
  // check metadata for validity
  if (PhzConfig::getValue(METADATA_KEY, data)) {
    global_settings.current_app_id = Unpack(data, PackLocation{0, 16});
    global_settings.encoders_enable_acceleration = Unpack(data, PackLocation{16, 1});
    global_settings.valid = Unpack(data, PackLocation{17, 1});
    // 15 bits empty...
    // TODO:
    //global_settings.DAC_scaling = Unpack(data, PackLocation{32, 32});
    //OC::DAC::restore_scaling(global_settings.DAC_scaling);
  }

  if (reset_settings || !global_settings.valid) {
    if (ui.ConfirmReset()) {
      APPS_SERIAL_PRINTLN("Erase EEPROM ...");
      EEPtr d = EEPROM_GLOBALSETTINGS_START;
      size_t len = EEPROMStorage::LENGTH - EEPROM_GLOBALSETTINGS_START;
      while (len--)
        *d++ = 0;
      APPS_SERIAL_PRINTLN("...done");
      APPS_SERIAL_PRINTLN("Skip settings, using defaults...");
#ifdef __IMXRT1062__
      PhzConfig::eraseFiles();
#else
      global_settings_storage.Init();
#endif
      app_data_storage.Init();
      global_settings.valid = true;
      SaveGlobalSettings();
    } else {
      reset_settings = false;
    }
  }

  if (!reset_settings) {
#ifdef __IMXRT1062__
    // User Scales
    char filename[] = "000.SCL";
    bool scala_file_loaded[Scales::SCALE_USER_COUNT] = {false};
    for (size_t i = 0; i < Scales::SCALE_USER_COUNT; ++i) {
      if (SDcard_Ready && SD.exists(filename)) {
        filename[2] = char('0' + i);
        File file = SD.open(filename);
        if (file) {
          Scales::LoadScala(user_scales[i], file);
          scala_file_loaded[i] = true;
        }
        file.close();
      }
    }

    // Metadata
    if (global_settings.valid) {
      // User Scales
      for (size_t i = 0; i < Scales::SCALE_USER_COUNT; ++i) {
        if (scala_file_loaded[i] ||
            !PhzConfig::getValue(USER_SCALES_KEY | (i << 4) | SCALE_METADATA, data))
            continue;

        user_scales[i].span = (data >> 16) & 0xffff;
        user_scales[i].num_notes = data & 0x00ff;

        for (size_t nn = 0; nn < user_scales[i].num_notes; ++nn) {
          // the first of every 4 values needs a new config chunk
          if ((nn & 0x3) == 0x0) {
            data = 0;
            if (!PhzConfig::getValue(USER_SCALES_KEY | (i << 4) | (SCALE_NOTEDATA + (nn >> 2)), data))
              break;
          }
          user_scales[i].notes[nn] = Unpack(data, PackLocation{(nn & 0x3)*16, 16});
        }
      }

      // User Patterns aka Sequences
      for (size_t i = 0; i < Patterns::PATTERN_USER_COUNT; ++i) {
        for (size_t step = 0; step < ARRAY_SIZE(Pattern::notes); ++step) {
          if ((step & 0x3) == 0x0) {
            data = 0;
            if (!PhzConfig::getValue(SEQUENCES_KEY | (i << 3) | (step >> 2), data))
              break;
          }
          user_patterns[i].notes[step] = Unpack(data, PackLocation{(step & 0x3)*16, 16});
        }
      }

      // User Chords (progression sequences from Acid Curds)
      for (size_t i = 0; i < Chords::CHORDS_USER_COUNT; ++i) {
        data = 0;
        if (!PhzConfig::getValue(CHORDS_KEY | i, data))
          break;
        user_chords[i].quality = Unpack(data, PackLocation{0, 8});
        user_chords[i].inversion = Unpack(data, PackLocation{8, 8});
        user_chords[i].voicing = Unpack(data, PackLocation{16,8});
        user_chords[i].base_note = Unpack(data, PackLocation{24,8});
        user_chords[i].octave = Unpack(data, PackLocation{32,8});
      }

      // -- User Turing Machines (for Enigma and friends)
      for (size_t i = 0; i < HS::TURING_MACHINE_COUNT; ++i) {
        data = 0;
        if (!PhzConfig::getValue(TURING_MACHINES_KEY | i, data))
          break;
        HS::user_turing_machines[i].reg = Unpack(data, PackLocation{0, 16});
        HS::user_turing_machines[i].len = Unpack(data, PackLocation{16, 8});
        HS::user_turing_machines[i].favorite = Unpack(data, PackLocation{24, 1});
      }

#ifndef NO_HEMISPHERE
      // -- User Waveform (custom VectorOsc shapes)
      for (size_t i = 0; i < HS::VO_SEGMENT_COUNT; ++i) {
        if ((i & 0x3) == 0x0) {
          data = 0;
          if (!PhzConfig::getValue(WAVEFORMS_KEY | (i >> 2), data))
            break;
        }
        uint16_t wavedata = Unpack(data, PackLocation{(i & 0x3) * 16, 16});
        HS::user_waveforms[i].level = (wavedata >> 8) & 0xff;
        HS::user_waveforms[i].time = wavedata & 0xff;
      }
#endif

      // -- Auto Calibration Data
      /*
      for (size_t i = 0; i < DAC_CHANNEL_COUNT; ++i) {
        data = 0;
        if (!PhzConfig::getValue(AUTOCAL_KEY | (0xff - i), data))
          break;
        auto_calibration_data[i].use_auto_calibration_ = data;
        for (size_t oct = 0; oct < OCTAVES + 1; ++oct) {
          if ((oct & 0x3) == 0x0) {
            data = 0;
            if (!PhzConfig::getValue(AUTOCAL_KEY | (i << 4) | (oct >> 2), data))
              break;
          }
          auto_calibration_data[i].auto_calibrated_octaves[oct] = Unpack(data, PackLocation{(oct & 0x3) * 16, 16});
        }
      }
      */
    }

#else // Teensy 3.2
    APPS_SERIAL_PRINTLN("Load global settings: size: %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(GlobalSettings),
                  GlobalSettingsStorage::PAGESIZE,
                  GlobalSettingsStorage::PAGES,
                  GlobalSettingsStorage::LENGTH);

    if (!global_settings_storage.Load(global_settings)) {
      APPS_SERIAL_PRINTLN("Settings invalid, using defaults!");
    } else {
      APPS_SERIAL_PRINTLN("Loaded settings from page_index %d, current_app_id is %02x",
                    global_settings_storage.page_index(),global_settings.current_app_id);
      memcpy(user_scales, global_settings.user_scales, sizeof(user_scales));
      memcpy(user_patterns, global_settings.user_patterns, sizeof(user_patterns));
#ifdef ENABLE_APP_CHORDS
      memcpy(user_chords, global_settings.user_chords, sizeof(user_chords));
#else
      memcpy(HS::user_turing_machines, global_settings.user_turing_machines, sizeof(HS::user_turing_machines));
#endif
#ifndef NO_HEMISPHERE
      memcpy(HS::user_waveforms, global_settings.user_waveforms, sizeof(HS::user_waveforms));
#endif

      // restore q_engines and midi_maps
      for (int i = 0; i < QUANT_CHANNEL_COUNT; ++i) {
        HS::q_engine[i].scale     = global_settings.q_engines[i].scale;
        HS::q_engine[i].mask      = global_settings.q_engines[i].mask;
        HS::q_engine[i].octave    = global_settings.q_engines[i].octave;
        HS::q_engine[i].root_note = global_settings.q_engines[i].root_note;
        HS::q_engine[i].Reconfig();
      }
      for (int i = 0; i < MIDIMAP_MAX; ++i) {
        HS::frame.MIDIState.mapping[i].channel       = global_settings.midi_maps[i].channel      ;
        HS::frame.MIDIState.mapping[i].dac_polyvoice = global_settings.midi_maps[i].dac_polyvoice;
        HS::frame.MIDIState.mapping[i].function      = global_settings.midi_maps[i].function     ;
        HS::frame.MIDIState.mapping[i].function_cc   = global_settings.midi_maps[i].function_cc  ;
        HS::frame.MIDIState.mapping[i].transpose     = global_settings.midi_maps[i].transpose    ;
        HS::frame.MIDIState.mapping[i].range_low     = global_settings.midi_maps[i].range_low    ;
        HS::frame.MIDIState.mapping[i].range_high    = global_settings.midi_maps[i].range_high   ;
      }
      HS::frame.MIDIState.UpdateMidiChannelFilter();
      HS::frame.MIDIState.UpdateMaxPolyphony();
    }
#endif

    // old school EEPROM storage for legacy apps
    APPS_SERIAL_PRINTLN("Load app data: size is %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(AppData),
                  AppDataStorage::PAGESIZE,
                  AppDataStorage::PAGES,
                  AppDataStorage::LENGTH);

    if (!app_data_storage.Load(app_data)) {
      APPS_SERIAL_PRINTLN("Data not loaded, using defaults!");
    } else {
      RestoreAppData();
    }
  }

  // Validation to guard against junk data
  Chords::Validate();
  Scales::Validate();
#ifndef NO_HEMISPHERE
  WaveformManager::Validate();
#endif
  for (int i = 0; i < HS::TURING_MACHINE_COUNT; ++i) {
    HS::user_turing_machines[i].Validate();
  }

  size_t current_app_index = app_container.IndexOfAppByID(global_settings.current_app_id);
  if (current_app_index >= app_container.num_apps()) {
    APPS_SERIAL_PRINTLN("App id %02x not found, using default!", global_settings.current_app_id);
    current_app_index = DEFAULT_APP_INDEX;
  }

  APPS_SERIAL_PRINTLN("Encoder acceleration: %s", global_settings.encoders_enable_acceleration ? "enabled" : "disabled");
  ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  set_current_app(current_app_index);

  delay(100);
}

FLASHMEM
void draw_save_message(uint8_t c) {
  GRAPHICS_BEGIN_FRAME(true);
  uint8_t _size = c % 120;
  graphics.setPrintPos(37, 18);
  graphics.print("Saving...");
  graphics.drawRect(0, 28, _size, 8);
  GRAPHICS_END_FRAME();
}

FLASHMEM
bool Ui::AppSettings(bool drawmenu) {
  static menu::ScreenCursor<5> cursor;
  static bool change_app = false;
  static bool save = false;
  static bool opened = false;

  // --- state change: entering App Menu
  if (!opened) {
    cursor.Init(0, app_container.num_apps() - 1);
    cursor.Scroll(app_container.IndexOfAppByID(global_settings.current_app_id));
    opened = true;
  }

  // View - graphics
  if (drawmenu) {
    // assumes this is called from within a graphics frame context
    if (global_settings.encoders_enable_acceleration)
      graphics.drawBitmap8(120, 1, 4, bitmap_indicator_4x8);

    menu::SettingsListItem item;
    item.x = menu::kIndentDx + 8;
    item.y = (64 - (5 * menu::kMenuLineH)) / 2;

    for (int current = max(cursor.first_visible(), 0);
         current <= cursor.last_visible() && current < (int)app_container.num_apps();
         ++current, item.y += menu::kMenuLineH) {
      item.selected = current == cursor.cursor_pos();
      item.SetPrintPos();
      graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
      // todo: make a secret button combo to switch to boring names
      // if (your_mom_is_boring)
      //   graphics.print(app_container[current]->boring_name());
      // else
      graphics.print(app_container[current].name());
      if (global_settings.current_app_id == app_container[current].id())
        graphics.drawBitmap8(0, item.y + 1, 8, ZAP_ICON);

      item.DrawCustom();
    }

#ifdef VOR
    VBiasManager *vbias_m = vbias_m->get();
    vbias_m->DrawPopupPerhaps();
#endif

    return true;
  }

  // UI - event handling
  if (!change_app && idle_time() < APP_SELECTION_TIMEOUT_MS) {
    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (IgnoreEvent(event))
        continue;

      switch (event.control) {
      case CONTROL_ENCODER_R:
        if (UI::EVENT_ENCODER == event.type)
          cursor.Scroll(event.value);
        break;

      case CONTROL_BUTTON_R:
        save = event.type == UI::EVENT_BUTTON_LONG_PRESS;
        change_app = event.type != UI::EVENT_BUTTON_DOWN; // true on button release
        break;
      case CONTROL_BUTTON_L:
        if (UI::EVENT_BUTTON_PRESS == event.type)
            ui.DebugStats();
        break;
      case CONTROL_BUTTON_UP:
#ifdef VOR
        // VBias menu for units without Range button
        if (UI::EVENT_BUTTON_LONG_PRESS == event.type || UI::EVENT_BUTTON_DOWN == event.type) {
          VBiasManager *vbias_m = vbias_m->get();
          vbias_m->AdvanceBias();
        }
#endif
        break;
      case CONTROL_BUTTON_DOWN:
        if (UI::EVENT_BUTTON_PRESS == event.type) {
            bool enabled = !global_settings.encoders_enable_acceleration;
            APPS_SERIAL_PRINTLN("Encoder acceleration: %s", enabled ? "enabled" : "disabled");
            ui.encoders_enable_acceleration(enabled);
            global_settings.encoders_enable_acceleration = enabled;
        }
        break;

        default: break;
      }
    }

    return true;
  }
  // else... idle time expired, or an app was selected via UI
  // cleanup and exit
  event_queue_.Flush();
  event_queue_.Poke();

  // --- state change: exiting App menu
  CORE::app_isr_enabled = false;
  delay(1);

  if (change_app) {
    app_switcher.set_current_app(cursor.cursor_pos());
    FreqMeasure.end();
    OC::DigitalInputs::reInit();
    if (save) {
      SaveAppData();
      // draw message:
      int cnt = 0;
      while(idle_time() < SETTINGS_SAVE_TIMEOUT_MS)
        draw_save_message((cnt++) >> 4);
      save = false;
    }
    change_app = false;
  }

  OC::ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  // Restore state
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  CORE::app_isr_enabled = true;

  opened = false;
  return false; // close menu
}

FLASHMEM
bool Ui::ConfirmReset() {

  SetButtonIgnoreMask();

  bool done = false;
  bool confirm = false;

  do {
    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (IgnoreEvent(event))
        continue;
      if (CONTROL_BUTTON_R == event.control && UI::EVENT_BUTTON_PRESS == event.type) {
        confirm = true;
        done = true;
      } else if (CONTROL_BUTTON_L == event.control && UI::EVENT_BUTTON_PRESS == event.type) {
        confirm = false;
        done = true;
      }
    }

    GRAPHICS_BEGIN_FRAME(true);
    graphics.setPrintPos(1, 2);
    graphics.print("Setup: Reset");
    graphics.drawLine(0, 10, 127, 10);
    graphics.drawLine(0, 12, 127, 12);

    graphics.setPrintPos(1, 15);
    graphics.print("Reset application");
    graphics.setPrintPos(1, 25);
    graphics.print("settings on EEPROM?");

    graphics.setPrintPos(0, 55);
    graphics.print("[CANCEL]         [OK]");

    GRAPHICS_END_FRAME();

  } while (!done);

  return confirm;
}

FLASHMEM
void start_calibration() {
  OC::calibration_data.set_calstart();
  OC::app_switcher.set_current_app(0);
}

}; // namespace OC
