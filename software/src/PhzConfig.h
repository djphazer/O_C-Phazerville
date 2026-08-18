#pragma once

#include <stdint.h>

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

  extern LittleFS_Program myfs;

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

  void setData(KEY key, VALUE value);
  bool getData(KEY key, VALUE &value);
  void deleteData(KEY key);

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

  inline void setValue(KEY, VALUE) {}
  inline bool getValue(KEY, VALUE &) { return false; }
  inline void deleteKey(KEY) {}

  inline void setData(KEY, VALUE) {}
  inline bool getData(KEY, VALUE &) { return false; }
  inline void deleteData(KEY) {}
}
#endif
