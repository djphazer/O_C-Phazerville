#ifndef AUDIOANALYZESTROBE_H_
#define AUDIOANALYZESTROBE_H_

#include <Arduino.h>
#include <AudioStream.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Quadrature demodulator - the engine of a real strobe tuner.
//
// A mechanical strobe does not measure frequency and print a number: it
// compares the signal against a reference and shows the PHASE between them.
// A phase that stands still means the two frequencies are identical; a phase
// that creeps means they differ by exactly the creep rate. That is why a
// strobe out-resolves a frequency counter - you integrate by eye for as long
// as you care to, and the resolution keeps improving.
//
// This is the DSP form of the same idea: mix the input with a complex
// reference exp(-j*w*t) and lowpass the result. The surviving I/Q vector
// sits at the difference frequency, so
//     atan2(Q, I)          = the strobe phase (rotate the display by it)
//     d/dt atan2(Q, I)     = 2*pi*(f_in - f_ref), the tuning error
//     |I,Q|                = how much signal is actually at the reference
//
// The reference oscillator is a rotating unit vector (4 multiplies per
// sample) rather than sinf/cosf per sample, renormalized once per block to
// stop the rotation from spiralling.
// ---------------------------------------------------------------------------
class AudioAnalyzeStrobe : public AudioStream {
public:
  AudioAnalyzeStrobe() : AudioStream(1, inputQueueArray) {
    setReference(440.0f);
    setSmoothing(60.0f);
  }

  // Demodulation frequency: the EXACT pitch you are tuning to.
  void setReference(float hz) {
    if (hz < 1.0f) hz = 1.0f;
    if (hz > AUDIO_SAMPLE_RATE_EXACT * 0.45f) hz = AUDIO_SAMPLE_RATE_EXACT * 0.45f;
    const float w = 2.0f * (float)M_PI * hz / AUDIO_SAMPLE_RATE_EXACT;
    const float c = cosf(w), s = sinf(w);
    __disable_irq();
    ref_hz = hz;
    cos_inc = c;
    sin_inc = s;
    __enable_irq();
  }
  float reference() const { return ref_hz; }

  // Only burn cycles while the tuner is on screen.
  void setActive(bool on) {
    if (on && !active) { i_acc = q_acc = 0.0f; osc_c = 1.0f; osc_s = 0.0f; }
    active = on;
  }
  bool isActive() const { return active; }

  // I/Q lowpass time constant. Longer = steadier phase and finer resolution,
  // at the cost of lag; 40-80ms tracks a hand on a knob without jitter.
  void setSmoothing(float ms) {
    const float tau = ms * 0.001f * AUDIO_SAMPLE_RATE_EXACT;
    lp = (tau > 1.0f) ? (1.0f / tau) : 1.0f;
  }

  // Snapshot both accumulators together - a torn I/Q pair would read as a
  // phase glitch, which on a strobe display looks like a real tuning jump.
  void read(float &phase_out, float &mag_out) {
    __disable_irq();
    const float i = i_acc, q = q_acc;
    __enable_irq();
    phase_out = atan2f(q, i);
    mag_out = sqrtf(i * i + q * q);
  }

  virtual void update(void);

private:
  audio_block_t *inputQueueArray[1];
  volatile float i_acc = 0.0f, q_acc = 0.0f;
  float osc_c = 1.0f, osc_s = 0.0f;      // rotating reference vector
  volatile float cos_inc = 1.0f, sin_inc = 0.0f;
  float lp = 0.001f;
  volatile float ref_hz = 440.0f;
  volatile bool active = false;
};

// Audio-ISR context: never FLASHMEM.
inline void AudioAnalyzeStrobe::update(void) {
  audio_block_t *blk = receiveReadOnly(0);
  if (!blk) return;
  if (!active) { release(blk); return; }

  float c = osc_c, s = osc_s;
  float i = i_acc, q = q_acc;
  const float ci = cos_inc, si = sin_inc, k = lp;

  for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
    const float x = (float)blk->data[n] * (1.0f / 32768.0f);
    // mix with exp(-j*w*t) and lowpass (one pole, in-place)
    i += k * (x * c - i);
    q += k * (-x * s - q);
    // rotate the reference by one sample of w
    const float nc = c * ci - s * si;
    s = c * si + s * ci;
    c = nc;
  }

  // one Newton step back onto the unit circle - cheaper than a sqrt and
  // exact enough when the magnitude is already within a hair of 1
  const float g = 1.5f - 0.5f * (c * c + s * s);
  osc_c = c * g;
  osc_s = s * g;
  i_acc = i;
  q_acc = q;
  release(blk);
}

#endif  // AUDIOANALYZESTROBE_H_
