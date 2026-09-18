#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __IMXRT1062__
#include <LittleFS.h>
#include <SD.h>
#include <unordered_map>

extern bool SDcard_Ready;

namespace PhzConfig {
  using KEY = uint16_t;
  using VALUE = uint64_t;
  using ConfigMap = std::unordered_map<KEY, VALUE>;

  const char * const CONFIG_FILENAME = "GLOBALS.CFG";

  // XenoFS takes over the flash geometry only where there is real littlefs
  // under it (LFS_VERSION comes from the core's lfs.h); a RAM-backed stand-in
  // for LittleFS_Program keeps its own begin().
#if defined(ARDUINO_TEENSY41) && defined(LFS_VERSION)
#define XENOFS_OWNS_GEOMETRY 1
#endif

  // LittleFS_Program plus the things the library keeps protected that this
  // module has to see or set.
  //
  // On Teensy 4.1 it also owns the flash GEOMETRY. The core mounts the
  // program-flash partition in 64 KB erase blocks (SECTOR_SIZE in the core's
  // LittleFS.cpp), and a block is both the allocation unit and the unit of
  // erase: every file, 588 bytes or 60 KB, takes a whole one, and writing
  // one costs a 64 KB block erase with interrupts OFF -- measured 250-270 ms
  // on the bench, during which audio, USB and the preset bus all stop. The
  // W25Q64JV underneath erases 4 KB sectors in ~45 ms typical, so begin()
  // here mounts the same partition in 4 KB blocks instead: the same file
  // write stalls for one sector at a time, and a 4 MB partition holds 1024
  // blocks instead of 64 (this module's 37 files occupied 2.4 MB of the old
  // layout for 220 KB of content).
  //
  // A partition written in the old geometry does not mount in the new one.
  // begin() recognises that case, lifts every file into RAM, formats, and
  // writes them back -- once, on the first boot after the change -- so
  // nothing stored under the core's layout is lost. See XenoFS::begin.
  //
  // rootLogBytes/metadataMax: every open() on littlefs walks the root
  // directory's metadata log from the start, CRC-checking each commit, so a
  // log that has grown fat on small commits (a CrashReport append, a rename)
  // taxes every file operation afterwards. The threshold (config.metadata_max)
  // is read by littlefs at commit time through the pointer it kept at mount,
  // so it can be set after begin().
  class XenoFS : public LittleFS_Program {
  public:
#ifdef XENOFS_OWNS_GEOMETRY
    bool begin(uint32_t size);        // 4 KB sectors; migrates a 64 KB-layout partition
#endif
    uint32_t blockBytes() const { return config.block_size; }  // allocation unit; 0 = unconfigured
    uint32_t rootLogBytes();          // bytes of the root log in use; 0 = unmounted
    uint32_t metadataMax() const { return config.metadata_max; }  // 0 = block_size
    void setMetadataMax(uint32_t bytes) { config.metadata_max = bytes; }
#ifdef XENOFS_OWNS_GEOMETRY
  private:
    static uint32_t flash_base;       // 0x60000000 + FLASH_SIZE - partition size
    static int flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buf, lfs_size_t size);
    static int flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buf, lfs_size_t size);
    static int flash_erase(const struct lfs_config *c, lfs_block_t block);
    static int flash_sync(const struct lfs_config *) { return 0; }
    void configure(uint32_t partition_bytes, uint32_t block_bytes);
    bool migrate_legacy();
#endif
  };

  extern XenoFS myfs;

  // Forward Decl
  void Init();
  void listFiles(FS &fs = myfs);
  bool load_config(const char* filename = CONFIG_FILENAME, FS &fs = myfs);
  bool save_config(const char* filename = CONFIG_FILENAME, FS &fs = myfs);
  void clear_config();

  void setValue(KEY key, VALUE value);
  bool getValue(KEY key, VALUE &value);
  void deleteKey(KEY key);

  // Save a filtered/remapped copy of the currently loaded map to a file,
  // leaving the live map untouched. Used by the preset engine to extract
  // one preset's keys out of a bank file. remap may be null (identity).
  bool save_filtered(const char* filename, FS &fs,
                     bool (*pred)(KEY), KEY (*remap)(KEY));

  // The same bytes save_config would write, into RAM (optionally filtered
  // and remapped like save_filtered), and back. The preset engine packs
  // config sections into its slot container straight from RAM so no scratch
  // file -- and no interrupts-off block erase -- stands between the map and
  // the container. serialize returns bytes used, 0 if cap is too small.
  size_t serialize(uint8_t *buf, size_t cap,
                   bool (*pred)(KEY) = nullptr, KEY (*remap)(KEY) = nullptr);
  bool deserialize(const uint8_t *buf, size_t len);
  // One config value out of a serialized image WITHOUT touching the map.
  // For readers that run while some app owns the map (a slot import peeking
  // at the container's manifest) and must leave it exactly as they found it.
  // Scans the "PZ" chunk only; false when the image is malformed or the key
  // is absent.
  bool peek(const uint8_t *buf, size_t len, KEY key, VALUE &value);
  // The write side of peek: replace one EXISTING value in a serialized image
  // in place, keeping the chunk's checksum right. False (image untouched)
  // when the key is not there -- this never adds a record, since that would
  // move every byte after it.
  bool poke(uint8_t *buf, size_t len, KEY key, VALUE value);

  // True when the map differs from the file it was last loaded from or saved
  // to. save_config skips the write when it is false and the target is that
  // same file.
  bool unsaved_changes();

  void setData(KEY key, VALUE value);
  bool getData(KEY key, VALUE &value);
  void deleteData(KEY key);

  // Copy GLOBALS.CFG -> GLOBALS.BAK (called once per boot after a good
  // load, so a corrupt primary can be recovered instead of blocking boot
  // at the ConfirmReset prompt). Returns false on any I/O failure.
  bool backup_config();
  const char * const BACKUP_FILENAME = "GLOBALS.BAK";

  void printDirectory(FS &fs = myfs);
  void printDirectory(File dir, int numSpaces);
  void printSpaces(int num);
  void eraseFiles(FS &fs = myfs);

}
#else
// Teensy 3.x: no LittleFS/SD. Inert stubs keep shared code compiling
// (HemisphereApplet GetData/SetData, Main.cpp setup); persistence on T3
// goes through the EEPROM PageStorage paths instead.
namespace PhzConfig {
  using KEY = uint16_t;
  using VALUE = uint64_t;

  inline void Init() {}
  inline bool load_config(const char* = nullptr) { return false; }
  inline bool save_config(const char* = nullptr) { return false; }
  inline void clear_config() {}
  inline bool backup_config() { return false; }

  inline size_t serialize(uint8_t *, size_t, bool (*)(KEY) = nullptr, KEY (*)(KEY) = nullptr) { return 0; }
  inline bool deserialize(const uint8_t *, size_t) { return false; }
  inline bool peek(const uint8_t *, size_t, KEY, VALUE &) { return false; }
  inline bool poke(uint8_t *, size_t, KEY, VALUE) { return false; }
  inline bool unsaved_changes() { return false; }

  inline void setValue(KEY, VALUE) {}
  inline bool getValue(KEY, VALUE &) { return false; }
  inline void deleteKey(KEY) {}

  inline void setData(KEY, VALUE) {}
  inline bool getData(KEY, VALUE &) { return false; }
  inline void deleteData(KEY) {}
}
#endif
