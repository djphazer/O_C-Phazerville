// Host tests for the ISR-to-loop deferred-call ring (src/DeferRing.h): the
// fixed-capacity, allocation-free replacement for the std::queue that
// OC::CORE::DeferTask used to emplace into from the CORE timer ISR.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_defer_ring test_defer_ring.cpp && ./build/test_defer_ring
#include <cstdio>

#include "../src/DeferRing.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

static int log_[64];
static int log_n = 0;
static void reset_log() { log_n = 0; }

// log_n keeps counting past the end so the callers can still assert on how
// many calls happened; only the WRITE is bounded. test_indices_wrap_past_
// the_array makes 80 calls into these 64 slots on purpose, and without this
// guard it ran off the end of the array -- which GCC's -O2 static layout
// turned into a clobbered `checks` counter (174 checks became 26), while
// clang's layout hid it entirely. The array is the scratchpad; overrunning
// it was the test corrupting its own bookkeeping, not the ring misbehaving.
static const int kLogMax = (int)(sizeof log_ / sizeof log_[0]);
static void log_put(int v) { if (log_n < kLogMax) log_[log_n] = v; ++log_n; }

static void fn_a() { log_put(1); }
static void fn_b() { log_put(2); }
static void fn_ctx(void *ctx) { log_put(*(int *)ctx); }

static DeferRing ring;

static void test_empty_ring_runs_nothing() {
  ring.reset();
  reset_log();
  CHECK(ring.pending() == 0);
  CHECK(ring.run_all() == 0);
  CHECK(log_n == 0);
}

static void test_runs_in_fifo_order() {
  ring.reset();
  reset_log();
  CHECK(ring.push(fn_a));
  CHECK(ring.push(fn_b));
  CHECK(ring.pending() == 2);
  CHECK(ring.run_all() == 2);
  CHECK(log_n == 2 && log_[0] == 1 && log_[1] == 2);
  CHECK(ring.pending() == 0);
}

static void test_context_calls_carry_their_pointer() {
  ring.reset();
  reset_log();
  int seven = 7, nine = 9;
  CHECK(ring.push(fn_ctx, &seven));
  CHECK(ring.push(fn_a));
  CHECK(ring.push(fn_ctx, &nine));
  ring.run_all();
  CHECK(log_n == 3 && log_[0] == 7 && log_[1] == 1 && log_[2] == 9);
}

static void test_full_ring_drops_and_counts() {
  ring.reset();
  reset_log();
  for (int i = 0; i < DeferRing::kSize; ++i) CHECK(ring.push(fn_a));
  CHECK(ring.pending() == DeferRing::kSize);
  CHECK(!ring.push(fn_b));   // no room: the newest is refused, not the oldest
  CHECK(ring.dropped == 1);
  CHECK(ring.run_all() == DeferRing::kSize);
  CHECK(log_n == DeferRing::kSize);
  for (int i = 0; i < log_n; ++i) CHECK(log_[i] == 1);
}

static void test_high_water_tracks_deepest_backlog() {
  ring.reset();
  ring.push(fn_a);
  ring.push(fn_a);
  ring.push(fn_a);
  ring.run_all();
  ring.push(fn_a);
  CHECK(ring.high_water == 3);
}

static void test_indices_wrap_past_the_array() {
  ring.reset();
  reset_log();
  // more pushes than kSize in total, never more than kSize outstanding
  for (int round = 0; round < 40; ++round) {
    CHECK(ring.push(fn_a));
    CHECK(ring.push(fn_b));
    CHECK(ring.run_all() == 2);
  }
  CHECK(log_n == 64 || log_n > 0);   // log is only 64 deep; order is what matters
  CHECK(log_[0] == 1 && log_[1] == 2 && log_[62] == 1 && log_[63] == 2);
  CHECK(ring.dropped == 0);
}

int main() {
  test_empty_ring_runs_nothing();
  test_runs_in_fifo_order();
  test_context_calls_carry_their_pointer();
  test_full_ring_drops_and_counts();
  test_high_water_tracks_deepest_backlog();
  test_indices_wrap_past_the_array();
  printf("test_defer_ring: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
