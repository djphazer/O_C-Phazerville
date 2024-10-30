// Copyright (c) 2016 Patrick Dowling
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
#include "OC_ui.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_config.h"
#include "OC_digital_inputs.h"
#include "OC_autotune.h"
#include "OC_calibration.h"
#include "OC_patterns.h"
#include "enigma/TuringMachine.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_pagestorage.h"
#include "util/EEPROMStorage.h"
#include "VBiasManager.h"
#include "HSClockManager.h"

namespace menu = OC::menu;

#ifndef NO_HEMISPHERE

#ifdef ARDUINO_TEENSY41
#include "APP_QUADRANTS.h"
#endif
#include "APP_HEMISPHERE.h"

#endif

#include "APP_CALIBR8OR.h"
#include "APP_SCENES.h"
#include "APP_ASR.h"
#include "APP_H1200.h"
#include "APP_AUTOMATONNETZ.h"
#include "APP_QQ.h"
#include "APP_DQ.h"
#include "APP_POLYLFO.h"
#include "APP_LORENZ.h"
#include "APP_ENVGEN.h"
#include "APP_SEQ.h"
#include "APP_BBGEN.h"
#include "APP_BYTEBEATGEN.h"
#include "APP_CHORDS.h"
#include "APP_REFS.h"
#include "APP_PASSENCORE.h"
#include "APP_MIDI.h"
#include "APP_THEDARKESTTIMELINE.h"
#include "APP_ENIGMA.h"
#include "APP_NeuralNetwork.h"
#include "APP_SCALEEDITOR.h"
#include "APP_WAVEFORMEDITOR.h"
#include "APP_PONGGAME.h"
#include "APP_Backup.h"
#include "APP_SETTINGS.h"

#define DECLARE_APP(a, b, name, prefix) \
{ TWOCC<a,b>::value, name, \
  prefix ## _init, prefix ## _storageSize, prefix ## _save, prefix ## _restore, \
  prefix ## _handleAppEvent, \
  prefix ## _loop, prefix ## _menu, prefix ## _screensaver, \
  prefix ## _handleButtonEvent, \
  prefix ## _handleEncoderEvent, \
  prefix ## _isr \
}

static constexpr OC::App available_apps[] = {
  DECLARE_APP('S','E', "Setup / About", Settings),

  #ifdef ENABLE_APP_CALIBR8OR
  DECLARE_APP('C','8', "Calibr8or", Calibr8or),
  #endif
  #ifdef ENABLE_APP_SCENES
  DECLARE_APP('S','X', "Scenes", ScenesApp),
  #endif

#ifndef NO_HEMISPHERE
  #ifdef ARDUINO_TEENSY41
  DECLARE_APP('Q','S', "Quadrants", QUADRANTS),
  #endif
  DECLARE_APP('H','S', "Hemisphere", HEMISPHERE),
#endif
  #ifdef ENABLE_APP_ASR
  DECLARE_APP('A','S', "CopierMaschine", ASR),
  #endif
  #ifdef ENABLE_APP_H1200
  DECLARE_APP('H','A', "Harrington 1200", H1200),
  #endif
  #ifdef ENABLE_APP_AUTOMATONNETZ
  DECLARE_APP('A','T', "Automatonnetz", Automatonnetz),
  #endif
  #ifdef ENABLE_APP_QUANTERMAIN
  DECLARE_APP('Q','Q', "Quantermain", QQ),
  #endif
  #ifdef ENABLE_APP_METAQ
  DECLARE_APP('M','!', "Meta-Q", DQ),
  #endif
  #ifdef ENABLE_APP_POLYLFO
  DECLARE_APP('P','L', "Quadraturia", POLYLFO),
  #endif
  #ifdef ENABLE_APP_LORENZ
  DECLARE_APP('L','R', "Low-rents", LORENZ),
  #endif
  #ifdef ENABLE_APP_PIQUED
  DECLARE_APP('E','G', "Piqued", ENVGEN),
  #endif
  #ifdef ENABLE_APP_SEQUINS
  DECLARE_APP('S','Q', "Sequins", SEQ),
  #endif
  #ifdef ENABLE_APP_BBGEN
  DECLARE_APP('B','B', "Dialectic Pong", BBGEN),
  #endif
  #ifdef ENABLE_APP_BYTEBEATGEN
  DECLARE_APP('B','Y', "Viznutcracker", BYTEBEATGEN),
  #endif
  #ifdef ENABLE_APP_CHORDS
  DECLARE_APP('A','C', "Acid Curds", CHORDS),
  #endif
  #ifdef ENABLE_APP_FPART
  DECLARE_APP('F','P', "4 Parts", FPART),
  #endif
  #ifdef ENABLE_APP_PASSENCORE
  // boring name version
  // DECLARE_APP('P','Q', "Tension", PASSENCORE),
  DECLARE_APP('P','Q', "Passencore", PASSENCORE),
  #endif
  #ifdef ENABLE_APP_MIDI
  DECLARE_APP('M','I', "Captain MIDI", MIDI),
  #endif
  #ifdef ENABLE_APP_DARKEST_TIMELINE
  DECLARE_APP('D','2', "Darkest Timeline", TheDarkestTimeline),
  #endif
  #ifdef ENABLE_APP_ENIGMA
  DECLARE_APP('E','N', "Enigma", EnigmaTMWS),
  #endif
  #ifdef ENABLE_APP_NEURAL_NETWORK
  DECLARE_APP('N','N', "Neural Net", NeuralNetwork),
  #endif
  DECLARE_APP('S','C', "Scale Editor", SCALEEDITOR),
  DECLARE_APP('W','A', "Waveform Editor", WaveformEditor),
  #ifdef ENABLE_APP_PONG
  DECLARE_APP('P','O', "Pong", PONGGAME),
  #endif
  #ifdef ENABLE_APP_REFERENCES
  DECLARE_APP('R','F', "References", REFS),
  #endif
  DECLARE_APP('B','R', "Backup / Restore", Backup),
};

static constexpr int NUM_AVAILABLE_APPS = ARRAY_SIZE(available_apps);

namespace OC {

// Global settings are stored separately to actual app setings.
// The theory is that they might not change as often.
struct GlobalSettings {
  static constexpr uint32_t FOURCC = FOURCC<'O','C','S',2>::value;

  bool encoders_enable_acceleration;
  bool reserved0;
  bool reserved1;
  uint32_t DAC_scaling;
  uint16_t current_app_id;

  OC::Scale user_scales[OC::Scales::SCALE_USER_LAST];
  OC::Pattern user_patterns[OC::Patterns::PATTERN_USER_ALL];
  // These both occupy 160 bytes
#ifdef ENABLE_APP_CHORDS
  OC::Chord user_chords[OC::Chords::CHORDS_USER_LAST];
#else
  HS::TuringMachine user_turing_machines[HS::TURING_MACHINE_COUNT];
#endif
  HS::VOSegment user_waveforms[HS::VO_SEGMENT_COUNT];
  OC::Autotune_data auto_calibration_data[DAC_CHANNEL_LAST];
};

// App settings are packed into a single blob of binary data; each app's chunk
// gets its own header with id and the length of the entire chunk. This makes
// this a bit more flexible during development.
// Chunks are aligned on 2-byte boundaries for arbitrary reasons (thankfully M4
// allows unaligned access...)
struct AppChunkHeader {
  uint16_t id;
  uint16_t length;
} __attribute__((packed));

struct AppData {
  static constexpr uint32_t FOURCC = FOURCC<'O','C','A',4>::value;

  static constexpr size_t kAppDataSize = EEPROM_APPDATA_BINARY_SIZE;
  char data[kAppDataSize];
  size_t used;
};

typedef PageStorage<EEPROMStorage, EEPROM_GLOBALSETTINGS_START, EEPROM_GLOBALSETTINGS_END, GlobalSettings> GlobalSettingsStorage;
typedef PageStorage<EEPROMStorage, EEPROM_APPDATA_START, EEPROM_APPDATA_END, AppData> AppDataStorage;

DMAMEM GlobalSettings global_settings;
DMAMEM GlobalSettingsStorage global_settings_storage;

DMAMEM AppData app_settings;
DMAMEM AppDataStorage app_data_storage;

static constexpr int DEFAULT_APP_INDEX = 1;
static const uint16_t DEFAULT_APP_ID = available_apps[DEFAULT_APP_INDEX].id;

void save_global_settings() {
  SERIAL_PRINTLN("Save global settings");

  memcpy(global_settings.user_scales, OC::user_scales, sizeof(OC::user_scales));
  memcpy(global_settings.user_patterns, OC::user_patterns, sizeof(OC::user_patterns));
#ifdef ENABLE_APP_CHORDS
  memcpy(global_settings.user_chords, OC::user_chords, sizeof(OC::user_chords));
#else
  memcpy(global_settings.user_turing_machines, HS::user_turing_machines, sizeof(HS::user_turing_machines));
#endif
  memcpy(global_settings.user_waveforms, HS::user_waveforms, sizeof(HS::user_waveforms));
  memcpy(global_settings.auto_calibration_data, OC::auto_calibration_data, sizeof(OC::auto_calibration_data));
  // scaling settings:
  global_settings.DAC_scaling = OC::DAC::store_scaling();

  global_settings_storage.Save(global_settings);
  SERIAL_PRINTLN("Saved global settings: page_index %d", global_settings_storage.page_index());
}

static constexpr size_t total_storage_size() {
    size_t used = 0;
    for (size_t i = 0; i < NUM_AVAILABLE_APPS; ++i) {
        used += available_apps[i].storageSize() + sizeof(AppChunkHeader);
        if (used & 1) ++used; // align on 2-byte boundaries
    }
    return used;
}

static constexpr size_t totalsize = total_storage_size();
static_assert(totalsize < OC::AppData::kAppDataSize, "EEPROM Allocation Exceeded");

void save_app_data() {
  SERIAL_PRINTLN("Save app data... (%u bytes available)", OC::AppData::kAppDataSize);

  app_settings.used = 0;
  char *data = app_settings.data;
  char *data_end = data + OC::AppData::kAppDataSize;

  size_t start_app = random(NUM_AVAILABLE_APPS);
  for (size_t i = 0; i < NUM_AVAILABLE_APPS; ++i) {
    const App &app = available_apps[(start_app + i) % NUM_AVAILABLE_APPS];
    size_t storage_size = app.storageSize() + sizeof(AppChunkHeader);
    if (storage_size & 1) ++storage_size; // Align chunks on 2-byte boundaries
    if (storage_size > sizeof(AppChunkHeader) && app.Save) {
      if (data + storage_size > data_end) {
        SERIAL_PRINTLN("%s: ERROR: %u BYTES NEEDED, %u BYTES AVAILABLE OF %u BYTES TOTAL", app.name, storage_size, data_end - data, AppData::kAppDataSize);
        continue;
      }

      AppChunkHeader *chunk = reinterpret_cast<AppChunkHeader *>(data);
      chunk->id = app.id;
      chunk->length = storage_size;
      #ifdef PRINT_DEBUG
        SERIAL_PRINTLN("* %s (%02x) : Saved %u bytes... (%u)", app.name, app.id, app.Save(chunk + 1), storage_size);
      #else
        app.Save(chunk + 1);
      #endif
      app_settings.used += chunk->length;
      data += chunk->length;
    }
  }
  SERIAL_PRINTLN("App settings used: %u/%u", app_settings.used, EEPROM_APPDATA_BINARY_SIZE);
  app_data_storage.Save(app_settings);
  SERIAL_PRINTLN("Saved app settings in page_index %d", app_data_storage.page_index());
}

void restore_app_data() {
  SERIAL_PRINTLN("Restoring app data from page_index %d, used=%u", app_data_storage.page_index(), app_settings.used);

  const char *data = app_settings.data;
  const char *data_end = data + app_settings.used;
  size_t restored_bytes = 0;

  while (data < data_end) {
    const AppChunkHeader *chunk = reinterpret_cast<const AppChunkHeader *>(data);
    if (data + chunk->length > data_end) {
      SERIAL_PRINTLN("App chunk length %u exceeds available space (%u)", chunk->length, data_end - data);
      break;
    }

    const App *app = apps::find(chunk->id);
    if (!app) {
      SERIAL_PRINTLN("App %02x not found, ignoring chunk... skipping %u", chunk->id, chunk->length);
      if (!chunk->length)
        break;
      data += chunk->length;
      continue;
    }
    const size_t expected_length = ((app->storageSize() + sizeof(AppChunkHeader) + 1) >> 1) << 1; // round up
    //if (expected_length & 0x1) ++expected_length;
    if (chunk->length != expected_length) {
      SERIAL_PRINTLN("* %s (%02x): chunk length %u != %u (storageSize=%u), skipping...", app->name, chunk->id, chunk->length, expected_length, app->storageSize());
      data += chunk->length;
      continue;
    }

    size_t restored = 0;
    if (app->Restore) {
        restored = app->Restore(chunk + 1);
      #ifdef PRINT_DEBUG
        SERIAL_PRINTLN("* %s (%02x): Restored %u from %u (chunk length %u)...", app->name, chunk->id, restored, chunk->length - sizeof(AppChunkHeader), chunk->length);
      #endif
    }
    restored = ((restored + sizeof(AppChunkHeader) + 1) >> 1) << 1; // round up

    restored_bytes += restored;
    data += restored;
  }

  SERIAL_PRINTLN("App data restored: %u, expected %u", restored_bytes, app_settings.used);
}

namespace apps {

void set_current_app(int index) {
  current_app = &available_apps[index];
  global_settings.current_app_id = current_app->id;
  #ifdef VOR
  VBiasManager *vbias_m = vbias_m->get();
  vbias_m->SetStateForApp(current_app);
  #endif
}

const App *current_app = &available_apps[DEFAULT_APP_INDEX];

const App *find(uint16_t id) {
  for (auto &app : available_apps)
    if (app.id == id) return &app;
  return nullptr;
}

int index_of(uint16_t id) {
  int i = 0;
  for (const auto &app : available_apps) {
    if (app.id == id) return i;
    ++i;
  }
  return i;
}

void Init(bool reset_settings) {

  Scales::Init();
  AUTOTUNE::Init();
  HS::Init();
  for (auto &app : available_apps)
    app.Init();

  global_settings.current_app_id = DEFAULT_APP_ID;
  global_settings.encoders_enable_acceleration = OC_ENCODERS_ENABLE_ACCELERATION_DEFAULT;
  global_settings.reserved0 = false;
  global_settings.reserved1 = false;
  global_settings.DAC_scaling = VOLTAGE_SCALING_1V_PER_OCT;

  if (reset_settings) {
    if (ui.ConfirmReset()) {
      SERIAL_PRINTLN("Erase EEPROM ...");
      EEPtr d = EEPROM_GLOBALSETTINGS_START;
      size_t len = EEPROMStorage::LENGTH - EEPROM_GLOBALSETTINGS_START;
      while (len--)
        *d++ = 0;
      SERIAL_PRINTLN("...done");
      SERIAL_PRINTLN("Skip settings, using defaults...");
      global_settings_storage.Init();
      app_data_storage.Init();
    } else {
      reset_settings = false;
    }
  }

  if (!reset_settings) {
    SERIAL_PRINTLN("Load global settings: size: %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(GlobalSettings),
                  GlobalSettingsStorage::PAGESIZE,
                  GlobalSettingsStorage::PAGES,
                  GlobalSettingsStorage::LENGTH);

    if (!global_settings_storage.Load(global_settings)) {
      SERIAL_PRINTLN("Settings invalid, using defaults!");
    } else {
      SERIAL_PRINTLN("Loaded settings from page_index %d, current_app_id is %02x",
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
      memcpy(auto_calibration_data, global_settings.auto_calibration_data, sizeof(auto_calibration_data));
      DAC::choose_calibration_data(); // either use default data, or auto_calibration_data
      DAC::restore_scaling(global_settings.DAC_scaling); // recover output scaling settings
      Scales::Validate();
    }

    SERIAL_PRINTLN("Load app data: size is %u, PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                  sizeof(AppData),
                  AppDataStorage::PAGESIZE,
                  AppDataStorage::PAGES,
                  AppDataStorage::LENGTH);

    if (!app_data_storage.Load(app_settings)) {
      SERIAL_PRINTLN("Data not loaded, using defaults!");
    } else {
      restore_app_data();
    }
  }

  int current_app_index = apps::index_of(global_settings.current_app_id);
  if (current_app_index < 0 || current_app_index >= NUM_AVAILABLE_APPS) {
    SERIAL_PRINTLN("App id %02x not found, using default!", global_settings.current_app_id);
    global_settings.current_app_id = DEFAULT_APP_INDEX;
    current_app_index = DEFAULT_APP_INDEX;
  }

  SERIAL_PRINTLN("Encoder acceleration: %s", global_settings.encoders_enable_acceleration ? "enabled" : "disabled");
  ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  set_current_app(current_app_index);
  current_app->HandleAppEvent(APP_EVENT_RESUME);

  delay(100);
}

}; // namespace apps

void draw_app_menu(const menu::ScreenCursor<5> &cursor) {
  GRAPHICS_BEGIN_FRAME(true);

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
    graphics.movePrintPos(weegfx::kFixedFontW, 0);
    graphics.print(available_apps[current].name);

    if (global_settings.current_app_id == available_apps[current].id)
      graphics.drawBitmap8(0, item.y + 1, 8, ZAP_ICON);

    item.DrawCustom();
  }

#ifdef VOR
  VBiasManager *vbias_m = vbias_m->get();
  vbias_m->DrawPopupPerhaps();
#endif

  GRAPHICS_END_FRAME();
}

void draw_save_message(uint8_t c) {
  GRAPHICS_BEGIN_FRAME(true);
  uint8_t _size = c % 120;
  graphics.setPrintPos(37, 18);
  graphics.print("Saving...");
  graphics.drawRect(0, 28, _size, 8);
  GRAPHICS_END_FRAME();
}

void Ui::AppSettings() {

  SetButtonIgnoreMask();

  apps::current_app->HandleAppEvent(APP_EVENT_SUSPEND);

  menu::ScreenCursor<5> cursor;
  cursor.Init(0, NUM_AVAILABLE_APPS - 1);
  cursor.Scroll(apps::index_of(global_settings.current_app_id));

  bool change_app = false;
  bool save = false;
  while (!change_app && idle_time() < APP_SELECTION_TIMEOUT_MS) {

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
            SERIAL_PRINTLN("Encoder acceleration: %s", enabled ? "enabled" : "disabled");
            ui.encoders_enable_acceleration(enabled);
            global_settings.encoders_enable_acceleration = enabled;
        }
        break;

        default: break;
      }
    }

    draw_app_menu(cursor);
    delay(2); // VOR calibration hack
  }

  event_queue_.Flush();
  event_queue_.Poke();

  CORE::app_isr_enabled = false;
  delay(1);

  if (change_app) {
    apps::set_current_app(cursor.cursor_pos());
    FreqMeasure.end();
    OC::DigitalInputs::reInit();
    if (save) {
      save_global_settings();
      save_app_data();
      // draw message:
      int cnt = 0;
      while(idle_time() < SETTINGS_SAVE_TIMEOUT_MS)
        draw_save_message((cnt++) >> 4);
    }
  }

  OC::ui.encoders_enable_acceleration(global_settings.encoders_enable_acceleration);

  // Restore state
  apps::current_app->HandleAppEvent(APP_EVENT_RESUME);
  CORE::app_isr_enabled = true;
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
  OC::apps::set_current_app(0); // switch to Settings app
  Settings_instance.Calibration(); // Set up calibration mode in Settings app
}

}; // namespace OC
