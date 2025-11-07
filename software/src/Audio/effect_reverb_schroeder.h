#pragma once
#include <AudioStream.h>
#include <arm_math.h>   // Teensy core
#include <imxrt.h>

// ---- Schroeder/Moorer Reverb (wet only) ----
// Parallel comb filters -> series allpass filters
// Tunable decay time (RT60) in seconds

class AudioEffectReverbSchroeder : public AudioStream {
public:
  AudioEffectReverbSchroeder() : AudioStream(1, inputQueueArray) {
    setDecayTime(2.5f);   // default ~2.5s
    setDamping(0.5f);     // 0 = bright, 1 = dark
    reset();
  }

  // Set decay time in seconds (approximate RT60)
  void setDecayTime(float seconds) {
    if (seconds < 0.1f) seconds = 0.1f;
    if (seconds > 20.0f) seconds = 20.0f;

    float avgDelay = 0.0f;
    for (int i = 0; i < COMB_COUNT; i++) avgDelay += combLen[i];
    avgDelay /= COMB_COUNT;

    float delaySec = avgDelay / 44100.0f;
    float g = expf((-3.0f * delaySec) / seconds); // feedback coefficient
    if (g > 0.9999f) g = 0.9999f;

    __disable_irq();
    decayFeedback = g;
    __enable_irq();
  }

  // 0..1 where 1 = strong high-frequency damping
  void setDamping(float d) {
    if (d < 0.0f) d = 0.0f;
    if (d > 0.99f) d = 0.99f;
    __disable_irq();
    damp1 = d;
    damp2 = 1.0f - d;
    __enable_irq();
  }

  void reset() {
    __disable_irq();
    for (int i = 0; i < COMB_COUNT; ++i) {
      combIdx[i] = 0;
      combStore[i] = 0.0f;
      memset(combBuf[i], 0, sizeof(combBuf[i]));
    }
    for (int i = 0; i < ALLPASS_COUNT; ++i) {
      apIdx[i] = 0;
      memset(apBuf[i], 0, sizeof(apBuf[i]));
    }
    __enable_irq();
  }

  virtual void update(void) override {
    audio_block_t *in = receiveReadOnly(0);
    if (!in) return;

    audio_block_t *out = allocate();
    if (!out) {
      release(in);
      return;
    }

    float localDamp1 = damp1;
    float localDamp2 = damp2;
    float feedback   = decayFeedback;

    for (int n = 0; n < AUDIO_BLOCK_SAMPLES; ++n) {
      float x = in->data[n] * (1.0f / 32768.0f);

      // ---- Parallel combs ----
      float combSum = 0.0f;
      for (int i = 0; i < COMB_COUNT; ++i) {
        if (combIdx[i] < 0) combIdx[i] += combLen[i];
        if (combIdx[i] >= combLen[i]) combIdx[i] -= combLen[i];

        float y = combBuf[i][combIdx[i]];
        combStore[i] = (combStore[i] * localDamp2) + (y * localDamp1);
        combBuf[i][combIdx[i]] = x + feedback * combStore[i];
        combIdx[i]++; if (combIdx[i] >= combLen[i]) combIdx[i] = 0;
        combSum += y;
      }

      combSum *= (1.0f / COMB_COUNT);

      // ---- Series allpasses ----
      float apOut = combSum;
      for (int i = 0; i < ALLPASS_COUNT; ++i) {
        float bufOut = apBuf[i][apIdx[i]];
        float z = apOut + (-AP_GAIN * bufOut);
        apBuf[i][apIdx[i]] = z;
        apIdx[i]++; if (apIdx[i] >= apLen[i]) apIdx[i] = 0;
        apOut = bufOut + z * AP_GAIN;
      }

      float y = apOut * 0.6f; // wet only

      int32_t s = (int32_t)(y * 32767.0f);
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      out->data[n] = (int16_t)s;
    }

    transmit(out);
    release(out);
    release(in);
  }

private:
  audio_block_t *inputQueueArray[1];

  // Tunables
  volatile float decayFeedback = 0.85f; // per-comb feedback
  volatile float damp1 = 0.5f, damp2 = 0.5f;

  // Delay line layout
  static constexpr int sr = 44100;
  static constexpr int COMB_COUNT = 8;
  static constexpr int combLenConst[COMB_COUNT] = {
    1319,  // ~29.9 ms
    1493,  // ~33.9 ms
    1559,  // ~35.3 ms
    1613,  // ~36.6 ms
    1747,  // ~39.6 ms
    1873,  // ~42.5 ms
    2017,
    2153,
  };

  static constexpr int ALLPASS_COUNT = 4;
  static constexpr int apLenConst[ALLPASS_COUNT] = {
    int(0.0050f * sr + 0.5f),  // ~5 ms
    int(0.0017f * sr + 0.5f),   // ~1.7 ms
    int(0.0083f * sr + 0.5f),  // ~8.3 ms
    int(0.0126f * sr + 0.5f)   // ~12.6 ms
  };
  static constexpr float AP_GAIN = 0.5f;

  int combLen[COMB_COUNT] = {
    combLenConst[0], combLenConst[1], combLenConst[2], combLenConst[3],
    combLenConst[4], combLenConst[5], combLenConst[6], combLenConst[7],
  };
  int apLen[ALLPASS_COUNT] = { apLenConst[0], apLenConst[1], apLenConst[2], apLenConst[3] };

  // The maximum buffer size must fit the largest delay
  static constexpr int COMB_MAX = combLenConst[COMB_COUNT - 1];
  static constexpr int AP_MAX   = apLenConst[ALLPASS_COUNT - 1];

  float combBuf[COMB_COUNT][COMB_MAX] __attribute__((aligned(4)));
  float apBuf[ALLPASS_COUNT][AP_MAX]  __attribute__((aligned(4)));

  volatile uint16_t combIdx[COMB_COUNT] = {0};
  volatile uint16_t apIdx[ALLPASS_COUNT] = {0};

  float combStore[COMB_COUNT] = {0};
};
