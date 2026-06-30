#pragma once

// AudioSynthAdvancedKarplus — custom Karplus-Strong physical model
//
// Motivation:
//   The PJRC AudioSynthKarplusStrong uses an integer delay line (pitch
//   quantization at higher frequencies) and a hardcoded 2-point average
//   filter (no independent brightness/decay/body control). This object
//   addresses both limitations.
//
// Architecture:
//   - 4096-sample float32 circular delay line (internal heap; avoids QSPI ISR stall)
//   - Linear fractional interpolation on read: eliminates pitch quantization
//   - Tunable 1st-order IIR in feedback loop: independent Brightness control
//   - Pitch-compensated loop gain: perceptual Decay time independent of pitch
//   - Resonant bandpass "Body" filter applied to excitation noise burst
//   - Delay length applied directly each block (no smoothing — KS has no phase discontinuity)
//
// Thread safety:
//   setFrequency / setDecay / setBrightness / setBody: call anytime; atomic enough
//   noteOn():  sets a volatile flag; trigger handled safely in update()
//   Acquire() / Release(): call from Start() / Unload() in main loop only

#include <Audio.h>
#include "../../dsputils.h"

class AudioSynthAdvancedKarplus : public AudioStream {
public:
  // Delay line size must be a power of 2 for fast index masking
  static constexpr uint32_t BUFFER_SIZE = 4096;
  static constexpr uint32_t BUFFER_MASK = BUFFER_SIZE - 1;

  // Practical frequency limits for KS synthesis at 44.1 kHz:
  //   Low end: 20 Hz → delay = 2205 samples (fine, under BUFFER_SIZE)
  //   High end: 4000 Hz → delay = ~11 samples (below 11 is unstable)
  static constexpr float MIN_HZ = 20.0f;
  static constexpr float MAX_HZ = 4000.0f;

  AudioSynthAdvancedKarplus() : AudioStream(0, nullptr) {}

  // --- Lifecycle ---------------------------------------------------------

  // Allocate the delay line from internal heap (DTCM/OCRAM).
  // 16 KB in internal RAM avoids the QSPI stall that PSRAM causes
  // when memset fires inside the audio ISR. Called from applet Start().
  __attribute__((noinline)) void Acquire();
  __attribute__((noinline)) void Release();

  // --- Parameter setters (call every Controller() tick) -----------------

  // Target fundamental frequency in Hz.
  __attribute__((noinline)) void setFrequency(float hz);

  // Decay: 0.0 (shortest) → 1.0 (longest).
  // Internally maps to a target decay time on a log scale (0.05 s → 15 s).
  // The loop gain ρ is then pitch-compensated so that perceptual decay time
  // is independent of pitch (high notes decay as slowly as low notes).
  __attribute__((noinline)) void setDecay(float d);

  // Brightness: 0.0 (dark, heavy LPF) → 1.0 (bright, minimal LPF).
  // Controls α in the 1st-order IIR feedback filter:
  //   y[n] = α * x[n] + (1 - α) * y[n-1]
  // Large α passes high frequencies (bright); small α smooths heavily (dark).
  __attribute__((noinline)) void setBrightness(float b);

  // Body: 0.0 (flat noise excitation) → 1.0 (narrow resonant bandpass).
  // Controls the Q of a biquad bandpass applied to the noise burst on noteOn.
  // Higher values simulate an acoustic body cavity that emphasises the
  // fundamental before the string recirculates the excitation.
  __attribute__((noinline)) void setBody(float body);

  // --- Trigger -----------------------------------------------------------

  // Fire the string. velocity: 0.0 = silent, 1.0 = full amplitude.
  // Safe to call from Controller() (flag read inside audio interrupt update()).
  __attribute__((noinline)) void noteOn(float velocity = 1.0f);

  // --- AudioStream update (runs in audio interrupt) ----------------------

  // Tiny virtual stub — stays in ITCM (RAM1). All DSP work is in updateCore(),
  // defined in synth_advanced_karplus.cpp where FLASHMEM is honoured.
  void update() override { if (delay_line_) updateCore(); }

private:
  __attribute__((noinline)) void updateCore();  // defined in synth_advanced_karplus.cpp with FLASHMEM

  static constexpr float KS_TWO_PI = 6.28318530718f;

  // --- Delay line ---------------------------------------------------------
  float*   delay_line_ = nullptr;
  uint32_t write_idx_  = 0;

  // --- Pitch smoothing ----------------------------------------------------
  float target_hz_    = 441.0f;   // stored so setBrightness() can recompute delay
  float target_delay_ = 100.0f;   // samples (≈ 441 Hz default)

  // --- Feedback filter (Brightness) ---------------------------------------
  float iir_alpha_ = 0.5f;        // computed from brightness_param
  float iir_state_ = 0.0f;

  // --- Parameter state ----------------------------------------------------
  float decay_param_      = 0.5f;
  float brightness_param_ = 0.5f;
  float body_param_       = 0.3f;

  // --- Trigger (volatile: written in main loop, read in audio interrupt) --
  volatile bool  trigger_pending_  = false;
  volatile float trigger_velocity_ = 1.0f;

  // --- Progressive excitation state ---------------------------------------
  int      excite_remaining_ = 0;    // samples left to generate for current pluck
  float    excite_phase_     = 0.0f; // sine phase accumulator
  float    excite_phase_inc_ = 0.0f; // sine phase step per sample
  float    excite_scale_     = 0.0f; // velocity * 32767

  // --- Noise generation ---------------------------------------------------
  uint32_t noise_seed_ = 0xDEADBEEF;

  // --- Helpers ------------------------------------------------------------

  // Recompute target_delay_ from target_hz_ and the current IIR alpha.
  // The 1st-order IIR adds (1 - α) / α samples of DC group delay inside
  // the feedback loop. Subtracting it here keeps pitch accurate across the
  // full brightness range. Must be called whenever target_hz_ or iir_alpha_
  // changes (i.e. from setFrequency() and setBrightness()).
  __attribute__((noinline)) void recalculateDelay();
};
