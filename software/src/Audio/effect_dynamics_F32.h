#pragma once

// Float32 port of the Teensy Audio Library's dynamics processor
// (src/src/Audio/effect_dynamics.{h,cpp}, Marc Paquette, MIT). Same gate /
// compressor / brickwall-limiter dB math, attack/release smoothing, and
// auto-makeup-gain behavior, but the 50ms running-RMS window and the applied
// gain stay float32: samples are the chain's normalized floats, so the
// 20*log10(32768) full-scale term drops out of the RMS dB constant, and the
// output keeps float headroom (no saturate16) until the applet's edge
// converter. The squared-sample accumulator is double to avoid the drift the
// int16 version avoided with exact integer arithmetic.

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioEffectDynamicsF32 : public AudioStream_F32 {
public:
  // Same limits as the int16 version's MIN_DB/MAX_DB/MIN_T/MAX_T macros,
  // renamed to stay clear of those #defines.
  static constexpr float kMinDb = -90.0f;
  static constexpr float kMaxDb = 0.0f;
  static constexpr float kMinT = 0.003f;
  static constexpr float kMaxT = 4.00f;
  static constexpr float kRatioOff = 1.0f;
  static constexpr float kRatioInfinity = 60.0f;

  AudioEffectDynamicsF32(void) : AudioStream_F32(1, inputQueueArray) {
    reset();

    gate();
    compression();
    limit();
    autoMakeupGain();

    gatedb = kMinDb;
    compdb = kMinDb;
    limitdb = kMinDb;
  }

  void reset() {
    sumOfSamplesSquared = 0.0;
    sampleIndex = 0;
    std::fill_n(samplesSquared, sampleBufferSize, 0.0f);
  }

  // Sets the gate parameters.
  // threshold is in dbFS
  // attack & release are in seconds
  void gate(
    float threshold = -50.0f,
    float attack = kMinT,
    float release = 0.3f,
    float hysterisis = 6.0f
  ) {
    gateEnabled = threshold > kMinDb;

    gateThresholdOpen = constrain(threshold, kMinDb, kMaxDb);
    gateThresholdClose = gateThresholdOpen - constrain(hysterisis, 0.0f, 6.0f);

    float gateAttackTime = constrain(attack, kMinT, kMaxT);
    float gateReleaseTime = constrain(release, kMinT, kMaxT);

    aGateAttack = timeToAlpha(gateAttackTime);
    aOneMinusGateAttack = 1.0f - aGateAttack;
    aGateRelease = timeToAlpha(gateReleaseTime);
    aOneMinusGateRelease = 1.0f - aGateRelease;
  }

  // Sets the compression parameters.
  // threshold & kneeWidth are in db(FS)
  // attack and release are in seconds
  // ratio is expressed as x:1 i.e. 1 for no compression, 60 for brickwall
  // limiting. Set kneeWidth to 0 for hard knee
  void compression(
    float threshold = -40.0f,
    float attack = 0.05f,
    float release = 0.5f,
    float ratio = 35.0f,
    float kneeWidth = 6.0f
  ) {
    compEnabled = threshold < kMaxDb;

    compThreshold = constrain(threshold, kMinDb, kMaxDb);
    float compAttackTime = constrain(attack, kMinT, kMaxT);
    float compReleaseTime = constrain(release, kMinT, kMaxT);
    compRatio = 1.0f / constrain(fabsf(ratio), kRatioOff, kRatioInfinity);
    float compKneeWidth = constrain(fabsf(kneeWidth), 0.0f, 32.0f);
    computeMakeupGain();

    aCompAttack = timeToAlpha(compAttackTime);
    aOneMinusCompAttack = 1.0f - aCompAttack;
    aCompRelease = timeToAlpha(compReleaseTime);
    aOneMinusCompRelease = 1.0f - aCompRelease;
    aHalfKneeWidth = compKneeWidth / 2.0f;
    aTwoKneeWidth = 1.0f / (compKneeWidth * 2.0f);
    aKneeRatio = compRatio - 1.0f;
    aLowKnee = compThreshold - aHalfKneeWidth;
    aHighKnee = compThreshold + aHalfKneeWidth;
  }

  // Sets the hard limiter parameters
  // threshold is in dbFS
  // attack & release are in seconds
  void limit(float threshold = -3.0f, float attack = kMinT, float release = kMinT) {
    limiterEnabled = threshold < kMaxDb;

    limitThreshold = constrain(threshold, kMinDb, kMaxDb);
    float limitAttackTime = constrain(attack, kMinT, kMaxT);
    float limitReleaseTime = constrain(release, kMinT, kMaxT);

    computeMakeupGain();

    aLimitAttack = timeToAlpha(limitAttackTime);
    aOneMinusLimitAttack = 1.0f - aLimitAttack;
    aLimitRelease = timeToAlpha(limitReleaseTime);
  }

  // Enables automatic makeup gain setting
  // headroom is in dbFS
  void autoMakeupGain(float headroom = 6.0f) {
    mgAutoEnabled = true;
    mgHeadroom = constrain(headroom, 0.0f, 60.0f);
    computeMakeupGain();
  }

  // Sets a fixed makeup gain value.
  // gain is in dbFS
  void makeupGain(float gain = 0.0f) {
    mgAutoEnabled = false;
    makeupdb = constrain(gain, -12.0f, 30.0f);
  }

  const float get_total_gain() const {
    return gatedb + compdb + makeupdb + limitdb;
  }

  virtual void update(void) override {
    audio_block_f32_t* block = receiveWritable_f32(0);

    if (!block) return;

    if (!gateEnabled && !compEnabled && !limiterEnabled) {
      // Transmit & release
      AudioStream_F32::transmit(block);
      AudioStream_F32::release(block);
      return;
    }

    // Samples are normalized floats, so unlike the int16 version there is no
    // 90.3089987 (= 20*log10(32768)) full-scale term here.
    const float db_constant = 3.010299957f * log2f_approx((float)sampleBufferSize);

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      uint32_t nextSampleIndex = sampleIndex + 1;
      if (nextSampleIndex >= sampleBufferSize) nextSampleIndex = 0;

      float sampleSquaredToRemove = samplesSquared[nextSampleIndex];
      sumOfSamplesSquared -= sampleSquaredToRemove;

      float sample = block->data[i];
      float sampleSquared = sample * sample;
      samplesSquared[sampleIndex] = sampleSquared;
      sumOfSamplesSquared += sampleSquared;

      sampleIndex = nextSampleIndex;

      // Compute block RMS level in Db
      float inputdb = kMinDb;
      if (sumOfSamplesSquared > 0.0) {
        inputdb = 3.010299957f * log2f_approx((float)sumOfSamplesSquared) - db_constant;
      }

      // Gate
      if (gateEnabled) {
        if (inputdb >= gateThresholdOpen)
          gatedb = (aGateAttack * gatedb) + (aOneMinusGateAttack * kMaxDb);
        else if (inputdb < gateThresholdClose)
          gatedb = (aGateRelease * gatedb) + (aOneMinusGateRelease * kMinDb);
      } else gatedb = kMaxDb;

      // Compressor
      if (compEnabled) {
        float attdb = kMaxDb; // Below knee
        if (inputdb >= aLowKnee) {
          if (inputdb <= aHighKnee) {
            // Knee transition
            float knee = inputdb - aLowKnee;
            attdb = aKneeRatio * knee * knee * aTwoKneeWidth;
          } else {
            // Above knee
            attdb = compThreshold + ((inputdb - compThreshold) * compRatio) - inputdb;
          }
        }
        if (attdb <= compdb) compdb = (aCompAttack * compdb) + (aOneMinusCompAttack * attdb);
        else compdb = (aCompRelease * compdb) + (aOneMinusCompRelease * attdb);
      } else compdb = kMaxDb;

      // Brickwall Limiter
      if (limiterEnabled) {
        float outdb = inputdb + compdb + makeupdb;
        if (outdb >= limitThreshold) limitdb = (aLimitAttack * limitdb) +
                                               (aOneMinusLimitAttack * (limitThreshold - outdb));
        else limitdb *= aLimitRelease;
      } else limitdb = kMaxDb;

      // Compute linear gain
      float totalGain = gatedb + compdb + makeupdb + limitdb;

      float multiplier = dbToUnit(totalGain);
      // No saturate16: headroom stays float until the edge converter
      block->data[i] = sample * multiplier;
      // Apply gain to block
    }

    // Transmit & release
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

private:
  // number of samples to use for running RMS calulation
  // = 1/20th of a second = 50ms
  static constexpr unsigned int sampleBufferSize = AUDIO_SAMPLE_RATE / 20;

  float samplesSquared[sampleBufferSize] = { 0 };
  double sumOfSamplesSquared = 0.0;
  uint16_t sampleIndex = 0;

  audio_block_f32_t* inputQueueArray[1];

  bool gateEnabled = false;
  bool compEnabled = false;
  bool limiterEnabled = false;
  bool mgAutoEnabled = false;

  float gateThresholdOpen;
  float gateThresholdClose;
  float gatedb;

  float compThreshold;
  float compRatio;
  float compdb;

  float limitThreshold;
  float limitdb;

  float mgHeadroom;
  float makeupdb;

  float aGateAttack;
  float aOneMinusGateAttack;
  float aGateRelease;
  float aOneMinusGateRelease;
  float aHalfKneeWidth;
  float aTwoKneeWidth;
  float aKneeRatio;
  float aLowKnee;
  float aHighKnee;
  float aCompAttack;
  float aOneMinusCompAttack;
  float aCompRelease;
  float aOneMinusCompRelease;
  float aLimitAttack;
  float aOneMinusLimitAttack;
  float aLimitRelease;

  void computeMakeupGain() {
    if (mgAutoEnabled) {
      // I'm deciding not to include the Limiter threshold in the makeup gain.
      // It can serve as a ceiling. -djphazer
      makeupdb = -compThreshold + (compThreshold * compRatio) - mgHeadroom; // - limitThreshold;
    }
  }

  // Computes smoothing time constants for a 10% to 90% change
  float timeToAlpha(float time) {
    return expf(-0.9542f / (((float)AUDIO_SAMPLE_RATE_EXACT / (float)AUDIO_BLOCK_SAMPLES) * time));
  }

  // Fast log2 approximation, same 3rd-order polynomial as effect_dynamics.cpp
  // (accurate to ~8e-3 dB when computing db20). static: the int16 original
  // exports a non-inline global of the same shape from its .cpp.
  static float log2f_approx(float X) {
    static const float coeff[4] = {
      1.23149591368684f, -4.11852516267426f, 6.02197014179219f, -3.13396450166353f
    };
    float Y;
    float F;
    int E;

    // This is the approximation to log2()
    // F = frexpf(fabsf(X), &E);
    // Optimized frexp for positive X
    union { float f; uint32_t i; } u = { X };
    E = ((u.i >> 23) & 0xff) - 126;
    u.i &= 0x7fffff;
    u.i |= 0x3f000000;
    F = u.f;

    const float* C = &coeff[0];
    Y = *C++;
    Y *= F;
    Y += (*C++);
    Y *= F;
    Y += (*C++);
    Y *= F;
    Y += (*C++);
    Y += E;
    return (Y);
  }

  // https://codingforspeed.com/using-faster-exponential-approximation/
  static float expf_approx(float x) {
    x = 1.0f + x / 1024;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x;
    return x;
  }

  static float dbToUnit(float db) {
    return expf_approx(db * 2.302585092994046f * 0.05f);
  }
};
