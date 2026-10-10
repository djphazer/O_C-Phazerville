#pragma once

// Float32 port of the Teensy Audio Library wavefolder (effect_wavefolder.cpp,
// Mark Tillotson, MIT). Input 0 is the signal, input 1 the fold drive. The
// product is scaled up to 16x so it can fold up to 16 times in each polarity,
// then triangle-folded back into +-1.0 -- the same transfer curve as the
// int16 bit-twiddling version (~s1 band flipping + 16-bit wrap), computed in
// float so the fold never quantizes.

#include <Arduino.h>
#include <math.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioEffectWaveFolderF32 : public AudioStream_F32 {
public:
  AudioEffectWaveFolderF32() : AudioStream_F32(2, inputQueueArray) {}

  virtual void update(void) override {
    audio_block_f32_t* blocka = receiveWritable_f32(0);
    if (!blocka) return;
    audio_block_f32_t* blockb = receiveReadOnly_f32(1);
    if (!blockb) {
      AudioStream_F32::release(blocka);
      return;
    }
    float* pa = blocka->data;
    const float* pb = blockb->data;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      // scale up to 16 times input, as in the int16 version's
      // s1 = (a12 * b12 + 0x400) >> 11
      float p = 16.0f * pa[i] * pb[i];
      // triangle fold, period 4: [-1,1] passes through, 1..3 reflects, etc.
      float g = (p + 1.0f) * 0.25f;
      pa[i] = 1.0f - 4.0f * fabsf(g - floorf(g) - 0.5f);
    }
    AudioStream_F32::transmit(blocka);
    AudioStream_F32::release(blocka);
    AudioStream_F32::release(blockb);
  }

private:
  audio_block_f32_t* inputQueueArray[2];
};
