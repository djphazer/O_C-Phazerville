#pragma once

#include <Audio.h>
#include <smalloc.h>

template <typename T>
constexpr inline T InterpHermite(T xm1, T x0, T x1, T x2, float t) {
  // https://github.com/pichenettes/stmlib/blob/d18def816c51d1da0c108236928b2bbd25c17481/dsp/dsp.h#L52
  const float c = (x1 - xm1) * 0.5f;
  const float v = x0 - x1;
  const float w = c + v;
  const float a = w + v + (x2 - x0) * 0.5f;
  const float b_neg = w + a;
  return ((((a * t) - b_neg) * t + c) * t + x0);
}

// TODO: this should be backed by an ordinary ring buffer. I don't think the
// std::copy helps performance much, so extracting and simplifying the ring
// logic would both make this easier to work with and simplify things.
template <size_t NumSamples, typename T = int16_t> class AudioBuffer {
public:
  /**
   * Make sure buffer has NumSamples elements and that it is 0 filled!
   **/
  AudioBuffer(T *buffer) : buffer(buffer) {}

  void WriteSample(const T sample) {
    buffer[write_ix++] = sample;
    if (write_ix >= NumSamples)
      write_ix = 0;
  }

  T ReadSample(size_t samples_back) {
    int read_ix = (NumSamples + write_ix - samples_back) % NumSamples;
    return buffer[read_ix];
  }

  void Write(const audio_block_t *block) { Write(block->data); }

  template <size_t N = AUDIO_BLOCK_SAMPLES> void Write(const T (&data)[N]) {
    size_t copied = 0;
    // Just in case we're writing more data than we have, loop here instead of
    // just if... We could precalculate exactly what would be written in that
    // case but meh.
    while (N - copied + write_ix > NumSamples) {
      std::copy_n(data + copied, NumSamples - write_ix, buffer + write_ix);
      copied += NumSamples - write_ix;
      write_ix = 0;
    }
    std::copy_n(data + copied, N - copied, buffer + write_ix);
    write_ix += N - copied;
    // Above code takes care of wrapping, so no need to do it here.
  }

  template <size_t N = AUDIO_BLOCK_SAMPLES>
  void ReadFromSecondsAgo(float seconds_ago, T (&data)[N]) {
    ReadFromSamplesAgo(static_cast<size_t>(seconds_ago * AUDIO_SAMPLE_RATE),
                       data);
  }

  void ReadFromSecondsAgo(float seconds_ago, audio_block_t *block) {
    ReadFromSamplesAgo(static_cast<size_t>(seconds_ago * AUDIO_SAMPLE_RATE),
                       block);
  }

  void ReadFromSamplesAgo(size_t samples_back, audio_block_t *block) {
    ReadFromSamplesAgo(samples_back, block->data);
  }

  template <size_t N = AUDIO_BLOCK_SAMPLES>
  void ReadFromSamplesAgo(size_t samples_back, T (&data)[N]) {
    // TODO: This can read past the write head. It should stop reading at the
    // write head instead and return the number of samples read.
    size_t read = 0;
    size_t read_ix = (write_ix - samples_back + NumSamples) % NumSamples;
    while (N - read + read_ix > NumSamples) {
      std::copy_n(buffer + read_ix, NumSamples - read_ix, data + read);
      // memcpy(data + read, ext_buffer + read_ix,
      //        (NumSamples - read_ix) * sizeof(int16_t));
      read += NumSamples - read_ix;
      read_ix = 0;
    }
    // memcpy(data + read, ext_buffer + read_ix, (N - read) * sizeof(int16_t));
    std::copy_n(buffer + read_ix, N - read, data + read);
  }

  // TODO:: This is prett innefficient for reading sequences. We should read
  // based on an array of ts instead, reducing dup reads and calcs
  int16_t ReadInterp(float seconds_ago) {
    // int16_t x[4];
    float samples_back = seconds_ago * AUDIO_SAMPLE_RATE;
    size_t samples_back_int = static_cast<size_t>(samples_back);
    // Need point from before target point and we're between samples_back_int
    // and +1, so + 2
    float t = 1.0f - (samples_back - samples_back_int);
    // Faster to read individual samples than copy block of 4
    return InterpHermite(
        ReadSample(samples_back_int + 2), ReadSample(samples_back_int + 1),
        ReadSample(samples_back_int), ReadSample(samples_back_int - 1), t);
  }

protected:
  T *buffer; // = (T *)extmem_calloc(NumSamples, sizeof(int16_t));
  size_t write_ix = 0;
};

template <size_t NumSamples, typename T = int16_t>
class ExtAudioBuffer : public AudioBuffer<NumSamples> {
public:
  // TODO: Might need to 0 data; not sure if better init or uninit though:
  // - extmem_calloc assumes the data is already 0-filled
  // https://github.com/PaulStoffregen/cores/blob/58224e5554d0cda93f92c52078a500a0d898a996/teensy4/extmem.c#L38
  // - The extmem pool isn't 0-filled on uninit
  // https://github.com/PaulStoffregen/cores/blob/master/teensy4/startup.c#L500
  ExtAudioBuffer()
      : AudioBuffer<NumSamples>((T *)extmem_calloc(NumSamples, sizeof(T))) {}
  ~ExtAudioBuffer() { extmem_free(this->buffer); }
};
