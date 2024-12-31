#pragma once

#include <Audio.h>
#include <cfloat>

class AudioVCA : public AudioStream {
public:
  AudioVCA() : AudioStream(2, inputQueueArray) {}
  void level(float gain) {
    level_ = gain;
  }

  void bias(float gain) {
    bias_ = gain;
  }

  void rectify(float r) {
    rectify_ = r;
  }

  virtual void update(void) {
    auto* signal = receiveReadOnly(0);
    if (signal == NULL) return;
    float signal_f32[AUDIO_BLOCK_SAMPLES];
    arm_q15_to_float(signal->data, signal_f32, AUDIO_BLOCK_SAMPLES);
    release(signal);

    float mod[AUDIO_BLOCK_SAMPLES];
    arm_fill_f32(bias_, mod, AUDIO_BLOCK_SAMPLES);
    if (level_ != 0.0f) {
      auto* cv = receiveReadOnly(1);
      if (cv) {
        float cv_f32[AUDIO_BLOCK_SAMPLES];
        arm_q15_to_float(cv->data, cv_f32, AUDIO_BLOCK_SAMPLES);
        release(cv);
        arm_scale_f32(cv_f32, level_, cv_f32, AUDIO_BLOCK_SAMPLES);
        arm_add_f32(cv_f32, mod, mod, AUDIO_BLOCK_SAMPLES);
      }
    }
    if (rectify_) {
      // Alas, no generalized arm_clip_f32(). Hopefully the compiler can vectorize this...
      for (float& x : mod) {
        if (x < 0.0f) x = 0.0f;
      }
    }

    arm_mult_f32(mod, signal_f32, signal_f32, AUDIO_BLOCK_SAMPLES);
    auto* out = allocate();
    arm_float_to_q15(signal_f32, out->data, AUDIO_BLOCK_SAMPLES);
    transmit(out);
    release(out);
  }

private:
  audio_block_t* inputQueueArray[2];
  float level_ = 0.0f;
  float bias_ = 0.0f;
  bool rectify_ = false;
};

