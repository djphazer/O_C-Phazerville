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
#include "OC_ui.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_config.h"
#include "OC_digital_inputs.h"
#include "OC_storage.h"
#include "OC_app_switcher.h"
#include "OC_global_settings.h"
#include "util/util_misc.h"


#include "OC_calibration.h"
#include "OC_patterns.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_pagestorage.h"
#include "src/drivers/EEPROMStorage.h"
#include "PhzConfig.h"
#include "VBiasManager.h"
#include "HSClockManager.h"

namespace menu = OC::menu;

#ifndef NO_HEMISPHERE

#ifdef ARDUINO_TEENSY41
#include "apps/Quadrants.h"
#endif
#include "apps/Hemisphere.h"

#endif

#include "apps/Calibr8or.h"
#include "apps/Scenery.h"
#include "apps/ASR.h"
#ifdef ENABLE_APP_H1200
#include "apps/H1200.h"
#endif
#ifdef ENABLE_APP_AUTOMATONNETZ
#include "apps/Automatonnetz.h"
#endif
#include "apps/Sequins.h"
#include "apps/QQ.h"
#include "apps/DQ.h"
#include "apps/Quadraturia.h"
#include "apps/Lorenz.h"
#include "apps/Piqued.h"
#include "apps/BBGEN.h"
#include "apps/Viznutcracker.h"
#include "apps/Chords.h"
#include "apps/References.h"
// #include "apps/Passencore.h"
// #include "apps/CaptainMIDI.h"
// #include "apps/TheDarkestTimeline.h"
// #include "apps/Enigma.h"
// #include "apps/NeuralNetwork.h"
// #include "apps/ScaleEditor.h"
// #include "apps/WaveformEditor.h"
// #include "apps/PongGame.h"
// #include "apps/Backup.h"
#include "apps/SETTINGS.h"

/*
// The order here is not inconsequential.
// Each app's Start() method is called in sequence.
// For example, the default quantizer settings from Hemisphere
// are overwritten when Calibr8or loads its settings
static constexpr OC::App app_container[] = {
  DECLARE_APP("SE", "Setup / About", Settings),

#ifndef NO_HEMISPHERE
  #ifdef ARDUINO_TEENSY41
  DECLARE_APP("QS", "Quadrants", QUADRANTS),
  #endif
  DECLARE_APP("HS", "Hemisphere", HEMISPHERE),
#endif

  #ifdef ENABLE_APP_CALIBR8OR
  DECLARE_APP("C8", "Calibr8or", Calibr8or),
  #endif
  #ifdef ENABLE_APP_SCENES
  DECLARE_APP("SX", "Scenes", ScenesApp),
  #endif

  #ifdef ENABLE_APP_ASR
  DECLARE_APP("AS", "CopierMaschine", ASR),
  #endif
  #ifdef ENABLE_APP_H1200
  DECLARE_APP("HA", "Harrington 1200", H1200),
  #endif
  #ifdef ENABLE_APP_AUTOMATONNETZ
  DECLARE_APP("AT", "Automatonnetz", Automatonnetz),
  #endif
  #ifdef ENABLE_APP_QUANTERMAIN
  DECLARE_APP("QQ", "Quantermain", QQ),
  #endif
  #ifdef ENABLE_APP_METAQ
  DECLARE_APP("DQ", "Meta-Q", DQ),
  #endif
  #ifdef ENABLE_APP_POLYLFO
  DECLARE_APP("PL", "Quadraturia", POLYLFO),
  #endif
  #ifdef ENABLE_APP_LORENZ
  DECLARE_APP("LR", "Low-rents", LORENZ),
  #endif
  #ifdef ENABLE_APP_PIQUED
  DECLARE_APP("EG", "Piqued", ENVGEN),
  #endif
  #ifdef ENABLE_APP_SEQUINS
  DECLARE_APP("SQ", "Sequins", SEQ),
  #endif
  #ifdef ENABLE_APP_BBGEN
  DECLARE_APP("BB", "Dialectic Pong", BBGEN),
  #endif
  #ifdef ENABLE_APP_BYTEBEATGEN
  DECLARE_APP("BY", "Viznutcracker", BYTEBEATGEN),
  #endif
  #ifdef ENABLE_APP_CHORDS
  DECLARE_APP("AC", "Acid Curds", CHORDS),
  #endif
  #ifdef ENABLE_APP_FPART
  DECLARE_APP("FP", "4 Parts", FPART),
  #endif
  #ifdef ENABLE_APP_PASSENCORE
  // boring name version
  // DECLARE_APP("PQ", "Tension", PASSENCORE),
  DECLARE_APP("PQ", "Passencore", PASSENCORE),
  #endif
  #ifdef ENABLE_APP_MIDI
  DECLARE_APP("MI", "Captain MIDI", MIDI),
  #endif
  #ifdef ENABLE_APP_DARKEST_TIMELINE
  DECLARE_APP("D2", "Darkest Timeline", TheDarkestTimeline),
  #endif
  #ifdef ENABLE_APP_ENIGMA
  DECLARE_APP("EN", "Enigma", EnigmaTMWS),
  #endif
  #ifdef ENABLE_APP_NEURAL_NETWORK
  DECLARE_APP("NN", "Neural Net", NeuralNetwork),
  #endif
  DECLARE_APP("SC", "Scale Editor", SCALEEDITOR),
  DECLARE_APP("WA", "Waveform Editor", WaveformEditor),
  #ifdef ENABLE_APP_PONG
  DECLARE_APP("PO", "Pong", PONGGAME),
  #endif
  #ifdef ENABLE_APP_REFERENCES
  DECLARE_APP("RF", "References", REFS),
  #endif
  DECLARE_APP("BR", "Backup / Restore", Backup),
};
*/

namespace OC {

// NOTE These are slightly wasteful, in that the PageStorage implementation and
// the local data both retain a copy of the data. Removing this would in theory
// reclaim some memory, although RAM isn't currently an issue.
/*extern*/ GlobalSettings global_settings;
/*extern*/ AppSwitcher app_switcher;
static AppData app_data;
#ifndef __IMXRT1062__
static GlobalSettingsStorage global_settings_storage;
#endif
static AppDataStorage app_data_storage;

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

// Instantiate the available apps.
// Any type not listed here should not exist, i.e. the linker should be able to
// triage all code (minus any dangling static parts). (Yeah, this still relies
// on the fugly .ino compilation method, don't @ me).
static AppContainer<void // this space intentionally left blank
  , AppSettings
#ifndef NO_HEMISPHERE
  #ifdef ARDUINO_TEENSY41
  , AppQuadrants
  #endif
  , AppHemisphere
#endif
#ifdef ENABLE_APP_CALIBR8OR
  , AppCalibr8or
#endif
#ifdef ENABLE_APP_SCENES
  , AppScenery
#endif
#ifdef ENABLE_APP_ASR
  , AppASR
#endif
#ifdef ENABLE_APP_H1200
  , AppH1200
#endif
#ifdef ENABLE_APP_AUTOMATONNETZ
  , AppAutomatonnetz
#endif
#ifdef ENABLE_APP_QUANTERMAIN
  , AppQuadQuantizer
#endif
#ifdef ENABLE_APP_METAQ
  , AppDualQuantizer
#endif
#ifdef ENABLE_APP_POLYLFO
  , AppPolyLfo
#endif
#ifdef ENABLE_APP_LORENZ
  , AppLorenzGenerator
#endif
#ifdef ENABLE_APP_PIQUED
  , AppQuadEnvelopeGenerator
#endif
#ifdef ENABLE_APP_SEQUINS
  , AppDualSequencer
#endif
#ifdef ENABLE_APP_BBGEN
  , AppQuadBouncingBalls
#endif
#ifdef ENABLE_APP_BYTEBEATGEN
  , AppQuadByteBeats
#endif
#ifdef ENABLE_APP_CHORDS
  , AppChordQuantizer
#endif
#ifdef ENABLE_APP_REFERENCES
  , AppReferences
#endif
> app_container;
static_assert(decltype(app_container)::TotalAppDataStorageSize() < AppData::kAppDataSize,
              "Apps use too much EEPROM space!");

static constexpr int DEFAULT_APP_INDEX = 1;
static constexpr uint16_t DEFAULT_APP_ID = decltype(app_container)::GetAppIDAtIndex<DEFAULT_APP_INDEX>();

static void SaveGlobalSettings() {
  APPS_SERIAL_PRINTLN("Save global settings");

#ifdef __IMXRT1062__
  //PhzConfig::clear_config();
  PhzConfig::load_config(); // use default config file

  // Metadata
  uint64_t data = 0;
  Pack(data, PackLocation{0, 16}, global_settings.current_app_id);
  Pack(data, PackLocation{16, 1}, global_settings.encoders_enable_acceleration);
  PhzConfig::setValue(METADATA_KEY, data);

  // User Scales
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

  data = 0;
  // User Waveform (custom VectorOsc shapes)
  for (size_t i = 0; i < HS::VO_SEGMENT_COUNT; ++i) {
    Pack(data, PackLocation{(i & 0x3) * 16, 16}, uint16_t(HS::user_waveforms[i].level) << 8 | HS::user_waveforms[i].time);

    if ((i & 0x3) == 0x3) {
      PhzConfig::setValue(WAVEFORMS_KEY | (i >> 2), data);
      data = 0;
    }
  }

  // Auto Calibration Data
  /*
  for (size_t i = 0; i < DAC_CHANNEL_LAST; ++i) {
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
  memcpy(global_settings.user_waveforms, HS::user_waveforms, sizeof(HS::user_waveforms));

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

static void SaveAppData() {
  SaveGlobalSettings(); // yeah, why not
  APPS_SERIAL_PRINTLN("Save app data... (%u bytes available)", OC::AppData::kAppDataSize);

  app_data.used = 0;
  uint8_t *data = app_data.data;
  uint8_t *data_end = data + OC::AppData::kAppDataSize;

  size_t start_app = random(app_container.num_apps());
  for (size_t i = 0; i < app_container.num_apps(); ++i) {
    const auto app = app_container[(start_app + i) % app_container.num_apps()];
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

void AppSwitcher::set_current_app(size_t index)
{
  current_app_ = app_container[index];
  global_settings.current_app_id = current_app_->id();
  #ifdef VOR
  VBiasManager *vbias_m = vbias_m->get();
  vbias_m->SetStateForApp(current_app_);
  #endif
}

void AppSwitcher::Init(bool reset_settings) {

  APPS_SERIAL_PRINTLN("Init");
  app_container.for_each([](AppBase *app) {
    APPS_SERIAL_PRINTLN("> %s", app->name());
    app->InitDefaults();
  });

  current_app_ = app_container[DEFAULT_APP_INDEX];

  Scales::Init();
  HS::Init();
  HS::frame.Init();

  global_settings.Init();
  global_settings.encoders_enable_acceleration = OC_ENCODERS_ENABLE_ACCELERATION_DEFAULT;
  global_settings.reserved0 = false;
  global_settings.reserved1 = false;
  global_settings.reserved2 = 0U;
  global_settings.current_app_id = DEFAULT_APP_ID;

  if (reset_settings) {
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
    } else {
      reset_settings = false;
    }
  }

  if (!reset_settings) {
#ifdef __IMXRT1062__
    PhzConfig::load_config(); // use default config file

    // Metadata
    uint64_t data = 0;
    if (PhzConfig::getValue(METADATA_KEY, data)) {
      global_settings.current_app_id = Unpack(data, PackLocation{0, 16});
      global_settings.encoders_enable_acceleration = Unpack(data, PackLocation{16, 1});

      // User Scales
      for (size_t i = 0; i < Scales::SCALE_USER_COUNT; ++i) {
        if (!PhzConfig::getValue(USER_SCALES_KEY | (i << 4) | SCALE_METADATA, data))
          break;

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

      // -- Auto Calibration Data
      /*
      for (size_t i = 0; i < DAC_CHANNEL_LAST; ++i) {
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
      Chords::Validate();
#else
      memcpy(HS::user_turing_machines, global_settings.user_turing_machines, sizeof(HS::user_turing_machines));
#endif
      memcpy(HS::user_waveforms, global_settings.user_waveforms, sizeof(HS::user_waveforms));
      Scales::Validate();

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

void draw_save_message(uint8_t c) {
  GRAPHICS_BEGIN_FRAME(true);
  uint8_t _size = c % 120;
  graphics.setPrintPos(37, 18);
  graphics.print("Saving...");
  graphics.drawRect(0, 28, _size, 8);
  GRAPHICS_END_FRAME();
}

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

    for (int current = cursor.first_visible();
         current <= cursor.last_visible();
         ++current, item.y += menu::kMenuLineH) {
      item.selected = current == cursor.cursor_pos();
      item.SetPrintPos();
      graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
      if (NorthernLightModular)
        graphics.print(app_container[current]->boring_name());
      else
        graphics.print(app_container[current]->name());
      if (global_settings.current_app_id == app_container[current]->id())
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

void start_calibration() {
  OC::calibration_data.set_calstart();
  OC::app_switcher.set_current_app(0);
}

}; // namespace OC
