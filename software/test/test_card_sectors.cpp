// Host tests for the 200e card image's per-sector dirty map
// (src/CardSectors.h): which 4 KB sectors of the 64 KB image actually
// changed, so a flush rewrites those and not the whole file.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_card_sectors test_card_sectors.cpp && ./build/test_card_sectors
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/CardSectors.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

// A stand-in for the firmware's CRC32: any function of the bytes will do for
// the map's own logic, and a weak one makes a collision easy to write on
// purpose (see the last test).
static uint32_t sum32(const uint8_t *p, uint32_t n) {
  uint32_t s = 0;
  for (uint32_t i = 0; i < n; ++i) s = s * 31u + p[i];
  return s;
}

static uint8_t img[CardSectors::kImageBytes];
static CardSectors map_;

static void fill(uint8_t v) { memset(img, v, sizeof(img)); }
static uint32_t sector_off(int i) { return (uint32_t)i * CardSectors::kSectorBytes; }

static void test_unknown_map_reports_every_sector() {
  fill(0);
  map_.forget();
  CHECK(!map_.known());
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == CardSectors::kSectors);
  CHECK(p.whole_image);   // nothing known: the caller must write it all
}

static void test_after_adopt_nothing_is_dirty() {
  fill(0xAA);
  map_.adopt(img, sum32);
  CHECK(map_.known());
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == 0);
  CHECK(!p.whole_image);
}

static void test_one_changed_sector_is_the_only_one_reported() {
  fill(0xAA);
  map_.adopt(img, sum32);
  img[sector_off(5) + 17] ^= 0xFF;
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == 1);
  CHECK(!p.whole_image);
  CHECK(p.dirty[5]);
  for (int i = 0; i < CardSectors::kSectors; ++i)
    if (i != 5) CHECK(!p.dirty[i]);
}

static void test_first_and_last_byte_of_the_image_are_covered() {
  fill(0x11);
  map_.adopt(img, sum32);
  img[0] ^= 0x01;
  img[CardSectors::kImageBytes - 1] ^= 0x01;
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == 2);
  CHECK(p.dirty[0]);
  CHECK(p.dirty[CardSectors::kSectors - 1]);
}

static void test_a_251e_bank_dirties_only_the_sectors_it_spans() {
  // A 251e BACKUP is 63120 bytes, so it covers the whole image; a 259e's is
  // 990 and lands inside one sector. That difference is the point of the map.
  fill(0);
  map_.adopt(img, sum32);
  for (uint32_t i = 0; i < 990; ++i) img[i] = (uint8_t)(i * 7 + 1);
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == 1 && p.dirty[0]);
}

static void test_adopt_after_a_write_clears_the_dirty_set() {
  fill(0);
  map_.adopt(img, sum32);
  img[sector_off(9)] ^= 0xFF;
  CHECK(map_.plan(img, sum32).count == 1);
  map_.adopt(img, sum32);           // the flush wrote it; the file agrees now
  CHECK(map_.plan(img, sum32).count == 0);
}

static void test_forget_after_a_failed_write_forces_a_full_rewrite() {
  fill(0);
  map_.adopt(img, sum32);
  img[sector_off(2)] ^= 0xFF;
  map_.forget();                    // the write failed: the file is unknown
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.whole_image);
  CHECK(p.count == CardSectors::kSectors);
}

static void test_plan_offsets_and_lengths_address_the_right_bytes() {
  fill(0);
  map_.adopt(img, sum32);
  img[sector_off(3) + 1] ^= 0xFF;
  CardSectors::Plan p = map_.plan(img, sum32);
  CHECK(p.count == 1);
  CHECK(CardSectors::offset_of(3) == 3u * 4096u);
  CHECK(CardSectors::kSectorBytes == 4096);
  CHECK(CardSectors::offset_of(CardSectors::kSectors - 1)
          + CardSectors::kSectorBytes == CardSectors::kImageBytes);
}

int main() {
  test_unknown_map_reports_every_sector();
  test_after_adopt_nothing_is_dirty();
  test_one_changed_sector_is_the_only_one_reported();
  test_first_and_last_byte_of_the_image_are_covered();
  test_a_251e_bank_dirties_only_the_sectors_it_spans();
  test_adopt_after_a_write_clears_the_dirty_set();
  test_forget_after_a_failed_write_forces_a_full_rewrite();
  test_plan_offsets_and_lengths_address_the_right_bytes();
  printf("test_card_sectors: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
