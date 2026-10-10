#pragma once

// Float32 port of the Teensy Audio Library white noise source
// (synth_whitenoise.{h,cpp}, Paul Stoffregen / PJRC, MIT-style license as in
// the original headers). The Park-Miller-Carta PRNG state advance is kept
// bit-exact with the int16 original (same seed-per-instance scheme, same
// sample sequence); only the output scaling is float, taking the low 16 bits
// as a signed sample at full scale +-1.0 instead of the fixed-point
// gain-multiply requantization.

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioSynthNoiseWhiteF32 : public AudioStream_F32 {
public:
  AudioSynthNoiseWhiteF32() : AudioStream_F32(0, nullptr) {
    seed = 1 + instance_count()++;
  }

  void amplitude(float n) {
    if (n < 0.0f) n = 0.0f;
    else if (n > 1.0f) n = 1.0f;
    level = n;
  }

  virtual void update(void) override {
    if (level == 0.0f) return;
    audio_block_f32_t* block = AudioStream_F32::allocate_f32();
    if (!block) return;
    const float scale = level * (1.0f / 32768.0f);
    uint32_t lo = seed;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      // Park-Miller-Carta, http://www.firstpr.com.au/dsp/rand31/
      uint32_t hi = 16807 * (lo >> 16);
      lo = 16807 * (lo & 0xFFFF);
      lo += (hi & 0x7FFF) << 16;
      lo += hi >> 15;
      lo = (lo & 0x7FFFFFFF) + (lo >> 31);
      block->data[i] =
        static_cast<float>(static_cast<int16_t>(lo & 0xFFFF)) * scale;
    }
    seed = lo;
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

private:
  static uint16_t& instance_count() {
    static uint16_t count = 0;
    return count;
  }
  uint32_t seed; // must start at 1
  float level = 0.0f;
};
