#pragma once

// Float32 port of AudioVCA (AudioVCA.h). The int16 original already computed
// the gain curve in float and only quantized at its edges; here the signal
// and CV stay float32 end to end, so the VCA neither dequantizes its input
// nor requantizes its output. Same semantics: out = in * (bias + level * cv),
// optionally rectified (negative gains clamped to zero).

#include <Arduino.h>
#include <arm_math.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioVCA_F32 : public AudioStream_F32 {
public:
  AudioVCA_F32() : AudioStream_F32(2, inputQueueArray) {}

  void level(float gain) {
    level_ = gain;
  }

  void bias(float gain) {
    bias_ = gain;
  }

  void rectify(bool r) {
    rectify_ = r;
  }

  virtual void update(void) override {
    audio_block_f32_t* signal = receiveWritable_f32(0);
    if (signal == nullptr) return;

    float mod[AUDIO_BLOCK_SAMPLES];
    arm_fill_f32(bias_, mod, AUDIO_BLOCK_SAMPLES);
    if (level_ != 0.0f) {
      audio_block_f32_t* cv = receiveReadOnly_f32(1);
      if (cv) {
        float cv_scaled[AUDIO_BLOCK_SAMPLES];
        arm_scale_f32(cv->data, level_, cv_scaled, AUDIO_BLOCK_SAMPLES);
        AudioStream_F32::release(cv);
        arm_add_f32(cv_scaled, mod, mod, AUDIO_BLOCK_SAMPLES);
      }
    }
    if (rectify_) {
      for (float& x : mod) {
        if (x < 0.0f) x = 0.0f;
      }
    }

    arm_mult_f32(mod, signal->data, signal->data, AUDIO_BLOCK_SAMPLES);
    AudioStream_F32::transmit(signal);
    AudioStream_F32::release(signal);
  }

private:
  audio_block_f32_t* inputQueueArray[2];
  float level_ = 1.0f;
  float bias_ = 0.0f;
  bool rectify_ = false;
};
