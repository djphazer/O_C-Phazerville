// Host tests for the 200e storage-card slave emulator (src/PresetBusCard.cpp):
// 24xx/FRAM wire protocol -- pointer writes, sequential/wrapping reads and
// writes, pointer persistence across transactions, the WPM empty-probe
// no-op, lost-STOP recovery, and detached-card behaviour.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_buscard test_buscard.cpp ../src/PresetBusCard.cpp && ./build/test_buscard
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/PresetBusCard.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

static uint8_t img[BUSCARD_SIZE];

// ---- master-side helpers (what the wire would carry) ------------------------

static void m_write(const uint8_t *bytes, uint32_t n) {  // full write txn
  BusCardStart(0);
  for (uint32_t i = 0; i < n; ++i) BusCardRxByte(bytes[i]);
  BusCardStop();
}

static void m_write_at(uint16_t addr, const uint8_t *d, uint32_t n) {
  BusCardStart(0);
  BusCardRxByte(addr >> 8);
  BusCardRxByte(addr & 0xFF);
  for (uint32_t i = 0; i < n; ++i) BusCardRxByte(d[i]);
  BusCardStop();
}

static void m_read(uint8_t *out, uint32_t n) {  // current-address read txn
  BusCardStart(1);
  for (uint32_t i = 0; i < n; ++i) out[i] = BusCardTxByte();
  BusCardStop();
}

static void m_read_at(uint16_t addr, uint8_t *out, uint32_t n) {
  // pointer write, repeated-start, read -- the standard random-access read
  BusCardStart(0);
  BusCardRxByte(addr >> 8);
  BusCardRxByte(addr & 0xFF);
  BusCardStart(1);  // repeated start closes the write txn
  for (uint32_t i = 0; i < n; ++i) out[i] = BusCardTxByte();
  BusCardStop();
}

// ---- tests -------------------------------------------------------------------

static void test_init_gating() {
  CHECK(BusCardInit(img, 100) == -1);       // not a power of two
  CHECK(!BusCardAttached());
  CHECK(BusCardInit(img, 2) == -1);         // too small
  CHECK(BusCardInit(NULL, 0) == 0);         // detach ok
  CHECK(!BusCardAttached());
  CHECK(BusCardInit(img, BUSCARD_SIZE) == 0);
  CHECK(BusCardAttached());
  CHECK(!BusCardDirty());
  CHECK(BusCardPointer() == 0);
}

static void test_write_read_roundtrip() {
  BusCardInit(img, BUSCARD_SIZE);
  memset(img, 0xEE, sizeof(img));

  const uint8_t d[4] = { 0x11, 0x22, 0x33, 0x44 };
  m_write_at(0x1234, d, 4);
  CHECK(BusCardDirty());
  CHECK(img[0x1234] == 0x11 && img[0x1237] == 0x44);
  CHECK(img[0x1233] == 0xEE && img[0x1238] == 0xEE);  // neighbours untouched
  CHECK(BusCardPointer() == 0x1238);

  uint8_t r[4] = {};
  m_read_at(0x1234, r, 4);
  CHECK(memcmp(r, d, 4) == 0);
  CHECK(BusCardGetStats()->bytes_written == 4);
  CHECK(BusCardGetStats()->bytes_read == 4);
}

static void test_pointer_persistence() {
  BusCardInit(img, BUSCARD_SIZE);
  for (uint32_t i = 0; i < 8; ++i) img[0x0100 + i] = (uint8_t)i;

  uint8_t r[4];
  m_read_at(0x0100, r, 4);          // leaves pointer at 0x0104
  CHECK(BusCardPointer() == 0x0104);
  m_read(r, 4);                     // current-address read continues
  CHECK(r[0] == 4 && r[3] == 7);
}

static void test_wrap() {
  BusCardInit(img, BUSCARD_SIZE);
  const uint8_t d[3] = { 0xA1, 0xA2, 0xA3 };
  m_write_at(BUSCARD_SIZE - 2, d, 3);   // crosses the end
  CHECK(img[BUSCARD_SIZE - 2] == 0xA1);
  CHECK(img[BUSCARD_SIZE - 1] == 0xA2);
  CHECK(img[0] == 0xA3);                // linear wrap, no page boundary
  CHECK(BusCardPointer() == 1);

  uint8_t r[3];
  m_read_at(BUSCARD_SIZE - 2, r, 3);
  CHECK(r[0] == 0xA1 && r[1] == 0xA2 && r[2] == 0xA3);
}

static void test_probe_is_side_effect_free() {
  BusCardInit(img, BUSCARD_SIZE);
  memset(img, 0x5A, sizeof(img));
  BusCardClearDirty();
  const uint32_t p0 = BusCardPointer();

  m_write(NULL, 0);                 // the WPM presence probe: empty write
  CHECK(!BusCardDirty());
  CHECK(BusCardPointer() == p0);    // pointer untouched by a 0-byte write
  CHECK(BusCardGetStats()->bytes_written == 0);
  for (uint32_t i = 0; i < BUSCARD_SIZE; ++i)
    if (img[i] != 0x5A) { CHECK(!"image modified by empty probe"); break; }
}

static void test_short_writes() {
  BusCardInit(img, BUSCARD_SIZE);
  memset(img, 0, sizeof(img));

  // 1-byte write = hi-only pointer positioning (lo forced 0), no data
  const uint8_t hi = 0x12;
  m_write(&hi, 1);
  CHECK(BusCardPointer() == 0x1200);
  CHECK(!BusCardDirty());

  // 2-byte write = full pointer positioning, no data
  const uint8_t p[2] = { 0x00, 0x40 };
  m_write(p, 2);
  CHECK(BusCardPointer() == 0x0040);
  CHECK(!BusCardDirty());
}

static void test_lost_stop_recovery() {
  BusCardInit(img, BUSCARD_SIZE);
  memset(img, 0, sizeof(img));

  // transport drops a STOP: a new START must close the stale transaction
  BusCardStart(0);
  BusCardRxByte(0x30);              // half a pointer, then the STOP is lost
  BusCardStart(0);                  // next transaction begins
  BusCardRxByte(0x00);
  BusCardRxByte(0x10);
  BusCardRxByte(0x77);
  BusCardStop();
  CHECK(img[0x0010] == 0x77);       // new pointer parsed from scratch
  CHECK(img[0x3000] == 0 && img[0x3010] == 0);
}

static void test_tx_prefetch_rewind() {
  // The transport's TDF feeder always has one unsent byte in the TX
  // register when the master NAKs; BusCardTxRewind() on read-leg close
  // must land the pointer exactly where a real 24xx counter would.
  BusCardInit(img, BUSCARD_SIZE);
  for (uint32_t i = 0; i < 12; ++i) img[0x0200 + i] = (uint8_t)(0xC0 + i);

  // master chunk-reads 4 bytes; transport fed 5 (one prefetched)
  BusCardStart(0);
  BusCardRxByte(0x02);
  BusCardRxByte(0x00);
  BusCardStart(1);
  uint8_t r[5];
  for (int i = 0; i < 5; ++i) r[i] = BusCardTxByte();  // 5th = prefetch
  BusCardTxRewind();   // transport: NAK'd byte never left the wire
  BusCardStop();
  CHECK(r[3] == 0xC3);
  CHECK(BusCardPointer() == 0x0204);  // == start + 4, 24xx-compatible

  // next chunk via current-address read continues seamlessly: master takes
  // 4 more bytes, transport again fed one extra before the NAK
  BusCardStart(1);
  for (int i = 0; i < 5; ++i) r[i % 5] = BusCardTxByte();
  BusCardTxRewind();
  BusCardStop();
  CHECK(BusCardPointer() == 0x0208);

  BusCardStart(1);
  CHECK(BusCardTxByte() == 0xC8);  // exactly where a 24xx would be
  BusCardStop();
}

static void test_detached() {
  BusCardInit(NULL, 0);
  BusCardStart(1);
  CHECK(BusCardTxByte() == 0xFF);   // floating-bus reads
  BusCardStop();
  const uint8_t d[3] = { 0x00, 0x00, 0x99 };
  m_write(d, 3);                    // data write with no card: dropped
  CHECK(!BusCardDirty());
  CHECK(BusCardGetStats()->bytes_written == 0);
  // transactions still counted while detached
  CHECK(BusCardGetStats()->txns_write == 1);
  CHECK(BusCardGetStats()->txns_read == 1);
}

int main() {
  test_init_gating();
  test_write_read_roundtrip();
  test_pointer_persistence();
  test_wrap();
  test_probe_is_side_effect_free();
  test_short_writes();
  test_lost_stop_recovery();
  test_tx_prefetch_rewind();
  test_detached();
  printf("test_buscard: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
