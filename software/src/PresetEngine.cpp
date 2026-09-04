// Whole-module preset engine for the 200e preset bus. See PresetEngine.h.
#if defined(ARDUINO_TEENSY41)

#include <Arduino.h>
#include "src/extern/dspinst.h"  // before <Audio.h>: same include guard,
                                 // and Audio's copy lacks some intrinsics
#include <Audio.h>
#include <SD.h>

#include "PresetEngine.h"
#include "OC_apps.h"
#include "OC_app_switcher.h"
#include "OC_storage.h"
#include "OC_core.h"
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_ui.h"
#include "PhzConfig.h"
#include "HSUtils.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

extern uint_fast8_t MENU_REDRAW;  // Main.cpp

namespace OC {

// from OC_apps.cpp
extern void BuildAppData(AppData &data);
extern void ApplyAppData(const AppData &data);
extern void BuildGlobalSettingsValues();
extern void RestoreGlobalSettingsFromConfig(uint8_t scala_loaded_mask);
extern size_t ResolveAppIndexByID(uint16_t app_id);

namespace PresetEngine {

static constexpr uint16_t kQuadrantsAppId = TWOCCS("QS");
static constexpr uint8_t kScratchBank = 255;    // reserved: recall staging

// slot manifest keys, in the PRESETBUS namespace of PB_NN_G.CFG
static constexpr uint16_t kManifestKey = 8 << 8;    // == PRESETBUS_KEY
static constexpr uint16_t kSchemaKey  = kManifestKey | 0;
static constexpr uint16_t kFlagsKey   = kManifestKey | 1;
static constexpr uint64_t kSchemaVersion = 1;
// current bus slot, persisted (debounced) into GLOBALS.CFG so boot can
// restore the preset the case was on -- 200e power-up semantics
static constexpr uint16_t kCurSlotKey = kManifestKey | 0x13;

// Quadrants writes its live preset id under this bare (bank-globals) key
// when handling APP_EVENT_FLUSH, so the extractor knows which preset block
// to pull out of the bank map. 253 is unused in the bank key map.
static constexpr uint16_t kQuadLivePresetKey = 253;

enum ContentFlags : uint8_t {
  CONTENT_BANK    = 1 << 0,   // PB_NN_B.DAT present (Quadrants was active)
  CONTENT_SCENERY = 1 << 1,
  CONTENT_CAPTAIN = 1 << 2,
};

// ---- state -----------------------------------------------------------------
static volatile int8_t pending_save = -1;    // last-wins
static volatile int8_t pending_recall = -1;
static int8_t last_slot = -1;
static bool last_was_save = false;
static uint32_t cur_slot_dirty_ms = 0;   // 0 = clean
static uint32_t op_count = 0;            // completed save/recall operations
static bool last_save_ok = false;
static bool busy = false;
static int quad_recall_hint = -1;
static bool skip_captain_restore = false;  // boot recall only

static DMAMEM AppData capture;               // RAM capture buffer (~4KB)

// active Quadrants preset during bank extraction (predicate/remap context)
static uint8_t extract_preset;

// ---- helpers ---------------------------------------------------------------

static FS &slot_fs() { return SDcard_Ready ? (FS &)SD : (FS &)PhzConfig::myfs; }
static void load_names();  // defined with the name store below

FLASHMEM static void slot_name(char *buf, uint8_t slot, char kind, const char *ext) {
  // PB_NN_K.EXT
  buf[0] = 'P'; buf[1] = 'B'; buf[2] = '_';
  buf[3] = '0' + slot / 10; buf[4] = '0' + slot % 10;
  buf[5] = '_'; buf[6] = kind; buf[7] = '.';
  buf[8] = ext[0]; buf[9] = ext[1]; buf[10] = ext[2]; buf[11] = 0;
}

FLASHMEM static bool copy_file(FS &fs, const char *from, const char *to) {
  File src = fs.open(from);
  if (!src) return false;
  fs.remove(to);
  File dst = fs.open(to, FILE_WRITE_BEGIN);
  if (!dst) { src.close(); return false; }
  uint8_t buf[512];
  bool ok = true;
  int n;
  while ((n = src.read(buf, sizeof(buf))) > 0) {
    if (dst.write(buf, n) != (size_t)n) { ok = false; break; }
  }
  src.close();
  dst.close();
  if (!ok) fs.remove(to);
  return ok;
}

// PB_NN_A.BIN framing
struct AppDataHeader {
  uint32_t fourcc;    // 'OCPB'
  uint16_t version;
  uint16_t used;
  uint16_t checksum;  // 16-bit sum over payload
  uint16_t reserved;
};
static constexpr uint32_t kAppDataFourcc = 0x4243504FUL;  // "OPCB" LE = 'O','P','C','B'... spelled: bytes O C P B
static uint16_t sum16(const uint8_t *p, size_t n) {
  uint16_t s = 0;
  while (n--) s += *p++;
  return s;
}

FLASHMEM static bool write_appdata_file(uint8_t slot) {
  char name[12];
  slot_name(name, slot, 'A', "BIN");
  FS &fs = slot_fs();
  fs.remove(name);
  File f = fs.open(name, FILE_WRITE_BEGIN);
  if (!f) return false;
  AppDataHeader h = { kAppDataFourcc, 1, (uint16_t)capture.used,
                      sum16(capture.data, capture.used), 0 };
  bool ok = f.write((const uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            f.write(capture.data, capture.used) == capture.used;
  f.close();
  if (!ok) fs.remove(name);
  return ok;
}

FLASHMEM static bool read_appdata_file(uint8_t slot) {
  char name[12];
  slot_name(name, slot, 'A', "BIN");
  File f = slot_fs().open(name);
  if (!f) return false;
  AppDataHeader h;
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            h.fourcc == kAppDataFourcc && h.version == 1 &&
            h.used <= AppData::kAppDataSize;
  if (ok) {
    ok = f.read(capture.data, h.used) == h.used &&
         sum16(capture.data, h.used) == h.checksum;
    capture.used = h.used;
  }
  f.close();
  return ok;
}

// bank-key extraction: keep the active preset's block + the bank globals
// (bare keys 100-255) + VERSION_KEY; remap the preset block to preset 0.
FLASHMEM static bool bank_pred(PhzConfig::KEY k) {
  if (k == 0xFFFF) return true;                     // VERSION_KEY
  const uint16_t block = k >> 11;
  const uint16_t low = k & 0x7FF;
  if (block == 0 && low >= 100 && low < 256) return true;  // bank globals
  if (block == extract_preset && (low < 100 || low >= 256)) return true;
  return false;
}
FLASHMEM static PhzConfig::KEY bank_remap(PhzConfig::KEY k) {
  if (k == 0xFFFF) return k;
  const uint16_t low = k & 0x7FF;
  if ((k >> 11) == extract_preset && (low < 100 || low >= 256))
    return low;  // move to preset block 0
  return k;
}

// ---- save ------------------------------------------------------------------

FLASHMEM bool SaveSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  serial_printf("PresetEngine: save slot %d\n", slot);

  // free-space guard (LittleFS only; SD is effectively unbounded).
  // usedSize() has been observed reporting == totalSize() on a healthy FS,
  // so treat that state as "unknown" and rely on post-write verification.
  if (!SDcard_Ready) {
    const uint64_t total = PhzConfig::myfs.totalSize();
    const uint64_t used = PhzConfig::myfs.usedSize();
    if (used < total && total - used < 24 * 1024) {
      HS::PokePopup(HS::MESSAGE_POPUP, "Disk full !!");
      busy = false;
      return false;
    }
  }

  const uint16_t app_id = app_switcher.current_app()->id();
  uint8_t flags = 0;
  char name[12], name2[12];

  // 1. capture the app-data chunk stream to RAM (fast; ISR-bracketed)
  CORE::app_isr_enabled = false;
  delay(1);
  BuildAppData(capture);
  CORE::app_isr_enabled = true;

  // 2. ask the active app to flush its file-backed state
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_FLUSH);

  // 3. Quadrants active: extract its live preset + bank globals from the map
  if (app_id == kQuadrantsAppId) {
    uint64_t p = 0;
    if (PhzConfig::getValue(kQuadLivePresetKey, p) && p < 32) {
      extract_preset = (uint8_t)p;
      slot_name(name, slot, 'B', "DAT");
      if (PhzConfig::save_filtered(name, slot_fs(), bank_pred, bank_remap))
        flags |= CONTENT_BANK;
    }
  }

  // 4. copy the file-backed app stores
  slot_name(name, slot, 'S', "DAT");
  if (copy_file(slot_fs(), "SCENERY.DAT", name)) flags |= CONTENT_SCENERY;
  else if (!SDcard_Ready && copy_file(PhzConfig::myfs, "SCENERY.DAT", name)) flags |= CONTENT_SCENERY;
  slot_name(name, slot, 'C', "DAT");
  if (copy_file(slot_fs(), "CAPTAIN.DAT", name)) flags |= CONTENT_CAPTAIN;

  // 5. globals + manifest into PB_NN_G.CFG
  PhzConfig::load_config();  // base map = GLOBALS.CFG
  BuildGlobalSettingsValues();
  PhzConfig::setValue(kSchemaKey, kSchemaVersion);
  PhzConfig::setValue(kFlagsKey, flags);
  slot_name(name, slot, 'G', "CFG");
  bool ok = PhzConfig::save_config(name, slot_fs());
  // verify on disk: LittleFS has produced 0-byte files while save_config
  // reported success (full/degraded FS) — a slot that can't recall is worse
  // than a failed save
  if (ok) {
    File f = slot_fs().open(name, FILE_READ);
    ok = f && f.size() > 16;
    if (f) f.close();
  }

  // 6. app-data chunk stream
  bool ok2 = write_appdata_file(slot);

  // 7. hand the config map back to the active app (Resume reloads its file)
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);

  last_slot = slot;
  last_was_save = true;
  last_save_ok = ok && ok2;
  cur_slot_dirty_ms = millis() | 1;
  op_count++;
  busy = false;
  (void)name2;
  HS::PokePopup(HS::MESSAGE_POPUP, (ok && ok2) ? "Bus save OK" : "Bus save ERR");
  serial_printf("PresetEngine: save slot %d %s (flags %02x)\n",
                slot, (ok && ok2) ? "ok" : "FAILED", flags);
  return ok && ok2;
}

// ---- recall ----------------------------------------------------------------

FLASHMEM bool RecallSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  serial_printf("PresetEngine: recall slot %d\n", slot);

  char name[12];

  // 1. validate everything before touching live state
  if (!read_appdata_file(slot)) {
    HS::PokePopup(HS::MESSAGE_POPUP, "Empty preset");
    // put the map back (read_appdata doesn't touch it, but stay safe)
    busy = false;
    return false;
  }
  slot_name(name, slot, 'G', "CFG");
  if (!PhzConfig::load_config(name, slot_fs())) {
    // map now cleared/partial: restore base-map ownership, then let the
    // app reload its own file so no later writer inherits slot content
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset");
    busy = false;
    return false;
  }
  uint64_t schema = 0, flags = 0, meta = 0;
  PhzConfig::getValue(kSchemaKey, schema);
  PhzConfig::getValue(kFlagsKey, flags);
  PhzConfig::getValue((uint16_t)(1 << 8) /* METADATA_KEY */, meta);
  const uint16_t slot_app_id = meta & 0xFFFF;
  if (schema != kSchemaVersion) {
    PhzConfig::load_config();  // drop slot content, re-own the base map
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset ver");
    busy = false;
    return false;
  }

  // 2. freeze the app world
  CORE::app_isr_enabled = false;
  CORE::app_loop_enabled = false;
  delay(1);

  // 3. stage the file-backed stores
  if (flags & CONTENT_BANK) {
    slot_name(name, slot, 'B', "DAT");
    copy_file(slot_fs(), name, "BANK_255.DAT");
    if (SDcard_Ready) copy_file(SD, name, "BANK_255.DAT");
    quad_recall_hint = kScratchBank;
  }
  if (flags & CONTENT_SCENERY) {
    slot_name(name, slot, 'S', "DAT");
    copy_file(slot_fs(), name, "SCENERY.DAT");
  }
  // Boot recall deliberately keeps the LIVE Captain config: it's the
  // module's MIDI-interface setup (autosaved continuously), not scene
  // state - restoring the slot's snapshot at power-up silently rewound
  // the owner's mapping edits. Explicit recalls still restore it.
  if ((flags & CONTENT_CAPTAIN) && !skip_captain_restore) {
    slot_name(name, slot, 'C', "DAT");
    copy_file(slot_fs(), name, "CAPTAIN.DAT");
  }

  // 4. apply global settings (map still holds PB_NN_G.CFG) + app chunks
  RestoreGlobalSettingsFromConfig(0);
  Scales::Validate();
  Chords::Validate();
  for (int i = 0; i < HS::TURING_MACHINE_COUNT; ++i)
    HS::user_turing_machines[i].Validate();
  ApplyAppData(capture);

  // 5. switch to the slot's app (missing app: stay put, partial recall)
  const size_t idx = ResolveAppIndexByID(slot_app_id);

  FreqMeasure.end();
  DigitalInputs::reInit();

  AudioNoInterrupts();
  app_switcher.set_current_app(idx);
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  AudioInterrupts();

  // 6. run
  CORE::app_isr_enabled = true;
  CORE::app_loop_enabled = true;
  ::MENU_REDRAW = 1;

  last_slot = slot;
  last_was_save = false;
  cur_slot_dirty_ms = millis() | 1;
  op_count++;
  busy = false;
  HS::PokePopup(HS::MESSAGE_POPUP, "Bus recall OK");
  serial_printf("PresetEngine: recall slot %d done (app %04x)\n", slot, slot_app_id);
  return true;
}

// ---- service ---------------------------------------------------------------

FLASHMEM void Init() {
  pending_save = pending_recall = -1;
  quad_recall_hint = -1;
  load_names();
}

void RequestSave(uint8_t slot) {
  if (slot < kNumSlots) pending_save = slot;
}
void RequestRecall(uint8_t slot) {
  if (slot < kNumSlots) pending_recall = slot;
}

// persist the current slot ~3s after preset activity settles, so
// trigger-driven preset cycling never write-hammers GLOBALS.CFG
FLASHMEM static void persist_cur_slot() {
  cur_slot_dirty_ms = 0;
  if (last_slot < 0) return;
  PhzConfig::load_config();
  uint64_t v = 0;
  PhzConfig::getValue(kCurSlotKey, v);
  const bool changed = (int64_t)v != last_slot;
  if (changed) {
    PhzConfig::setValue(kCurSlotKey, (uint64_t)last_slot);
    PhzConfig::save_config();
  }
  // CRITICAL: hand the shared map back to the active app. Quadrants
  // assumes bank-map residency; leaving GLOBALS loaded here would make
  // its next preset save overwrite the bank file with the wrong map.
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
}

FLASHMEM void BootRecall() {
  // GLOBALS.CFG is the loaded map right after boot restore
  uint64_t v = 0;
  PhzConfig::load_config();
  if (!PhzConfig::getValue(kCurSlotKey, v) || v >= kNumSlots) return;
  if (!SlotUsed((uint8_t)v)) return;
  serial_printf("PresetEngine: boot recall slot %d\n", (int)v);
  skip_captain_restore = true;   // cleared after the first Process() pass
  RequestRecall((uint8_t)v);     // local only: no bus broadcast at power-up
}

FLASHMEM void Process() {
  const int8_t s = pending_save;
  if (s >= 0) {
    pending_save = -1;
    SaveSlot(s);
  }
  const int8_t r = pending_recall;
  if (r >= 0) {
    pending_recall = -1;
    RecallSlot(r);
    skip_captain_restore = false;   // only ever true for the boot recall
  }
  if (cur_slot_dirty_ms && millis() - cur_slot_dirty_ms > 3000)
    persist_cur_slot();
}

FLASHMEM int ConsumeQuadrantsRecallHint() {
  const int h = quad_recall_hint;
  quad_recall_hint = -1;
  return h;
}

int8_t LastSlot() { return last_slot; }
uint32_t OpCount() { return op_count; }
bool LastSaveOk() { return last_save_ok; }

// ---- slot names --------------------------------------------------------
// One flat 30x16 byte file, whole thing cached in RAM. Deliberately not a
// PhzConfig file: reads/writes must never disturb the shared config map.
static char name_cache[kNumSlots][kNameLen + 1];  // +1: always NUL-safe

FLASHMEM static void load_names() {
  memset(name_cache, 0, sizeof(name_cache));
  File f = slot_fs().open("PBNAMES.BIN", FILE_READ);
  if (!f) return;
  for (int i = 0; i < kNumSlots; ++i) {
    if (f.read((uint8_t *)name_cache[i], kNameLen) != kNameLen) break;
    name_cache[i][kNameLen] = 0;
  }
  f.close();
}

const char *SlotName(uint8_t slot) {
  return (slot < kNumSlots) ? name_cache[slot] : "";
}

FLASHMEM void SetSlotName(uint8_t slot, const char *name) {
  if (slot >= kNumSlots) return;
  memset(name_cache[slot], 0, sizeof(name_cache[slot]));
  strncpy(name_cache[slot], name, kNameLen);
  // trim trailing spaces so "unnamed" stays honest
  for (int i = (int)strlen(name_cache[slot]) - 1;
       i >= 0 && name_cache[slot][i] == ' '; --i)
    name_cache[slot][i] = 0;
  File f = slot_fs().open("PBNAMES.BIN", FILE_WRITE_BEGIN);
  if (!f) return;
  for (int i = 0; i < kNumSlots; ++i)
    f.write((const uint8_t *)name_cache[i], kNameLen);
  f.close();
}

FLASHMEM bool SlotUsed(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  char name[12];
  slot_name(name, slot, 'G', "CFG");
  File f = slot_fs().open(name, FILE_READ);
  const bool used = f && f.size() > 16;
  if (f) f.close();
  return used;
}
bool LastWasSave() { return last_was_save; }
bool Busy() { return busy; }

}  // namespace PresetEngine
}  // namespace OC

#endif  // ARDUINO_TEENSY41
