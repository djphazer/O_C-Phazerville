// Host tests for the 200e whole-bank write guard
// (src/Buchla200eWriteGuard.cpp). No hardware, no bus.
//
// These guards are the only thing standing between an edit and overwriting 30
// real presets, so the emphasis is on the REFUSAL cases: every field that can
// block a write is tested in isolation, and the permit case is verified to
// need all of them simultaneously.
//
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_buchla200e_write_guard
//   test_buchla200e_write_guard.cpp ../src/Buchla200eWriteGuard.cpp &&
//   ./build/test_buchla200e_write_guard
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/Buchla200eWriteGuard.h"

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                            \
  do {                                                         \
    ++checks;                                                  \
    if (!(cond)) {                                             \
      ++failures;                                              \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                          \
  } while (0)

// A context that passes every guard. Each test below breaks exactly one
// field, which is what makes a failure point at a single cause.
static Buchla200eWriteContext Good() {
  Buchla200eWriteContext c;
  c.have_read = true;
  c.read_addr = 0x5C;
  c.read_type = 1;            // MODTYPE_251E
  c.target_addr = 0x5C;
  c.target_type = 1;
  c.bytes_transferred = 63120;  // 30 * 2104
  c.expected_bank_bytes = 63120;
  c.card_serving = true;
  c.image_valid = true;
  c.master_idle = true;
  c.changed_bytes = 5;
  c.image_matches_read = true;
  c.patches_in_range = true;
  return c;
}

static void test_permits_only_when_everything_holds() {
  printf("test_permits_only_when_everything_holds\n");
  CHECK(Buchla200eCheckWrite(Good()) == BUCHLA200E_WRITE_OK);
}

static void test_requires_a_read() {
  printf("test_requires_a_read\n");
  Buchla200eWriteContext c = Good();
  c.have_read = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_NO_READ);
}

static void test_read_must_be_of_this_module() {
  printf("test_read_must_be_of_this_module\n");
  // Read 0x5C then retarget 0x28: the resident image is a 251e bank and the
  // target is a 259e. This is the case that would push 63,120 bytes of the
  // wrong module's data into a module expecting 990.
  Buchla200eWriteContext c = Good();
  c.target_addr = 0x28;
  c.target_type = 2;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_WRONG_MODULE);

  // Same address, different type -- a clone can squat an address, so address
  // alone must not be treated as proof of identity.
  c = Good();
  c.target_type = 2;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_WRONG_MODULE);

  // Different address, same type: still another module's bank.
  c = Good();
  c.target_addr = 0x5D;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_WRONG_MODULE);
}

static void test_short_read_blocks_the_write() {
  printf("test_short_read_blocks_the_write\n");
  // The real incident: one record short, reported as success.
  Buchla200eWriteContext c = Good();
  c.expected_bank_bytes = 990;
  c.bytes_transferred = 957;   // 990 - 33
  c.read_type = 2; c.target_type = 2;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_SHORT_READ);

  // One byte short is still short.
  c = Good();
  c.bytes_transferred = 63119;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_SHORT_READ);

  // Zero expected bytes means the caller could not size the bank -- refuse
  // rather than treat "0 >= 0" as a pass.
  c = Good();
  c.expected_bank_bytes = 0;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_SHORT_READ);

  // A longer-than-expected transfer is fine: the bank is a prefix of it.
  c = Good();
  c.bytes_transferred = 65536;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_OK);
}

static void test_image_must_still_exist() {
  printf("test_image_must_still_exist\n");
  Buchla200eWriteContext c = Good();
  c.card_serving = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_NO_IMAGE);

  c = Good();
  c.image_valid = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_NO_IMAGE);
}

static void test_busy_blocks_and_outranks_everything() {
  printf("test_busy_blocks_and_outranks_everything\n");
  Buchla200eWriteContext c = Good();
  c.master_idle = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_BUSY);

  // Busy is checked first on purpose: starting a transfer while another is in
  // flight corrupts both, so it must win even when other things are also wrong.
  c = Good();
  c.master_idle = false;
  c.have_read = false;
  c.card_serving = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_BUSY);
}

static void test_no_changes_is_refused_not_permitted() {
  printf("test_no_changes_is_refused_not_permitted\n");
  Buchla200eWriteContext c = Good();
  c.changed_bytes = 0;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_NO_CHANGES);

  // A single changed byte is a real write.
  c = Good();
  c.changed_bytes = 1;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_OK);
}

static void test_diff_overflow_never_reads_as_no_changes() {
  printf("test_diff_overflow_never_reads_as_no_changes\n");
  // Buchla251eDiffSlot returns -1 when the patch list overflows. Treating a
  // negative as "nothing changed" would silently permit an unverified write;
  // treating it as falsy (0) would too. It must block.
  Buchla200eWriteContext c = Good();
  c.changed_bytes = -1;
  const Buchla200eWriteBlock b = Buchla200eCheckWrite(c);
  CHECK(b != BUCHLA200E_WRITE_OK);
  CHECK(b != BUCHLA200E_WRITE_NO_CHANGES);
}

static void test_ordering_reports_the_root_cause_first() {
  printf("test_ordering_reports_the_root_cause_first\n");
  // No read AND nothing changed: "read the module first" is the useful
  // message; "no changes" would be technically true and actively misleading.
  Buchla200eWriteContext c = Good();
  c.have_read = false;
  c.changed_bytes = 0;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_NO_READ);

  // Wrong module AND short read: wrong module is the more fundamental error.
  c = Good();
  c.target_addr = 0x28;
  c.bytes_transferred = 100;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_WRONG_MODULE);
}

// --- the shared-image checks ----------------------------------------------

static void test_stale_image_blocks_the_write() {
  printf("test_stale_image_blocks_the_write\n");
  // The card image is shared with the console 'w' patcher and the USB bridge.
  // Every other field here says "this is your bank, you read it, it is whole"
  // -- and all of them are counters and flags that cannot notice a third party
  // rewriting the bytes. Only this one looks at the bytes themselves.
  Buchla200eWriteContext c = Good();
  c.image_matches_read = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_IMAGE_CHANGED);
}

static void test_out_of_range_slot_blocks_the_write() {
  printf("test_out_of_range_slot_blocks_the_write\n");
  // A slot window that reaches past the bank does not fail on the wire: the
  // transfer is whole-bank either way, so the bad bytes land on a neighbouring
  // preset (or on whatever follows the buffer) and the module accepts them.
  Buchla200eWriteContext c = Good();
  c.patches_in_range = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_PATCH_RANGE);
}

static void test_stale_image_outranks_no_changes() {
  printf("test_stale_image_outranks_no_changes\n");
  // If someone else replaced the image, "no changes" is a statement about the
  // WRONG bytes. The user must be told to re-read, not that there is nothing
  // to do -- the latter reads as reassurance.
  Buchla200eWriteContext c = Good();
  c.image_matches_read = false;
  c.changed_bytes = 0;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_IMAGE_CHANGED);

  // ...and the same for an out-of-range window.
  c = Good();
  c.patches_in_range = false;
  c.changed_bytes = 0;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_PATCH_RANGE);
}

static void test_short_read_outranks_the_byte_checks() {
  printf("test_short_read_outranks_the_byte_checks\n");
  // A truncated transfer makes the hash comparison meaningless (it is taken
  // over bytes that were never received), so the size problem must be the one
  // reported.
  Buchla200eWriteContext c = Good();
  c.bytes_transferred = 60000;
  c.image_matches_read = false;
  c.patches_in_range = false;
  CHECK(Buchla200eCheckWrite(c) == BUCHLA200E_WRITE_SHORT_READ);
}

// --- fingerprinting --------------------------------------------------------

static void test_crc_detects_what_a_checksum_would_miss() {
  printf("test_crc_detects_what_a_checksum_would_miss\n");
  // Known answer first. The two sides of every comparison in the app use the
  // same function, so a wrong-but-consistent table would pass every other test
  // here while quietly being a weaker code than intended. This pins it to
  // standard CRC-32 (poly 0xEDB88320, init/final 0xFFFFFFFF).
  CHECK(Buchla200eCrc32((const uint8_t *)"123456789", 9) == 0xCBF43926UL);

  uint8_t a[64];
  for (int i = 0; i < 64; ++i) a[i] = (uint8_t)(i * 7 + 3);
  const uint32_t base = Buchla200eCrc32(a, 64);

  // Every single-byte change is seen.
  for (int i = 0; i < 64; ++i) {
    uint8_t b[64];
    memcpy(b, a, 64);
    b[i] ^= 0x01;
    CHECK(Buchla200eCrc32(b, 64) != base);
  }

  // A transposition -- the shape a mis-seated record transfer takes, and the
  // exact case a byte sum cannot see.
  uint8_t t[64];
  memcpy(t, a, 64);
  const uint8_t tmp = t[10];
  t[10] = t[40];
  t[40] = tmp;
  CHECK(Buchla200eCrc32(t, 64) != base);

  // Deterministic, and a null buffer is not silently "fine".
  CHECK(Buchla200eCrc32(a, 64) == base);
  CHECK(Buchla200eCrc32(nullptr, 64) == 0);
}

static void test_hole_hash_isolates_the_edited_slot() {
  printf("test_hole_hash_isolates_the_edited_slot\n");
  uint8_t bank[300];
  for (int i = 0; i < 300; ++i) bank[i] = (uint8_t)(i % 251);

  const uint32_t hole_off = 100, hole_len = 50;
  const Buchla200eBankHash before =
      Buchla200eHashBank(bank, 300, hole_off, hole_len);

  // Changing every byte INSIDE the hole must leave `outside` alone and move
  // `whole`. That pair is what lets the app prove a patch without keeping a
  // second copy of a 63,120-byte bank.
  for (uint32_t i = hole_off; i < hole_off + hole_len; ++i) bank[i] ^= 0xFF;
  const Buchla200eBankHash after =
      Buchla200eHashBank(bank, 300, hole_off, hole_len);
  CHECK(after.outside == before.outside);
  CHECK(after.whole != before.whole);

  // One byte OUTSIDE the hole -- the "we clobbered a neighbouring preset"
  // case -- must move `outside`. This is the check that catches a memcpy
  // that ran long.
  bank[hole_off + hole_len] ^= 0x01;
  const Buchla200eBankHash spill =
      Buchla200eHashBank(bank, 300, hole_off, hole_len);
  CHECK(spill.outside != after.outside);

  // A byte just before the hole, too: off-by-one at the low edge.
  uint8_t b2[300];
  for (int i = 0; i < 300; ++i) b2[i] = (uint8_t)(i % 251);
  const Buchla200eBankHash h0 = Buchla200eHashBank(b2, 300, hole_off, hole_len);
  b2[hole_off - 1] ^= 0x01;
  CHECK(Buchla200eHashBank(b2, 300, hole_off, hole_len).outside != h0.outside);
}

static void test_hole_is_skipped_not_zero_filled() {
  printf("test_hole_is_skipped_not_zero_filled\n");
  // If the hole were fed to the CRC as zeroes, a hole whose contents happen to
  // BE zero would hash the same as one that was skipped -- and then two
  // different banks would agree. Skipping keeps `outside` a function of length
  // as well as content.
  uint8_t a[64], b[64];
  memset(a, 0, 64);
  memset(b, 0, 64);
  for (int i = 0; i < 64; ++i) a[i] = b[i] = (uint8_t)i;
  memset(a + 16, 0, 8);   // zeroes inside what will be b's hole

  const Buchla200eBankHash ha = Buchla200eHashBank(a, 64, 0, 0);   // no hole
  const Buchla200eBankHash hb = Buchla200eHashBank(b, 64, 16, 8);  // holed
  CHECK(ha.outside != hb.outside);

  // No hole means outside == whole, so the two-hash API degrades sanely.
  CHECK(ha.outside == ha.whole);
  CHECK(Buchla200eHashBank(nullptr, 64, 0, 0).whole == 0);
}

static void test_every_block_has_distinct_text() {
  printf("test_every_block_has_distinct_text\n");
  const Buchla200eWriteBlock all[] = {
      BUCHLA200E_WRITE_OK,        BUCHLA200E_WRITE_NO_READ,
      BUCHLA200E_WRITE_WRONG_MODULE, BUCHLA200E_WRITE_SHORT_READ,
      BUCHLA200E_WRITE_NO_IMAGE,  BUCHLA200E_WRITE_BUSY,
      BUCHLA200E_WRITE_NO_CHANGES, BUCHLA200E_WRITE_IMAGE_CHANGED,
      BUCHLA200E_WRITE_PATCH_RANGE, BUCHLA200E_WRITE_BUILD_FAILED};
  const int n = (int)(sizeof(all) / sizeof(all[0]));
  for (int i = 0; i < n; ++i) {
    const char *a = Buchla200eWriteBlockText(all[i]);
    CHECK(a != nullptr);
    CHECK(a && a[0] != '\0');
    // Must fit a 21-character OLED line.
    CHECK(a && strlen(a) <= 21);
    for (int j = i + 1; j < n; ++j) {
      const char *b = Buchla200eWriteBlockText(all[j]);
      CHECK(a && b && strcmp(a, b) != 0);
    }
  }
}

int main() {
  test_permits_only_when_everything_holds();
  test_requires_a_read();
  test_read_must_be_of_this_module();
  test_short_read_blocks_the_write();
  test_image_must_still_exist();
  test_busy_blocks_and_outranks_everything();
  test_no_changes_is_refused_not_permitted();
  test_diff_overflow_never_reads_as_no_changes();
  test_ordering_reports_the_root_cause_first();
  test_stale_image_blocks_the_write();
  test_out_of_range_slot_blocks_the_write();
  test_stale_image_outranks_no_changes();
  test_short_read_outranks_the_byte_checks();
  test_crc_detects_what_a_checksum_would_miss();
  test_hole_hash_isolates_the_edited_slot();
  test_hole_is_skipped_not_zero_filled();
  test_every_block_has_distinct_text();

  printf("\ntest_buchla200e_write_guard: %d checks, %d failures\n", checks,
         failures);
  return failures == 0 ? 0 : 1;
}
