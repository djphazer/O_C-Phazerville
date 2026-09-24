#if defined(ARDUINO_TEENSY41)

#include <Arduino.h>
#include "src/extern/dspinst.h"
#include <Audio.h>
#include <SD.h>

#include "PresetEngine.h"
#include "PresetStage.h"
#include "RtStats.h"
#include "OC_apps.h"
#include "OC_app_switcher.h"
#include "OC_storage.h"
#include "OC_core.h"
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_ui.h"
#include "PhzConfig.h"
#include "PresetBus.h"
#include "Buchla200eWriteGuard.h"
#include "HSUtils.h"
#include "PresetOpQueue.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

extern uint_fast8_t MENU_REDRAW;
extern void watchdog_feed();
__attribute__((weak)) uint32_t stack_low_water() { return 0xFFFFFFFFu; }

namespace OC {

extern void BuildAppData(AppData &data);
extern void ApplyAppData(const AppData &data);
extern void BuildGlobalSettingsValues();
extern void RestoreGlobalSettingsFromConfig(uint8_t scala_loaded_mask);
extern size_t ResolveAppIndexByID(uint16_t app_id);

namespace PresetEngine {

static constexpr uint16_t kQuadrantsAppId = TWOCCS("QS");
static constexpr uint8_t kScratchBank = 255;

static constexpr uint16_t kManifestKey = 8 << 8;
static constexpr uint16_t kSchemaKey  = kManifestKey | 0;
static constexpr uint16_t kFlagsKey   = kManifestKey | 1;
static constexpr uint16_t kNameKey0   = kManifestKey | 2;
static constexpr uint16_t kNameKey1   = kManifestKey | 3;
static constexpr uint64_t kSchemaVersion = 1;
static constexpr uint16_t kCurSlotKey = kManifestKey | 0x13;

static constexpr uint8_t kCurRecMagic = 'P';

FLASHMEM static bool cur_rec_load(int8_t *slot, uint16_t *app) {
  uint8_t rec[EEPROM_PRESETBUS_SIZE];
  EEPROMStorage::read(EEPROM_PRESETBUS_START, rec, sizeof(rec));
  *slot = -1;
  *app = 0;
  if (rec[0] != kCurRecMagic) return false;
  if (rec[2] == (uint8_t)~rec[1] && rec[1] < kNumSlots) *slot = (int8_t)rec[1];
  if (rec[5] == (uint8_t)~rec[3] && rec[6] == (uint8_t)~rec[4])
    *app = (uint16_t)(rec[3] | (rec[4] << 8));
  return true;
}

FLASHMEM static void cur_rec_store(int8_t slot, uint16_t app) {
  const uint8_t s = slot < 0 ? 0xFF : (uint8_t)slot;
  const uint8_t lo = (uint8_t)app, hi = (uint8_t)(app >> 8);
  const uint8_t rec[EEPROM_PRESETBUS_SIZE] = {
    kCurRecMagic, s, (uint8_t)~s, lo, hi, (uint8_t)~lo, (uint8_t)~hi, 0xFF,
  };
  EEPROMStorage::update(EEPROM_PRESETBUS_START, rec, sizeof(rec));
}

static constexpr uint16_t kQuadLivePresetKey = 253;

static constexpr uint16_t kQuadLinkBankKey = (8 << 8) | 0x13;
static constexpr uint16_t kQuadLinkEnabledKey = (8 << 8) | 0x14;
static constexpr uint8_t kQuadPresetCount = 32;
static constexpr uint8_t kLinkSlots = kNumSlots < kQuadPresetCount ? kNumSlots : kQuadPresetCount;
static constexpr uint16_t kMetadataKey = 1 << 8;

enum ContentFlags : uint8_t {
  CONTENT_BANK    = 1 << 0,
  CONTENT_SCENERY = 1 << 1,
  CONTENT_CAPTAIN = 1 << 2,
};
static_assert(int(CONTENT_BANK) == int(REPLACES_BANK) &&
              int(CONTENT_SCENERY) == int(REPLACES_SCENERY) &&
              int(CONTENT_CAPTAIN) == int(REPLACES_CAPTAIN),
              "RecallReplaces mirrors ContentFlags");

static uint32_t lfs_block_bytes() {
  const uint32_t b = PhzConfig::myfs.blockBytes();
  return b ? b : 4096;
}
static constexpr uint32_t kSaveBytesNeeded = 65536;

enum ReqOp : uint8_t { REQ_SAVE, REQ_RECALL, REQ_BOOT_RECALL };
static PresetOpQueue<8, REQ_RECALL> req_q;

static int8_t last_slot = -1;
static int8_t bus_slot = -1;
static bool last_was_save = false;
static uint32_t cur_slot_dirty_ms = 0;
static int8_t persisted_slot = -1;
static uint16_t persisted_app = 0;
static uint32_t op_count = 0;
static bool last_save_ok = false;
static const char *last_recall_err = nullptr;
static bool busy = false;
static int quad_recall_hint = -1;
static uint32_t deferred_since = 0;
static constexpr uint32_t kDeferCapMs = 45000;
static bool boot_recall = false;
static uint8_t recall_replacing = 0;

static DMAMEM AppData capture;

static constexpr uint32_t kSecBufBytes = 16384;
static DMAMEM uint8_t sec_buf[kSecBufBytes];

static constexpr uint32_t kStageArenaBytes = 24576;
static DMAMEM uint8_t stage_arena[kStageArenaBytes];
static PresetStage recall_stage(stage_arena, kStageArenaBytes);
static constexpr uint32_t kStageTmpBytes = 12288;
static DMAMEM uint8_t stage_tmp[kStageTmpBytes];
static uint32_t last_recall_ms = 0;

static uint8_t extract_preset;

struct WallClock {
  uint32_t last;
  uint64_t cycles;
  void start() { last = ARM_DWT_CYCCNT; cycles = 0; }
  uint32_t lap_ms() {
    const uint32_t now = ARM_DWT_CYCCNT;
    const uint32_t d = now - last;
    last = now;
    cycles += d;
    return d / (F_CPU_ACTUAL / 1000);
  }
  uint32_t total_ms() const { return (uint32_t)(cycles / (F_CPU_ACTUAL / 1000)); }
};

static FS &preset_fs() { return PhzConfig::myfs; }

static FS &quad_fs() { return SDcard_Ready ? (FS &)SD : (FS &)PhzConfig::myfs; }

static FS *card_fs() { return SDcard_Ready ? (FS *)&SD : nullptr; }
static void load_names();
static void pack_name(uint8_t slot, uint64_t out[2]);

FLASHMEM static void slot_name(char *buf, uint8_t slot, char kind, const char *ext) {
  buf[0] = 'P'; buf[1] = 'B'; buf[2] = '_';
  buf[3] = '0' + slot / 10; buf[4] = '0' + slot % 10;
  buf[5] = '_'; buf[6] = kind; buf[7] = '.';
  buf[8] = ext[0]; buf[9] = ext[1]; buf[10] = ext[2]; buf[11] = 0;
}

FLASHMEM static bool copy_file(FS &sfs, const char *from, FS &dfs, const char *to) {
  File src = sfs.open(from);
  if (!src) return false;
  dfs.remove(to);
  File dst = dfs.open(to, FILE_WRITE_BEGIN);
  if (!dst) { src.close(); return false; }
  uint8_t buf[512];
  bool ok = true;
  int n;
  while ((n = src.read(buf, sizeof(buf))) > 0) {
    if (dst.write(buf, n) != (size_t)n) { ok = false; break; }
  }
  src.close();
  dst.close();
  if (!ok) dfs.remove(to);
  return ok;
}

struct AppDataHeader {
  uint32_t fourcc;
  uint16_t version;
  uint16_t used;
  uint16_t checksum;
  uint16_t reserved;
};
static constexpr uint32_t kAppDataFourcc = 0x4243504FUL;
static uint16_t sum16(const uint8_t *p, size_t n) {
  uint16_t s = 0;
  while (n--) s += *p++;
  return s;
}

FLASHMEM static bool read_appdata_stream(File &f) {
  AppDataHeader h;
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            h.fourcc == kAppDataFourcc && h.version == 1 &&
            h.used <= AppData::kAppDataSize;
  if (ok) {
    ok = f.read(capture.data, h.used) == h.used &&
         sum16(capture.data, h.used) == h.checksum;
    capture.used = h.used;
  }
  return ok;
}

FLASHMEM static bool read_appdata_file(uint8_t slot, FS &fs = preset_fs()) {
  char name[12];
  slot_name(name, slot, 'A', "BIN");
  File f = fs.open(name);
  if (!f) return false;
  const bool ok = read_appdata_stream(f);
  f.close();
  return ok;
}

static constexpr uint32_t kContainerFourcc = 0x53425058UL;
static constexpr uint16_t kContainerVersion = 1;
static constexpr int kMaxSections = 6;

struct ContainerHeader {
  uint32_t fourcc;
  uint16_t version;
  uint16_t count;
  uint32_t reserved0;
  uint32_t reserved1;
};
struct SectionEntry {
  uint8_t  kind;
  uint8_t  pad;
  uint16_t checksum;
  uint32_t offset;
  uint32_t length;
};
static_assert(sizeof(ContainerHeader) == 16, "container header must be 16 bytes");
static_assert(sizeof(SectionEntry) == 12, "section entry must be 12 bytes");

static constexpr uint32_t kPayloadStart =
    sizeof(ContainerHeader) + kMaxSections * sizeof(SectionEntry);

FLASHMEM static void container_name(char *buf, uint8_t slot) {
  buf[0] = 'P'; buf[1] = 'B'; buf[2] = '_';
  buf[3] = '0' + slot / 10; buf[4] = '0' + slot % 10;
  buf[5] = '.'; buf[6] = 'P'; buf[7] = 'B'; buf[8] = 'S'; buf[9] = 0;
}

struct SectionSrc {
  FS *fs;
  const char *path;
  const uint8_t *mem, *mem2;
  uint32_t len, len2;
};

struct ContainerWriter {
  SectionEntry sec[kMaxSections];
  SectionSrc src[kMaxSections];
  int n;
  uint32_t pos;
  bool ok;
};

FLASHMEM static void cw_begin(ContainerWriter &w) {
  w.n = 0;
  w.pos = kPayloadStart;
  w.ok = true;
}

FLASHMEM static bool cw_plan_file(ContainerWriter &w, char kind,
                                  FS &fs, const char *path) {
  if (!w.ok || w.n >= kMaxSections) return false;
  File s = fs.open(path, FILE_READ);
  if (!s) return false;
  const uint32_t want = (uint32_t)s.size();
  uint8_t buf[256];
  uint16_t sum = 0;
  uint32_t len = 0;
  int r;
  while ((r = s.read(buf, sizeof(buf))) > 0) {
    for (int i = 0; i < r; ++i) sum += buf[i];
    len += (uint32_t)r;
  }
  s.close();
  if (!len) return false;
  if (len != want) {
    serial_printf("PresetEngine: section '%c' short: %lu of %lu bytes\n",
                  kind, (unsigned long)len, (unsigned long)want);
    w.ok = false;
    return false;
  }
  w.sec[w.n] = { (uint8_t)kind, 0, sum, w.pos, len };
  w.src[w.n] = { &fs, path, nullptr, nullptr, 0, 0 };
  w.n++;
  w.pos += len;
  return true;
}

FLASHMEM static bool cw_plan_mem(ContainerWriter &w, char kind,
                                 const uint8_t *a, uint32_t la,
                                 const uint8_t *b = nullptr, uint32_t lb = 0) {
  if (!w.ok || w.n >= kMaxSections || !la) return false;
  const uint16_t sum = (uint16_t)(sum16(a, la) + (b ? sum16(b, lb) : 0));
  w.sec[w.n] = { (uint8_t)kind, 0, sum, w.pos, la + lb };
  w.src[w.n] = { nullptr, nullptr, a, b, la, lb };
  w.n++;
  w.pos += la + lb;
  return true;
}

static AppDataHeader appdata_hdr;
FLASHMEM static bool cw_plan_appdata(ContainerWriter &w) {
  appdata_hdr = { kAppDataFourcc, 1, (uint16_t)capture.used,
                  sum16(capture.data, capture.used), 0 };
  return cw_plan_mem(w, 'A', (const uint8_t *)&appdata_hdr, sizeof(appdata_hdr),
                     capture.data, capture.used);
}

FLASHMEM static bool cw_put(File &f, const uint8_t *p, uint32_t n) {
  return n == 0 || f.write(p, n) == n;
}

FLASHMEM static bool cw_copy_file(File &f, FS &fs, const char *path, uint32_t want) {
  File s = fs.open(path, FILE_READ);
  if (!s) return false;
  uint8_t buf[256];
  uint32_t len = 0;
  int r;
  bool ok = true;
  while (ok && (r = s.read(buf, sizeof(buf))) > 0) {
    ok = f.write(buf, r) == (size_t)r;
    len += (uint32_t)r;
    watchdog_feed();
  }
  s.close();
  return ok && len == want;
}

FLASHMEM static bool cw_commit(ContainerWriter &w, const char *tmp, const char *final_name) {
  if (!w.ok) return false;
  preset_fs().remove(tmp);
  File f = preset_fs().open(tmp, FILE_WRITE_BEGIN);
  if (!f) return false;

  const ContainerHeader h = { kContainerFourcc, kContainerVersion,
                              (uint16_t)w.n, 0, 0 };
  bool ok = cw_put(f, (const uint8_t *)&h, sizeof(h));
  for (int i = 0; i < kMaxSections && ok; ++i) {
    const SectionEntry e = (i < w.n) ? w.sec[i] : SectionEntry{ 0, 0, 0, 0, 0 };
    ok = cw_put(f, (const uint8_t *)&e, sizeof(e));
  }
  for (int i = 0; i < w.n && ok; ++i) {
    const SectionSrc &src = w.src[i];
    if (src.fs) ok = cw_copy_file(f, *src.fs, src.path, w.sec[i].length);
    else ok = cw_put(f, src.mem, src.len) && cw_put(f, src.mem2, src.len2);
    watchdog_feed();
  }
  f.close();
  if (!ok) { preset_fs().remove(tmp); return false; }
  File v = preset_fs().open(tmp, FILE_READ);
  ok = v && (uint32_t)v.size() == w.pos;
  if (v) v.close();
  if (!ok) { preset_fs().remove(tmp); return false; }
  ok = preset_fs().rename(tmp, final_name);
  if (!ok) {
    preset_fs().remove(final_name);
    ok = preset_fs().rename(tmp, final_name);
    if (!ok)
      serial_printf("PresetEngine: %s left in place -- it is the only copy "
                    "of this slot\n", tmp);
    return ok;
  }
  return ok;
}

FLASHMEM static bool container_parse(File &f, SectionEntry *sec, int &n) {
  ContainerHeader h;
  if (f.read((uint8_t *)&h, sizeof(h)) != sizeof(h) ||
      h.fourcc != kContainerFourcc || h.version != kContainerVersion ||
      h.count > kMaxSections)
    return false;
  n = h.count;
  for (int i = 0; i < n; ++i) {
    if (f.read((uint8_t *)&sec[i], sizeof(sec[i])) != sizeof(sec[i]))
      return false;
    if (sec[i].offset < kPayloadStart ||
        (uint64_t)sec[i].offset + sec[i].length > (uint64_t)f.size())
      return false;
  }
  return true;
}

FLASHMEM static bool container_open(uint8_t slot, File &f, SectionEntry *sec, int &n) {
  char name[12];
  container_name(name, slot);
  f = preset_fs().open(name, FILE_READ);
  if (!f) return false;
  if (!container_parse(f, sec, n)) {
    f.close();
    return false;
  }
  return true;
}

FLASHMEM static void remove_legacy_slot(uint8_t slot) {
  static const char kinds[] = { 'G', 'A', 'B', 'S', 'C' };
  static const char *const exts[] = { "CFG", "BIN", "DAT", "DAT", "DAT" };
  char name[12];
  for (unsigned i = 0; i < sizeof(kinds); ++i) {
    slot_name(name, slot, kinds[i], exts[i]);
    preset_fs().remove(name);
  }
}

FLASHMEM static const SectionEntry *find_section(const SectionEntry *sec, int n, char kind) {
  for (int i = 0; i < n; ++i)
    if (sec[i].kind == (uint8_t)kind) return &sec[i];
  return nullptr;
}

FLASHMEM static bool section_to_mem(File &f, const SectionEntry &e,
                                    uint8_t *buf, uint32_t cap) {
  if (e.length > cap || !f.seek(e.offset)) return false;
  if (f.read(buf, e.length) != (int)e.length) return false;
  return sum16(buf, e.length) == e.checksum;
}

FLASHMEM static bool section_sum_ok(File &f, const SectionEntry &e) {
  if (!f.seek(e.offset)) return false;
  uint8_t buf[128];
  uint32_t left = e.length;
  uint16_t sum = 0;
  while (left) {
    const uint32_t want = left < sizeof(buf) ? left : sizeof(buf);
    if (f.read(buf, want) != (int)want) return false;
    sum += sum16(buf, want);
    left -= want;
  }
  return sum == e.checksum;
}

FLASHMEM static bool container_verify(FS &fs, const char *name) {
  File f = fs.open(name, FILE_READ);
  if (!f) return false;
  SectionEntry sec[kMaxSections];
  int n = 0;
  bool ok = container_parse(f, sec, n);
  for (int i = 0; ok && i < n; ++i) ok = section_sum_ok(f, sec[i]);
  if (ok) ok = find_section(sec, n, 'A') && find_section(sec, n, 'G');
  f.close();
  return ok;
}

static constexpr int32_t kSectionSame = -1;
static constexpr int32_t kSectionNoFile = -2;
FLASHMEM static int32_t section_diff_offset(File &f, const SectionEntry &e,
                                            const char *dest, FS &fs,
                                            uint32_t *file_size) {
  File d = fs.open(dest, FILE_READ);
  if (!d) { *file_size = 0; return kSectionNoFile; }
  *file_size = d.size();
  const uint32_t common = *file_size < e.length ? *file_size : e.length;
  int32_t result = kSectionSame;
  if (!f.seek(e.offset)) result = 0;
  uint8_t a[128], b[128];
  uint32_t pos = 0;
  while (result == kSectionSame && pos < common) {
    const uint32_t want = common - pos < sizeof(a) ? common - pos : sizeof(a);
    if (f.read(a, want) != (int)want || d.read(b, want) != (int)want) {
      result = (int32_t)pos;
      break;
    }
    for (uint32_t i = 0; i < want; ++i) {
      if (a[i] != b[i]) { result = (int32_t)(pos + i); break; }
    }
    pos += want;
  }
  if (result == kSectionSame && *file_size != e.length) result = (int32_t)common;
  d.close();
  return result;
}

FLASHMEM static bool section_to_file(File &f, const SectionEntry &e,
                                     const char *dest, FS &fs) {
  uint32_t on_flash = 0;
  const int32_t diff = section_diff_offset(f, e, dest, fs, &on_flash);
  if (diff == kSectionSame) return true;
  if (!f.seek(e.offset)) return false;
  WallClock wall;
  wall.start();
  static const char *const kScratch = "PB_XTR.TMP";
  fs.remove(kScratch);
  File d = fs.open(kScratch, FILE_WRITE_BEGIN);
  if (!d) return false;
  uint8_t buf[256];
  uint32_t left = e.length;
  uint16_t sum = 0;
  bool ok = true;
  while (left) {
    const uint32_t want = left < sizeof(buf) ? left : sizeof(buf);
    const int r = f.read(buf, want);
    if (r != (int)want) { ok = false; break; }
    if (d.write(buf, r) != (size_t)r) { ok = false; break; }
    for (int i = 0; i < r; ++i) sum += buf[i];
    left -= want;
    watchdog_feed();
  }
  d.close();
  const uint32_t write_ms = wall.lap_ms();
  if (ok) ok = (sum == e.checksum);
  if (!ok) { fs.remove(kScratch); return false; }
  fs.remove(dest);
  ok = fs.rename(kScratch, dest);
  if (!ok) fs.remove(kScratch);
  const unsigned long rename_ms = wall.lap_ms();
  if (diff == kSectionNoFile)
    serial_printf("PresetEngine: restored %s (%lu bytes, was absent) write %lu ms, rename %lu ms\n",
                  dest, (unsigned long)e.length, (unsigned long)write_ms, rename_ms);
  else
    serial_printf("PresetEngine: restored %s (%lu bytes, was %lu, differs at +%ld) write %lu ms, rename %lu ms\n",
                  dest, (unsigned long)e.length, (unsigned long)on_flash,
                  (long)diff, (unsigned long)write_ms, rename_ms);
  return ok;
}

FLASHMEM static bool bank_pred(PhzConfig::KEY k) {
  if (k == 0xFFFF) return true;
  const uint16_t block = k >> 11;
  const uint16_t low = k & 0x7FF;
  if (block == 0 && low >= 100 && low < 256) return true;
  if (block == extract_preset && (low < 100 || low >= 256)) return true;
  return false;
}
FLASHMEM static PhzConfig::KEY bank_remap(PhzConfig::KEY k) {
  if (k == 0xFFFF) return k;
  const uint16_t low = k & 0x7FF;
  if ((k >> 11) == extract_preset && (low < 100 || low >= 256))
    return low;
  return k;
}

static uint8_t inject_preset;
FLASHMEM static bool inject_pred(PhzConfig::KEY k) {
  const uint16_t low = k & 0x7FF;
  return (k >> 11) == 0 && (low < 100 || low >= 256);
}
FLASHMEM static PhzConfig::KEY inject_remap(PhzConfig::KEY k) {
  return (PhzConfig::KEY)(((uint16_t)inject_preset << 11) | (k & 0x7FF));
}

static uint8_t quad_link_bank = 0;
static bool quad_link_enabled = false;

uint8_t QuadLinkBank() { return quad_link_bank; }
bool QuadLinkEnabled() { return quad_link_enabled; }

FLASHMEM void SetQuadLinkBank(uint8_t bank) {
  if (bank >= kQuadBankCount) bank = kQuadBankCount - 1;
  quad_link_bank = bank;
  PhzConfig::load_config();
  PhzConfig::setValue(kQuadLinkBankKey, bank);
  PhzConfig::save_config();
}

FLASHMEM void SetQuadLinkEnabled(bool on) {
  quad_link_enabled = on;
  PhzConfig::load_config();
  PhzConfig::setValue(kQuadLinkEnabledKey, on ? 1 : 0);
  PhzConfig::save_config();
}

FLASHMEM static void bank_filename(char *buf, uint8_t bank) {
  snprintf(buf, 13, "BANK_%03u.DAT", bank);
}

FLASHMEM static void clear_slot(uint8_t slot) {
  if (SlotUsed(slot)) {
    char name[12];
    container_name(name, slot);
    preset_fs().remove(name);
    remove_legacy_slot(slot);
  }
  if (SlotName(slot)[0]) SetSlotName(slot, "");
}

FLASHMEM static bool build_slot_from_bank_preset(uint8_t bank, uint8_t slot) {
  char fname[13];
  bank_filename(fname, bank);
  if (!PhzConfig::load_config(fname, quad_fs())) return false;

  extract_preset = slot;
  const size_t blen = PhzConfig::serialize(sec_buf, kSecBufBytes, bank_pred, bank_remap);
  char label[kNameLen + 1];
  if (!blen || !QuadrantsPresetLabel(slot, label, sizeof(label))) {
    clear_slot(slot);
    return false;
  }

  if (!SlotName(slot)[0]) SetSlotName(slot, label);

  AppData quad_chunk;
  const bool have_quad_chunk = BuildSingleAppData(kQuadrantsAppId, quad_chunk);
  const AppDataHeader quad_hdr = { kAppDataFourcc, 1, (uint16_t)quad_chunk.used,
                                   sum16(quad_chunk.data, quad_chunk.used), 0 };

  PhzConfig::load_config();
  BuildGlobalSettingsValues();
  PhzConfig::setValue(kSchemaKey, kSchemaVersion);
  PhzConfig::setValue(kFlagsKey, (uint64_t)CONTENT_BANK);
  {
    uint64_t meta = 0;
    Pack(meta, PackLocation{0, 16}, (uint64_t)kQuadrantsAppId);
    PhzConfig::setValue(kMetadataKey, meta);
  }
  {
    uint64_t nm[2];
    pack_name(slot, nm);
    PhzConfig::setValue(kNameKey0, nm[0]);
    PhzConfig::setValue(kNameKey1, nm[1]);
  }
  const size_t glen = PhzConfig::serialize(sec_buf + blen, kSecBufBytes - blen);

  ContainerWriter w;
  cw_begin(w);
  char final_name[12];
  container_name(final_name, slot);
  const bool ok_b = cw_plan_mem(w, 'B', sec_buf, blen);
  const bool ok_a = have_quad_chunk &&
                    cw_plan_mem(w, 'A', (const uint8_t *)&quad_hdr, sizeof(quad_hdr),
                               quad_chunk.data, (uint32_t)quad_chunk.used);
  const bool ok_g = glen && cw_plan_mem(w, 'G', sec_buf + blen, glen);
  const bool ok = ok_b && ok_a && ok_g && cw_commit(w, "PB_CTR.TMP", final_name);
  if (!ok)
    serial_printf("PresetEngine: bank %u -> slot %u failed (B=%d A=%d G=%d)\n",
                  bank, slot, ok_b, ok_a, ok_g);
  return ok;
}

FLASHMEM int CopyBankToSlots(uint8_t bank) {
  if (busy) return -1;
  busy = true;
  const OC::RT::PersistenceWindow window("banklink", 5000);

  CORE::app_isr_enabled = false;
  delay(1);
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_FLUSH);
  watchdog_feed();

  {
    char fname[13];
    bank_filename(fname, bank);
    if (!PhzConfig::load_config(fname, quad_fs())) {
      CORE::app_isr_enabled = true;
      busy = false;
      serial_printf("PresetEngine: bank %u -> slots: no such bank\n", bank);
      return -1;
    }
  }

  int copied = 0;
  for (uint8_t i = 0; i < kLinkSlots; ++i) {
    if (build_slot_from_bank_preset(bank, i)) ++copied;
    watchdog_feed();
  }

  PhzConfig::load_config();
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);

  CORE::app_isr_enabled = true;
  busy = false;
  serial_printf("PresetEngine: bank %u -> %d slot(s)\n", bank, copied);
  return copied;
}

FLASHMEM void SyncPresetToSlot(uint8_t bank, uint8_t preset_id) {
  if (!Buchla200eHardware()) return;
  if (!quad_link_enabled || bank != quad_link_bank || preset_id >= (uint8_t)kNumSlots)
    return;
  if (busy) return;
  busy = true;
  const OC::RT::PersistenceWindow window("banklink", 500);
  build_slot_from_bank_preset(bank, preset_id);
  char fname[13];
  bank_filename(fname, bank);
  PhzConfig::load_config(fname, quad_fs());
  busy = false;
}

FLASHMEM static bool merge_slot_into_bank_map(uint8_t slot) {
  File f;
  SectionEntry sec[kMaxSections];
  int ns = 0;
  if (!container_open(slot, f, sec, ns)) return false;
  const SectionEntry *e = find_section(sec, ns, 'B');
  const bool have = e && section_to_mem(f, *e, sec_buf, kSecBufBytes);
  f.close();
  if (!have) return false;
  inject_preset = slot;
  return PhzConfig::deserialize_merge(sec_buf, e->length, inject_pred, inject_remap);
}

FLASHMEM static void sync_store_to_bank(uint8_t slot) {
  if (!quad_link_enabled || slot >= kLinkSlots) return;
  char fname[13];
  bank_filename(fname, quad_link_bank);
  PhzConfig::load_config(fname, quad_fs());
  if (merge_slot_into_bank_map(slot))
    PhzConfig::save_config(fname, quad_fs());
  PhzConfig::load_config();
}

FLASHMEM int CopySlotsToBank(uint8_t bank) {
  if (busy) return -1;
  busy = true;
  const OC::RT::PersistenceWindow window("banklink", 5000);
  char fname[13];
  bank_filename(fname, bank);

  CORE::app_isr_enabled = false;
  delay(1);
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_FLUSH);
  watchdog_feed();

  PhzConfig::load_config(fname, quad_fs());

  int copied = 0;
  for (uint8_t i = 0; i < kLinkSlots; ++i) {
    if (merge_slot_into_bank_map(i)) ++copied;
    watchdog_feed();
  }

  const bool ok = copied > 0 ? PhzConfig::save_config(fname, quad_fs()) : true;

  CORE::app_isr_enabled = true;
  busy = false;
  serial_printf("PresetEngine: bank %u <- %d slot(s)%s\n", bank, copied,
                ok ? "" : " (save FAILED)");
  return ok ? copied : -1;
}

FLASHMEM bool SaveSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  bus_slot = (int8_t)slot;
  serial_printf("PresetEngine: save slot %d\n", slot);

  const OC::RT::PersistenceWindow window("save");

  {
    const uint64_t total = PhzConfig::myfs.totalSize();
    const uint64_t used = PhzConfig::myfs.usedSize();
    const uint32_t block = lfs_block_bytes();
    const uint32_t blocks_needed = (kSaveBytesNeeded + block - 1) / block;
    if (used < total && (total - used) / block < blocks_needed) {
      HS::PokePopup(HS::MESSAGE_POPUP, "Disk full !!");
      busy = false;
      serial_printf("PresetEngine: save refused, %lu KB free < %lu blocks\n",
                    (unsigned long)((total - used) >> 10),
                    (unsigned long)blocks_needed);
      return false;
    }
  }

  const uint16_t app_id = app_switcher.current_app()->id();
  uint8_t flags = 0;
  static const char *const kCtrTmp = "PB_CTR.TMP";
  char final_name[12];
  container_name(final_name, slot);

  const uint32_t t_start = millis();
  const uint32_t isr_start = PresetBus::GetStats().isr_count;
  WallClock wall;
  wall.start();
  uint32_t ph[9] = { 0 };

  CORE::app_isr_enabled = false;
  delay(1);
  BuildAppData(capture);
  ph[0] = wall.lap_ms();

  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_FLUSH);
  watchdog_feed();
  ph[1] = wall.lap_ms();

  ContainerWriter w;
  cw_begin(w);
  uint32_t sec_used = 0;

  if (app_id == kQuadrantsAppId) {
    uint64_t p = 0;
    if (PhzConfig::getValue(kQuadLivePresetKey, p) && p < 32) {
      extract_preset = (uint8_t)p;
      if (!SlotName(slot)[0]) {
        char label[kNameLen + 1];
        if (QuadrantsPresetLabel((uint8_t)p, label, sizeof(label)))
          SetSlotName(slot, label);
      }
      const size_t n = PhzConfig::serialize(sec_buf, kSecBufBytes,
                                            bank_pred, bank_remap);
      if (n && cw_plan_mem(w, 'B', sec_buf, n)) {
        flags |= CONTENT_BANK;
        sec_used = n;
      } else if (!n) {
        serial_printf("PresetEngine: bank extract exceeds %lu bytes\n",
                      (unsigned long)kSecBufBytes);
        w.ok = false;
      }
    }
    watchdog_feed();
  }
  ph[2] = wall.lap_ms();

  if (cw_plan_file(w, 'S', PhzConfig::myfs, "SCENERY.DAT")) flags |= CONTENT_SCENERY;
  watchdog_feed();
  if (cw_plan_file(w, 'C', PhzConfig::myfs, "CAPTAIN.DAT")) flags |= CONTENT_CAPTAIN;
  watchdog_feed();
  ph[3] = wall.lap_ms();

  PhzConfig::load_config();
  BuildGlobalSettingsValues();
  PhzConfig::setValue(kSchemaKey, kSchemaVersion);
  PhzConfig::setValue(kFlagsKey, flags);
  {
    uint64_t nm[2];
    pack_name(slot, nm);
    PhzConfig::setValue(kNameKey0, nm[0]);
    PhzConfig::setValue(kNameKey1, nm[1]);
  }
  bool ok = false;
  {
    const size_t n = PhzConfig::serialize(sec_buf + sec_used,
                                          kSecBufBytes - sec_used);
    if (n) ok = cw_plan_mem(w, 'G', sec_buf + sec_used, n);
    else serial_printf("PresetEngine: globals exceed %lu bytes\n",
                       (unsigned long)(kSecBufBytes - sec_used));
  }
  watchdog_feed();
  ph[4] = wall.lap_ms();

  bool ok2 = cw_plan_appdata(w);
  watchdog_feed();
  ph[5] = wall.lap_ms();

  const bool committed = ok && ok2 && cw_commit(w, kCtrTmp, final_name);
  ok = committed;
  ok2 = committed;
  ph[6] = wall.lap_ms();

  if (committed) {
    File v;
    SectionEntry vsec[kMaxSections];
    int vn = 0;
    if (container_open(slot, v, vsec, vn)) {
      v.close();
      remove_legacy_slot(slot);
    } else {
      serial_printf("PresetEngine: slot %d container did not re-open; "
                    "legacy files kept\n", slot);
    }
  }
  watchdog_feed();
  ph[7] = wall.lap_ms();

  if (committed && (flags & CONTENT_BANK)) sync_store_to_bank(slot);
  watchdog_feed();

  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  CORE::app_isr_enabled = true;
  watchdog_feed();
  ph[8] = wall.lap_ms();

  last_slot = slot;
  last_was_save = true;
  last_save_ok = ok && ok2;
  cur_slot_dirty_ms = StampMs();
  op_count++;
  busy = false;
  {
    static char popup_text[20];
    snprintf(popup_text, sizeof(popup_text),
             (ok && ok2) ? "Stored %u" : "Store ERR %u", slot + 1);
    HS::PokePopup(HS::MESSAGE_POPUP, popup_text);
  }
  serial_printf("PresetEngine: save slot %d %s (flags %02x)\n",
                slot, (ok && ok2) ? "ok" : "FAILED", flags);
  serial_printf("PresetEngine: save took %lums wall (millis saw %lums), "
                "bus ISRs %lu, stack unused %lu bytes\n",
                (unsigned long)wall.total_ms(),
                (unsigned long)(millis() - t_start),
                (unsigned long)(PresetBus::GetStats().isr_count - isr_start),
                (unsigned long)stack_low_water());
  serial_printf("PresetEngine:   capture %lu flush %lu bank %lu files %lu "
                "globals %lu appdata %lu commit %lu verify %lu resume %lu\n",
                (unsigned long)ph[0], (unsigned long)ph[1], (unsigned long)ph[2],
                (unsigned long)ph[3], (unsigned long)ph[4], (unsigned long)ph[5],
                (unsigned long)ph[6], (unsigned long)ph[7], (unsigned long)ph[8]);
  return ok && ok2;
}

enum RecallStage : uint8_t { STAGE_OK = 0, STAGE_EMPTY = 1, STAGE_BAD = 2 };

FLASHMEM static RecallStage recall_stage_head(uint8_t slot, bool &from_container) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  from_container = container_open(slot, f, sec, n);

  if (from_container) {
    const SectionEntry *a = find_section(sec, n, 'A');
    const SectionEntry *g = find_section(sec, n, 'G');
    bool ok = a && g && f.seek(a->offset) && read_appdata_stream(f);
    if (ok) ok = section_to_mem(f, *g, sec_buf, kSecBufBytes);
    for (int i = 0; ok && i < n; ++i) {
      if (sec[i].kind == 'A' || sec[i].kind == 'G') continue;
      ok = section_sum_ok(f, sec[i]);
    }
    f.close();
    if (!ok) return STAGE_BAD;
    return PhzConfig::deserialize(sec_buf, g->length) ? STAGE_OK : STAGE_BAD;
  }

  if (!read_appdata_file(slot)) return STAGE_EMPTY;
  char name[12];
  slot_name(name, slot, 'G', "CFG");
  return PhzConfig::load_config(name, preset_fs()) ? STAGE_OK : STAGE_BAD;
}

FLASHMEM static bool stage_section(File &f, const SectionEntry &e, const char *dest) {
  if (e.length > kStageTmpBytes) return false;
  if (!f.seek(e.offset)) return false;
  uint32_t left = e.length, pos = 0;
  uint16_t sum = 0;
  while (left) {
    const uint32_t want = left < 256 ? left : 256;
    const int r = f.read(stage_tmp + pos, want);
    if (r != (int)want) return false;
    for (int i = 0; i < r; ++i) sum += stage_tmp[pos + i];
    pos += want;
    left -= want;
  }
  if (sum != e.checksum) return false;
  if (!recall_stage.put(dest, stage_tmp, e.length)) return false;
  serial_printf("PresetEngine: staged %s (%lu bytes) in RAM, disk syncs at idle\n",
                dest, (unsigned long)e.length);
  return true;
}

FLASHMEM static bool file_matches(FS &fs, const char *name, const uint8_t *data, uint32_t len) {
  File d = fs.open(name, FILE_READ);
  if (!d) return false;
  bool same = d.size() == len;
  uint8_t b[128];
  uint32_t pos = 0;
  while (same && pos < len) {
    const uint32_t want = len - pos < sizeof(b) ? len - pos : sizeof(b);
    if (d.read(b, want) != (int)want || memcmp(b, data + pos, want) != 0) same = false;
    pos += want;
  }
  d.close();
  return same;
}

FLASHMEM static bool bytes_to_file(FS &fs, const char *dest, const uint8_t *data, uint32_t len) {
  if (file_matches(fs, dest, data, len)) return true;
  static const char *const kScratch = "PB_XTR.TMP";
  fs.remove(kScratch);
  File d = fs.open(kScratch, FILE_WRITE_BEGIN);
  if (!d) return false;
  bool ok = true;
  uint32_t pos = 0;
  while (ok && pos < len) {
    const uint32_t want = len - pos < 256 ? len - pos : 256;
    if (d.write(data + pos, want) != (size_t)want) ok = false;
    pos += want;
    watchdog_feed();
  }
  d.close();
  if (!ok) { fs.remove(kScratch); return false; }
  fs.remove(dest);
  ok = fs.rename(kScratch, dest);
  if (!ok) fs.remove(kScratch);
  return ok;
}

FLASHMEM static void recall_stage_files(uint8_t slot, bool from_container,
                                        uint64_t flags) {
  recall_stage.begin_recall();
  if (from_container) {
    File f;
    SectionEntry sec[kMaxSections];
    int n = 0;
    if (!container_open(slot, f, sec, n)) return;
    const SectionEntry *e;
    if ((flags & CONTENT_BANK) && (e = find_section(sec, n, 'B')) != nullptr) {
      if (stage_section(f, *e, "BANK_255.DAT") ||
          section_to_file(f, *e, "BANK_255.DAT", quad_fs()))
        quad_recall_hint = kScratchBank;
      watchdog_feed();
    }
    if ((flags & CONTENT_SCENERY) && (e = find_section(sec, n, 'S')) != nullptr) {
      if (!stage_section(f, *e, "SCENERY.DAT"))
        section_to_file(f, *e, "SCENERY.DAT", PhzConfig::myfs);
      watchdog_feed();
    }
    if ((flags & CONTENT_CAPTAIN) && !boot_recall &&
        (e = find_section(sec, n, 'C')) != nullptr) {
      if (!stage_section(f, *e, "CAPTAIN.DAT"))
        section_to_file(f, *e, "CAPTAIN.DAT", PhzConfig::myfs);
      watchdog_feed();
    }
    f.close();
    return;
  }

  char name[12];
  if (flags & CONTENT_BANK) {
    slot_name(name, slot, 'B', "DAT");
    copy_file(preset_fs(), name, quad_fs(), "BANK_255.DAT");
    quad_recall_hint = kScratchBank;
    watchdog_feed();
  }
  if (flags & CONTENT_SCENERY) {
    slot_name(name, slot, 'S', "DAT");
    copy_file(preset_fs(), name, PhzConfig::myfs, "SCENERY.DAT");
    watchdog_feed();
  }
  if ((flags & CONTENT_CAPTAIN) && !boot_recall) {
    slot_name(name, slot, 'C', "DAT");
    copy_file(preset_fs(), name, PhzConfig::myfs, "CAPTAIN.DAT");
    watchdog_feed();
  }
}

FLASHMEM static bool recall_refused(uint8_t slot, const char *why) {
  last_recall_err = why;
  last_was_save = false;
  op_count++;
  busy = false;
  serial_printf("PresetEngine: recall slot %d refused (%s)\n", slot, why);
  return false;
}

FLASHMEM bool RecallSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  bus_slot = (int8_t)slot;
  serial_printf("PresetEngine: recall slot %d\n", slot);
  const uint32_t t_start = millis();
  WallClock wall;
  wall.start();
  uint32_t ph[5] = { 0 };

  bool from_container = false;
  const RecallStage stage = recall_stage_head(slot, from_container);
  ph[0] = wall.lap_ms();
  if (stage == STAGE_EMPTY) {
    HS::PokePopup(HS::MESSAGE_POPUP, "Empty preset");
    return recall_refused(slot, "EMPTY SLOT");
  }
  if (stage == STAGE_BAD) {
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset");
    return recall_refused(slot, "BAD PRESET");
  }
  uint64_t schema = 0, flags = 0, meta = 0;
  PhzConfig::getValue(kSchemaKey, schema);
  PhzConfig::getValue(kFlagsKey, flags);
  PhzConfig::getValue((uint16_t)(1 << 8) , meta);
  const uint16_t slot_app_id = meta & 0xFFFF;
  if (schema != kSchemaVersion) {
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset ver");
    return recall_refused(slot, "OLD PRESET");
  }

  recall_replacing = (uint8_t)(flags & (CONTENT_BANK | CONTENT_SCENERY | CONTENT_CAPTAIN));
  if (boot_recall) recall_replacing &= (uint8_t)~CONTENT_CAPTAIN;
  recall_replacing |= RECALL_SUSPEND;
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_SUSPEND);
  recall_replacing = 0;
  if (recall_stage_head(slot, from_container) != STAGE_OK) {
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset");
    return recall_refused(slot, "BAD PRESET");
  }
  ph[1] = wall.lap_ms();

  CORE::app_isr_enabled = false;
  CORE::app_loop_enabled = false;
  delay(1);

  recall_stage_files(slot, from_container, flags);
  ph[2] = wall.lap_ms();

  RestoreGlobalSettingsFromConfig(0);
  Scales::Validate();
  Chords::Validate();
  for (int i = 0; i < HS::TURING_MACHINE_COUNT; ++i)
    HS::user_turing_machines[i].Validate();
  ApplyAppData(capture);
  watchdog_feed();
  ph[3] = wall.lap_ms();

  const size_t idx = ResolveAppIndexByID(boot_recall ? global_settings.current_app_id
                                                     : slot_app_id);

  FreqMeasure.end();
  DigitalInputs::reInit();

  app_switcher.set_current_app(idx);

  AudioNoInterrupts();
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  AudioInterrupts();
  watchdog_feed();

  CORE::app_isr_enabled = true;
  CORE::app_loop_enabled = true;
  ::MENU_REDRAW = 1;
  ph[4] = wall.lap_ms();

  last_slot = slot;
  last_was_save = false;
  last_recall_err = nullptr;
  cur_slot_dirty_ms = StampMs();
  op_count++;
  busy = false;
  last_recall_ms = StampMs();
  {
    static char popup_text[20];
    snprintf(popup_text, sizeof(popup_text), "Recalled %u", slot + 1);
    HS::PokePopup(HS::MESSAGE_POPUP, popup_text);
  }
  serial_printf("PresetEngine: recall slot %d done (app %04x)\n", slot, slot_app_id);
  serial_printf("PresetEngine: recall took %lums wall (millis saw %lums): "
                "validate %lu suspend %lu files %lu apply %lu resume %lu\n",
                (unsigned long)wall.total_ms(),
                (unsigned long)(millis() - t_start),
                (unsigned long)ph[0], (unsigned long)ph[1],
                (unsigned long)ph[2], (unsigned long)ph[3],
                (unsigned long)ph[4]);
  return true;
}

static constexpr uint32_t kStageSyncDelayMs = 3000;
FLASHMEM static void stage_sync_pump() {
  if (!recall_stage.pending()) return;
  if (busy || !req_q.empty()) return;
  if (PresetBus::MasterTransferring()) return;
  if (!last_recall_ms || millis() - last_recall_ms < kStageSyncDelayMs) return;
  const PresetStage::Image *img = recall_stage.next_unsynced();
  if (!img) return;
  char name[PresetStage::kNameMax + 1];
  strncpy(name, img->name, PresetStage::kNameMax);
  name[PresetStage::kNameMax] = 0;
  FS &fs = strcmp(name, "BANK_255.DAT") == 0 ? quad_fs() : PhzConfig::myfs;
  WallClock wall;
  wall.start();
  const bool ok = bytes_to_file(fs, name, img->data, img->len);
  recall_stage.mark_synced(name);
  serial_printf("PresetEngine: synced %s to disk %s in %lu ms wall\n", name,
                ok ? "ok" : "FAILED", (unsigned long)wall.lap_ms());
}

FLASHMEM void Init() {
  req_q.clear();
  quad_recall_hint = -1;
  load_names();
  {
    uint64_t v = 0;
    if (PhzConfig::getValue(kQuadLinkBankKey, v) && v < kQuadBankCount) quad_link_bank = (uint8_t)v;
    v = 0;
    if (PhzConfig::getValue(kQuadLinkEnabledKey, v)) quad_link_enabled = v != 0;
  }

  const int on_card = CardSlotCount();
  if (on_card > 0)
    serial_printf("PresetEngine: %d preset%s on the card, import to load "
                  "%s into internal storage\n",
                  on_card, on_card == 1 ? "" : "s",
                  on_card == 1 ? "it" : "them");
}

static bool enqueue(uint8_t op, uint8_t slot) {
  if (slot >= kNumSlots) return false;
  if (req_q.push(op, slot)) return true;
  serial_printf("PresetEngine: queue full, %s %d refused\n",
                op == REQ_SAVE ? "save" : "recall", slot);
  return false;
}
bool RequestSave(uint8_t slot) { return enqueue(REQ_SAVE, slot); }
bool RequestRecall(uint8_t slot) { return enqueue(REQ_RECALL, slot); }
uint32_t RequestsDropped() { return req_q.dropped; }

FLASHMEM static void persist_cur_record() {
  cur_slot_dirty_ms = 0;
  const uint16_t app = global_settings.current_app_id;
  if (persisted_slot == last_slot && persisted_app == app) return;
  cur_rec_store(last_slot, app);
  int8_t s = -1;
  uint16_t a = 0;
  const bool ok = cur_rec_load(&s, &a) && s == last_slot && a == app;
  persisted_slot = ok ? last_slot : -1;
  persisted_app = ok ? app : 0;
  serial_printf("PresetEngine: slot %d app %04x persisted%s\n", last_slot, app,
                ok ? "" : " FAILED");
}

void NoteAppOnScreen() { cur_slot_dirty_ms = StampMs(); }

FLASHMEM bool BootAppChoice(uint16_t *app_id) {
  int8_t slot = -1;
  uint16_t app = 0;
  if (!cur_rec_load(&slot, &app) || !app) return false;
  *app_id = app;
  return true;
}

FLASHMEM void ForgetCurrent() {
  cur_rec_store(-1, 0);
  persisted_slot = -1;
  persisted_app = 0;
  cur_slot_dirty_ms = 0;
}

FLASHMEM void BootRecall() {
  if (!Buchla200eHardware()) return;
  int8_t slot = -1;
  uint16_t app = 0;
  if (!cur_rec_load(&slot, &app)) {
    uint64_t v = 0;
    PhzConfig::load_config();
    if (!PhzConfig::getValue(kCurSlotKey, v) || v >= kNumSlots) return;
    slot = (int8_t)v;
    cur_rec_store(slot, 0);
    serial_printf("PresetEngine: slot %d migrated from GLOBALS.CFG\n", slot);
  }
  persisted_slot = slot;
  persisted_app = app;
  if (app && app != global_settings.current_app_id) cur_slot_dirty_ms = StampMs();
  if (slot < 0 || !SlotUsed((uint8_t)slot)) return;
  serial_printf("PresetEngine: boot recall slot %d\n", slot);
  enqueue(REQ_BOOT_RECALL, slot);
}

FLASHMEM void Process() {
  if (!req_q.empty() && PresetBus::MasterTransferring()) {
    if (!deferred_since) {
      deferred_since = StampMs();
      serial_printf("PresetEngine: request deferred, bank transfer on the wire\n");
    }
    if (millis() - deferred_since < kDeferCapMs) return;
    serial_printf("PresetEngine: transfer still on the wire after %lu s; "
                  "running the request anyway\n",
                  (unsigned long)(kDeferCapMs / 1000));
  }
  if (!req_q.empty()) {
    if (deferred_since) {
      serial_printf("PresetEngine: deferred request runs, waited %lu ms\n",
                    (unsigned long)(millis() - deferred_since));
      deferred_since = 0;
    }
    const auto r = req_q.front();
    req_q.pop();
    switch (r.op) {
      case REQ_SAVE:
        SaveSlot(r.slot);
        break;
      case REQ_RECALL:
        RecallSlot(r.slot);
        boot_recall = false;
        break;
      case REQ_BOOT_RECALL:
        boot_recall = true;
        RecallSlot(r.slot);
        boot_recall = false;
        break;
    }
  }
  if (cur_slot_dirty_ms && millis() - cur_slot_dirty_ms > 3000 &&
      !PresetBus::MasterTransferring())
    persist_cur_record();
  stage_sync_pump();
  FlushSlotNames();
}

uint8_t RecallReplacing() { return recall_replacing; }

FLASHMEM int ConsumeQuadrantsRecallHint() {
  const int h = quad_recall_hint;
  quad_recall_hint = -1;
  return h;
}

uint32_t StampMs() { const uint32_t t = millis(); return t ? t : 1; }

int8_t LastSlot() { return last_slot; }
int8_t BusSlot() { return bus_slot; }
uint32_t OpCount() { return op_count; }
bool LastSaveOk() { return last_save_ok; }
const char *LastRecallError() { return last_recall_err; }

static char name_cache[kNumSlots][kNameLen + 1];

FLASHMEM static void load_names() {
  memset(name_cache, 0, sizeof(name_cache));
  File f = preset_fs().open("PBNAMES.BIN", FILE_READ);
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

static void pack_name(uint8_t slot, uint64_t out[2]) {
  out[0] = out[1] = 0;
  for (size_t i = 0; i < kNameLen; ++i)
    out[i / 8] |= (uint64_t)(uint8_t)name_cache[slot][i] << (8 * (i % 8));
}

static void unpack_name(const uint64_t in[2], char *out ) {
  for (size_t i = 0; i < kNameLen; ++i)
    out[i] = (char)(uint8_t)(in[i / 8] >> (8 * (i % 8)));
  out[kNameLen] = 0;
}

static bool names_dirty = false;

FLASHMEM static void names_flush() {
  File f = preset_fs().open("PBNAMES.BIN", FILE_WRITE_BEGIN);
  if (!f) return;
  for (int i = 0; i < kNumSlots; ++i)
    f.write((const uint8_t *)name_cache[i], kNameLen);
  f.close();
  names_dirty = false;
}

static void name_set_cached(uint8_t slot, const char *name) {
  memset(name_cache[slot], 0, sizeof(name_cache[slot]));
  strncpy(name_cache[slot], name, kNameLen);
  for (int i = (int)strlen(name_cache[slot]) - 1;
       i >= 0 && name_cache[slot][i] == ' '; --i)
    name_cache[slot][i] = 0;
  names_dirty = true;
}

FLASHMEM void SetSlotName(uint8_t slot, const char *name) {
  if (slot >= kNumSlots) return;
  name_set_cached(slot, name);
  names_flush();
}

FLASHMEM static void name_from_container(uint8_t slot) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  if (!container_open(slot, f, sec, n)) return;
  const SectionEntry *g = find_section(sec, n, 'G');
  const bool have = g && section_to_mem(f, *g, sec_buf, kSecBufBytes);
  f.close();
  if (!have) return;
  uint64_t nm[2];
  if (!PhzConfig::peek(sec_buf, g->length, kNameKey0, nm[0]) ||
      !PhzConfig::peek(sec_buf, g->length, kNameKey1, nm[1]))
    return;
  char name[kNameLen + 1];
  unpack_name(nm, name);
  if (strcmp(name, name_cache[slot]) != 0) name_set_cached(slot, name);
}

FLASHMEM void FlushSlotNames() {
  if (names_dirty) names_flush();
}

FLASHMEM bool SlotUsed(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  char name[12];
  container_name(name, slot);
  {
    File c = preset_fs().open(name, FILE_READ);
    const bool have = c && c.size() >= (int)kPayloadStart;
    if (c) c.close();
    if (have) return true;
  }
  slot_name(name, slot, 'G', "CFG");
  File f = preset_fs().open(name, FILE_READ);
  const bool used = f && f.size() > 16;
  if (f) f.close();
  return used;
}
bool LastWasSave() { return last_was_save; }
bool Busy() { return busy; }

static constexpr uint32_t kSnapFourcc = 0x50414E53UL;
struct SnapHeader {
  uint32_t fourcc;
  uint8_t  addr;
  uint8_t  pad[3];
  uint32_t length;
  uint32_t crc;
};
static_assert(sizeof(SnapHeader) == 16, "SnapHeader must stay 16 bytes");
static const char *const kSnapFile = "PBSNAP.BIN";

FLASHMEM bool SnapshotBank(uint8_t addr, const uint8_t *bank, uint32_t len,
                           uint32_t crc) {
  if (!bank || !len) return false;
  static const char *const kSnapTmp = "PB_SNP.TMP";
  preset_fs().remove(kSnapTmp);
  File f = preset_fs().open(kSnapTmp, FILE_WRITE_BEGIN);
  if (!f) return false;
  const SnapHeader h = { kSnapFourcc, addr, {0, 0, 0}, len, crc };
  bool ok = f.write((const uint8_t *)&h, sizeof(h)) == sizeof(h);
  uint32_t left = len;
  const uint8_t *p = bank;
  while (ok && left) {
    const uint32_t n = left < 512 ? left : 512;
    ok = f.write(p, n) == n;
    p += n; left -= n;
    watchdog_feed();
  }
  f.close();
  if (!ok) { preset_fs().remove(kSnapTmp); return false; }
  if (!preset_fs().rename(kSnapTmp, kSnapFile)) {
    preset_fs().remove(kSnapFile);
    if (!preset_fs().rename(kSnapTmp, kSnapFile)) return false;
  }
  serial_printf("PresetEngine: snapshot %lu bytes from %02X\n",
                (unsigned long)len, addr);
  return true;
}

FLASHMEM bool SnapshotInfo(uint8_t *addr_out, uint32_t *len_out) {
  File f = preset_fs().open(kSnapFile, FILE_READ);
  if (!f) return false;
  SnapHeader h;
  const bool ok = f.read((uint8_t *)&h, sizeof(h)) == (int)sizeof(h) &&
                  h.fourcc == kSnapFourcc && h.length &&
                  (uint64_t)h.length + sizeof(h) <= (uint64_t)f.size();
  f.close();
  if (!ok) return false;
  if (addr_out) *addr_out = h.addr;
  if (len_out) *len_out = h.length;
  return true;
}

FLASHMEM bool LoadSnapshot(uint8_t addr, uint8_t *dest, uint32_t cap,
                           uint32_t *len_out) {
  if (!dest) return false;
  File f = preset_fs().open(kSnapFile, FILE_READ);
  if (!f) return false;
  SnapHeader h;
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == (int)sizeof(h) &&
            h.fourcc == kSnapFourcc && h.length && h.length <= cap &&
            (uint64_t)h.length + sizeof(h) <= (uint64_t)f.size();
  if (ok && h.addr != addr) ok = false;
  if (ok) ok = (uint32_t)f.read(dest, h.length) == h.length;
  f.close();
  if (!ok) return false;

  if (Buchla200eCrc32(dest, h.length) != h.crc) {
    serial_printf("PresetEngine: snapshot CRC mismatch, refusing\n");
    return false;
  }
  if (len_out) *len_out = h.length;
  return true;
}

FLASHMEM void DiscardSnapshot() { preset_fs().remove(kSnapFile); }

FLASHMEM static bool export_container(uint8_t slot, FS &card, const char *name) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  if (!container_open(slot, f, sec, n)) return false;
  SectionEntry *g = nullptr;
  for (int i = 0; i < n; ++i)
    if (sec[i].kind == 'G') g = &sec[i];
  bool patch = g && section_to_mem(f, *g, sec_buf, kSecBufBytes);
  if (patch) {
    uint64_t nm[2];
    pack_name(slot, nm);
    patch = PhzConfig::poke(sec_buf, g->length, kNameKey0, nm[0]) &&
            PhzConfig::poke(sec_buf, g->length, kNameKey1, nm[1]);
    if (patch) g->checksum = sum16(sec_buf, g->length);
  }
  if (!patch && g && !section_to_mem(f, *g, sec_buf, kSecBufBytes)) {
    f.close();
    return false;
  }

  ContainerHeader h;
  if (!f.seek(0) || f.read((uint8_t *)&h, sizeof(h)) != sizeof(h)) {
    f.close();
    return false;
  }
  card.remove(name);
  File dst = card.open(name, FILE_WRITE_BEGIN);
  if (!dst) { f.close(); return false; }
  bool ok = cw_put(dst, (const uint8_t *)&h, sizeof(h));
  for (int i = 0; i < kMaxSections && ok; ++i) {
    const SectionEntry e = (i < n) ? sec[i] : SectionEntry{ 0, 0, 0, 0, 0 };
    ok = cw_put(dst, (const uint8_t *)&e, sizeof(e));
  }
  const uint32_t size = (uint32_t)f.size();
  uint32_t pos = kPayloadStart;
  ok = ok && f.seek(pos);
  uint8_t buf[512];
  while (ok && pos < size) {
    const uint32_t want = (size - pos) < sizeof(buf) ? (size - pos) : sizeof(buf);
    ok = f.read(buf, want) == (int)want;
    if (ok && g) {
      const uint32_t lo = pos > g->offset ? pos : g->offset;
      const uint32_t hi = (pos + want) < (g->offset + g->length)
                              ? (pos + want) : (g->offset + g->length);
      if (lo < hi) memcpy(buf + (lo - pos), sec_buf + (lo - g->offset), hi - lo);
    }
    ok = ok && dst.write(buf, want) == want;
    pos += want;
    watchdog_feed();
  }
  f.close();
  dst.close();
  if (!ok) card.remove(name);
  return ok;
}

FLASHMEM ExportResult ExportSlot(uint8_t slot) {
  if (slot >= kNumSlots) return EXPORT_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return EXPORT_NO_CARD;
  if (!SlotUsed(slot)) return EXPORT_EMPTY;

  char name[12];
  container_name(name, slot);

  {
    File c; SectionEntry sec[kMaxSections]; int n = 0;
    if (!container_open(slot, c, sec, n)) return EXPORT_LEGACY;
    c.close();
  }
  if (!container_verify(preset_fs(), name)) return EXPORT_BAD_FILE;

  if (!export_container(slot, *card, name)) return EXPORT_FAILED;

  if (!container_verify(*card, name)) { card->remove(name); return EXPORT_FAILED; }

  serial_printf("PresetEngine: exported slot %d to card\n", slot);
  return EXPORT_OK;
}

FLASHMEM ExportResult ImportSlot(uint8_t slot) {
  if (slot >= kNumSlots) return EXPORT_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return EXPORT_NO_CARD;

  char name[12];
  container_name(name, slot);
  {
    File s = card->open(name, FILE_READ);
    const bool have = s && s.size() >= (int)kPayloadStart;
    if (s) s.close();
    if (!have) return EXPORT_EMPTY;
  }

  static const char *const kImpTmp = "PB_IMP.TMP";
  preset_fs().remove(kImpTmp);
  if (!copy_file(*card, name, preset_fs(), kImpTmp)) {
    preset_fs().remove(kImpTmp);
    return EXPORT_FAILED;
  }
  if (!container_verify(preset_fs(), kImpTmp)) {
    preset_fs().remove(kImpTmp);
    return EXPORT_BAD_FILE;
  }

  if (!preset_fs().rename(kImpTmp, name)) {
    preset_fs().remove(name);
    if (!preset_fs().rename(kImpTmp, name)) return EXPORT_FAILED;
  }
  remove_legacy_slot(slot);
  name_from_container(slot);
  serial_printf("PresetEngine: imported slot %d from card\n", slot);
  return EXPORT_OK;
}

FLASHMEM int CardSlotCount() {
  FS *card = card_fs();
  if (!card) return -1;
  int n = 0;
  char name[12];
  for (int i = 0; i < kNumSlots; ++i) {
    container_name(name, (uint8_t)i);
    File f = card->open(name, FILE_READ);
    if (f && f.size() >= (int)kPayloadStart) ++n;
    if (f) f.close();
  }
  return n;
}

FLASHMEM RecoverResult RecoverLegacyFromCard(uint8_t slot) {
  if (slot >= kNumSlots) return RECOVER_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return RECOVER_NO_CARD;
  if (SlotUsed(slot)) return RECOVER_OCCUPIED;

  char gname[12], aname[12], bname[12], sname[12], cname[12];
  slot_name(gname, slot, 'G', "CFG");
  slot_name(aname, slot, 'A', "BIN");
  slot_name(bname, slot, 'B', "DAT");
  slot_name(sname, slot, 'S', "DAT");
  slot_name(cname, slot, 'C', "DAT");

  {
    File g = card->open(gname, FILE_READ);
    const bool g_have = (bool)g;
    if (g) g.close();
    File a = card->open(aname, FILE_READ);
    const bool a_have = (bool)a;
    if (a) a.close();
    if (!g_have || !a_have) return RECOVER_EMPTY;
  }

  if (!read_appdata_file(slot, *card)) return RECOVER_BAD_FILE;

  ContainerWriter w;
  cw_begin(w);
  const bool g_ok = cw_plan_file(w, 'G', *card, gname);
  const bool a_ok = cw_plan_appdata(w);
  if (!g_ok || !a_ok || !w.ok) return RECOVER_BAD_FILE;
  cw_plan_file(w, 'B', *card, bname);
  cw_plan_file(w, 'S', *card, sname);
  cw_plan_file(w, 'C', *card, cname);
  if (!w.ok) return RECOVER_BAD_FILE;

  static const char *const kRecTmp = "PB_REC.TMP";
  char final_name[12];
  container_name(final_name, slot);
  if (!cw_commit(w, kRecTmp, final_name)) return RECOVER_FAILED;

  {
    File v;
    SectionEntry vsec[kMaxSections];
    int vn = 0;
    if (!container_open(slot, v, vsec, vn)) {
      preset_fs().remove(final_name);
      return RECOVER_FAILED;
    }
    v.close();
  }
  name_from_container(slot);
  serial_printf("PresetEngine: recovered legacy slot %d from card\n", slot);
  return RECOVER_OK;
}

FLASHMEM int RecoverAllLegacyFromCard() {
  if (!card_fs()) return -1;
  int n = 0;
  for (uint8_t i = 0; i < kNumSlots; ++i) {
    if (RecoverLegacyFromCard(i) == RECOVER_OK) ++n;
    watchdog_feed();
  }
  FlushSlotNames();
  return n;
}

}
}

PresetStage &OC::RecallStage() { return OC::PresetEngine::recall_stage; }

#endif
