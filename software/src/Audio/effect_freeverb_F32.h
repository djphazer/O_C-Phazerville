#pragma once

// Float32 port of the Teensy Audio Library's fixed-point Freeverb
// (effect_freeverb.{h,cpp}, Paul Stoffregen / Jezar at Dreampoint, MIT).
// Same comb/allpass tunings and the same gain staging as the int16 original
// (input * 8738/131072, comb sum * 31457/131072, output * 30), but the comb
// feedback and damping state recirculate in float32, so the reverb tail never
// truncates or saturates to int16 between passes. The loops are rolled up
// (arrays instead of 8x unrolled member pairs); the arithmetic per sample is
// identical modulo the removed sat16() clamps.

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioEffectFreeverbF32 : public AudioStream_F32 {
public:
  AudioEffectFreeverbF32() : AudioStream_F32(1, inputQueueArray) {
    size_t offset = 0;
    for (int i = 0; i < COMB_COUNT; i++) {
      combbuf[i] = combstore + offset;
      offset += comblen[i];
    }
    offset = 0;
    for (int i = 0; i < ALLPASS_COUNT; i++) {
      allpassbuf[i] = allpassstore + offset;
      offset += allpasslen[i];
    }
    memset(combstore, 0, sizeof(combstore));
    memset(allpassstore, 0, sizeof(allpassstore));
  }

  // same mapping as int16: combfeeback = n * 9175.04/32768 + 22937/32768
  void roomsize(float n) {
    if (n > 1.0f) n = 1.0f;
    else if (n < 0.0f) n = 0.0f;
    combfeedback = n * (9175.04f / 32768.0f) + (22937.0f / 32768.0f);
  }

  // same mapping as int16: damp1 = n * 13107.2/32768 = n * 0.4
  void damping(float n) {
    if (n > 1.0f) n = 1.0f;
    else if (n < 0.0f) n = 0.0f;
    float x1 = n * (13107.2f / 32768.0f);
    __disable_irq();
    combdamp1 = x1;
    combdamp2 = 1.0f - x1;
    __enable_irq();
  }

  virtual void update(void) override {
    audio_block_f32_t* outblock = AudioStream_F32::allocate_f32();
    if (!outblock) {
      audio_block_f32_t* tmp = receiveReadOnly_f32(0);
      if (tmp) AudioStream_F32::release(tmp);
      return;
    }
    // like the int16 version's zeroblock: keep the tail ringing with no input
    audio_block_f32_t* block = receiveReadOnly_f32(0);

    const float damp1 = combdamp1;
    const float damp2 = combdamp2;
    const float feedback = combfeedback;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      // 8738/131072 input attenuation for numerical headroom, as in int16
      float input =
        (block ? block->data[i] : 0.0f) * (8738.0f / 131072.0f);
      float sum = 0.0f;

      for (int c = 0; c < COMB_COUNT; c++) {
        float bufout = combbuf[c][combindex[c]];
        sum += bufout;
        combfilter[c] = bufout * damp2 + combfilter[c] * damp1;
        combbuf[c][combindex[c]] = input + combfilter[c] * feedback;
        if (++combindex[c] >= comblen[c]) combindex[c] = 0;
      }

      float output = sum * (31457.0f / 131072.0f);

      for (int a = 0; a < ALLPASS_COUNT; a++) {
        float bufout = allpassbuf[a][allpassindex[a]];
        allpassbuf[a][allpassindex[a]] = output + bufout * 0.5f;
        output = (bufout - output) * 0.5f;
        if (++allpassindex[a] >= allpasslen[a]) allpassindex[a] = 0;
      }

      // x30 makeup gain as in int16, but no hard sat16 clamp here: headroom
      // stays float until the applet's output edge converter
      outblock->data[i] = output * 30.0f;
    }

    AudioStream_F32::transmit(outblock);
    AudioStream_F32::release(outblock);
    if (block) AudioStream_F32::release(block);
  }

private:
  static constexpr int COMB_COUNT = 8;
  static constexpr uint16_t comblen[COMB_COUNT] = {
    1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617
  };
  static constexpr int COMB_TOTAL =
    1116 + 1188 + 1277 + 1356 + 1422 + 1491 + 1557 + 1617;

  static constexpr int ALLPASS_COUNT = 4;
  static constexpr uint16_t allpasslen[ALLPASS_COUNT] = { 556, 441, 341, 225 };
  static constexpr int ALLPASS_TOTAL = 556 + 441 + 341 + 225;

  audio_block_f32_t* inputQueueArray[1];

  // int16 constructor defaults: combdamp1=6553, combdamp2=26215, combfeeback=27524
  volatile float combdamp1 = 6553.0f / 32768.0f;
  volatile float combdamp2 = 26215.0f / 32768.0f;
  volatile float combfeedback = 27524.0f / 32768.0f;

  float combstore[COMB_TOTAL];
  float allpassstore[ALLPASS_TOTAL];
  float* combbuf[COMB_COUNT];
  float* allpassbuf[ALLPASS_COUNT];
  uint16_t combindex[COMB_COUNT] = { 0 };
  uint16_t allpassindex[ALLPASS_COUNT] = { 0 };
  float combfilter[COMB_COUNT] = { 0 };
};
