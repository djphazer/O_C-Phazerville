#include <Arduino.h>
#include <CrashReport.h>

#include "RtStats.h"
#include "PresetStage.h"
#include "Fade.h"
#ifdef XENO_CODEC_AUDIO
#include "extern/f32/output_i2s2_F32.h"
#endif

namespace OC {
namespace RT {

void LoopPass() {
  static uint32_t last_cycles = 0;
  static uint32_t last_persist_ms = 0;
  const uint32_t now = ARM_DWT_CYCCNT;
  if (last_cycles) {
    const uint32_t us = cycles_to_us(now - last_cycles);
    if (window_seen) {
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
  if (midi_window_seen) {
    midi_window_seen = false;
    stats.midi_hist.add(us);
    return;
  }
  stats.midi_hist.add(us);
  if (us > stats.midi_gap_max_us) stats.midi_gap_max_us = us;
  if (us > Budget::kMidiGapBudgetUs) stats.midi_gap_violations++;
}

static constexpr uint32_t kFadeOutMs = 15;
static constexpr uint32_t kFadeInMs = 25;

#ifdef XENO_CODEC_AUDIO
FLASHMEM static void run_ramp(const Fade::Ramp &r) {
  for (uint32_t ms = 0; ; ++ms) {
    AudioOutputI2S2_F32::master_gain = r.gain_at(ms);
    if (r.done(ms)) break;
    delay(1);
  }
}
#endif

static inline uint32_t window_primask() {
  uint32_t primask;
  __asm__ volatile("mrs %0, primask\n" : "=r"(primask)::);
  return primask;
}

static uint8_t window_depth = 0;

PersistenceWindow::PersistenceWindow(const char *reason, uint32_t declared_max_ms)
    : reason_(reason), declared_max_ms_(declared_max_ms) {
  faded_ = false;
#ifdef XENO_CODEC_AUDIO
  if (!window_primask() && window_depth == 0) {
    Fade::Ramp out;
    out.start(kFadeOutMs, true);
    run_ramp(out);
    AudioOutputI2S2_F32::silence_now();
    faded_ = true;
  }
#endif
  window_depth++;
  window_open = true;
  window_seen = true;
  midi_window_seen = true;
  start_cycles_ = ARM_DWT_CYCCNT;
  stats.window_count++;
}

PersistenceWindow::~PersistenceWindow() {
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
    AudioOutputI2S2_F32::master_gain = 1.0f;
  }
#endif
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

}
}
