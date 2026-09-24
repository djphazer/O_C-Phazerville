/* Phazerville Config File
 *
 * Primarily stored on LittleFS in flash storage,
 * or SD card if available, or other any similar FS object.
 * Supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include <string.h>
#include <algorithm>
#include <vector>
#include "PhzConfig.h"
#include "OC_gpio.h"
#include "PresetStage.h"
#include "HSUtils.h"
#include "util/util_misc.h"
#include "usb_desc.h"

#ifdef MTP_INTERFACE
#include <MTP_Teensy.h>
#endif

#ifdef XENOFS_OWNS_GEOMETRY
extern "C" void eepromemu_flash_write(void *addr, const void *data, uint32_t len);
extern "C" void eepromemu_flash_erase_sector(void *addr);
extern "C" void eepromemu_flash_erase_64K_block(void *addr);
extern unsigned long _flashimagelen;
#endif

namespace PhzConfig {

XenoFS myfs;
File dataFile;

uint32_t XenoFS::rootLogBytes() {
  if (!mounted) return 0;
  lfs_dir_t dir;
  if (lfs_dir_open(&lfs, &dir, "/") < 0) return 0;
  const uint32_t used = dir.m.off;
  lfs_dir_close(&lfs, &dir);
  return used;
}

#ifdef XENOFS_OWNS_GEOMETRY

static constexpr uint32_t kFlashBytes   = 0x7C0000;
static constexpr uint32_t kXenoBlock    = 4096;
static constexpr uint32_t kLegacyBlock  = 65536;

uint32_t XenoFS::flash_base = 0;

int XenoFS::flash_read(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, void *buf, lfs_size_t size) {
  memcpy(buf, (const uint8_t *)(flash_base + block * c->block_size + off), size);
  return 0;
}
int XenoFS::flash_prog(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, const void *buf, lfs_size_t size) {
  eepromemu_flash_write((uint8_t *)(flash_base + block * c->block_size + off), buf, size);
  return 0;
}
int XenoFS::flash_erase(const struct lfs_config *c, lfs_block_t block) {
  uint8_t *p = (uint8_t *)(flash_base + block * c->block_size);
  if (c->block_size == kLegacyBlock) eepromemu_flash_erase_64K_block(p);
  else eepromemu_flash_erase_sector(p);
  return 0;
}

FLASHMEM void XenoFS::configure(uint32_t partition_bytes, uint32_t block_bytes) {
  memset(&lfs, 0, sizeof(lfs));
  memset(&config, 0, sizeof(config));
  config.context = (void *)flash_base;
  config.read = &flash_read;
  config.prog = &flash_prog;
  config.erase = &flash_erase;
  config.sync = &flash_sync;
  config.read_size = 128;
  config.prog_size = 128;
  config.block_size = block_bytes;
  config.block_count = partition_bytes / block_bytes;
  config.block_cycles = 800;
  config.cache_size = 128;
  config.lookahead_size = 128;
  config.name_max = LFS_NAME_MAX;
}

FLASHMEM static uint32_t superblock_block_size(uint32_t base, uint32_t partition) {
  static const uint32_t offsets[] = { 0, kXenoBlock };
  for (uint32_t off : offsets) {
    const uint8_t *p = (const uint8_t *)(base + off);
    for (uint32_t i = 4; i + 16 + 12 <= kXenoBlock; ++i) {
      if (memcmp(p + i, "littlefs", 8) != 0) continue;
      uint32_t bs, bc;
      memcpy(&bs, p + i + 8 + 4 + 4, 4);
      memcpy(&bc, p + i + 8 + 4 + 8, 4);
      if ((bs == kXenoBlock || bs == kLegacyBlock) && bc == partition / bs) return bs;
    }
  }
  return 0;
}

FLASHMEM bool XenoFS::begin(uint32_t size) {
  configured = false;
  mounted = false;
  flash_base = 0;
  size = (size + 0xFFFF) & 0xFFFF0000;
  if (size == 0) return false;
  const uint32_t program_size = (uint32_t)&_flashimagelen;
  if (program_size >= kFlashBytes || size > kFlashBytes - program_size) return false;
  flash_base = 0x60000000 + kFlashBytes - size;

  const bool fast_layout = OC::Buchla200eHardware();

  const uint32_t on_disk = superblock_block_size(flash_base, size);
  if (on_disk == kLegacyBlock) {
    configure(size, kLegacyBlock);
    configured = true;
    if (lfs_mount(&lfs, &config) >= 0) {
      if (!fast_layout) {
        mounted = true;
        return true;
      }
      serial_printf("LittleFS: 64 KB layout found, migrating to 4 KB sectors\n");
      return migrate_legacy();
    }
    serial_printf("LittleFS: 64 KB layout will not mount; reformatting\n");
  } else if (on_disk == kXenoBlock) {
    configure(size, kXenoBlock);
    configured = true;
    if (lfs_mount(&lfs, &config) >= 0) {
      mounted = true;
      return true;
    }
    serial_printf("LittleFS: 4 KB layout will not mount; reformatting\n");
  } else {
    serial_printf("LittleFS: no superblock; formatting\n");
  }

  configure(size, fast_layout ? kXenoBlock : kLegacyBlock);
  configured = true;
  if (lfs_format(&lfs, &config) < 0) return false;
  if (lfs_mount(&lfs, &config) < 0) return false;
  mounted = true;
  return true;
}

FLASHMEM bool XenoFS::migrate_legacy() {
  struct Held { char name[16]; uint8_t *data; uint32_t size; bool kept; };
  static constexpr int kMaxHeld = 96;
  Held held[kMaxHeld];
  int count = 0, over = 0;
  const uint32_t partition = config.block_count * config.block_size;

  lfs_dir_t dir;
  if (lfs_dir_open(&lfs, &dir, "/") < 0) {
    serial_printf("LittleFS: legacy root unreadable, staying on 64 KB layout\n");
    mounted = true;
    return true;
  }
  struct lfs_info info;
  while (lfs_dir_read(&lfs, &dir, &info) > 0) {
    if (info.type != LFS_TYPE_REG) continue;
    if (strlen(info.name) >= sizeof(held[0].name)) { ++over; continue; }
    if (count == kMaxHeld) { ++over; continue; }
    Held &h = held[count++];
    strcpy(h.name, info.name);
    h.data = nullptr;
    h.size = info.size;
    h.kept = false;
  }
  lfs_dir_close(&lfs, &dir);

  uint32_t bytes = 0;
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < count; ++i) {
      Held &h = held[i];
      const bool globals = strncmp(h.name, "GLOBALS", 7) == 0;
      if (globals != (pass == 0)) continue;
      if (h.size) {
        h.data = (uint8_t *)malloc(h.size);
        if (!h.data) {
          serial_printf("LittleFS: no RAM for %s (%lu bytes), dropped\n",
                        h.name, (unsigned long)h.size);
          continue;
        }
        lfs_file_t f;
        if (lfs_file_open(&lfs, &f, h.name, LFS_O_RDONLY) < 0) {
          free(h.data); h.data = nullptr;
          serial_printf("LittleFS: cannot read %s, dropped\n", h.name);
          continue;
        }
        const lfs_ssize_t got = lfs_file_read(&lfs, &f, h.data, h.size);
        lfs_file_close(&lfs, &f);
        if (got != (lfs_ssize_t)h.size) {
          free(h.data); h.data = nullptr;
          serial_printf("LittleFS: short read on %s, dropped\n", h.name);
          continue;
        }
      }
      h.kept = true;
      bytes += h.size;
    }
  }
  lfs_unmount(&lfs);

  configure(partition, kXenoBlock);
  if (lfs_format(&lfs, &config) < 0 || lfs_mount(&lfs, &config) < 0) {
    for (int i = 0; i < count; ++i) free(held[i].data);
    serial_printf("LittleFS: format failed, %d files lost\n", count);
    return false;
  }
  mounted = true;

  int written = 0;
  for (int i = 0; i < count; ++i) {
    Held &h = held[i];
    if (!h.kept) continue;
    lfs_file_t f;
    bool ok = lfs_file_open(&lfs, &f, h.name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) >= 0;
    if (ok) {
      ok = h.size == 0 || lfs_file_write(&lfs, &f, h.data, h.size) == (lfs_ssize_t)h.size;
      ok = (lfs_file_close(&lfs, &f) >= 0) && ok;
    }
    if (ok) ++written;
    else serial_printf("LittleFS: could not write %s back\n", h.name);
    free(h.data);
    h.data = nullptr;
  }
  serial_printf("LittleFS: migrated %d files, %lu bytes (%d dropped)\n",
                written, (unsigned long)bytes, count + over - written);
  return true;
}
#endif

ConfigMap cfg_store;
ConfigMap data_store;

static bool map_dirty = true;
static char map_source[16] = "";
static FS *map_source_fs = nullptr;

static void mark_clean(const char *filename, FS &fs) {
  strncpy(map_source, filename, sizeof(map_source) - 1);
  map_source[sizeof(map_source) - 1] = 0;
  map_source_fs = &fs;
  map_dirty = false;
}
static void mark_dirty() {
  map_dirty = true;
}
static bool is_clean_copy_of(const char *filename, FS &fs) {
  return !map_dirty && map_source_fs == &fs && map_source[0] &&
         strcmp(map_source, filename) == 0;
}

// Specify size to use of onboard Teensy Program Flash chip.
// the maximum flash available for LittleFS is 960 blocks of 1024 bytes
#if defined(ARDUINO_TEENSY41)
static constexpr uint32_t diskSize = 1024 * 1024 * 4;
#else
static constexpr uint32_t diskSize = 1024 * 512;
#endif
// custom file format header
static constexpr uint32_t HEADER_SIZE = 12;

FLASHMEM
void Init()
{
#ifdef MTP_INTERFACE
  MTP.begin();
#endif
  if (SDcard_Ready) {
#ifdef MTP_INTERFACE
    MTP.addFilesystem(SD, "SD_Card");
#endif
    SERIAL_PRINTLN("SD card available for preset storage");
    //listFiles(SD);
  }

  // This mounts or creates a LittleFS drive in Teensy PCB Flash.
  if (!myfs.begin(diskSize)) {
    SERIAL_PRINTLN("LittleFS unavailable!! Settings WILL NOT BE SAVED!");
    return;
  }
  SERIAL_PRINTLN("LittleFS initialized.");

  if (myfs.blockBytes() > 8192) myfs.setMetadataMax(8192);

#ifdef MTP_INTERFACE
  MTP.addFilesystem(myfs, "Internal_LFS");
#endif

  /*
  if (myfs.mediaPresent()) {
    listFiles(myfs);
    //load_config();
  }
  */
}

void clear_config() {
  cfg_store.clear();
  data_store.clear();
  mark_dirty();
}

void setValue(KEY key, VALUE value)
{
  auto it = cfg_store.find(key);
  if (it != cfg_store.end()) {
    if (it->second == value) return;
    it->second = value;
  } else {
    cfg_store.emplace(key, value);
  }
  mark_dirty();
}

bool getValue(KEY key, VALUE &value)
{
  auto thing = cfg_store.find(key);
  if (thing != cfg_store.end()) {
    value = thing->second;
    return true;
  }
  return false;
}

void deleteKey(KEY key) {
  if (cfg_store.erase(key)) mark_dirty();
}

void setData(KEY key, VALUE value) {
  auto it = data_store.find(key);
  if (it != data_store.end()) {
    if (it->second == value) return;
    it->second = value;
  } else {
    data_store.emplace(key, value);
  }
  mark_dirty();
}
bool getData(KEY key, VALUE &value) {
  auto thing = data_store.find(key);
  if (thing != data_store.end()) {
    value = thing->second;
    return true;
  }
  return false;
}
void deleteData(KEY key) {
  if (data_store.erase(key)) mark_dirty();
}

bool unsaved_changes() { return map_dirty; }

static void chunk_header(uint8_t *h, const char *sig, ConfigMap &store) {
  uint16_t record_count = 0;
  uint64_t checksum = 0;
  for (auto &i : store) {
    checksum ^= i.second;
    record_count++;
  }
  h[0] = sig[0]; h[1] = sig[1];
  h[2] = record_count & 0xFF; h[3] = record_count >> 8;
  for (int i = 0; i < 8; ++i) h[4 + i] = (uint8_t)(checksum >> (8 * i));
}

static std::vector<KEY> sorted_keys(const ConfigMap &store) {
  std::vector<KEY> keys;
  keys.reserve(store.size());
  for (auto &i : store) keys.push_back(i.first);
  std::sort(keys.begin(), keys.end());
  return keys;
}

size_t save_chunk(const size_t offset, const char* sig, ConfigMap &store) {
  uint8_t header_buf[HEADER_SIZE];
  chunk_header(header_buf, sig, store);
  if (dataFile.write(header_buf, HEADER_SIZE) != HEADER_SIZE) {
    SERIAL_PRINTLN("!! ERROR while writing file header !!\n");
    return 0;
  }

  size_t bytes_written = 0;
  for (const KEY key : sorted_keys(store))
  {
    const VALUE value = store.find(key)->second;
    int result = dataFile.write((const uint8_t*)&key, sizeof(key)) +
                dataFile.write((const uint8_t*)&value, sizeof(value));
    if (result != (sizeof(key) + sizeof(value))) {
      // something went wrong
      SERIAL_PRINTLN("!! ERROR while writing file !!\n   Result = %d\n", result);
      return 0;
    }
    bytes_written += result;
  }

  SERIAL_PRINTLN("Bytes written = %u\n", bytes_written);
  return offset + HEADER_SIZE + bytes_written;
}

static size_t chunk_to_mem(uint8_t *buf, size_t cap, const char *sig,
                           ConfigMap &store) {
  const size_t need = HEADER_SIZE + store.size() * (sizeof(KEY) + sizeof(VALUE));
  if (need > cap) return 0;
  chunk_header(buf, sig, store);
  uint8_t *p = buf + HEADER_SIZE;
  for (const KEY key : sorted_keys(store)) {
    const VALUE value = store.find(key)->second;
    memcpy(p, &key, sizeof(key));     p += sizeof(key);
    memcpy(p, &value, sizeof(value)); p += sizeof(value);
  }
  return need;
}

FLASHMEM size_t serialize(uint8_t *buf, size_t cap,
                          bool (*pred)(KEY), KEY (*remap)(KEY)) {
  ConfigMap *cfg = &cfg_store, *data = &data_store;
  ConfigMap cfg_out, data_out;
  if (pred || remap) {
    for (auto &kv : cfg_store) {
      if (pred && !pred(kv.first)) continue;
      cfg_out[remap ? remap(kv.first) : kv.first] = kv.second;
    }
    for (auto &kv : data_store) {
      if (pred && !pred(kv.first)) continue;
      data_out[remap ? remap(kv.first) : kv.first] = kv.second;
    }
    cfg = &cfg_out; data = &data_out;
  }
  const size_t a = chunk_to_mem(buf, cap, "PZ", *cfg);
  if (!a) return 0;
  const size_t b = chunk_to_mem(buf + a, cap - a, "PX", *data);
  if (!b) return 0;
  return a + b;
}

enum ChunkResult : uint8_t { CHUNK_ABSENT, CHUNK_OK, CHUNK_CORRUPT };
static ChunkResult chunk_from_mem(const uint8_t *buf, size_t len, size_t *pos,
                                  const char *sig, ConfigMap &store,
                                  bool (*pred)(KEY) = nullptr,
                                  KEY (*remap)(KEY) = nullptr) {
  if (*pos + HEADER_SIZE > len) return CHUNK_ABSENT;
  const uint8_t *h = buf + *pos;
  if (h[0] != sig[0] || h[1] != sig[1]) return CHUNK_ABSENT;
  const size_t count = (size_t)h[2] | (size_t)h[3] << 8;
  uint64_t expected = 0;
  for (int i = 0; i < 8; ++i) expected |= (uint64_t)h[4 + i] << (8 * i);
  const size_t body = count * (sizeof(KEY) + sizeof(VALUE));
  if (*pos + HEADER_SIZE + body > len) return CHUNK_CORRUPT;
  const uint8_t *p = h + HEADER_SIZE;
  uint64_t computed = 0;
  for (size_t i = 0; i < count; ++i) {
    KEY k; VALUE v;
    memcpy(&k, p, sizeof(k)); p += sizeof(k);
    memcpy(&v, p, sizeof(v)); p += sizeof(v);
    computed ^= v;
    if (!pred || pred(k)) store.insert_or_assign(remap ? remap(k) : k, v);
  }
  *pos += HEADER_SIZE + body;
  return computed == expected ? CHUNK_OK : CHUNK_CORRUPT;
}

FLASHMEM bool deserialize(const uint8_t *buf, size_t len) {
  cfg_store.clear();
  data_store.clear();
  mark_dirty();
  size_t pos = 0;
  bool any = false;
  while (pos < len) {
    const size_t before = pos;
    const ChunkResult c = chunk_from_mem(buf, len, &pos, "PZ", cfg_store);
    const ChunkResult d = chunk_from_mem(buf, len, &pos, "PX", data_store);
    if (c == CHUNK_CORRUPT || d == CHUNK_CORRUPT ||
        (c == CHUNK_ABSENT && d == CHUNK_ABSENT)) {
      SERIAL_PRINTLN("PhzConfig: bad image at %u\n", (unsigned)before);
      (void)before;
      cfg_store.clear();
      data_store.clear();
      return false;
    }
    any = true;
  }
  return any;
}

FLASHMEM bool deserialize_merge(const uint8_t *buf, size_t len,
                                bool (*pred)(KEY), KEY (*remap)(KEY)) {
  mark_dirty();
  size_t pos = 0;
  bool any = false;
  while (pos < len) {
    const size_t before = pos;
    const ChunkResult c = chunk_from_mem(buf, len, &pos, "PZ", cfg_store, pred, remap);
    const ChunkResult d = chunk_from_mem(buf, len, &pos, "PX", data_store, pred, remap);
    if (c == CHUNK_CORRUPT || d == CHUNK_CORRUPT ||
        (c == CHUNK_ABSENT && d == CHUNK_ABSENT)) {
      SERIAL_PRINTLN("PhzConfig: bad merge image at %u\n", (unsigned)before);
      (void)before;
      return false;
    }
    any = true;
  }
  return any;
}

static uint8_t *pz_find(uint8_t *buf, size_t len, KEY key) {
  if (len < HEADER_SIZE || buf[0] != 'P' || buf[1] != 'Z') return nullptr;
  const size_t count = (size_t)buf[2] | (size_t)buf[3] << 8;
  const size_t body = count * (sizeof(KEY) + sizeof(VALUE));
  if (HEADER_SIZE + body > len) return nullptr;
  uint8_t *p = buf + HEADER_SIZE;
  for (size_t i = 0; i < count; ++i) {
    KEY k;
    memcpy(&k, p, sizeof(k));
    if (k == key) return p + sizeof(k);
    p += sizeof(KEY) + sizeof(VALUE);
  }
  return nullptr;
}

FLASHMEM bool peek(const uint8_t *buf, size_t len, KEY key, VALUE &value) {
  const uint8_t *v = pz_find(const_cast<uint8_t *>(buf), len, key);
  if (!v) return false;
  memcpy(&value, v, sizeof(value));
  return true;
}

FLASHMEM bool poke(uint8_t *buf, size_t len, KEY key, VALUE value) {
  uint8_t *v = pz_find(buf, len, key);
  if (!v) return false;
  VALUE old;
  memcpy(&old, v, sizeof(old));
  memcpy(v, &value, sizeof(value));
  for (int i = 0; i < 8; ++i)
    buf[4 + i] ^= (uint8_t)((old ^ value) >> (8 * i));
  return true;
}

bool save_config(const char* filename, FS &fs)
{
    SERIAL_PRINTLN("\nSaving Config: %s\n", filename);

    if (is_clean_copy_of(filename, fs) && fs.exists(filename)) {
      SERIAL_PRINTLN("PhzConfig: %s unchanged, not rewritten\n", filename);
      return true;
    }

    const char* const TEMPFILE = "PEWPEW.TMP";
    bool success = true;

    // opens a file or creates a file if not present,
    // FILE_WRITE will append data
    // FILE_WRITE_BEGIN will overwrite from 0
    // O_TRUNC to truncate file size to what was written
    fs.remove(TEMPFILE);
    dataFile = fs.open(TEMPFILE, FILE_WRITE_BEGIN);
    if (dataFile) {
      size_t sz  = save_chunk( 0, "PZ",  cfg_store);
      if (sz) sz = save_chunk(sz, "PX", data_store);
      dataFile.close();
      if (!sz) {
        HS::PokePopup(HS::MESSAGE_POPUP, "Write ERROR !!");
        success = false;
        fs.remove(TEMPFILE);
      }
    } else {
      SERIAL_PRINTLN("PhzConfig: Error opening %s\n", filename);
      HS::PokePopup(HS::MESSAGE_POPUP, "File ERROR !!");
      success = false;
    }

    if (success) {
      success = fs.rename(TEMPFILE, filename);
      if (!success) {
        fs.remove(filename);
        success = fs.rename(TEMPFILE, filename);
      }
      if (!success)
        HS::PokePopup(HS::MESSAGE_POPUP, "TempFile ERR !!");
    }

    if (success) {
      mark_clean(filename, fs);
#ifdef ARDUINO_TEENSY41
      OC::RecallStage().mark_synced(filename);
#endif
    }
    return success;
}

FLASHMEM bool save_filtered(const char* filename, FS &fs,
                   bool (*pred)(KEY), KEY (*remap)(KEY))
{
    SERIAL_PRINTLN("\nSaving filtered config: %s\n", filename);

    ConfigMap cfg_out, data_out;
    for (auto &kv : cfg_store) {
      if (pred && !pred(kv.first)) continue;
      cfg_out[remap ? remap(kv.first) : kv.first] = kv.second;
    }
    for (auto &kv : data_store) {
      if (pred && !pred(kv.first)) continue;
      data_out[remap ? remap(kv.first) : kv.first] = kv.second;
    }

    const char* const TEMPFILE = "PEWPEW.TMP";
    bool success = true;

    fs.remove(TEMPFILE);
    dataFile = fs.open(TEMPFILE, FILE_WRITE_BEGIN);
    if (dataFile) {
      size_t sz  = save_chunk( 0, "PZ", cfg_out);
      if (sz) sz = save_chunk(sz, "PX", data_out);
      dataFile.close();
      if (!sz) success = false;
    } else {
      SERIAL_PRINTLN("PhzConfig: Error opening %s\n", filename);
      success = false;
    }

    if (success) {
      success = fs.rename(TEMPFILE, filename);
      if (!success) {
        fs.remove(filename);
        success = fs.rename(TEMPFILE, filename);
      }
    }

    return success;
}

bool load_chunk(const uint8_t *hdr, const char *sig, ConfigMap &store) {
  // quick signature check
  if (hdr[0] != sig[0] || hdr[1] != sig[1]) return false;

  size_t record_count = 0;
  size_t expected_record_count = uint16_t(hdr[2]) | uint16_t(hdr[3]) << 8;
  uint64_t expected_checksum =
          (uint64_t)hdr[4] |
          (uint64_t)hdr[5] << 8 |
          (uint64_t)hdr[6] << 16 |
          (uint64_t)hdr[7] << 24 |
          (uint64_t)hdr[8] << 32 |
          (uint64_t)hdr[9] << 40 |
          (uint64_t)hdr[10] << 48 |
          (uint64_t)hdr[11] << 56;
  uint64_t computed_checksum = 0;

  static_assert(sizeof(KEY) + sizeof(VALUE) == 10, "config data size mismatch");
  uint8_t buf[sizeof(KEY) + sizeof(VALUE)];
  size_t pos = 0;
  while (record_count < expected_record_count && dataFile.available()) {
    uint8_t n = dataFile.read();
    buf[pos++] = n;

    if (pos >= (sizeof(KEY) + sizeof(VALUE))) {
      store.insert_or_assign(
          (uint16_t)buf[0] |
          (uint16_t)buf[1] << 8,

          (uint64_t)buf[2] |
          (uint64_t)buf[3] << 8 |
          (uint64_t)buf[4] << 16 |
          (uint64_t)buf[5] << 24 |
          (uint64_t)buf[6] << 32 |
          (uint64_t)buf[7] << 40 |
          (uint64_t)buf[8] << 48 |
          (uint64_t)buf[9] << 56
          );

      computed_checksum ^=
          (uint64_t)buf[2] |
          (uint64_t)buf[3] << 8 |
          (uint64_t)buf[4] << 16 |
          (uint64_t)buf[5] << 24 |
          (uint64_t)buf[6] << 32 |
          (uint64_t)buf[7] << 40 |
          (uint64_t)buf[8] << 48 |
          (uint64_t)buf[9] << 56;

      ++record_count;
      pos = 0;
    }
  }
  if (record_count != expected_record_count || pos != 0) {
    SERIAL_PRINTLN("Loaded %u Records. (expected %u) -- truncated\n",
                   record_count, expected_record_count);
    HS::PokePopup(HS::MESSAGE_POPUP, "Corrupt File!!");
    return false;
  }
  SERIAL_PRINTLN("Loaded %u Records. (expected %u)\n", record_count, expected_record_count);
  SERIAL_PRINTLN("Checksum: %s (actual: %lx%lx)\n",
      (computed_checksum == expected_checksum)? "OK" : "ERROR",
      (uint32_t)computed_checksum, (uint32_t)(computed_checksum >> 32));
  SERIAL_PRINTLN("(File header checksum: %lx%lx)\n",
      (uint32_t)expected_checksum, (uint32_t)(expected_checksum >> 32));

  if (computed_checksum != expected_checksum)
    HS::PokePopup(HS::MESSAGE_POPUP, "Corrupt File!!");

  return (computed_checksum == expected_checksum);
}

bool load_config(const char* filename, FS &fs)
{
  cfg_store.clear();
  data_store.clear();
  mark_dirty();

#ifdef ARDUINO_TEENSY41
  if (const PresetStage::Image *img = OC::RecallStage().find(filename)) {
    const bool ok = deserialize(img->data, img->len);
    if (ok) mark_clean(filename, fs);
    SERIAL_PRINTLN("PhzConfig: %s served from the recall stage (%lu bytes)%s\n",
                   filename, (unsigned long)img->len, ok ? "" : " -- BAD IMAGE");
    return ok;
  }
#endif

  SERIAL_PRINTLN("\nLoading Config: %s\n", filename);
  dataFile = fs.open(filename);
  if (!dataFile) {
    SERIAL_PRINTLN("ERROR opening %s\n", filename);
    return false;
  }

  uint8_t buf[HEADER_SIZE];
  size_t pos = 0;

  do {
    // read in header
    pos = 0;
    while (dataFile.available() && pos < HEADER_SIZE) {
      uint8_t n = dataFile.read();
      buf[pos++] = n;
    }
    if (pos != HEADER_SIZE) {
      SERIAL_PRINTLN("PhzConfig: short header (%u bytes)\n", (unsigned)pos);
      dataFile.close();
      return false;
    }

    // check for every chunk signature
    const bool has_config = load_chunk(buf, "PZ", cfg_store);
    const bool has_data = load_chunk(buf, "PX", data_store);

    if (!has_config && !has_data) {
      SERIAL_PRINTLN("PhzConfig: Bad signature... %x %x", buf[0], buf[1]);
      dataFile.close();
      return false; // no bueno
    }

  } while (dataFile.available());

  dataFile.close();
  mark_clean(filename, fs);
  return true; // everything was fine!
}

FLASHMEM
bool backup_config()
{
  File src = myfs.open(CONFIG_FILENAME, FILE_READ);
  if (!src) return false;
  myfs.remove(BACKUP_FILENAME);
  File dst = myfs.open(BACKUP_FILENAME, FILE_WRITE_BEGIN);
  if (!dst) { src.close(); return false; }
  uint8_t buf[256];
  int n;
  bool ok = true;
  while ((n = src.read(buf, sizeof(buf))) > 0) {
    if (dst.write(buf, n) != (size_t)n) { ok = false; break; }
  }
  src.close();
  dst.close();
  if (!ok) myfs.remove(BACKUP_FILENAME);
  return ok;
}

FLASHMEM
void listFiles(FS &fs)
{
  Serial.print("\n     Space Used = ");
  Serial.println(fs.usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(fs.totalSize());

  printDirectory(fs);
}

FLASHMEM
void eraseFiles(FS &fs)
{
  //myfs.quickFormat();
  fs.format();
  Serial.println("\nFilesystem formatted - All files erased !");
}

FLASHMEM
void printDirectory(FS &fs) {
  Serial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
  Serial.println();
}

FLASHMEM
void printDirectory(File dir, int numSpaces) {
   while(true) {
     File entry = dir.openNextFile();
     if (! entry) {
       //Serial.println("** no more files **");
       break;
     }
     printSpaces(numSpaces);
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numSpaces+2);
     } else {
       // files have sizes, directories do not
       printSpaces(36 - numSpaces - strlen(entry.name()));
       Serial.print("  ");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

FLASHMEM
void printSpaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(" ");
  }
}

} // namespace PhzConfig
#endif
