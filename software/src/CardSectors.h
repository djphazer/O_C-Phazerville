#ifndef CARDSECTORS_H_
#define CARDSECTORS_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Per-sector dirty map for the 64 KB 200e card image.
//
// The image is mirrored to PBCARD.BIN so a module that backed itself up into
// a WPM-less bus still has its bank after a power cycle. Rewriting the whole
// file costs 1088 ms with interrupts masked (bench 2026-09-02, 64 KB through
// XenoFS's 4 KB sectors), during which audio, USB, the display and the bus
// slave all stall. Most of that is wasted: a 259e's bank is 990 bytes and
// lands in one sector, and even a 251e's 63120-byte bank usually differs
// from the last one in only part of the image.
//
// So the flush keeps a CRC per 4 KB sector -- the filesystem's own erase
// unit, which is the granularity the cost is actually paid in -- and writes
// only the sectors whose CRC moved.
//
// BSP-free and host-tested (test/test_card_sectors.cpp). The CRC function is
// passed in so this header does not drag in the firmware's.
// ---------------------------------------------------------------------------
class CardSectors {
 public:
  static const uint32_t kImageBytes = 65536;
  static const uint32_t kSectorBytes = 4096;
  static const int kSectors = (int)(kImageBytes / kSectorBytes);   // 16

  typedef uint32_t (*Crc32Fn)(const uint8_t *data, uint32_t len);

  struct Plan {
    bool dirty[kSectors];
    int count;
    // True when nothing is known about the file (never loaded, or the last
    // write failed): the caller must write the whole image, not seek into a
    // file whose contents it cannot vouch for.
    bool whole_image;
  };

  static uint32_t offset_of(int sector) { return (uint32_t)sector * kSectorBytes; }

  CardSectors() { forget(); }

  bool known() const { return known_; }

  // The file's contents are unknown: every sector counts as dirty and the
  // caller writes the image whole.
  void forget() {
    known_ = false;
    for (int i = 0; i < kSectors; ++i) crc_[i] = 0;
  }

  // The file now holds exactly these bytes (it was just written, or just
  // read back at boot).
  void adopt(const uint8_t *image, Crc32Fn crc32) {
    for (int i = 0; i < kSectors; ++i)
      crc_[i] = crc32(image + offset_of(i), kSectorBytes);
    known_ = true;
  }

  // Which sectors of `image` differ from what the file is believed to hold.
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

#endif  // CARDSECTORS_H_
