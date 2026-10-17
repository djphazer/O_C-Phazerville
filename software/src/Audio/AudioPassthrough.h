#pragma once
#include <Audio.h>
#include "../extern/f32/AudioStream_F32.h"
#include "../extern/f32/AudioConvert_F32.h"

template <uint8_t NumChannels>
class AudioPassthrough : public AudioStream {
public:
  AudioPassthrough() : AudioStream(NumChannels, inputQueueArray) {}
  virtual void update(void) {
    audio_block_t* block;
    for (int i = 0; i < NumChannels; i++) {
      block = receiveReadOnly(i);
      if (block) {
        transmit(block, i);
        release(block);
      }
    }
  }

private:
  audio_block_t* inputQueueArray[NumChannels];
};

// --- Float32 adapters below ---

// N-channel int16 -> float32 edge adapter (one object, N lanes)
template <uint8_t N>
class AudioConvertI16toF32Multi : public AudioStream_F32 {
public:
  AudioConvertI16toF32Multi() : AudioStream_F32(0, nullptr, N, iq) {}
  void update(void) override {
    for (uint8_t i = 0; i < N; i++) {
      audio_block_t* in = AudioStream::receiveReadOnly(i);
      if (!in) continue;
      audio_block_f32_t* out = AudioStream_F32::allocate_f32();
      if (out) {
        AudioConvert_I16toF32::convertAudio_I16toF32(in, out, out->length);
        AudioStream_F32::transmit(out, i);
        AudioStream_F32::release(out);
      }
      AudioStream::release(in);
    }
  }
private:
  audio_block_t* iq[N];
};

// N-channel float32 -> int16 edge adapter
template <uint8_t N>
class AudioConvertF32toI16Multi : public AudioStream_F32 {
public:
  AudioConvertF32toI16Multi() : AudioStream_F32(N, fq) {}
  void update(void) override {
    for (uint8_t i = 0; i < N; i++) {
      audio_block_f32_t* in = receiveReadOnly_f32(i);
      if (!in) continue;
      audio_block_t* out = AudioStream::allocate();
      if (out) {
        AudioConvert_F32toI16::convertAudio_F32toI16(in, out, in->length);
        AudioStream::transmit(out, i);
        AudioStream::release(out);
      }
      AudioStream_F32::release(in);
    }
  }
private:
  audio_block_f32_t* fq[N];
};

template <uint8_t NumChannels>
class AudioPassthroughF32 : public AudioStream_F32 {
public:
  AudioPassthroughF32() : AudioStream_F32(NumChannels, inputQueueArray) {}
  virtual void update(void) {
    audio_block_f32_t* block;
    for (int i = 0; i < NumChannels; i++) {
      block = receiveReadOnly_f32(i);
      if (block) {
        transmit(block, i);
        release(block);
      }
    }
  }

private:
  audio_block_f32_t* inputQueueArray[NumChannels];
};
