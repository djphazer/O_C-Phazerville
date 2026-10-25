/* Phazerville Config File
 *
 * Primarily stored on LittleFS in flash storage,
 * or SD card if available, or other any similar FS object.
 * Supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include <string.h>
#include "PhzConfig.h"
#include "OC_gpio.h"
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

#ifdef XENOFS_OWNS_GEOMETRY

static constexpr uint32_t kFlashBytes   = 0x7C0000;
static constexpr uint32_t kXenoBlock    = 4096;
static constexpr uint32_t kLegacyBlock  = 65536;
static constexpr uint32_t kStockSize    = 1024 * 512;

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
  // other hardware: the stock LittleFS_Program, 64 KB blocks, unchanged
  if (!OC::Buchla200eHardware()) return LittleFS_Program::begin(size);

  configured = false;
  mounted = false;
  flash_base = 0;
  size = (size + 0xFFFF) & 0xFFFF0000;
  if (size == 0) return false;
  const uint32_t program_size = (uint32_t)&_flashimagelen;
  if (program_size >= kFlashBytes || size > kFlashBytes - program_size) return false;
  const uint32_t base = 0x60000000 + kFlashBytes - size;

  // The stock partition (512 KB, 64 KB blocks) sits at the top of ours.
  // Its superblock is erased after a migration, so finding one means the
  // stock firmware ran last: its files are the current ones.
  const uint32_t stock_base = 0x60000000 + kFlashBytes - kStockSize;
  if (size > kStockSize &&
      superblock_block_size(stock_base, kStockSize) == kLegacyBlock) {
    serial_printf("LittleFS: stock 512 KB partition found, migrating\n");
    if (migrate(stock_base, kStockSize, kLegacyBlock, base, size)) return mounted;
    serial_printf("LittleFS: stock partition will not mount; ignored\n");
  }

  flash_base = base;
  const uint32_t on_disk = superblock_block_size(base, size);
  if (on_disk == kXenoBlock) {
    configure(size, kXenoBlock);
    configured = true;
    if (lfs_mount(&lfs, &config) >= 0) {
      mounted = true;
      return true;
    }
    serial_printf("LittleFS: 4 KB layout will not mount; reformatting\n");
  } else if (on_disk == kLegacyBlock) {
    // a full-size partition still in 64 KB blocks
    serial_printf("LittleFS: 64 KB layout found, migrating to 4 KB sectors\n");
    if (migrate(base, size, kLegacyBlock, base, size)) return mounted;
    serial_printf("LittleFS: 64 KB layout will not mount; reformatting\n");
  } else {
    serial_printf("LittleFS: no superblock; formatting\n");
  }

  flash_base = base;
  configure(size, kXenoBlock);
  configured = true;
  if (lfs_format(&lfs, &config) < 0) return false;
  if (lfs_mount(&lfs, &config) < 0) return false;
  mounted = true;
  return true;
}

// Lift every file of the source partition into RAM (GLOBALS first), then
// format the destination in 4 KB blocks and write them back. Returns false
// only if the source does not mount (nothing has been touched then).
FLASHMEM bool XenoFS::migrate(uint32_t src_base, uint32_t src_size, uint32_t src_block,
                              uint32_t dst_base, uint32_t dst_size) {
  flash_base = src_base;
  configure(src_size, src_block);
  configured = true;
  if (lfs_mount(&lfs, &config) < 0) return false;

  struct Held { char name[16]; uint8_t *data; uint32_t size; bool kept; };
  static constexpr int kMaxHeld = 96;
  // 2.7 KB, kept off the stack
  static DMAMEM Held held[kMaxHeld];
  int count = 0, over = 0;

  lfs_dir_t dir;
  if (lfs_dir_open(&lfs, &dir, "/") >= 0) {
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
  } else {
    serial_printf("LittleFS: source root unreadable\n");
  }

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

  // a stock partition loses its superblock, so it is not migrated twice
  if (src_base != dst_base) {
    eepromemu_flash_erase_64K_block((void *)src_base);
    eepromemu_flash_erase_64K_block((void *)(src_base + kLegacyBlock));
  }

  flash_base = dst_base;
  configure(dst_size, kXenoBlock);
  if (lfs_format(&lfs, &config) < 0 || lfs_mount(&lfs, &config) < 0) {
    for (int i = 0; i < count; ++i) free(held[i].data);
    serial_printf("LittleFS: format failed, %d files lost\n", count);
    return true;  // mounted stays false
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

// Specify size to use of onboard Teensy Program Flash chip.
// the maximum flash available for LittleFS is 960 blocks of 1024 bytes
static constexpr uint32_t diskSize = 1024 * 512;
#ifdef XENOFS_OWNS_GEOMETRY
// Buchla hardware: 4 MB for the 200e preset slots
static uint32_t disk_size() { return OC::Buchla200eHardware() ? 1024 * 1024 * 4 : diskSize; }
#else
static uint32_t disk_size() { return diskSize; }
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
  if (!myfs.begin(disk_size())) {
    SERIAL_PRINTLN("LittleFS unavailable!! Settings WILL NOT BE SAVED!");
    return;
  }
  SERIAL_PRINTLN("LittleFS initialized.");

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
}

void setValue(KEY key, VALUE value)
{
  cfg_store[key] = value;
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
  cfg_store.erase(key);
}

void setData(KEY key, VALUE value) {
  data_store[key] = value;
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
  data_store.erase(key);
}

size_t save_chunk(const size_t offset, const char* sig, ConfigMap &store) {
  size_t record_count = 0;
  size_t bytes_written = 0;

  // 12-byte header:
  char header_buf[HEADER_SIZE] = {
    sig[0], sig[1], // signature
    0, 0, // record count
    0, 0, 0, 0, 0, 0, 0, 0, // checksum
  };

  dataFile.seek(offset);
  dataFile.write(header_buf, HEADER_SIZE);

  uint64_t checksum = 0;
  for (auto &i : store)
  {
    checksum ^= i.second;
    int result = dataFile.write((const uint8_t*)&i.first, sizeof(i.first)) +
                dataFile.write((const uint8_t*)&i.second, sizeof(i.second));
    if (result != (sizeof(i.first) + sizeof(i.second))) {
      // something went wrong
      SERIAL_PRINTLN("!! ERROR while writing file !!\n   Result = %d\n", result);
      return 0;
    }
    bytes_written += result;

    record_count += 1;
  }

  if (dataFile.seek(offset + 2)) {
    dataFile.write((const uint8_t*)&record_count, 2);
    dataFile.write((const uint8_t*)&checksum, 8);
  }

  SERIAL_PRINTLN("Records written = %u\n", record_count);
  SERIAL_PRINTLN("Bytes written = %u\n", bytes_written);
  SERIAL_PRINTLN("Checksum: %lx%lx\n",
      (uint32_t)checksum, (uint32_t)(checksum >> 32));

  return offset + HEADER_SIZE + bytes_written;
}
bool save_config(const char* filename, FS &fs)
{
    SERIAL_PRINTLN("\nSaving Config: %s\n", filename);

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
      if (sz) { // success!
        dataFile.close();
      } else {
        HS::PokePopup(HS::ERROR_POPUP, "Write ERROR !!");
      }
    } else {
      SERIAL_PRINTLN("PhzConfig: Error opening %s\n", filename);
      HS::PokePopup(HS::ERROR_POPUP, "File ERROR !!");
      success = false;
    }

    if (success) {
      fs.remove(filename);
      success = fs.rename(TEMPFILE, filename);
      if (!success)
        HS::PokePopup(HS::ERROR_POPUP, "TempFile ERR !!");
    }

    return success;
}

FLASHMEM bool save_filtered(const char* filename, FS &fs,
                   bool (*pred)(KEY), KEY (*remap)(KEY))
{
    SERIAL_PRINTLN("\nSaving filtered config: %s\n", filename);

    // Build filtered/remapped copies; the live map stays untouched.
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
      fs.remove(filename);
      success = fs.rename(TEMPFILE, filename);
    }

    return success;
}

bool load_chunk(uint8_t *buf, const char *sig, ConfigMap &store) {
  // quick signature check
  if (buf[0] != sig[0] || buf[1] != sig[1]) return false;

  size_t record_count = 0;
  size_t expected_record_count = uint16_t(buf[2]) | uint16_t(buf[3]) << 8;
  uint64_t expected_checksum =
          (uint64_t)buf[4] |
          (uint64_t)buf[5] << 8 |
          (uint64_t)buf[6] << 16 |
          (uint64_t)buf[7] << 24 |
          (uint64_t)buf[8] << 32 |
          (uint64_t)buf[9] << 40 |
          (uint64_t)buf[10] << 48 |
          (uint64_t)buf[11] << 56;
  uint64_t computed_checksum = 0;

  size_t pos = 0;
  while (dataFile.available()) {
    uint8_t n = dataFile.read();
    buf[pos++] = n;

    static_assert(sizeof(KEY) + sizeof(VALUE) == 10, "config data size mismatch");
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

      // Multiple chunks can be packed in series in one file
      if (record_count == expected_record_count) break;
    }
  }
  SERIAL_PRINTLN("Loaded %u Records. (expected %u)\n", record_count, expected_record_count);
  SERIAL_PRINTLN("Checksum: %s (actual: %lx%lx)\n",
      (computed_checksum == expected_checksum)? "OK" : "ERROR",
      (uint32_t)computed_checksum, (uint32_t)(computed_checksum >> 32));
  SERIAL_PRINTLN("(File header checksum: %lx%lx)\n",
      (uint32_t)expected_checksum, (uint32_t)(expected_checksum >> 32));

  if (computed_checksum != expected_checksum)
    HS::PokePopup(HS::ERROR_POPUP, "Corrupt File!!");

  return (computed_checksum == expected_checksum);
}

bool load_config(const char* filename, FS &fs)
{
  cfg_store.clear();
  data_store.clear();

  SERIAL_PRINTLN("\nLoading Config: %s\n", filename);
  dataFile = fs.open(filename);
  if (!dataFile) {
    SERIAL_PRINTLN("ERROR opening %s\n", filename);
    return false;
  }

  uint8_t buf[12];
  size_t pos = 0;

  do {
    // read in header
    pos = 0;
    while (dataFile.available() && pos < HEADER_SIZE) {
      uint8_t n = dataFile.read();
      buf[pos++] = n;
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
  if (!ok) myfs.remove(BACKUP_FILENAME);  // no partial backups
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
