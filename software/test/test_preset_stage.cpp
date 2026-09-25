// Host tests for the recall staging registry (src/PresetStage.h): the RAM
// images a preset recall registers instead of writing files, served to
// PhzConfig::load_config by name until a deferred sync has put the bytes
// on disk. Zero-write recall depends on this being exact.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_preset_stage test_preset_stage.cpp && ./build/test_preset_stage
#include <cstdio>
#include <cstring>

#include "../src/PresetStage.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

static uint8_t arena[256];
static PresetStage stage(arena, sizeof(arena));

static const uint8_t kBank[] = {1, 2, 3, 4, 5};
static const uint8_t kCaptain[] = {9, 8, 7};

static void test_empty_registry_serves_nothing() {
  stage.clear();
  CHECK(stage.find("BANK_255.DAT") == nullptr);
  CHECK(stage.pending() == 0);
  CHECK(stage.next_unsynced() == nullptr);
}

static void test_put_then_find_returns_a_copy_of_the_bytes() {
  stage.clear();
  CHECK(stage.put("BANK_255.DAT", kBank, sizeof(kBank)));
  const PresetStage::Image *img = stage.find("BANK_255.DAT");
  CHECK(img != nullptr);
  CHECK(img && img->len == sizeof(kBank));
  CHECK(img && memcmp(img->data, kBank, sizeof(kBank)) == 0);
  CHECK(img && img->data != kBank);   // copied into the arena, caller's buffer is free
  CHECK(stage.find("CAPTAIN.DAT") == nullptr);
}

static void test_find_keeps_serving_until_synced() {
  // Quadrants reloads its bank file on every app switch: the RAM image
  // must answer every load until the deferred sync has written the file.
  stage.clear();
  stage.put("BANK_255.DAT", kBank, sizeof(kBank));
  CHECK(stage.find("BANK_255.DAT") != nullptr);
  CHECK(stage.find("BANK_255.DAT") != nullptr);
  CHECK(stage.pending() == 1);
  stage.mark_synced("BANK_255.DAT");
  CHECK(stage.find("BANK_255.DAT") == nullptr);   // back to the file on disk
  CHECK(stage.pending() == 0);
}

static void test_put_replaces_same_name() {
  stage.clear();
  stage.put("CAPTAIN.DAT", kBank, sizeof(kBank));
  CHECK(stage.put("CAPTAIN.DAT", kCaptain, sizeof(kCaptain)));
  CHECK(stage.pending() == 1);
  const PresetStage::Image *img = stage.find("CAPTAIN.DAT");
  CHECK(img && img->len == sizeof(kCaptain));
  CHECK(img && memcmp(img->data, kCaptain, sizeof(kCaptain)) == 0);
}

static void test_next_unsynced_hands_the_flusher_entries_in_order() {
  stage.clear();
  stage.put("BANK_255.DAT", kBank, sizeof(kBank));
  stage.put("CAPTAIN.DAT", kCaptain, sizeof(kCaptain));
  const PresetStage::Image *a = stage.next_unsynced();
  CHECK(a && strcmp(a->name, "BANK_255.DAT") == 0);
  stage.mark_synced(a->name);
  const PresetStage::Image *b = stage.next_unsynced();
  CHECK(b && strcmp(b->name, "CAPTAIN.DAT") == 0);
  stage.mark_synced(b->name);
  CHECK(stage.next_unsynced() == nullptr);
}

static void test_registering_over_unsynced_entries_is_counted() {
  // A new recall while the previous one's files were never synced means
  // the disk still holds the older preset: the flusher fell behind. It is
  // not an error the caller can act on, so it is counted, not asserted.
  stage.clear();
  stage.put("BANK_255.DAT", kBank, sizeof(kBank));
  stage.begin_recall();
  CHECK(stage.superseded == 1);
  stage.put("BANK_255.DAT", kCaptain, sizeof(kCaptain));
  stage.mark_synced("BANK_255.DAT");
  stage.begin_recall();
  CHECK(stage.superseded == 1);
}

static void test_arena_full_refuses_and_counts() {
  stage.clear();
  static uint8_t big[200];
  CHECK(stage.put("BANK_255.DAT", big, sizeof(big)));
  CHECK(!stage.put("CAPTAIN.DAT", big, sizeof(big)));   // 400 > 256
  CHECK(stage.refused == 1);
  CHECK(stage.find("CAPTAIN.DAT") == nullptr);
}

static void test_arena_space_is_reclaimed_after_all_synced() {
  stage.clear();
  static uint8_t big[200];
  CHECK(stage.put("BANK_255.DAT", big, sizeof(big)));
  stage.mark_synced("BANK_255.DAT");
  CHECK(stage.put("CAPTAIN.DAT", big, sizeof(big)));   // room again
}

static void test_too_many_names_refused() {
  stage.clear();
  CHECK(stage.put("A.DAT", kBank, 1));
  CHECK(stage.put("B.DAT", kBank, 1));
  CHECK(stage.put("C.DAT", kBank, 1));
  CHECK(stage.put("D.DAT", kBank, 1));
  CHECK(!stage.put("E.DAT", kBank, 1));   // kSlots = 4
  CHECK(stage.refused == 1);
}

static void test_long_name_refused() {
  stage.clear();
  CHECK(!stage.put("THIS_NAME_IS_TOO_LONG.DAT", kBank, 1));
}

int main() {
  test_empty_registry_serves_nothing();
  test_put_then_find_returns_a_copy_of_the_bytes();
  test_find_keeps_serving_until_synced();
  test_put_replaces_same_name();
  test_next_unsynced_hands_the_flusher_entries_in_order();
  test_registering_over_unsynced_entries_is_counted();
  test_arena_full_refuses_and_counts();
  test_arena_space_is_reclaimed_after_all_synced();
  test_too_many_names_refused();
  test_long_name_refused();
  printf("test_preset_stage: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
