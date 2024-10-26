#pragma once
#include <Audio.h>

template <uint8_t NumChannels>
class AudioPassthrough : public AudioStream
{
public:
  AudioPassthrough() : AudioStream(NumChannels, inputQueueArray) {}
  virtual void update(void) {
    audio_block_t *block;
    for(int i=0; i<NumChannels; i++) {
      block = receiveReadOnly(i);
      if(block) {
        transmit(block, i);
        release(block);
      }
    }
  }
private:
  audio_block_t *inputQueueArray[NumChannels];
};

