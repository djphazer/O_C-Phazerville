// Host tests for the preset-op queue (src/PresetOpQueue.h) that both the
// engine's request queue and the transport's broadcast queue are built on:
// the recall-merge rule, what never merges, refusal when full, wraparound.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_preset_op_queue test_preset_op_queue.cpp && ./build/test_preset_op_queue
#include <cstdio>

#include "../src/PresetOpQueue.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

// the engine's shape: SAVE 0, RECALL 1, BOOT_RECALL 2, eight deep
enum : uint8_t { SAVE = 0, RECALL = 1, BOOT = 2 };
using Q = PresetOpQueue<8, RECALL>;

static bool at(const Q &q, uint8_t depth, uint8_t op, uint8_t slot) {
  const auto &e = q.q[(uint8_t)(q.head + depth) % 8];
  return e.op == op && e.slot == slot;
}

static void test_fifo_saves() {
  Q q;
  CHECK(q.empty());
  CHECK(q.push(SAVE, 3));
  CHECK(q.push(SAVE, 3));   // a repeated save is two saves: never merged
  CHECK(q.push(SAVE, 7));
  CHECK(q.size() == 3);
  CHECK(at(q, 0, SAVE, 3) && at(q, 1, SAVE, 3) && at(q, 2, SAVE, 7));
  q.pop(); q.pop(); q.pop();
  CHECK(q.empty());
  q.pop();                  // pop on empty is a no-op, not an underflow
  CHECK(q.empty() && q.head == 3);
}

static void test_recall_replaces_trailing_recall() {
  Q q;
  CHECK(q.push(RECALL, 3));
  CHECK(q.push(RECALL, 5));
  CHECK(q.push(RECALL, 9));
  CHECK(q.size() == 1);
  CHECK(at(q, 0, RECALL, 9));   // only the last of a run is ever heard
}

static void test_nothing_merges_across_a_save() {
  Q q;
  CHECK(q.push(RECALL, 3));
  CHECK(q.push(SAVE, 3));
  CHECK(q.push(RECALL, 5));
  CHECK(q.size() == 3);         // recall 3, save 3, recall 5: all three run
  CHECK(at(q, 0, RECALL, 3) && at(q, 1, SAVE, 3) && at(q, 2, RECALL, 5));
  CHECK(q.push(RECALL, 6));     // ...and the new tail still merges
  CHECK(q.size() == 3 && at(q, 2, RECALL, 6));
}

static void test_boot_recall_is_not_a_recall() {
  // the boot recall carries its own flag (keep the live Captain), so a bus
  // recall behind it must not fold into it and inherit that
  Q q;
  CHECK(q.push(BOOT, 4));
  CHECK(q.push(RECALL, 5));
  CHECK(q.size() == 2 && at(q, 0, BOOT, 4) && at(q, 1, RECALL, 5));
  CHECK(q.push(BOOT, 6));       // nor does a boot recall merge into a recall
  CHECK(q.size() == 3 && at(q, 2, BOOT, 6));
}

static void test_full_refuses_the_newest() {
  Q q;
  for (uint8_t i = 0; i < 8; ++i) CHECK(q.push(SAVE, i));
  CHECK(q.size() == 8);
  CHECK(!q.push(SAVE, 20));     // refused and counted; nothing older is lost
  CHECK(q.dropped == 1);
  CHECK(!q.push(RECALL, 21));   // a recall behind a save tail is a real entry
  CHECK(q.dropped == 2 && q.size() == 8);
  CHECK(at(q, 0, SAVE, 0) && at(q, 7, SAVE, 7));
  // a full queue whose tail IS a recall still takes a recall: it merges,
  // which is what bounds the queue against a manager sweeping presets
  q.pop();
  CHECK(q.push(RECALL, 1));
  CHECK(q.size() == 8);
  CHECK(q.push(RECALL, 2));
  CHECK(q.size() == 8 && q.dropped == 2 && at(q, 7, RECALL, 2));
}

static void test_wraparound_keeps_order() {
  Q q;
  uint8_t next_in = 0, next_out = 0;
  for (int round = 0; round < 50; ++round) {
    for (int k = 0; k < 5; ++k) CHECK(q.push(SAVE, next_in++));
    for (int k = 0; k < 5; ++k) {
      CHECK(q.front().op == SAVE && q.front().slot == next_out++);
      q.pop();
    }
  }
  CHECK(q.empty());
  // and the merge finds the tail correctly across the wrap
  for (int k = 0; k < 6; ++k) CHECK(q.push(SAVE, (uint8_t)k));
  for (int k = 0; k < 6; ++k) q.pop();
  CHECK(q.head == (uint8_t)((50 * 5 + 6) % 8));
  CHECK(q.push(SAVE, 10));
  CHECK(q.push(RECALL, 11));
  CHECK(q.push(RECALL, 12));
  CHECK(q.size() == 2 && at(q, 0, SAVE, 10) && at(q, 1, RECALL, 12));
}

static void test_clear() {
  Q q;
  q.push(SAVE, 1); q.push(RECALL, 2);
  q.clear();
  CHECK(q.empty());
  CHECK(q.push(RECALL, 7) && q.size() == 1 && at(q, 0, RECALL, 7));
}

static void test_transport_shape() {
  // the bus queue: wire command bytes, four deep -- same rule
  PresetOpQueue<4, 0x01> b;
  CHECK(b.push(0x02, 1));       // SAVE
  CHECK(b.push(0x01, 2));       // RECALL
  CHECK(b.push(0x01, 3));       // merges
  CHECK(b.size() == 2);
  CHECK(b.front().op == 0x02 && b.front().slot == 1);
  b.pop();
  CHECK(b.front().op == 0x01 && b.front().slot == 3);
}

int main() {
  test_fifo_saves();
  test_recall_replaces_trailing_recall();
  test_nothing_merges_across_a_save();
  test_boot_recall_is_not_a_recall();
  test_full_refuses_the_newest();
  test_wraparound_keeps_order();
  test_clear();
  test_transport_shape();
  printf("%s: %d checks, %d failures\n", fails ? "FAIL" : "ok", checks, fails);
  return fails ? 1 : 0;
}
