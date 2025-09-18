#pragma once

#include "AudioParam.h"
#include "arm_math.h"
#include "dsputils.h"
#include <Audio.h>

template <size_t NumChannels>
class AudioMixer : public AudioStream {
public:
  AudioMixer() : AudioStream(NumChannels, inputQueueArray) {}

  void gain(size_t channel, float gain) {
    if (channel >= NumChannels) return;
    gains[channel] = gain;
  }

  virtual void update(void) {
    float aux[AUDIO_BLOCK_SAMPLES];
    float out[AUDIO_BLOCK_SAMPLES];
    bool out_filled = false;

    // Originally I had this scaling and mixing all with q15 but that has much
    // less desirable saturation behavior with phase correlated material.
    for (size_t channel = 0; channel < NumChannels; channel++) {
      auto* in = receiveReadOnly(channel);
      if (in) {
        float gain = gains[channel];
        if (!out_filled) {
          arm_fill_f32(0.0f, out, AUDIO_BLOCK_SAMPLES);
          out_filled = true;
        }
        if (gain != 0.0f) {
          arm_q15_to_float(in->data, aux, AUDIO_BLOCK_SAMPLES);
          if (gain != 1.0f) {
            arm_scale_f32(aux, gain, aux, AUDIO_BLOCK_SAMPLES);
          }
          arm_add_f32(aux, out, out, AUDIO_BLOCK_SAMPLES);
        }
        release(in);
      }
    }

    if (out_filled) {
      audio_block_t* out_block = allocate();
      arm_float_to_q15(out, out_block->data, AUDIO_BLOCK_SAMPLES);
      transmit(out_block);
      release(out_block);
    }
  }

private:
  audio_block_t* inputQueueArray[NumChannels];
  std::array<float, NumChannels> gains = {0.0f};
};
