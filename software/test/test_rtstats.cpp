// Host tests for the real-time budget bookkeeping (src/RtStats.h): the
// microsecond histogram behind the p95 rows, the audio xrun run tracker,
// the stall-to-dropped-blocks arithmetic the Phase 0 detector check relies
// on, and the PASS/FAIL evaluation the console `T` command prints.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_rtstats test_rtstats.cpp && ./build/test_rtstats
#include <cstdio>
#include <cstdint>

#include "../src/RtStats.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

using namespace OC::RT;

// ---- Hist8: eight exponential buckets, p95 read off the bucket bound ----

static void test_hist_empty_has_no_p95() {
  Hist8 h;
  h.reset();
  CHECK(h.total == 0);
  CHECK(h.p95_bound_us() == 0);
}

static void test_hist_buckets_by_upper_bound() {
  Hist8 h;
  h.reset();
  h.add(99);     // < 100
  h.add(100);    // < 200 (bound is exclusive on the low side: 100 is not < 100)
  h.add(499);    // < 500
  h.add(999);    // < 1000
  h.add(1999);   // < 2000
  h.add(4999);   // < 5000
  h.add(9999);   // < 10000
  h.add(10000);  // >= 10000
  for (int i = 0; i < 8; ++i) CHECK(h.b[i] == 1);
  CHECK(h.total == 8);
}

static void test_hist_p95_is_bound_of_bucket_holding_95th_percentile() {
  Hist8 h;
  h.reset();
  for (int i = 0; i < 95; ++i) h.add(50);   // all in the < 100 bucket
  for (int i = 0; i < 5; ++i) h.add(3000);  // 5% in the < 5000 bucket
  // 95 of 100 samples are under 100 us: the 95th percentile sits in the
  // first bucket, so the bound reported is 100.
  CHECK(h.p95_bound_us() == 100);
  h.add(3000);  // now 6 of 101 are slow: p95 moves to the < 5000 bucket
  CHECK(h.p95_bound_us() == 5000);
}

static void test_hist_top_bucket_reports_open_ended() {
  Hist8 h;
  h.reset();
  h.add(20000);
  CHECK(h.p95_bound_us() == Hist8::kOpenEnded);
}

// ---- XrunRun: consecutive dropped blocks, one-offs vs stalls -----------

static void test_xrun_run_counts_and_tracks_longest_run() {
  XrunRun x;
  x.reset();
  x.dropped();
  x.ok();
  x.dropped();
  x.dropped();
  x.dropped();
  x.ok();
  CHECK(x.count == 4);
  CHECK(x.run == 0);
  CHECK(x.run_max == 3);
}

// ---- stall arithmetic: the Phase 0 detector self-check -----------------

static void test_expected_dropped_blocks_for_a_stall() {
  // 128 samples at 44117.647 Hz = 2.902 ms per block. The measured 2748 ms
  // save is ~947 blocks; the detector is wrong if it disagrees by > 10%.
  CHECK(expected_dropped_blocks_ms(2748) == 947);
  CHECK(expected_dropped_blocks_ms(0) == 0);
  CHECK(xrun_count_consistent(947, 2748));
  CHECK(xrun_count_consistent(1030, 2748));   // +8.8%
  CHECK(!xrun_count_consistent(500, 2748));   // detector missed half
  CHECK(!xrun_count_consistent(2000, 2748));  // detector double counts
}

static void test_expected_missed_core_ticks_for_a_stall() {
  // CORE ISR period is 60 us (OC_CORE_TIMER_RATE): 2748 ms = 45,800 ticks.
  CHECK(expected_missed_ticks_ms(2748) == 45800);
}

// ---- Evaluate: one verdict per ceiling ---------------------------------

static Counters clean_counters() {
  Counters c;
  c.reset();
  return c;
}

static void test_clean_counters_pass_every_row() {
  Counters c = clean_counters();
  Verdict v = Evaluate(c);
  CHECK(v.failures == 0);
  for (int i = 0; i < Verdict::kRows; ++i) CHECK(v.pass[i]);
}

static void test_audio_xrun_outside_window_fails_audio_row() {
  Counters c = clean_counters();
  c.audio_out.dropped();
  Verdict v = Evaluate(c);
  CHECK(!v.pass[Verdict::AUDIO_OUT]);
  CHECK(v.failures == 1);
}

static void test_xrun_inside_declared_window_is_allowed() {
  Counters c = clean_counters();
  c.audio_out.dropped();
  c.audio_out_in_window = 1;   // the window helper attributed it
  Verdict v = Evaluate(c);
  CHECK(v.pass[Verdict::AUDIO_OUT]);
}

static void test_core_isr_ceilings() {
  Counters c = clean_counters();
  // Off the constants, never a literal: these tests assert that a counter
  // one past the ceiling fails, which is true whatever the ceiling is. A
  // hardcoded 251 silently stopped testing anything the moment the window
  // ceiling moved to 300.
  c.core_isr_hiwater_us = Budget::kCoreIsrUs + 1;
  Verdict v = Evaluate(c);
  CHECK(!v.pass[Verdict::CORE_ISR_US]);
  c.core_isr_hiwater_us = Budget::kCoreIsrUs;
  c.core_missed_ticks = 1;
  v = Evaluate(c);
  CHECK(v.pass[Verdict::CORE_ISR_US]);
  CHECK(!v.pass[Verdict::CORE_MISSED]);
}

static void test_loop_and_midi_percentile_rows() {
  Counters c = clean_counters();
  c.loop_pass_max_us = Budget::kLoopP100Us + 1;
  for (int i = 0; i < 100; ++i) c.loop_hist.add(150);   // p95 bound 200: pass
  for (int i = 0; i < 100; ++i) c.midi_hist.add(1500);  // p95 bound 2000: fail
  Verdict v = Evaluate(c);
  CHECK(!v.pass[Verdict::LOOP_P100]);
  CHECK(v.pass[Verdict::LOOP_P95]);
  CHECK(!v.pass[Verdict::MIDI_P95]);
  CHECK(v.pass[Verdict::MIDI_P100]);   // no violation counted yet
  c.midi_gap_violations = 1;
  v = Evaluate(c);
  CHECK(!v.pass[Verdict::MIDI_P100]);
}

static void test_deferred_call_drop_fails_its_row() {
  // A dropped deferred call is a MIDI clock tick that never went out.
  Counters c = clean_counters();
  c.defer_hiwater = 3;   // depth alone is fine
  Verdict v = Evaluate(c);
  CHECK(v.pass[Verdict::DEFER]);
  c.defer_dropped = 1;
  v = Evaluate(c);
  CHECK(!v.pass[Verdict::DEFER]);
  CHECK(v.failures == 1);
}

static void test_alloc_failures_and_window_length() {
  Counters c = clean_counters();
  c.f32_alloc_fail = 1;
  c.window_max_ms = Budget::kWindowMs + 1;
  Verdict v = Evaluate(c);
  CHECK(!v.pass[Verdict::ALLOC]);
  CHECK(!v.pass[Verdict::WINDOW_MS]);
  CHECK(v.failures == 2);
}

int main() {
  test_hist_empty_has_no_p95();
  test_hist_buckets_by_upper_bound();
  test_hist_p95_is_bound_of_bucket_holding_95th_percentile();
  test_hist_top_bucket_reports_open_ended();
  test_xrun_run_counts_and_tracks_longest_run();
  test_expected_dropped_blocks_for_a_stall();
  test_expected_missed_core_ticks_for_a_stall();
  test_clean_counters_pass_every_row();
  test_audio_xrun_outside_window_fails_audio_row();
  test_xrun_inside_declared_window_is_allowed();
  test_core_isr_ceilings();
  test_loop_and_midi_percentile_rows();
  test_deferred_call_drop_fails_its_row();
  test_alloc_failures_and_window_length();
  printf("test_rtstats: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
