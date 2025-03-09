/* Phazerville Config File
 *
 * Primarily stored on LittleFS in flash storage,
 * or SD card if available, or other any similar FS object.
 * Supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include "PhzConfig.h"
#include "HSUtils.h"

namespace PhzConfig {

LittleFS_Program myfs;
File dataFile;
ConfigMap cfg_store;
size_t record_count = 0;

// Specify size to use of onboard Teensy Program Flash chip.
// the maximum flash available for LittleFS is 960 blocks of 1024 bytes
static constexpr uint32_t diskSize = 1024 * 512;
// custom file format header
static constexpr uint32_t HEADER_SIZE = 12;

void setup()
{
  // This mounts or creates a LittleFS drive in Teensy PCB Flash.
  if (!myfs.begin(diskSize)) {
    Serial.println("LittleFS unavailable!! Settings WILL NOT BE SAVED!");
    return;
  }
  Serial.println("LittleFS initialized.");

  if (myfs.mediaPresent()) {
    listFiles(myfs);
    //load_config();
  }

  if (HS::wavplayer_available) {
    Serial.println("SD card available for preset storage");
    //listFiles(SD);
  }
}

void clear_config() {
  cfg_store.clear();
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

bool save_config(const char* filename, FS &fs)
{
    Serial.println("\nSaving Config!!!");
    Serial.printf("\nSaving Config: %s\n", filename);

    const char* const TEMPFILE = "PEWPEW.TMP";
    bool success = true;
    size_t bytes_written = 0;
    record_count = 0;

    // opens a file or creates a file if not present,
    // FILE_WRITE will append data
    // FILE_WRITE_BEGIN will overwrite from 0
    // O_TRUNC to truncate file size to what was written
    fs.remove(TEMPFILE);
    dataFile = fs.open(TEMPFILE, FILE_WRITE_BEGIN);
    if (dataFile) {
      // 12-byte header:
      const char header_buf[HEADER_SIZE] = {
        'P', 'Z', // signature
        0, 0, // record count
        0, 0, 0, 0, 0, 0, 0, 0, // checksum
      };
      dataFile.write(header_buf, HEADER_SIZE);

      uint64_t checksum = 0;
      for (auto &i : cfg_store)
      {
        checksum ^= i.second;
        int result = dataFile.write((const uint8_t*)&i.first, sizeof(i.first)) +
                    dataFile.write((const uint8_t*)&i.second, sizeof(i.second));
        if (result != (sizeof(i.first) + sizeof(i.second))) {
          // something went wrong
          Serial.printf("!! ERROR while writing file !!\n   Result = %d\n", result);
          HS::PokePopup(HS::MESSAGE_POPUP, HS::LFS_WRITE_ERROR);
          success = false;
          break;
        }
        bytes_written += result;

        record_count += 1;
      }

      if (success && dataFile.seek(2)) {
        dataFile.write((const uint8_t*)&record_count, 2);
        dataFile.write((const uint8_t*)&checksum, 8);
      }

      Serial.printf("Records written = %u\n", record_count);
      Serial.printf("Bytes written = %u\n", bytes_written);
      Serial.printf("Checksum: %lx%lx\n",
          (uint32_t)checksum, (uint32_t)(checksum >> 32));

      dataFile.close();
    } else {
      Serial.printf("error opening %s\n", filename);
      success = false;
    }

    if (success) {
      fs.remove(filename);
      fs.rename(TEMPFILE, filename);
    }

    return success;
}

bool load_config(const char* filename, FS &fs)
{
  cfg_store.clear();
  record_count = 0;

  Serial.printf("\nLoading Config: %s\n", filename);
  dataFile = fs.open(filename);
  if (!dataFile) {
    Serial.printf("ERROR opening %s\n", filename);
    return false;
  }

  uint8_t buf[12];
  size_t pos = 0;

  while (dataFile.available() && pos < HEADER_SIZE) {
    uint8_t n = dataFile.read();
    buf[pos++] = n;
  }

  // header signature
  if (buf[0] != 'P' || buf[1] != 'Z') {
    Serial.print("Bad PZ signature...");
    dataFile.close();
    return false;
  }

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

  pos = 0;
  while (dataFile.available()) {
    uint8_t n = dataFile.read();
    buf[pos++] = n;

    static_assert(sizeof(KEY) + sizeof(VALUE) == 10, "config data size mismatch");
    if (pos >= (sizeof(KEY) + sizeof(VALUE))) {
      cfg_store.insert_or_assign(
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

      // XXX: if we utilize the expected record count,
      // multiple chunks could be packed in series in one file... for whatever purpose.
      // For now, we'll just load everything regardless.
      //if (record_count == expected_record_count) break;
    }
  }
  Serial.printf("Loaded %u Records. (expected %u)\n", record_count, expected_record_count);
  Serial.printf("Checksum: %s (actual: %lx%lx)\n",
      (computed_checksum == expected_checksum)? "OK" : "ERROR",
      (uint32_t)computed_checksum, (uint32_t)(computed_checksum >> 32));
  Serial.printf("(File header checksum: %lx%lx)\n",
      (uint32_t)expected_checksum, (uint32_t)(expected_checksum >> 32));

  dataFile.close();
  return computed_checksum == expected_checksum;
}

void listFiles(FS &fs)
{
  Serial.print("\n     Space Used = ");
  Serial.println(fs.usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(fs.totalSize());

  printDirectory(fs);
}

void eraseFiles(FS &fs)
{
  //myfs.quickFormat();
  fs.format();
  Serial.println("\nFilesystem formatted - All files erased !");
}

void printDirectory(FS &fs) {
  Serial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
  Serial.println();
}

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

void printSpaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(" ");
  }
}

} // namespace PhzConfig
#endif
