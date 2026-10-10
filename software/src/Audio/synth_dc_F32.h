#pragma once

// Float32 DC source, matching the immediate-amplitude behavior of the Teensy
// Audio Library's AudioSynthWaveformDc (synth_dc.h): amplitude clamps to
// +-1.0 and full scale emits 2147418112/2^31 (= 0x7FFE in int16 terms), so
// F32 wavefolder drive levels line up with the int16 original. The slow-ramp
// amplitude(n, ms) overload is not ported (unused here).

#include <Arduino.h>
#include <arm_math.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioSynthWaveformDcF32 : public AudioStream_F32 {
public:
  AudioSynthWaveformDcF32() : AudioStream_F32(0, nullptr) {}

  // immediately jump to the new DC level
  void amplitude(float n) {
    if (n > 1.0f) n = 1.0f;
    else if (n < -1.0f) n = -1.0f;
    magnitude = n * (2147418112.0f / 2147483648.0f);
  }

  float read(void) {
    return magnitude;
  }

  virtual void update(void) override {
    audio_block_f32_t* block = AudioStream_F32::allocate_f32();
    if (!block) return;
    arm_fill_f32(magnitude, block->data, AUDIO_BLOCK_SAMPLES);
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

private:
  volatile float magnitude = 0.0f;
};
