#pragma once

#include "AudioParam.h"
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
    std::array<float, AUDIO_BLOCK_SAMPLES> mixed;
    bool got_input = false;

    for (size_t channel = 0; channel < NumChannels; channel++) {
      float g = gains[channel];
      if (g != 0.0f) {
        auto* in = receiveReadOnly(channel);
        if (in != NULL) {
          // The array needs to be 0 initialized, but doing it even if there's
          // not input incurs a bigger cpu hit than this conditional.
          if (!got_input) mixed = {0};
          got_input = true;
          for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            mixed[i] += g * in->data[i];
          }
          release(in);
        }
      }
    }

    if (got_input) {
      audio_block_t* out = allocate();
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        out->data[i] = Clip16(mixed[i]);
      }
      transmit(out);
      release(out);
    }
  }

private:
  audio_block_t* inputQueueArray[NumChannels];
  std::array<float, NumChannels> gains;
};
