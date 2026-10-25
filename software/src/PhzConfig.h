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

#if defined(ARDUINO_TEENSY41) && defined(LFS_VERSION)
#define XENOFS_OWNS_GEOMETRY 1
#endif

  class XenoFS : public LittleFS_Program {
  public:
#ifdef XENOFS_OWNS_GEOMETRY
    bool begin(uint32_t size);
#endif
    uint32_t blockBytes() const { return config.block_size; }
#ifdef XENOFS_OWNS_GEOMETRY
  private:
    static uint32_t flash_base;
    static int flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buf, lfs_size_t size);
    static int flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buf, lfs_size_t size);
    static int flash_erase(const struct lfs_config *c, lfs_block_t block);
    static int flash_sync(const struct lfs_config *) { return 0; }
    void configure(uint32_t partition_bytes, uint32_t block_bytes);
    bool migrate(uint32_t src_base, uint32_t src_size, uint32_t src_block,
                 uint32_t dst_base, uint32_t dst_size);
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

  size_t serialize(uint8_t *buf, size_t cap,
                   bool (*pred)(KEY) = nullptr, KEY (*remap)(KEY) = nullptr);
  bool deserialize(const uint8_t *buf, size_t len);
  bool deserialize_merge(const uint8_t *buf, size_t len,
                         bool (*pred)(KEY) = nullptr, KEY (*remap)(KEY) = nullptr);
  bool peek(const uint8_t *buf, size_t len, KEY key, VALUE &value);
  bool poke(uint8_t *buf, size_t len, KEY key, VALUE value);

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
  inline bool deserialize_merge(const uint8_t *, size_t, bool (*)(KEY) = nullptr, KEY (*)(KEY) = nullptr) { return false; }
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
