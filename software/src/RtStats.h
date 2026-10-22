#ifndef RTSTATS_H_
#define RTSTATS_H_

#include <stdint.h>

namespace OC {
namespace RT {

struct Hist8 {
  static constexpr uint32_t kOpenEnded = 0xFFFFFFFFu;
  static constexpr int kBuckets = 8;

  uint32_t b[kBuckets];
  uint32_t total;

  static uint32_t bound(int i) {
    static const uint32_t kBounds[kBuckets - 1] = {100, 200, 500, 1000, 2000, 5000, 10000};
    return i < kBuckets - 1 ? kBounds[i] : kOpenEnded;
  }

  void reset() {
    for (int i = 0; i < kBuckets; ++i) b[i] = 0;
    total = 0;
  }

  void add(uint32_t us) {
    int i = 0;
    while (i < kBuckets - 1 && us >= bound(i)) ++i;
    b[i]++;
    total++;
  }

  uint32_t p95_bound_us() const {
    if (!total) return 0;
    const uint32_t need = ((uint64_t)total * 95 + 99) / 100;
    uint32_t cum = 0;
    for (int i = 0; i < kBuckets; ++i) {
      cum += b[i];
      if (cum >= need) return bound(i);
    }
    return kOpenEnded;
  }
};

struct XrunRun {
  uint32_t count;
  uint32_t run;
  uint32_t run_max;

  void reset() { count = run = run_max = 0; }
  void dropped() {
    count++;
    run++;
    if (run > run_max) run_max = run;
  }
  void ok() { run = 0; }
};

struct Budget {
  static constexpr uint32_t kCoreIsrUs = 40;
  static constexpr uint32_t kLoopP100Us = 5000;
  static constexpr uint32_t kLoopP95Us = 200;
  static constexpr uint32_t kMidiP95Us = 1000;
  static constexpr uint32_t kMidiGapBudgetUs = 5000;
  static constexpr uint32_t kWindowMs = 300;
};

struct Counters {
  XrunRun audio_out;
  uint32_t audio_out_half;
  uint32_t audio_out_alloc_fail;
  uint32_t audio_out_len_mismatch;
  uint32_t audio_out_in_window;
  uint32_t audio_in_xrun;
  uint32_t audio_in_in_window;
  uint32_t audio_in_alloc_fail;
  uint32_t audio_in_peak;
  uint32_t f32_alloc_fail;
  uint32_t core_missed_ticks;
  uint32_t core_missed_in_window;
  uint32_t core_gap_max_us;
  uint32_t core_isr_hiwater_us;
  uint32_t loop_pass_max_us;
  Hist8 loop_hist;
  Hist8 midi_hist;
  uint32_t midi_gap_max_us;
  uint32_t midi_gap_violations;
  uint32_t window_count;
  uint32_t window_max_ms;
  uint32_t window_violations;
  uint32_t defer_dropped;
  uint32_t defer_hiwater;

  void reset() {
    defer_dropped = defer_hiwater = 0;
    audio_out.reset();
    audio_out_half = audio_out_alloc_fail = audio_out_in_window = 0;
    audio_out_len_mismatch = 0;
    audio_in_xrun = audio_in_in_window = audio_in_alloc_fail = 0;
    audio_in_peak = 0;
    f32_alloc_fail = 0;
    core_missed_ticks = core_missed_in_window = core_gap_max_us = core_isr_hiwater_us = 0;
    loop_pass_max_us = 0;
    loop_hist.reset();
    midi_hist.reset();
    midi_gap_max_us = midi_gap_violations = 0;
    window_count = window_max_ms = window_violations = 0;
  }
};

struct Verdict {
  enum Row {
    AUDIO_OUT, AUDIO_IN, CORE_ISR_US, CORE_MISSED, LOOP_P100, LOOP_P95,
    MIDI_P95, MIDI_P100, ALLOC, WINDOW_MS, DEFER, kRows
  };
  bool pass[kRows];
  int failures;

  static const char *name(int row) {
    static const char *const kNames[kRows] = {
      "audio out xrun outside window", "audio in xrun outside window",
      "core isr max us", "core missed ticks outside window",
      "loop pass max us", "loop pass p95 us",
      "midi gap p95 us", "midi gap violations",
      "audio alloc failures", "persistence window max ms",
      "deferred calls dropped",
    };
    return kNames[row];
  }
};

inline Verdict Evaluate(const Counters &c) {
  Verdict v;
  v.pass[Verdict::AUDIO_OUT] = c.audio_out.count == c.audio_out_in_window;
  v.pass[Verdict::AUDIO_IN] = c.audio_in_xrun == c.audio_in_in_window;
  v.pass[Verdict::CORE_ISR_US] = c.core_isr_hiwater_us <= Budget::kCoreIsrUs;
  v.pass[Verdict::CORE_MISSED] = c.core_missed_ticks == c.core_missed_in_window;
  v.pass[Verdict::LOOP_P100] = c.loop_pass_max_us <= Budget::kLoopP100Us;
  v.pass[Verdict::LOOP_P95] = c.loop_hist.p95_bound_us() <= Budget::kLoopP95Us;
  v.pass[Verdict::MIDI_P95] = c.midi_hist.p95_bound_us() <= Budget::kMidiP95Us;
  v.pass[Verdict::MIDI_P100] = c.midi_gap_violations == 0;
  v.pass[Verdict::ALLOC] =
      c.f32_alloc_fail + c.audio_out_alloc_fail + c.audio_in_alloc_fail == 0;
  v.pass[Verdict::WINDOW_MS] = c.window_max_ms <= Budget::kWindowMs;
  v.pass[Verdict::DEFER] = c.defer_dropped == 0;
  v.failures = 0;
  for (int i = 0; i < Verdict::kRows; ++i) if (!v.pass[i]) v.failures++;
  return v;
}

static constexpr uint32_t kSampleRateMilliHz = 44117647;
static constexpr uint32_t kBlockSamples = 128;
static constexpr uint32_t kCoreTickUs = 60;

inline uint32_t expected_dropped_blocks_ms(uint32_t stall_ms) {
  const uint64_t samples = (uint64_t)stall_ms * kSampleRateMilliHz / 1000000;
  return (uint32_t)(samples / kBlockSamples);
}

inline uint32_t expected_missed_ticks_ms(uint32_t stall_ms) {
  return stall_ms * 1000 / kCoreTickUs;
}

inline bool xrun_count_consistent(uint32_t counted, uint32_t stall_ms) {
  const uint32_t e = expected_dropped_blocks_ms(stall_ms);
  const uint32_t d = counted > e ? counted - e : e - counted;
  return d * 10 <= e;
}

inline Counters stats;
inline volatile bool window_open = false;
inline volatile bool window_seen = false;
inline volatile bool midi_window_seen = false;

class PersistenceWindow {
 public:
  explicit PersistenceWindow(const char *reason, uint32_t declared_max_ms = Budget::kWindowMs);
  ~PersistenceWindow();

  PersistenceWindow(const PersistenceWindow &) = delete;
  PersistenceWindow &operator=(const PersistenceWindow &) = delete;

 private:
  const char *reason_;
  uint32_t declared_max_ms_;
  uint32_t start_cycles_;
  bool faded_;
};

#ifndef ARDUINO
inline PersistenceWindow::PersistenceWindow(const char *reason, uint32_t declared_max_ms)
    : reason_(reason), declared_max_ms_(declared_max_ms), start_cycles_(0), faded_(false) {
  window_open = true;
  window_seen = true;
  stats.window_count++;
}
inline PersistenceWindow::~PersistenceWindow() {
  (void)reason_;
  (void)declared_max_ms_;
  (void)start_cycles_;
  (void)faded_;
  window_open = false;
}
#endif

#ifdef ARDUINO
#include <Arduino.h>

inline uint32_t cycles_to_us(uint32_t cycles) {
  return cycles / (F_CPU_ACTUAL / 1000000u);
}

inline void CoreIsrEntry(uint32_t now_cycles) {
  static uint32_t last = 0;
  if (last) {
    const uint32_t d = now_cycles - last;
    const uint32_t tick = (F_CPU_ACTUAL / 1000000u) * kCoreTickUs;
    if (d > tick + tick / 2) {
      const uint32_t missed = d / tick - 1;
      stats.core_missed_ticks += missed;
      if (window_open) stats.core_missed_in_window += missed;
    }
    const uint32_t us = cycles_to_us(d);
    if (us > stats.core_gap_max_us) stats.core_gap_max_us = us;
  }
  last = now_cycles;
}

inline void CoreIsrExit(uint32_t entry_cycles) {
  const uint32_t us = cycles_to_us(ARM_DWT_CYCCNT - entry_cycles);
  if (us > stats.core_isr_hiwater_us) stats.core_isr_hiwater_us = us;
}

inline void AudioOutDropped() {
  stats.audio_out.dropped();
  if (window_open) stats.audio_out_in_window++;
}
inline void AudioInDropped() {
  stats.audio_in_xrun++;
  if (window_open) stats.audio_in_in_window++;
}

void LoopPass();
void MidiGap(uint32_t us);
void Report(bool reset_after);
void Summary();
void Persist();
#endif

}
}

#endif
