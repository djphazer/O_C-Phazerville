#pragma once

#include "AudioBuffer.h"
#include "AudioParam.h"
#include "dsputils.h"
#include "util/util_macros.h"
#include <Audio.h>
#include <cstdint>

template <
  size_t BufferLength = static_cast<size_t>(AUDIO_SAMPLE_RATE),
  size_t Taps = 1,
  // ChunkSize *must* evenly divide AUDIO_BLOCK_SAMPLES
  size_t ChunkSize = 16,
  // Crossfade time will be AUDIO_SAMPLE_RATE / CrossfadeSamples
  // 2048 give ~48ms, or ~22hz, just below human audio range
  size_t CrossfadeSamples = 2048>
class AudioDelayExt : public AudioStream {
public:
  static constexpr float MAX_DELAY_SECS
    = BufferLength / AUDIO_SAMPLE_RATE_EXACT;
  static constexpr float MIN_DELAY_SECS = ChunkSize / AUDIO_SAMPLE_RATE_EXACT;

  AudioDelayExt() : AudioStream(1, input_queue_array) {
    delay_secs.fill(OnePole(Interpolated(0.0f, AUDIO_BLOCK_SAMPLES), 0.0002f));
    fb.fill(Interpolated(0.0f, AUDIO_BLOCK_SAMPLES / ChunkSize));
  }

  void Acquire() {
    buffer.Acquire();
    // TODO xfade lut should be static
    xfade_in_scalars = new q15_t[CrossfadeSamples];
    xfade_out_scalars = new q15_t[CrossfadeSamples];
    float n = static_cast<float>(CrossfadeSamples - 1);
    for (size_t i = 0; i < CrossfadeSamples; i++) {
      float out, in;
      EqualPowerFade(out, in, i / n);
      xfade_in_scalars[i] = float_to_q15(in);
      xfade_out_scalars[i] = float_to_q15(out);
    }
  }

  void Release() {
    delete[] xfade_in_scalars;
    delete[] xfade_out_scalars;
    buffer.Release();
  }

  void delay(size_t tap, float secs) {
    CONSTRAIN(secs, MIN_DELAY_SECS, MAX_DELAY_SECS);
    delay_secs[tap] = secs;
    if (delay_secs[tap].Read() == 0.0f) delay_secs[tap].Reset();
  }

  void cf_delay(size_t tap, float secs) {
    CONSTRAIN(secs, MIN_DELAY_SECS, MAX_DELAY_SECS);
    auto& t = target_delay[tap];
    if (t.phase >= CrossfadeSamples && t.target != secs) {
      // 0.0005f comes from variation
      // I observed a <0.0005 noise in detecting internal clock signal. External
      // I saw a bit larger with faster clocks. 0.001 seems reasonable, though
      // it means, with full delay, <12ms changes will be ignored.
      if (abs(t.target - secs) / t.target < 0.001f) {
        return;
      }
      t.phase = 0;
      t.target = secs;
      // t.phase_inc = 1;
      // size_t samples = secs * AUDIO_SAMPLE_RATE_EXACT;
      // t.phase_inc = CrossfadeSamples / samples + 1;
    }
  }

  void taps(size_t num) {
    taps_ = num;
  }

  void feedback(size_t tap, float fb) {
    this->fb[tap] = fb;
  }

  void update(void) override {
    if (!buffer.IsReady()) return;
    audio_block_t* in_block = receiveWritable();

    if (in_block == NULL) {
      // Even if we're not getting anything we need to process feedback.
      // e.g. a VCA will actually not output blocks when closed.
      in_block = allocate();
      std::fill(in_block->data, in_block->data + AUDIO_BLOCK_SAMPLES, 0);
    }

    audio_block_t* outs[Taps];
    for (uint_fast8_t tap = 0; tap < taps_; tap += 1) outs[tap] = allocate();

    q15_t temp_buff[ChunkSize];
    for (uint_fast8_t chunk_start = 0; chunk_start < AUDIO_BLOCK_SAMPLES;
         chunk_start += ChunkSize) {
      auto* in_chunk = in_block->data + chunk_start;
      for (uint_fast8_t tap = 0; tap < taps_; tap++) {
        auto& tap_delay = delay_secs[tap];
        auto& target = target_delay[tap];
        auto* chunk_out = outs[tap]->data + chunk_start;

        if (tap_delay.Done() || target.phase < CrossfadeSamples) {
          ReadCrossfadeChunk(delay_secs[tap], target, chunk_out, temp_buff);
        } else {
          ReadStretchChunk(tap_delay, chunk_out);
        }
        q15_t f = float_to_q15(fb[tap].ReadNext());
        arm_scale_q15(chunk_out, f, 0, temp_buff, ChunkSize);
        arm_add_q15(temp_buff, in_chunk, in_chunk, ChunkSize);
      }
      buffer.Write(in_chunk, ChunkSize);
    }
    release(in_block);
    for (uint_fast8_t tap = 0; tap < taps_; tap++) {
      transmit(outs[tap], tap);
      release(outs[tap]);
    }
  }

private:
  // 20 hz to be just below human hearing frequency.
  // Empirically most stuff sounds pretty good at this, but high pitched sine
  // waves will crackle a bit with modulation
  static constexpr float crossfade_dt = 20.0f / AUDIO_SAMPLE_RATE;
  struct CrossfadeTarget {
    float target = 0.0f;
    uint16_t phase = CrossfadeSamples; // out of CrossfadeSamples
    // size_t phase_inc = 1;
  };

  audio_block_t* input_queue_array[1];
  q15_t* xfade_in_scalars;
  q15_t* xfade_out_scalars;
  std::array<CrossfadeTarget, Taps> target_delay;
  std::array<OnePole<Interpolated>, Taps> delay_secs;
  std::array<Interpolated, Taps> fb;
  ExtAudioBuffer<BufferLength, int16_t> buffer;
  size_t taps_ = Taps;

  void ReadCrossfadeChunk(
    OnePole<Interpolated>& tap_delay,
    CrossfadeTarget& target,
    int16_t* chunk_out,
    int16_t* temp_buff
  ) {
    buffer.ReadFromSecondsAgo(tap_delay.Read(), chunk_out, ChunkSize);
    if (target.phase < CrossfadeSamples) {
      buffer.ReadFromSecondsAgo(target.target, temp_buff, ChunkSize);
      arm_mult_q15(
        temp_buff, xfade_in_scalars + target.phase, temp_buff, ChunkSize
      );
      arm_mult_q15(
        chunk_out, xfade_out_scalars + target.phase, chunk_out, ChunkSize
      );
      arm_add_q15(chunk_out, temp_buff, chunk_out, ChunkSize);
      target.phase += ChunkSize;
      // This was an attempt to do adaptive crossfade speeds to accomodate
      // e.g. karplus-strong. However, it was too jarring when doing, e.g.,
      // flanging.

      // for (size_t j = 0; j < ChunkSize && target.phase <=
      // CrossfadeSamples;
      //      j++) {
      //   int32_t in = ((int32_t)temp_buff[j]
      //                 * (int32_t)xfade_in_scalars[target.phase])
      //     >> 16;
      //   int32_t out = ((int32_t)chunk_out[j]
      //                  * (int32_t)xfade_out_scalars[target.phase])
      //     >> 16;
      //   chunk_out[j] = (q15_t)(__SSAT((q31_t)(in + out), 16));
      //   target.phase += target.phase_inc;
      // }
      if (target.phase >= CrossfadeSamples) {
        tap_delay = target.target;
        tap_delay.Reset();
      }
    }
  }

  // Bunch of attempts at doing faster pitch shifting modulation, but just doing
  // sample by sample is shockingly faster than all of them...
  void ReadStretchChunk(OnePole<Interpolated>& tap_delay, int16_t* chunk_out) {
    for (uint_fast8_t sample = 0; sample < ChunkSize; sample++) {
      chunk_out[sample] = Clip16(buffer.ReadInterp(
        tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample
      ));
    }
  }

  void ReadStretchChunkUnrolled(
    OnePole<Interpolated>& tap_delay, int16_t* chunk_out
  ) {
    for (uint_fast8_t sample = 0; sample < ChunkSize; sample += 4) {
      chunk_out[sample] = buffer.ReadInterp(
        tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample + 0
      );
      chunk_out[sample] = buffer.ReadInterp(
        tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample + 1
      );
      chunk_out[sample] = buffer.ReadInterp(
        tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample + 2
      );
      chunk_out[sample] = buffer.ReadInterp(
        tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample + 3
      );
    }
  }

  void ReadStretchVectorized(
    OnePole<Interpolated>& tap_delay, int16_t* chunk_out, int16_t* temp_buff
  ) {
    int16_t xm1[ChunkSize];
    int16_t x0[ChunkSize];
    int16_t x1[ChunkSize];
    int16_t x2[ChunkSize];
    int16_t read_buffer[4];
    for (uint_fast8_t sample = 0; sample < ChunkSize; sample++) {
      float samples = tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - sample;
      size_t samples_back_int = static_cast<size_t>(samples);
      temp_buff[sample] = float_to_q15(1.0f - (samples - samples_back_int));
      buffer.ReadFromSamplesAgo(samples_back_int + 2, read_buffer);
      // There will likely be a ton of overlap in the reads but hopefully
      // caching will help
      xm1[sample + 0] = read_buffer[0];
      x0[sample + 1] = read_buffer[1];
      x1[sample + 2] = read_buffer[2];
      x2[sample + 2] = read_buffer[3];
    }
    InterpHermiteQ15Vec(xm1, x0, x1, x2, temp_buff, chunk_out, ChunkSize);
  }

  void ReadStretchBulk(
    OnePole<Interpolated>& tap_delay, int16_t* chunk_out, int16_t* temp_buff
  ) {
    float samples_back[ChunkSize];
    for (uint_fast8_t i = 0; i < ChunkSize; i++) {
      samples_back[i] = tap_delay.ReadNext() * AUDIO_SAMPLE_RATE_EXACT - i;
    }
    size_t start_sample = static_cast<size_t>(samples_back[0]);
    size_t end_sample = static_cast<size_t>(samples_back[ChunkSize - 1]);
    // SERIAL_PRINTLN("%u %u", start_sample, end_sample);
    if (start_sample < end_sample) SWAP(start_sample, end_sample);
    start_sample += 2;
    end_sample -= 1;
    size_t l = start_sample - end_sample + 1;
    int16_t read_buff[l];
    // SERIAL_PRINTLN("%u %u %u", start_sample, end_sample, l);
    buffer.ReadFromSamplesAgo(start_sample, read_buff, l);
    for (uint_fast8_t i = 0; i < ChunkSize; i++) {
      float sb = start_sample - samples_back[i];
      size_t sbi = static_cast<size_t>(sb);
      float t = sb - sbi;
      chunk_out[i] = InterpHermite(
        read_buff[sbi],
        read_buff[sbi + 1],
        read_buff[sbi + 2],
        read_buff[sbi + 3],
        t
      );
    }
  }
};
