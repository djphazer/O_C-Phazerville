/* Phazerville Config File
 * stored on LittleFS in flash storage
 * supercedes previous EEPROM mechanism
 */
#ifdef __IMXRT1062__
#include <LittleFS.h>
#include "PhzConfig.h"

// NOTE: This option is only available on the Teensy 4.0, Teensy 4.1 and Teensy Micromod boards.
// With the additonal option for security on the T4 the maximum flash available for a
// program disk with LittleFS is 960 blocks of 1024 bytes
#define PROG_FLASH_SIZE 1024 * 512 // Specify size to use of onboard Teensy Program Flash chip
                                   // This creates a LittleFS drive in Teensy PCB FLash.

namespace PhzConfig {

const char * const EEPROM_FILENAME = "EEPROM.DAT";

LittleFS_Program myfs;
File dataFile;  // Specifes that dataFile is of File type

ConfigMap cfg_store;
int record_count = 0;
static constexpr uint32_t diskSize = PROG_FLASH_SIZE;

void setup()
{
  if (!myfs.begin(diskSize)) {
    Serial.println("LittleFS unavailable!! Settings WILL NOT BE SAVED!");
    return;
  }
  Serial.println("LittleFS initialized.");

  if (myfs.mediaPresent()) {
    listFiles();

    load_config();

    // convenient for testing and for longevity
    cfg_store[POWER_CYCLE_COUNT] += 1;
    save_config();
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

void save_config(const char* filename)
{
    Serial.println("\nSaving Config!!!");

    size_t bytes_written = 0;
    record_count = 0;

    // opens a file or creates a file if not present,
    // FILE_WRITE will append data
    // FILE_WRITE_BEGIN will overwrite from 0
    // O_TRUNC to truncate file size to what was written
    myfs.remove(filename);
    dataFile = myfs.open(filename, FILE_WRITE_BEGIN);
    if (dataFile) {
      for (auto &i : cfg_store)
      {
        bytes_written += dataFile.write((const uint8_t*)&i.first, sizeof(i.first));
        bytes_written += dataFile.write((const uint8_t*)&i.second, sizeof(i.second));

        // print to the serial port too:
        //Serial.println(dataString);

        record_count += 1;
      }

      Serial.printf("Records written = %d\n", record_count);
      Serial.printf("Bytes written = %u\n", bytes_written);
      dataFile.close();
    } else {
      Serial.printf("error opening %s\n", filename);
    }
}

bool load_config(const char* filename)
{
  Serial.println("\nLoading Config!!!");
  dataFile = myfs.open(filename);
  if (!dataFile) {
    Serial.printf("error opening %s\n", filename);
    return false;
  }

  uint8_t buf[12];
  size_t pos = 0;
  cfg_store.clear();

  while (dataFile.available()) {
    uint8_t n = dataFile.read();
    buf[pos++] = n;

    // debug print
    if (n < 16) Serial.print("0");
    Serial.print(n, HEX);

    static_assert(sizeof(KEY) + sizeof(VALUE) == 10, "config data size mismatch");
    if (pos >= 10) {
      cfg_store.insert_or_assign(
          (uint32_t)buf[0] |
          (uint32_t)buf[1] << 8,

          (uint64_t)buf[2] |
          (uint64_t)buf[3] << 8 |
          (uint64_t)buf[4] << 16 |
          (uint64_t)buf[5] << 24 |
          (uint64_t)buf[6] << 32 |
          (uint64_t)buf[7] << 40 |
          (uint64_t)buf[8] << 48 |
          (uint64_t)buf[9] << 56
          );
      pos = 0;
      Serial.println();
    }
  }

  dataFile.close();
  return true;
}

void listFiles()
{
  Serial.print("\n Space Used = ");
  Serial.println(myfs.usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(myfs.totalSize());

  printDirectory(myfs);
}

void eraseFiles()
{
  myfs.quickFormat();
  Serial.println("\nLittleFS quick-format - All files erased !");
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
