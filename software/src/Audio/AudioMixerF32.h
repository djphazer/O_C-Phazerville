#pragma once

// Float32 port of the N-channel AudioMixer template (AudioMixer.h). The int16
// original already mixed in float internally (for sane saturation behavior
// with phase-correlated material) and only quantized at its edges; this
// version keeps the blocks float32 throughout. Same semantics: missing inputs
// are silent, gains default to 0, and nothing is transmitted when no input
// block arrived this update.

#include <Arduino.h>
#include <arm_math.h>
#include <array>
#include "../extern/f32/AudioStream_F32.h"

template <size_t NumChannels>
class AudioMixerF32 : public AudioStream_F32 {
public:
  AudioMixerF32() : AudioStream_F32(NumChannels, inputQueueArray) {}

  void gain(size_t channel, float gain) {
    if (channel >= NumChannels) return;
    gains[channel] = gain;
  }

  virtual void update(void) override {
    float acc[AUDIO_BLOCK_SAMPLES];
    bool acc_filled = false;

    for (size_t channel = 0; channel < NumChannels; channel++) {
      audio_block_f32_t* in = receiveReadOnly_f32(channel);
      if (!in) continue;
      if (!acc_filled) {
        arm_fill_f32(0.0f, acc, AUDIO_BLOCK_SAMPLES);
        acc_filled = true;
      }
      float g = gains[channel];
      if (g == 1.0f) {
        arm_add_f32(in->data, acc, acc, AUDIO_BLOCK_SAMPLES);
      } else if (g != 0.0f) {
        float scaled[AUDIO_BLOCK_SAMPLES];
        arm_scale_f32(in->data, g, scaled, AUDIO_BLOCK_SAMPLES);
        arm_add_f32(scaled, acc, acc, AUDIO_BLOCK_SAMPLES);
      }
      AudioStream_F32::release(in);
    }

    if (acc_filled) {
      audio_block_f32_t* out = AudioStream_F32::allocate_f32();
      if (out) {
        arm_copy_f32(acc, out->data, AUDIO_BLOCK_SAMPLES);
        AudioStream_F32::transmit(out);
        AudioStream_F32::release(out);
      }
    }
  }

private:
  audio_block_f32_t* inputQueueArray[NumChannels];
  std::array<float, NumChannels> gains = {0.0f};
};
