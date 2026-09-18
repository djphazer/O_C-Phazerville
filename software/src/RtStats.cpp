// Real-time budget bookkeeping, firmware side. See RtStats.h and
// docs/Timing-Budget.md. Everything here runs in loop context; the ISR
// hooks are inline in the header.
#include <Arduino.h>
#include <CrashReport.h>

#include "RtStats.h"
#include "PresetStage.h"
#include "Fade.h"
// XENO_CODEC_AUDIO (platformio.ini), not AUDIO_INTERFACE and not
// ARDUINO_TEENSY41. AUDIO_INTERFACE is a USB descriptor interface number that
// exists only when USB audio is compiled in, and guarding the fade on it
// compiled the fade out of the console build, where the DAC is live.
// ARDUINO_TEENSY41 is true for trees that have no F32 audio engine at all --
// upstream Phazerville is one, and so is the simulator -- so it is not the
// question either. The question is whether AudioOutputI2S2_F32 exists to be
// faded, and that is exactly what XENO_CODEC_AUDIO means.
//
// Everything else in this file works without it: the window still stops the
// clock, silences MIDI and attributes the stall. A build with no codec simply
// has no audio to ramp.
#ifdef XENO_CODEC_AUDIO
#include "extern/f32/output_i2s2_F32.h"
#endif

namespace OC {
namespace RT {

// stats / window_open / window_seen are inline variables in RtStats.h so the
// host test binaries and the simulator get them without this file.

void LoopPass() {
  static uint32_t last_cycles = 0;
  static uint32_t last_persist_ms = 0;
  const uint32_t now = ARM_DWT_CYCCNT;
  if (last_cycles) {
    const uint32_t us = cycles_to_us(now - last_cycles);
    if (window_seen) {
      // this pass held a declared persistence window: its length is the
      // window's to report, not the loop's
      window_seen = false;
    } else {
      if (us > stats.loop_pass_max_us) stats.loop_pass_max_us = us;
      stats.loop_hist.add(us);
    }
  }
  last_cycles = now;

  const uint32_t ms = millis();
  if (ms - last_persist_ms >= 1000) {
    last_persist_ms = ms;
    Persist();
  }
}

void MidiGap(uint32_t us) {
  // The first poll after a declared window measures the window, not a
  // cadence failure: Captain's polling is loop-bound and a window stops the
  // loop on purpose. Counted in the histogram either way, so the shape of
  // the distribution stays honest, but not against the violation ceiling.
  if (midi_window_seen) {
    midi_window_seen = false;
    stats.midi_hist.add(us);
    return;
  }
  stats.midi_hist.add(us);
  if (us > stats.midi_gap_max_us) stats.midi_gap_max_us = us;
  if (us > Budget::kMidiGapBudgetUs) stats.midi_gap_violations++;
}

// ---- the declared persistence window ------------------------------------

// How long each side of the dip takes. Out is shorter than in: leaving is a
// decision the player already made by pressing STORE, arriving is the
// instrument coming back and wants to be gentler. Both are far longer than
// one 2.902 ms audio block, so the ramp is actually heard as a ramp.
static constexpr uint32_t kFadeOutMs = 15;
static constexpr uint32_t kFadeInMs = 25;

#ifdef XENO_CODEC_AUDIO
// Step the master gain from loop context while the audio ISR is still
// running and producing blocks. delay(1) advances the clock and lets the
// ISR run, which is the whole point: a fade written in one go would just be
// a jump.
FLASHMEM static void run_ramp(const Fade::Ramp &r) {
  for (uint32_t ms = 0; ; ++ms) {
    AudioOutputI2S2_F32::master_gain = r.gain_at(ms);
    if (r.done(ms)) break;
    delay(1);
  }
}
#endif

// The Teensy core has no CMSIS __get_PRIMASK(); this is the core's own
// idiom (EventResponder.h). Non-zero = interrupts are already masked.
static inline uint32_t window_primask() {
  uint32_t primask;
  __asm__ volatile("mrs %0, primask\n" : "=r"(primask)::);
  return primask;
}

// Windows never legitimately nest -- SaveSlot is the only holder and it
// cannot re-enter -- but counting means a nested one could not close the
// outer one's attribution early.
static uint8_t window_depth = 0;

PersistenceWindow::PersistenceWindow(const char *reason, uint32_t declared_max_ms)
    : reason_(reason), declared_max_ms_(declared_max_ms) {
  // The fade needs the audio ISR to keep running while the gain steps, and
  // delay() to advance the clock. Neither is true with interrupts already
  // masked or from inside an interrupt, so in that case the window does its
  // bookkeeping and skips the fade rather than hanging. This class is
  // documented as loop-context-only; this is what makes a mistake harmless
  // instead of a lockup.
  faded_ = false;
#ifdef XENO_CODEC_AUDIO
  if (!window_primask() && window_depth == 0) {
    Fade::Ramp out;
    out.start(kFadeOutMs, true);
    run_ramp(out);
    // The gain is zero now, but the buffer the DMA is playing still holds
    // the last non-silent block. Zero it, or the stall replays that.
    AudioOutputI2S2_F32::silence_now();
    faded_ = true;
  }
#endif
  // The stall is about to start. Everything from here to the destructor is
  // attributed to this window rather than counted as a budget violation.
  window_depth++;
  window_open = true;
  window_seen = true;
  midi_window_seen = true;
  // DWT cycles, not millis(): systick is masked along with everything else
  // while the flash programs, so millis() under-reports exactly the part
  // this number exists to measure. Measured on hardware 2026-09-14: a save
  // that took 193 ms wall reported 25 ms by millis(). One lap is enough --
  // the counter wraps at ~7 s and a window this long is already a failure.
  start_cycles_ = ARM_DWT_CYCCNT;
  stats.window_count++;
}

PersistenceWindow::~PersistenceWindow() {
  // Measured to HERE: the stall is over, and the reported number should be
  // the stall, not the stall plus the ramp back.
  const uint32_t ms = (ARM_DWT_CYCCNT - start_cycles_) / (F_CPU_ACTUAL / 1000);
  if (window_depth) window_depth--;
  if (ms > stats.window_max_ms) stats.window_max_ms = ms;
  if (ms > declared_max_ms_) {
    stats.window_violations++;
    Serial.printf("rt: window '%s' ran %lu ms, declared %lu\n", reason_,
                  (unsigned long)ms, (unsigned long)declared_max_ms_);
  }
#ifdef XENO_CODEC_AUDIO
  if (faded_) {
    Fade::Ramp in;
    in.start(kFadeInMs, false);
    run_ramp(in);
    AudioOutputI2S2_F32::master_gain = 1.0f;   // exactly unity, not nearly
  }
#endif
  // Closed only NOW, after the ramp. update_all() has not run for the whole
  // stall, so the first few block periods of the fade-in can still come up
  // empty while the graph refills -- that is the stall's recovery, not normal
  // operation, and counting it as an out-of-window xrun failed a budget row
  // for doing exactly what the design intends. Measured before this: 12 saves
  // produced 7 output and 2 input drops attributed outside any window, all in
  // runs of 1-2 blocks, with every missed CORE tick correctly inside one.
  // Same mistake as the first MIDI poll after a window, fixed earlier.
  if (!window_depth) window_open = false;
}

FLASHMEM static void print_hist(const char *label, const Hist8 &h) {
  Serial.printf("  %s:", label);
  for (int i = 0; i < Hist8::kBuckets; ++i) {
    if (i < Hist8::kBuckets - 1)
      Serial.printf(" <%lu:%lu", (unsigned long)Hist8::bound(i), (unsigned long)h.b[i]);
    else
      Serial.printf(" >=10000:%lu", (unsigned long)h.b[i]);
  }
  const uint32_t p95 = h.p95_bound_us();
  if (p95 == Hist8::kOpenEnded) Serial.println("  p95 >= 10000 us");
  else Serial.printf("  p95 < %lu us\n", (unsigned long)p95);
}

FLASHMEM void Report(bool reset_after) {
  const Counters &c = stats;
  Serial.println("=== rt budget ===");
  Serial.printf("audio out: xrun=%lu run_max=%lu half=%lu alloc_fail=%lu len_mismatch=%lu in_window=%lu\n",
                (unsigned long)c.audio_out.count, (unsigned long)c.audio_out.run_max,
                (unsigned long)c.audio_out_half, (unsigned long)c.audio_out_alloc_fail,
                (unsigned long)c.audio_out_len_mismatch, (unsigned long)c.audio_out_in_window);
  // The codec hands us LEFT-JUSTIFIED 32-bit words (I32_TO_F32_NORM_FACTOR
  // in basic_DSPutils.h is 1/(2^31 - 1)), so full scale is 2^31, not 2^24.
  Serial.printf("audio in peak: %lu (%ld%% of full scale)\n",
                (unsigned long)c.audio_in_peak,
                (long)((uint64_t)c.audio_in_peak * 100 / 2147483648u));
  Serial.printf("audio in:  xrun=%lu alloc_fail=%lu in_window=%lu   f32 alloc_fail=%lu\n",
                (unsigned long)c.audio_in_xrun, (unsigned long)c.audio_in_alloc_fail,
                (unsigned long)c.audio_in_in_window, (unsigned long)c.f32_alloc_fail);
  Serial.printf("core isr:  max=%luus gap_max=%luus missed=%lu in_window=%lu\n",
                (unsigned long)c.core_isr_hiwater_us, (unsigned long)c.core_gap_max_us,
                (unsigned long)c.core_missed_ticks, (unsigned long)c.core_missed_in_window);
  Serial.printf("loop pass: max=%luus\n", (unsigned long)c.loop_pass_max_us);
  print_hist("loop", c.loop_hist);
  Serial.printf("midi poll: gap_max=%luus violations=%lu (budget %luus)\n",
                (unsigned long)c.midi_gap_max_us, (unsigned long)c.midi_gap_violations,
                (unsigned long)Budget::kMidiGapBudgetUs);
  print_hist("midi", c.midi_hist);
  Serial.printf("windows:   count=%lu max=%lums violations=%lu\n",
                (unsigned long)c.window_count, (unsigned long)c.window_max_ms,
                (unsigned long)c.window_violations);
  Serial.printf("defer:     dropped=%lu hiwater=%lu (ring of %u)\n",
                (unsigned long)c.defer_dropped, (unsigned long)c.defer_hiwater, 16u);
  Serial.printf("stage:     pending=%d refused=%lu superseded=%lu (recall images awaiting disk sync)\n",
                RecallStage().pending(), (unsigned long)RecallStage().refused,
                (unsigned long)RecallStage().superseded);
  const Verdict v = Evaluate(c);
  for (int i = 0; i < Verdict::kRows; ++i)
    Serial.printf("  %s  %s\n", v.pass[i] ? "PASS" : "FAIL", Verdict::name(i));
  Serial.printf("=== rt budget: %d of %d rows FAIL ===\n", v.failures, (int)Verdict::kRows);
  if (reset_after) {
    stats.reset();
    Serial.println("rt counters reset");
  } else {
    Serial.println("('T' again within 3s resets the counters)");
  }
}

FLASHMEM void Summary() {
  const Verdict v = Evaluate(stats);
  Serial.printf("rt budget: %d of %d rows FAIL (xrun=%lu missed=%lu loop_max=%luus; 'T' for detail)\n",
                v.failures, (int)Verdict::kRows, (unsigned long)stats.audio_out.count,
                (unsigned long)stats.core_missed_ticks, (unsigned long)stats.loop_pass_max_us);
}

// Breadcrumbs 1-6 survive a warm reset and are printed with the crash
// report (Main.cpp appends it to CRASH.LOG). Written only on change: each
// write is a cache flush.
FLASHMEM void Persist() {
  static uint32_t last[6] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                             0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
  const uint32_t cur[6] = {
    stats.audio_out.count,
    stats.core_missed_ticks,
    stats.loop_pass_max_us,
    stats.f32_alloc_fail + stats.audio_out_alloc_fail + stats.audio_in_alloc_fail,
    stats.window_max_ms,
    stats.midi_gap_violations,
  };
  for (int i = 0; i < 6; ++i) {
    if (cur[i] != last[i]) {
      last[i] = cur[i];
      CrashReport.breadcrumb(i + 1, cur[i]);
    }
  }
}

}  // namespace RT
}  // namespace OC
