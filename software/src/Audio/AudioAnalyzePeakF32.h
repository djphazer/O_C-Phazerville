#pragma once

// Float32 port of the Teensy Audio Library peak analyzer (analyze_peak.h,
// Paul Stoffregen / PJRC, MIT-style license as in the original headers).
// Same semantics: update() widens a running min/max, available() reports and
// clears the new-data flag, read() returns the absolute peak (float full
// scale 1.0 == int16 full scale) and resets the window.

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioAnalyzePeakF32 : public AudioStream_F32 {
public:
  AudioAnalyzePeakF32() : AudioStream_F32(1, inputQueueArray) {}

  bool available(void) {
    __disable_irq();
    bool flag = new_output;
    if (flag) new_output = false;
    __enable_irq();
    return flag;
  }

  float read(void) {
    __disable_irq();
    float min = min_sample;
    float max = max_sample;
    min_sample = 1.0f;
    max_sample = -1.0f;
    __enable_irq();
    min = fabsf(min);
    max = fabsf(max);
    if (min > max) max = min;
    return max;
  }

  float readPeakToPeak(void) {
    __disable_irq();
    float min = min_sample;
    float max = max_sample;
    min_sample = 1.0f;
    max_sample = -1.0f;
    __enable_irq();
    return max - min;
  }

  virtual void update(void) override {
    audio_block_f32_t* block = receiveReadOnly_f32();
    if (!block) return;
    float min = min_sample;
    float max = max_sample;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      float d = block->data[i];
      if (d < min) min = d;
      if (d > max) max = d;
    }
    AudioStream_F32::release(block);
    min_sample = min;
    max_sample = max;
    new_output = true;
  }

private:
  audio_block_f32_t* inputQueueArray[1];
  volatile bool new_output = false;
  float min_sample = 1.0f;
  float max_sample = -1.0f;
};
