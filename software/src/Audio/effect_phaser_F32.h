#pragma once

// Float32 port of the six-stage TPT phaser (src/src/Audio/effect_phaser.h,
// Ivan Cohen / musical entropy, modified by djphazer). The DSP was already
// float internally; this version takes float32 blocks in and out, so the only
// changes are at the block edges: the int16 original scaled input by 1/32768
// and clamped output to int16, both of which disappear here. Coefficient
// updates, the triangle LFO, and the zero-delay feedback structure are
// arithmetic-identical.

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"
#include "../util/util_macros.h"

class AudioEffectPhazerF32 : public AudioStream_F32 {
public:
  AudioEffectPhazerF32() : AudioStream_F32(1, inputQueueArray) {
    setDepth(0.5f);
    setFeedback(0.5f);
    setRate(0.5f);
    reset();
  }

  void setDepth(float depth0To1) {
    if (depth0To1 < 0.f) depth0To1 = 0.f;
    else if (depth0To1 > 1.f) depth0To1 = 1.f;

    __disable_irq();
    depth = depth0To1;
    __enable_irq();
  }

  void setFeedback(float feedback0To1) {
    if (feedback0To1 < 0.f) feedback0To1 = 0.f;
    else if (feedback0To1 > 0.99f) feedback0To1 = 0.99f;

    __disable_irq();
    feed = feedback0To1;
    __enable_irq();
  }

  // input in Hz, from 0.01 to 100
  void setRate(float rate) {
    CONSTRAIN(rate, 0.01f, 100.0f);

    __disable_irq();
    rateFactor = rate / AUDIO_SAMPLE_RATE_EXACT;
    __enable_irq();
  }

  void reset() {
    __disable_irq();
    for (int n = 0; n < 6; n++) s1[n] = 0.f;
    updateCounter = 0;
    phaseLFO = 0;
    __enable_irq();
  }

  virtual void update(void) override {
    audio_block_f32_t* in = receiveReadOnly_f32(0);
    if (!in) return;

    audio_block_f32_t* out = AudioStream_F32::allocate_f32();
    if (!out) {
      AudioStream_F32::release(in);
      return;
    }

    float localDepth = depth;
    float localFeed = feed;
    float localFactor = rateFactor;

    float logMin = log10f(200.f); // min frequency for modulation
    float logMax = log10f(6600.f); // max frequency for modulation

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
      // input (already normalized float; int16 version divided by 32768 here)
      float x = in->data[i];

      if (updateCounter == 0) {
        // LFO generation
        float lfo = 0.f;
        if (phaseLFO < 0.5f) lfo = 4.f * phaseLFO - 1.f;
        else lfo = -4.f * phaseLFO + 3.f;
        phaseLFO += localFactor;
        while (phaseLFO >= 1.f) phaseLFO -= 1.f;

        // frequency mapping
        lfo = (lfo * 0.5f * localDepth + 0.5f);
        float frequency = powf(10.f, lfo * (logMax - logMin) + logMin);

        // all-pass filters variables part 1
        float g = tanf(M_PI * frequency / AUDIO_SAMPLE_RATE_EXACT);
        float G = g / (1.f + g);
        P = 2.f * G - 1.f;
        Q = 2.f * (1.f - G);
        P2 = P + 1.f;
        Q2 = -P;

        // all-pass filters variables part 2
        float factor = P;
        for (int n = 1; n < 6; n++) {
          factor *= P;
          coeffs[n] = Q;
          for (int k = 1; k < n; k++) coeffs[k] *= P;
        }

        alpha = localFeed;
        factor = 1.f / (1.f - alpha * factor);

        coeffin = P * factor;
        coeffs[0] = Q * factor;
        for (int n = 1; n < 6; n++) coeffs[n] *= (alpha * P * factor);
      }

      // processing
      float y = coeffin * x;
      for (int n = 0; n < 6; n++) y += coeffs[n] * s1[n];

      for (int n = 1; n < 6; n++) {
        float ap = P * y + Q * s1[n];
        s1[n] = P2 * y + Q2 * s1[n];
        y = ap;
      }

      s1[0] = P2 * (x + alpha * y) + Q2 * s1[0];

      // updates
      updateCounter++;
      if (updateCounter >= 8) updateCounter = 0;

      // output: no int16 clamp; headroom stays float until the edge converter
      out->data[i] = y;
    }

    AudioStream_F32::transmit(out);
    AudioStream_F32::release(out);
    AudioStream_F32::release(in);
  }

private:
  audio_block_f32_t* inputQueueArray[1];

  // tunables
  volatile float depth = 0.5f;
  volatile float feed = 0.5f;
  volatile float rateFactor = 0.f;

  // internal variables
  float P, Q, P2, Q2;
  float coeffin, coeffs[6];
  float alpha;

  // state variables
  volatile int updateCounter = 0;
  volatile float s1[6];
  volatile float phaseLFO = 0;
};
