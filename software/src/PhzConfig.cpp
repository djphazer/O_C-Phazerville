/* Phazerville Config File
 *
 * Primarily stored on LittleFS in flash storage,
 * or SD card if available, or other any similar FS object.
 * Supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include "PhzConfig.h"
#include "HSUtils.h"
#include "util/util_misc.h"
#include "usb_desc.h"

#ifdef MTP_INTERFACE
#include <MTP_Teensy.h>
#endif

namespace PhzConfig {

LittleFS_Program myfs;
File dataFile;
ConfigMap cfg_store;
ConfigMap data_store;

// Specify size to use of onboard Teensy Program Flash chip.
// the maximum flash available for LittleFS is 960 blocks of 1024 bytes
static constexpr uint32_t diskSize = 1024 * 512;
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
        HS::PokePopup(HS::MESSAGE_POPUP, "Write ERROR !!");
      }
    } else {
      SERIAL_PRINTLN("PhzConfig: Error opening %s\n", filename);
      HS::PokePopup(HS::MESSAGE_POPUP, "File ERROR !!");
      success = false;
    }

    if (success) {
      fs.remove(filename);
      success = fs.rename(TEMPFILE, filename);
      if (!success)
        HS::PokePopup(HS::MESSAGE_POPUP, "TempFile ERR !!");
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
    HS::PokePopup(HS::MESSAGE_POPUP, "Corrupt File!!");

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
