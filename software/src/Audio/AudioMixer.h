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
    int8_t shift = 0;
    while (gain < -1.0f || gain > 0.999969f) {
      gain /= 2.0f;
      shift++;
    }
    scale_fracts[channel] = float_to_q15(gain);
    shifts[channel] = shift;
  }

  virtual void update(void) {
    audio_block_t* out = NULL;
    q15_t aux[AUDIO_BLOCK_SAMPLES];

    for (size_t channel = 0; channel < NumChannels; channel++) {
      q15_t scalar = scale_fracts[channel];
      int8_t shift = shifts[channel];
      auto* in = receiveReadOnly(channel);
      if (in) {
        if (!out) {
          out = allocate();
          if (!out) {
            release(in);
            return;
          }
          arm_fill_q15(0, out->data, AUDIO_BLOCK_SAMPLES);
        }
        if (scalar != 0) {
          // Don't scale for unity gain; that's represented by 0.5 in q15 with a
          // shift of 1
          if (scalar != 16384 || shift != 1) {
            arm_scale_q15(in->data, scalar, shift, aux, AUDIO_BLOCK_SAMPLES);
            arm_add_q15(aux, out->data, out->data, AUDIO_BLOCK_SAMPLES);
          } else {
            arm_add_q15(in->data, out->data, out->data, AUDIO_BLOCK_SAMPLES);
          }
        }
        release(in);
      }
    }

    if (out != NULL) {
      transmit(out);
      release(out);
    }
  }

private:
  audio_block_t* inputQueueArray[NumChannels];
  std::array<q15_t, NumChannels> scale_fracts;
  std::array<int8_t, NumChannels> shifts;
};
