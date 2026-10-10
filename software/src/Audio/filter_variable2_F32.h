#pragma once

// Float32 port of the local filter_variable2 (Chamberlin state variable
// filter with 4x oversampling, from Paul Stoffregen's Teensy Audio Library,
// MIT). Same coefficient mappings as the fixed-point version (per-substep
// F = sin(freq*pi/(2*fs)) on the fixed path, linear-approx center frequency
// with exp2 octave control on the CV-modulated path, damp = 1/q), but the
// filter state stays float32 and the LP/BP/HP outputs are not saturated to
// int16 full scale.
//
// Inputs:  0 = signal, 1 = frequency control (+-1.0 full scale, like the
//              int16 version's 15-fractional-bit control signal)
// Outputs: 0 = lowpass, 1 = bandpass, 2 = highpass

#include <Arduino.h>
#include "../extern/f32/AudioStream_F32.h"

class AudioFilterStateVariable2F32 : public AudioStream_F32 {
public:
  AudioFilterStateVariable2F32() : AudioStream_F32(2, inputQueueArray) {
    frequency(1000);
    octaveControl(1.0f); // default values
    resonance(0.707f);
  }

  void frequency(float freq) {
    if (freq < 20.0f) freq = 20.0f;
    else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2.5f)
      freq = AUDIO_SAMPLE_RATE_EXACT / 2.5f;
    // linear (small-angle) coefficient, used by the CV-modulated path
    setting_fcenter = freq * (3.141592654f / (AUDIO_SAMPLE_RATE_EXACT * 2.0f));
    // sin() coefficient, used by the fixed path
    setting_fmult =
      sinf(freq * (3.141592654f / (AUDIO_SAMPLE_RATE_EXACT * 2.0f)));
  }

  void resonance(float q) {
    if (q < 0.5f) q = 0.5f; // q can be lower with 4x oversampling
    else if (q > 5.0f) q = 5.0f;
    setting_damp = 1.0f / q;
  }

  void octaveControl(float n) {
    // filter's corner frequency is Fcenter * 2^(control * N)
    // where "control" ranges from -1.0 to +1.0
    if (n < 0.0f) n = 0.0f;
    else if (n > 7.9998f) n = 7.9998f;
    setting_octavemult = n;
  }

  virtual void update(void) override {
    audio_block_f32_t* input_block = receiveReadOnly_f32(0);
    audio_block_f32_t* control_block = receiveReadOnly_f32(1);
    if (!input_block) {
      if (control_block) AudioStream_F32::release(control_block);
      return;
    }
    audio_block_f32_t* lp_block = AudioStream_F32::allocate_f32();
    audio_block_f32_t* bp_block = AudioStream_F32::allocate_f32();
    audio_block_f32_t* hp_block = AudioStream_F32::allocate_f32();
    if (!lp_block || !bp_block || !hp_block) {
      if (lp_block) AudioStream_F32::release(lp_block);
      if (bp_block) AudioStream_F32::release(bp_block);
      if (hp_block) AudioStream_F32::release(hp_block);
      AudioStream_F32::release(input_block);
      if (control_block) AudioStream_F32::release(control_block);
      return;
    }

    const float damp = setting_damp;
    float inputprev = state_inputprev;
    float lowpass = state_lowpass;
    float bandpass = state_bandpass;
    float highpass = 0.0f;
    float fmult = setting_fmult;

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      if (control_block) {
        // corner frequency = Fcenter * 2^(control * octavemult), clamped as
        // in the fixed-point version so q >= 0.5 stays stable
        float mod = control_block->data[i] * setting_octavemult;
        fmult = setting_fcenter * exp2f(mod);
        if (fmult > 0.707107f) fmult = 0.707107f;
      }
      // 4x oversampled Chamberlin SVF, interpolated input, as in int16
      float input = input_block->data[i];
      lowpass += fmult * bandpass;
      highpass = (input + 3.0f * inputprev) * 0.25f - lowpass - damp * bandpass;
      bandpass += fmult * highpass;
      lowpass += fmult * bandpass;
      highpass = (input + inputprev) * 0.5f - lowpass - damp * bandpass;
      bandpass += fmult * highpass;
      lowpass += fmult * bandpass;
      highpass = (3.0f * input + inputprev) * 0.25f - lowpass - damp * bandpass;
      bandpass += fmult * highpass;
      lowpass += fmult * bandpass;
      highpass = input - lowpass - damp * bandpass;
      bandpass += fmult * highpass;
      inputprev = input;
      // just decimate; no int16 saturation in the float path
      lp_block->data[i] = lowpass;
      bp_block->data[i] = bandpass;
      hp_block->data[i] = highpass;
    }

    state_inputprev = inputprev;
    state_lowpass = lowpass;
    state_bandpass = bandpass;

    AudioStream_F32::release(input_block);
    if (control_block) AudioStream_F32::release(control_block);
    AudioStream_F32::transmit(lp_block, 0);
    AudioStream_F32::release(lp_block);
    AudioStream_F32::transmit(bp_block, 1);
    AudioStream_F32::release(bp_block);
    AudioStream_F32::transmit(hp_block, 2);
    AudioStream_F32::release(hp_block);
  }

private:
  float setting_fcenter;
  float setting_fmult;
  float setting_octavemult;
  float setting_damp;
  float state_inputprev = 0.0f;
  float state_lowpass = 0.0f;
  float state_bandpass = 0.0f;
  audio_block_f32_t* inputQueueArray[2];
};
