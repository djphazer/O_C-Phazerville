#pragma once

// Float32 port of the Teensy Audio Library waveform oscillators
// (synth_waveform.{h,cpp}, Paul Stoffregen / PJRC, MIT-style license as in the
// original headers).
//
// Design notes:
// - The uint32 phase accumulator is kept bit-exact with the int16 original:
//   integer phase words have no cumulative rounding error (unlike float phase)
//   and the band-limited generators depend on them. What changes is everything
//   around the accumulator:
//   * Output samples are float32; the int16 magnitude requantization
//     ((val * magnitude) >> 16) becomes a float multiply, so amplitude scaling
//     never truncates.
//   * The sine table is float (built once from sinf()), so table lookups are
//     not 16-bit quantized.
//   * FM input arrives as float32 blocks (full precision instead of q15) and
//     the octave exponential uses exp2f() instead of the fast fixed-point
//     exp2 approximation, a strictly more accurate frequency mapping.
//   * PM offsets are computed from float modulator samples with the same
//     wrap-around semantics (2^31 phase words == 180 degrees).
// - BandLimitedWaveform (the step-table BLEP engine) is reused from the Teensy
//   library as-is: it is a fixed-point algorithm by design, already linked for
//   the int16 applets, and duplicating it would only spend ITCM. Its int16
//   result is scaled to float at the end, which is exactly where the original
//   applied its magnitude anyway.
// - Only the waveforms used by the F32 applets are implemented (plus their
//   documented shape-modulation fall-throughs); unknown types emit silence.

#include <Arduino.h>
#include <arm_math.h>
#include "synth_waveform.h" // WAVEFORM_* constants + BandLimitedWaveform
#include "../extern/f32/AudioStream_F32.h"

// 257-entry float sine table (one full cycle + wrap sample), built lazily on
// first oscillator construction (main thread, never the audio ISR).
inline const float* AudioWaveformSineF32() {
  static float table[257];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < 256; i++) {
      table[i] = sinf(static_cast<float>(i) * (6.283185307179586f / 256.0f));
    }
    table[256] = table[0];
    init = true;
  }
  return table;
}

// ---------------------------------------------------------------------------
// Non-modulated oscillator (AudioSynthWaveform port).
// Implemented waveforms: SINE, ARBITRARY (256-sample float table),
// TRIANGLE_VARIABLE, PULSE (integer skew/width math kept identical to the
// int16 original, incl. the width-0/width-max saw shapes that relied on ARM
// udiv-by-zero returning 0 - made explicit here; only the final magnitude
// scaling is float).
// ---------------------------------------------------------------------------
class AudioSynthWaveformF32 : public AudioStream_F32 {
public:
  AudioSynthWaveformF32() : AudioStream_F32(0, nullptr) {
    sine_table = AudioWaveformSineF32();
  }

  void frequency(float freq) {
    if (freq < 0.0f) freq = 0.0f;
    else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2.0f) freq = AUDIO_SAMPLE_RATE_EXACT / 2.0f;
    phase_increment = freq * (4294967296.0f / AUDIO_SAMPLE_RATE_EXACT);
    if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
  }
  void phase(float angle) {
    if (angle < 0.0f) angle = 0.0f;
    else if (angle > 360.0f) {
      angle = angle - 360.0f;
      if (angle >= 360.0f) return;
    }
    phase_offset = angle * (float)(4294967296.0 / 360.0);
  }
  void amplitude(float n) { // 0 to 1.0
    if (n < 0.0f) n = 0.0f;
    else if (n > 1.0f) n = 1.0f;
    magnitude = n;
  }
  void offset(float n) {
    if (n < -1.0f) n = -1.0f;
    else if (n > 1.0f) n = 1.0f;
    tone_offset = n;
  }
  void pulseWidth(float n) { // 0.0 to 1.0
    if (n < 0.0f) n = 0.0f;
    else if (n > 1.0f) n = 1.0f;
    // vcvt saturates n == 1.0 to 0xFFFFFFFF, same as the int16 original
    pulse_width = n * 4294967296.0f;
  }
  void begin(short t_type) {
    phase_offset = 0;
    tone_type = t_type;
  }
  void begin(float t_amp, float t_freq, short t_type) {
    amplitude(t_amp);
    frequency(t_freq);
    begin(t_type);
  }
  // 256-sample single-cycle float wavetable (+ index 0 reused for wrap).
  // maxFreq kept for API parity with the int16 original (unused there too).
  void arbitraryWaveform(const float* data, float maxFreq) {
    (void)maxFreq;
    arbdata = data;
  }

  virtual void update(void) override {
    uint32_t ph = phase_accumulator + phase_offset;
    const uint32_t inc = phase_increment;

    if (magnitude == 0.0f) {
      phase_accumulator += inc * AUDIO_BLOCK_SAMPLES;
      return;
    }
    audio_block_f32_t* block = AudioStream_F32::allocate_f32();
    if (!block) {
      phase_accumulator += inc * AUDIO_BLOCK_SAMPLES;
      return;
    }
    float* bp = block->data;
    const float mag = magnitude;

    switch (tone_type) {
      case WAVEFORM_SINE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          uint32_t index = ph >> 24;
          float frac = static_cast<float>((ph >> 8) & 0xFFFF) * (1.0f / 65536.0f);
          float val1 = sine_table[index];
          float val2 = sine_table[index + 1];
          *bp++ = (val1 + (val2 - val1) * frac) * mag;
          ph += inc;
        }
        break;

      case WAVEFORM_ARBITRARY: {
        if (!arbdata) {
          AudioStream_F32::release(block);
          phase_accumulator += inc * AUDIO_BLOCK_SAMPLES;
          return;
        }
        // table length 256, wraps to index 0
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          uint32_t index = ph >> 24;
          uint32_t index2 = (index + 1) & 0xFF;
          float frac = static_cast<float>((ph >> 8) & 0xFFFF) * (1.0f / 65536.0f);
          float val1 = arbdata[index];
          float val2 = arbdata[index2];
          *bp++ = (val1 + (val2 - val1) * frac) * mag;
          ph += inc;
        }
        break;
      }

      case WAVEFORM_TRIANGLE_VARIABLE: {
        // Integer skew math identical to the int16 original, including the
        // degenerate widths: pw16 == 0 gives the falling saw and pw16 ==
        // 0xFFFF the rising saw the original produced via udiv-by-zero == 0.
        const float tri_scale = mag * (1.0f / 32768.0f);
        const uint32_t pw16 = pulse_width >> 16;
        const uint32_t rise = pw16 ? 0xFFFFFFFFu / pw16 : 0;
        const uint32_t fall =
          (pw16 != 0xFFFFu) ? 0xFFFFFFFFu / (0xFFFFu - pw16) : 0;
        const uint32_t halfwidth = pulse_width / 2;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          uint32_t n;
          if (ph < halfwidth) {
            n = (ph >> 16) * rise;
            *bp++ = static_cast<float>(n >> 16) * tri_scale;
          } else if (ph < 0xFFFFFFFFu - halfwidth) {
            n = 0x7FFFFFFFu - (((ph - halfwidth) >> 16) * fall);
            *bp++ = static_cast<float>(static_cast<int32_t>(n) >> 16) * tri_scale;
          } else {
            n = ((ph + halfwidth) >> 16) * rise + 0x80000000u;
            *bp++ = static_cast<float>(static_cast<int32_t>(n) >> 16) * tri_scale;
          }
          ph += inc;
        }
        break;
      }

      case WAVEFORM_PULSE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          *bp++ = (ph < pulse_width) ? mag : -mag;
          ph += inc;
        }
        break;

      default:
        arm_fill_f32(0.0f, block->data, AUDIO_BLOCK_SAMPLES);
        ph += inc * AUDIO_BLOCK_SAMPLES;
        break;
    }
    phase_accumulator = ph - phase_offset;

    if (tone_offset != 0.0f) {
      arm_offset_f32(block->data, tone_offset, block->data, AUDIO_BLOCK_SAMPLES);
    }
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

private:
  uint32_t phase_accumulator = 0;
  uint32_t phase_increment = 0;
  uint32_t phase_offset = 0;
  uint32_t pulse_width = 0x40000000u; // same default as the int16 original
  float magnitude = 0.0f;
  float tone_offset = 0.0f;
  const float* arbdata = nullptr;
  const float* sine_table;
  short tone_type = WAVEFORM_SINE;
};

// ---------------------------------------------------------------------------
// Modulated oscillator (AudioSynthWaveformModulated port).
// Input 0: FM/PM modulator (float32). Input 1: shape (pulse width) (float32).
// Implemented waveforms: SINE, TRIANGLE, TRIANGLE_VARIABLE, BANDLIMIT_SQUARE,
// BANDLIMIT_PULSE, BANDLIMIT_SAWTOOTH(_REVERSE).
// ---------------------------------------------------------------------------
class AudioSynthWaveformModulatedF32 : public AudioStream_F32 {
public:
  AudioSynthWaveformModulatedF32() : AudioStream_F32(2, inputQueueArray) {
    sine_table = AudioWaveformSineF32();
  }

  void frequency(float freq) {
    if (freq < 0.0f) freq = 0.0f;
    else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2.0f) freq = AUDIO_SAMPLE_RATE_EXACT / 2.0f;
    phase_increment = freq * (4294967296.0f / AUDIO_SAMPLE_RATE_EXACT);
    if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
  }
  void amplitude(float n) { // 0 to 1.0
    if (n < 0.0f) n = 0.0f;
    else if (n > 1.0f) n = 1.0f;
    magnitude = n;
  }
  void offset(float n) {
    if (n < -1.0f) n = -1.0f;
    else if (n > 1.0f) n = 1.0f;
    tone_offset = n;
  }
  void begin(short t_type) {
    tone_type = t_type;
    if (t_type == WAVEFORM_BANDLIMIT_SQUARE)
      band_limit_waveform.init_square(phase_increment);
    else if (t_type == WAVEFORM_BANDLIMIT_PULSE)
      band_limit_waveform.init_pulse(phase_increment, 0x80000000u);
    else if (t_type == WAVEFORM_BANDLIMIT_SAWTOOTH || t_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE)
      band_limit_waveform.init_sawtooth(phase_increment);
  }
  void begin(float t_amp, float t_freq, short t_type) {
    amplitude(t_amp);
    frequency(t_freq);
    begin(t_type);
  }
  void frequencyModulation(float octaves) {
    if (octaves > 12.0f) octaves = 12.0f;
    else if (octaves < 0.1f) octaves = 0.1f;
    modulation_factor = octaves; // octaves at modulator full scale (+-1.0)
    modulation_type = 0;
  }
  void phaseModulation(float degrees) {
    if (degrees > 9000.0f) degrees = 9000.0f;
    else if (degrees < 30.0f) degrees = 30.0f;
    // phase words per unit modulator sample; 2^31 words == 180 degrees.
    // Larger shifts wrap around uint32, same as the int16 original.
    modulation_factor = degrees * (2147483648.0f / 180.0f);
    modulation_type = 1;
  }

  virtual void update(void) override {
    audio_block_f32_t* moddata = receiveReadOnly_f32(0);
    audio_block_f32_t* shapedata = receiveReadOnly_f32(1);

    // Pre-compute the phase angle for every output sample of this update
    uint32_t ph = phase_accumulator;
    const uint32_t inc = phase_increment;
    uint32_t priorphase = phasedata[AUDIO_BLOCK_SAMPLES - 1];
    if (moddata && modulation_type == 0) {
      // Frequency modulation: mod sample is +-1.0 == +-modulation_factor
      // octaves. exp2f() replaces the fixed-point exp2 approximation.
      const float octave_scale = modulation_factor;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float phstep = static_cast<float>(inc) * exp2f(moddata->data[i] * octave_scale);
        if (phstep < static_cast<float>(0x7FFE0000u)) {
          ph += static_cast<uint32_t>(phstep);
        } else {
          ph += 0x7FFE0000u;
        }
        phasedata[i] = ph;
      }
      AudioStream_F32::release(moddata);
    } else if (moddata) {
      // Phase modulation; > +-180 degree shifts wrap around, as the original.
      const float pm_scale = modulation_factor;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        uint32_t n = static_cast<uint32_t>(static_cast<int64_t>(moddata->data[i] * pm_scale));
        phasedata[i] = ph + n;
        ph += inc;
      }
      AudioStream_F32::release(moddata);
    } else {
      // No modulation input
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        phasedata[i] = ph;
        ph += inc;
      }
    }
    phase_accumulator = ph;

    // If the amplitude is zero, no output, but phase still increments properly
    if (magnitude == 0.0f) {
      if (shapedata) AudioStream_F32::release(shapedata);
      return;
    }
    audio_block_f32_t* block = AudioStream_F32::allocate_f32();
    if (!block) {
      if (shapedata) AudioStream_F32::release(shapedata);
      return;
    }
    float* bp = block->data;
    const float mag = magnitude;
    // band-limited generators return int16-scaled values
    const float blep_scale = magnitude * (1.0f / 32768.0f);

    switch (tone_type) {
      case WAVEFORM_SINE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          ph = phasedata[i];
          uint32_t index = ph >> 24;
          float frac = static_cast<float>((ph >> 8) & 0xFFFF) * (1.0f / 65536.0f);
          float val1 = sine_table[index];
          float val2 = sine_table[index + 1];
          *bp++ = (val1 + (val2 - val1) * frac) * mag;
        }
        break;

      case WAVEFORM_BANDLIMIT_PULSE:
        if (shapedata) {
          for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            uint32_t width = ShapeToWidth16(shapedata->data[i]) << 16;
            int32_t val = band_limit_waveform.generate_pulse(phasedata[i], width, i);
            *bp++ = static_cast<float>(val) * blep_scale;
          }
          break;
        } // else fall through to ordinary square without shape modulation
        [[fallthrough]];

      case WAVEFORM_BANDLIMIT_SQUARE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          int32_t val = band_limit_waveform.generate_square(phasedata[i], i);
          *bp++ = static_cast<float>(val) * blep_scale;
        }
        break;

      case WAVEFORM_BANDLIMIT_SAWTOOTH:
      case WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE:
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          float val = static_cast<float>(band_limit_waveform.generate_sawtooth(phasedata[i], i)) * blep_scale;
          *bp++ = (tone_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE) ? -val : val;
        }
        break;

      case WAVEFORM_TRIANGLE_VARIABLE:
        if (shapedata) {
          // Integer skew math kept identical to the int16 original; only the
          // final magnitude scaling is float.
          const float tri_scale = magnitude * (1.0f / 32768.0f);
          for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            uint32_t width = ShapeToWidth16(shapedata->data[i]);
            if (width < 1) width = 1;                // avoid division by zero
            else if (width > 0xFFFE) width = 0xFFFE; // (UDIV would yield 0)
            uint32_t rise = 0xFFFFFFFFu / width;
            uint32_t fall = 0xFFFFFFFFu / (0xFFFFu - width);
            uint32_t halfwidth = width << 15;
            uint32_t n;
            ph = phasedata[i];
            if (ph < halfwidth) {
              n = (ph >> 16) * rise;
              *bp++ = static_cast<float>(n >> 16) * tri_scale;
            } else if (ph < 0xFFFFFFFFu - halfwidth) {
              n = 0x7FFFFFFFu - (((ph - halfwidth) >> 16) * fall);
              *bp++ = static_cast<float>(static_cast<int32_t>(n) >> 16) * tri_scale;
            } else {
              n = ((ph + halfwidth) >> 16) * rise + 0x80000000u;
              *bp++ = static_cast<float>(static_cast<int32_t>(n) >> 16) * tri_scale;
            }
          }
          break;
        } // else fall through to ordinary triangle without shape modulation
        [[fallthrough]];

      case WAVEFORM_TRIANGLE: {
        const float tri_scale = magnitude * (1.0f / 32768.0f);
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          ph = phasedata[i];
          uint32_t phtop = ph >> 30;
          if (phtop == 1 || phtop == 2) {
            *bp++ = static_cast<float>(0xFFFFu - (ph >> 15)) * tri_scale;
          } else {
            *bp++ = static_cast<float>(static_cast<int32_t>(ph) >> 15) * tri_scale;
          }
        }
        break;
      }

      default:
        arm_fill_f32(0.0f, block->data, AUDIO_BLOCK_SAMPLES);
        break;
    }
    (void)priorphase; // (sample & hold not ported)

    if (tone_offset != 0.0f) {
      arm_offset_f32(block->data, tone_offset, block->data, AUDIO_BLOCK_SAMPLES);
    }
    if (shapedata) AudioStream_F32::release(shapedata);
    AudioStream_F32::transmit(block);
    AudioStream_F32::release(block);
  }

private:
  // Map a float shape sample (+-1.0) to the original's 16-bit width, matching
  // the q15 saturation the int16 path applied at the applet edge.
  static inline uint32_t ShapeToWidth16(float x) {
    int32_t q = static_cast<int32_t>(x * 32768.0f);
    if (q > 32767) q = 32767;
    else if (q < -32768) q = -32768;
    return static_cast<uint32_t>(q + 0x8000) & 0xFFFFu;
  }

  audio_block_f32_t* inputQueueArray[2];
  uint32_t phase_accumulator = 0;
  uint32_t phase_increment = 0;
  float modulation_factor = 8.0f;
  float magnitude = 0.0f;
  float tone_offset = 0.0f;
  uint32_t phasedata[AUDIO_BLOCK_SAMPLES] = {0};
  const float* sine_table;
  uint8_t tone_type = WAVEFORM_SINE;
  uint8_t modulation_type = 0;
  BandLimitedWaveform band_limit_waveform;
};
