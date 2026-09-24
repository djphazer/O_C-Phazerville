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
    uint32_t rootLogBytes();
    uint32_t metadataMax() const { return config.metadata_max; }
    void setMetadataMax(uint32_t bytes) { config.metadata_max = bytes; }
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

  bool backup_config();
  const char * const BACKUP_FILENAME = "GLOBALS.BAK";

  void printDirectory(FS &fs = myfs);
  void printDirectory(File dir, int numSpaces);
  void printSpaces(int num);
  void eraseFiles(FS &fs = myfs);

}
#else
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
