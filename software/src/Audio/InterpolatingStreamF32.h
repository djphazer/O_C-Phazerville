#pragma once

// Float32 port of InterpolatingStream (InterpolatingStream.h): a control-rate
// -> audio-rate resampler fed by Push() from the OC core ISR. Identical rate
// tracking and interpolation logic; the ring buffer stores float, so pushed
// CV never quantizes to q15. Push() saturates to the q15 range ([-1.0,
// 32767/32768]) to match what float_to_q15() did at the int16 applets' edge.

#include "../OC_config.h"
#include "../dsputils.h"
#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"
#include "InterpolatingStream.h" // InterpolationMethod

template <size_t BufferSize = AUDIO_BLOCK_SAMPLES>
class InterpolatingStreamF32 : public AudioStream_F32 {
public:
  InterpolatingStreamF32(float approx_input_rate = OC_CORE_ISR_FREQ)
    : AudioStream_F32(0, nullptr)
    , delta(approx_input_rate / AUDIO_SAMPLE_RATE_EXACT) {}

  void Acquire() {
    if (buffer == nullptr) {
      buffer = static_cast<float*>(calloc(BufferSize, sizeof(float)));
      Push(0.0f);
    }
  }

  void Release() {
    if (buffer != nullptr) {
      free(buffer);
      buffer = nullptr;
    }
  }

  void Push(float value) {
    if (buffer == nullptr) return;
    if (value > 32767.0f / 32768.0f) value = 32767.0f / 32768.0f;
    else if (value < -1.0f) value = -1.0f;
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
    // Stick to the ideal delta except when we can't (see the int16 original
    // for the reasoning; adaptive schemes never settled).
    float d = constrain(delta, lo, hi);
    audio_block_f32_t* out = AudioStream_F32::allocate_f32();
    if (out == nullptr) return;
    switch (method) {
      case INTERPOLATION_ZOH:
        UpdateZOH(d, out);
        break;
      case INTERPOLATION_LINEAR:
        UpdateLinear(d, out);
        break;
      case INTERPOLATION_HERMITE:
      default:
        UpdateHermite(d, out);
        break;
    }
    AudioStream_F32::transmit(out);
    AudioStream_F32::release(out);
  }

  void UpdateZOH(float d, audio_block_f32_t* out) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int read_int = static_cast<int>(read_ix);
      out->data[i] = buffer[read_int];
      read_ix += d;
      if (read_ix >= BufferSize) read_ix -= BufferSize;
    }
  }

  void UpdateLinear(float d, audio_block_f32_t* out) {
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

  void UpdateHermite(float d, audio_block_f32_t* out) {
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int read_int = static_cast<int>(read_ix);
      float read_dec = read_ix - read_int;
      // The int16 original clipped Hermite overshoot to the q15 range; keep
      // the same bounds so downstream modulation depths match.
      float v = InterpHermite(
        buffer[read_int],
        buffer[(read_int + 1) % BufferSize],
        buffer[(read_int + 2) % BufferSize],
        buffer[(read_int + 3) % BufferSize],
        read_dec
      );
      if (v > 32767.0f / 32768.0f) v = 32767.0f / 32768.0f;
      else if (v < -1.0f) v = -1.0f;
      out->data[i] = v;
      read_ix += d;
      if (read_ix >= BufferSize) read_ix -= BufferSize;
    }
  }

private:
  float* buffer = nullptr;
  size_t write_ix = 0;
  float read_ix = 0.0f;
  float delta;
  InterpolationMethod method = INTERPOLATION_ZOH;
};
