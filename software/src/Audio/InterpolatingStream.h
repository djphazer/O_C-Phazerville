#pragma once

#include "OC_config.h"
#include "dsputils.h"
#include <Audio.h>

enum InterpolationMethod {
  INTERPOLATION_ZOH,
  INTERPOLATION_LINEAR,
  INTERPOLATION_HERMITE
};

template <size_t BufferSize = AUDIO_BLOCK_SAMPLES>
class InterpolatingStream : public AudioStream {
public:
  InterpolatingStream(float approx_input_rate = OC_CORE_ISR_FREQ)
    : AudioStream(0, nullptr)
    , delta(approx_input_rate / AUDIO_SAMPLE_RATE_EXACT) {}

  void Acquire() {
    if (buffer == nullptr) {
      buffer = static_cast<int16_t*>(calloc(BufferSize, sizeof(int16_t)));
      Push(0.0f);
    }
  }

  void Release() {
    if (buffer != nullptr) {
      free(buffer);
      buffer = nullptr;
    }
  }

  void Push(int16_t value) {
    buffer[write_ix++] = value;
    write_ix %= AUDIO_BLOCK_SAMPLES;
  }

  inline float Length() const {
    float w = static_cast<float>(write_ix);
    return w > read_ix ? w - read_ix : w + BufferSize - read_ix;
  }

  void Method(InterpolationMethod val) {
    method = val;
  }

  void update(void) override {
    int padding;
    switch (method) {
      case INTERPOLATION_ZOH:
        padding = 0;
        break;
      case INTERPOLATION_LINEAR:
        padding = 1;
        break;
      case INTERPOLATION_HERMITE:
      default:
        padding = 3;
        break;
    }
    float l = Length();
    if (buffer == nullptr || l < padding + 1) return;
    // These should be boundary deltas that would result in consuming too many
    // or too few samples.
    float hi = floorf(l - (padding - 1)) / AUDIO_BLOCK_SAMPLES;
    float lo = ceilf(l - (padding + 1)) / AUDIO_BLOCK_SAMPLES;
    // I messed with various of ways of having delta adapt to the true relative
    // sample rates, but turns out better not to mess with it. The sample rates
    // do seem to drift, but kind of oscillate, so trying to adapt never
    // settles (or takes a really long time to). Just having it try to stick to
    // the ideal except when it can't is simple and worked the best.
    float d = constrain(delta, lo, hi);
    audio_block_t* out = allocate();
    switch (method) {
      case INTERPOLATION_ZOH:
        UpdateZOH(d, out);
        break;
      case INTERPOLATION_LINEAR:
        UpdateLinear(d, out);
        break;
      case INTERPOLATION_HERMITE:
        UpdateHermite(d, out);
        break;
    }
    transmit(out);
    release(out);
  }

  void UpdateZOH(float d, audio_block_t* out) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int read_int = static_cast<int>(read_ix);
      out->data[i] = buffer[read_int];
      read_ix += d;
      if (read_ix >= BufferSize) read_ix -= BufferSize;
    }
  }

  void UpdateLinear(float d, audio_block_t* out) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int read_int = static_cast<int>(read_ix);
      float read_dec = read_ix - read_int;
      out->data[i] = InterpLinear(
        buffer[read_int], buffer[(read_int + 1) % BufferSize], read_dec
      );
      read_ix += d;
      if (read_ix >= BufferSize) read_ix -= BufferSize;
    }
  }

  void UpdateHermite(float d, audio_block_t* out) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int read_int = static_cast<int>(read_ix);
      float read_dec = read_ix - read_int;
      out->data[i] = Clip16(InterpHermite(
        buffer[read_int],
        buffer[(read_int + 1) % BufferSize],
        buffer[(read_int + 2) % BufferSize],
        buffer[(read_int + 3) % BufferSize],
        read_dec
      ));
      read_ix += d;
      if (read_ix >= BufferSize) read_ix -= BufferSize;
    }
  }

private:
  int16_t* buffer = nullptr;
  size_t write_ix = 0;
  float read_ix = 0.0f;
  float delta;
  InterpolationMethod method = INTERPOLATION_ZOH;
};
