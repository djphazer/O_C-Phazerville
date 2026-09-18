// Real-time budget bookkeeping for the Xenomorpher: the counters the audio
// ISRs, the CORE timer ISR, loop() and Captain MIDI feed, the ceilings in
// docs/Timing-Budget.md, and the PASS/FAIL evaluation the console `T`
// command prints. BSP-free on purpose: test/test_rtstats.cpp runs it on
// the host, and the arithmetic helpers at the bottom are how a fresh
// detector is checked against a known stall before its numbers are trusted.
//
// Counters are plain 32-bit words written from ISR context and read from
// loop context. A torn read of a monotonically increasing stat is a stale
// value, never a wrong one; nothing here is used for control.
#ifndef RTSTATS_H_
#define RTSTATS_H_

#include <stdint.h>

namespace OC {
namespace RT {

// Eight exponential microsecond buckets: < 100, < 200, < 500, < 1000,
// < 2000, < 5000, < 10000, >= 10000. Cheap enough to update from loop()
// every pass and from every MIDI poll; a p95 is read off the bucket bound
// without sorting anything.
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

  // Upper bound of the bucket holding the 95th percentile; 0 when empty;
  // kOpenEnded when it lands in the top bucket.
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

// Dropped audio blocks: the total, the current consecutive run, and the
// longest run seen. A run of 1 is a one-off; a long run is a stall.
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

// The ceilings. Evidence is the console `T` report; the reasoning is in
// docs/Timing-Budget.md.
struct Budget {
  static constexpr uint32_t kCoreIsrUs = 40;      // of the 60 us period
  static constexpr uint32_t kLoopP100Us = 5000;
  static constexpr uint32_t kLoopP95Us = 200;
  static constexpr uint32_t kMidiP95Us = 1000;
  static constexpr uint32_t kMidiGapBudgetUs = 5000;  // p100: any gap above counts
  // 300, not the 250 this started at. Measured over 32 consecutive saves on
  // T41_console (2026-09-14): 129-139 ms for four saves in five, then
  // 192-259 ms on every fifth, deterministically -- 6 spikes in 6 at saves 5,
  // 10, 15, 20, 25, 30. The save is 98% one phase, cw_commit, writing a
  // 4940-byte container across two 4 KB flash blocks; that erase time is the
  // floor and no amount of code makes it smaller. The periodic step on top is
  // LittleFS compacting its directory metadata pair, which fills after about
  // five saves' worth of commits. LittleFS 2.4 is what ships here and it has
  // no lfs_fs_gc, so there is no way to make that happen at idle instead.
  // A 250 ms line cried wolf every fifth save on behaviour that is correct
  // and unimprovable; 300 still catches a real regression. See
  // docs/Timing-Budget.md.
  static constexpr uint32_t kWindowMs = 300;
};

struct Counters {
  XrunRun audio_out;               // output ISR had no block for either channel
  uint32_t audio_out_half;         // one channel's block missing
  uint32_t audio_out_alloc_fail;   // output update() could not get scratch blocks
  uint32_t audio_out_len_mismatch; // a block arrived with the wrong sample count
  uint32_t audio_out_in_window;    // of audio_out.count, attributed to a declared window
  uint32_t audio_in_xrun;          // input ISR had nowhere to put half a block
  uint32_t audio_in_in_window;
  uint32_t audio_in_alloc_fail;
  // Largest absolute sample the codec has handed us since the last reset.
  // Not a budget row -- there is no "correct" input level -- but it is the
  // only thing that distinguishes a silent patch cable from an input path
  // that is not running at all.
  uint32_t audio_in_peak;
  uint32_t f32_alloc_fail;
  uint32_t core_missed_ticks;      // CORE ISR entries that arrived >= 1.5 periods late
  uint32_t core_missed_in_window;
  uint32_t core_gap_max_us;        // longest entry-to-entry gap
  uint32_t core_isr_hiwater_us;    // longest CORE ISR body, never auto-reset
  uint32_t loop_pass_max_us;       // outside declared windows
  Hist8 loop_hist;
  Hist8 midi_hist;                 // MIDI poll-to-poll gaps
  uint32_t midi_gap_max_us;
  uint32_t midi_gap_violations;    // gaps above Budget::kMidiGapBudgetUs, never auto-reset
  uint32_t window_count;
  uint32_t window_max_ms;
  uint32_t window_violations;      // windows that ran past their declared max
  uint32_t defer_dropped;          // ISR->loop deferred calls refused (ring full)
  uint32_t defer_hiwater;          // deepest deferred-call backlog seen

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

// ---- detector self-check arithmetic -------------------------------------
// The audio block is 128 samples at 44117.647 Hz = 2.902 ms; the CORE ISR
// period is 60 us. A stall of known length must produce these counts, or
// the counter feeding them is wrong.
static constexpr uint32_t kSampleRateMilliHz = 44117647;
static constexpr uint32_t kBlockSamples = 128;
static constexpr uint32_t kCoreTickUs = 60;

inline uint32_t expected_dropped_blocks_ms(uint32_t stall_ms) {
  // ms * (Hz * 1000) / 1e6 = samples
  const uint64_t samples = (uint64_t)stall_ms * kSampleRateMilliHz / 1000000;
  return (uint32_t)(samples / kBlockSamples);
}

inline uint32_t expected_missed_ticks_ms(uint32_t stall_ms) {
  return stall_ms * 1000 / kCoreTickUs;
}

// Within 10% of the expected count for that stall.
inline bool xrun_count_consistent(uint32_t counted, uint32_t stall_ms) {
  const uint32_t e = expected_dropped_blocks_ms(stall_ms);
  const uint32_t d = counted > e ? counted - e : e - counted;
  return d * 10 <= e;
}

// The one instance, as a C++17 inline variable so that the firmware, the
// host test binaries and the simulator all get exactly one without any of
// them needing to compile RtStats.cpp. The audio and bus sources are shared
// between all three and increment it unconditionally.
inline Counters stats;
inline volatile bool window_open = false;
inline volatile bool window_seen = false;
inline volatile bool midi_window_seen = false;

// ---- the declared persistence window ------------------------------------
// Some writes to the program flash cannot be avoided and cannot be made
// quick: the core masks interrupts for the whole erase/program because code
// runs from the same chip, so audio, USB, the display and the bus slave all
// stop. Left alone the DAC replays its last buffer, heard as a click into a
// frozen tone; the CV outputs hold, which for a control voltage is right.
//
// A window makes that stall declared rather than accidental. Construct one
// around the write, IN LOOP CONTEXT ONLY, and it fades the audio to true
// silence first, zeroes the DMA buffer so nothing is replayed, and fades
// back in afterwards. Drops attributed to an open window do not count
// against the audio rows of the budget; the window's own length counts
// against the window row.
//
// Only a write the player asked for may fade: a fade is the instrument
// saying "you pressed STORE". A background write (the debounced current-slot
// record, the card image flush) must instead wait for an idle gap -- fading
// the audio for something nobody asked for is worse than the stall.
class PersistenceWindow {
 public:
  // `reason` is for the console; it is not copied, so pass a literal.
  explicit PersistenceWindow(const char *reason, uint32_t declared_max_ms = Budget::kWindowMs);
  ~PersistenceWindow();

  PersistenceWindow(const PersistenceWindow &) = delete;
  PersistenceWindow &operator=(const PersistenceWindow &) = delete;

 private:
  const char *reason_;
  uint32_t declared_max_ms_;
  uint32_t start_cycles_;
  // Whether this window actually took the audio down. False when it was
  // constructed somewhere a fade cannot run (interrupts already masked, or
  // inside another window), so the destructor knows not to fade back up
  // from a gain it never changed.
  bool faded_;
};

#ifndef ARDUINO
// Host builds (the tests, the simulator) have no audio to fade and no flash
// to stall on, but they do run this code, so the bookkeeping stays real and
// the fade is simply absent.
inline PersistenceWindow::PersistenceWindow(const char *reason, uint32_t declared_max_ms)
    : reason_(reason), declared_max_ms_(declared_max_ms), start_cycles_(0), faded_(false) {
  window_open = true;
  window_seen = true;
  stats.window_count++;
}
inline PersistenceWindow::~PersistenceWindow() {
  // The reason string, the declared maximum and the start time are all
  // about reporting a stall that a host build does not have. Named here so
  // the compiler can see they are ignored on purpose.
  (void)reason_;
  (void)declared_max_ms_;
  (void)start_cycles_;
  (void)faded_;
  window_open = false;
}
#endif

#ifdef ARDUINO
#include <Arduino.h>   // F_CPU_ACTUAL, ARM_DWT_CYCCNT
// ---- firmware side (RtStats.cpp) ----------------------------------------
// The hooks are inline so the ISR callers stay in ITCM without a call into
// flash. `window_open` is raised by a declared persistence window (Track C)
// so drops inside it are attributed, not counted against the budget;
// `window_seen` tells the loop-pass timer to skip the pass that held one.

inline uint32_t cycles_to_us(uint32_t cycles) {
  return cycles / (F_CPU_ACTUAL / 1000000u);
}

// CORE timer ISR entry: how late are we, and did ticks go missing?
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

// CORE timer ISR exit: the body's duration, high-water only.
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

// One loop() pass: called once per iteration from loop context.
void LoopPass();
// One MIDI poll-to-poll gap, microseconds, from Captain's poll cadence meter.
void MidiGap(uint32_t us);
// Console `T`: every counter with PASS/FAIL per ceiling; reset_after
// clears the counters once printed.
void Report(bool reset_after);
// One line for the selftest.
void Summary();
// Copy the headline counters into the CrashReport breadcrumbs so a reboot
// keeps them (they land in CRASH.LOG at the next boot).
void Persist();
#endif  // ARDUINO

}  // namespace RT
}  // namespace OC

#endif  // RTSTATS_H_
