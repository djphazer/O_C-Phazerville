#ifndef CARDSECTORS_H_
#define CARDSECTORS_H_

#include <stdint.h>

class CardSectors {
 public:
  static const uint32_t kImageBytes = 65536;
  static const uint32_t kSectorBytes = 4096;
  static const int kSectors = (int)(kImageBytes / kSectorBytes);

  typedef uint32_t (*Crc32Fn)(const uint8_t *data, uint32_t len);

  struct Plan {
    bool dirty[kSectors];
    int count;
    bool whole_image;
  };

  static uint32_t offset_of(int sector) { return (uint32_t)sector * kSectorBytes; }

  CardSectors() { forget(); }

  bool known() const { return known_; }

  void forget() {
    known_ = false;
    for (int i = 0; i < kSectors; ++i) crc_[i] = 0;
  }

  void adopt(const uint8_t *image, Crc32Fn crc32) {
    for (int i = 0; i < kSectors; ++i)
      crc_[i] = crc32(image + offset_of(i), kSectorBytes);
    known_ = true;
  }

  Plan plan(const uint8_t *image, Crc32Fn crc32) const {
    Plan p;
    p.count = 0;
    p.whole_image = !known_;
    for (int i = 0; i < kSectors; ++i) {
      const bool d = !known_ || crc32(image + offset_of(i), kSectorBytes) != crc_[i];
      p.dirty[i] = d;
      if (d) p.count++;
    }
    return p;
  }

 private:
  uint32_t crc_[kSectors];
  bool known_;
};

#endif
