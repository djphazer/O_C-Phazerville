#pragma once

// Float32 port of the Teensy Audio Library ladder filter (filter_ladder.{h,cpp},
// Richard van Hoesel's Huovilainen New Moog model, v1.5). The model was
// already all-float internally; this version keeps float32 at the block
// boundaries too (no int16 quantization into or out of the filter). Same
// coefficients, 4x oversampling, polyphase FIR interpolation/decimation, and
// parameter mappings. The FC and resonance modulation inputs take +-1.0 full
// scale F32 signals (where the int16 version took +-32768).

#include <Arduino.h>
#include <arm_math.h>
#include <math.h>
#include "../extern/f32/AudioStream_F32.h"

enum AudioFilterLadderInterpolationF32 {
  LADDER_FILTER_F32_INTERPOLATION_LINEAR,
  LADDER_FILTER_F32_INTERPOLATION_FIR_POLY
};

class AudioFilterLadderF32 : public AudioStream_F32 {
public:
  AudioFilterLadderF32() : AudioStream_F32(3, inputQueueArray) { initpoly(); }

  void frequency(float c) {
    Fbase = c;
    compute_coeffs(c);
  }

  void resonance(float res) {
    // maps resonance = 0->1 to K = 0 -> 4
    if (res > MAX_RESONANCE) {
      res = MAX_RESONANCE;
    } else if (res < 0.0f) {
      res = 0.0f;
    }
    K = 4.0f * res;
  }

  void octaveControl(float octaves) {
    if (octaves > 7.0f) {
      octaves = 7.0f;
    } else if (octaves < 0.0f) {
      octaves = 0.0f;
    }
    octaveScale = octaves;
  }

  void passbandGain(float passbandgain) {
    pbg = passbandgain;
    if (pbg > 0.5f) pbg = 0.5f;
    if (pbg < 0.0f) pbg = 0.0f;
    inputDrive(host_overdrive);
  }

  void inputDrive(float odrv) {
    host_overdrive = odrv;
    if (host_overdrive > 1.0f) {
      if (host_overdrive > 4.0f) host_overdrive = 4.0f;
      // max is 4 when pbg = 0, and 2.5 when pbg is 0.5
      overdrive = 1.0f + (host_overdrive - 1.0f) * (1.0f - pbg);
    } else {
      overdrive = host_overdrive;
      if (overdrive < 0.0f) overdrive = 0.0f;
    }
  }

  void interpolationMethod(AudioFilterLadderInterpolationF32 imethod) {
    if (imethod == LADDER_FILTER_F32_INTERPOLATION_FIR_POLY && polyCapable) {
      polyOn = true;
    } else {
      polyOn = false;
    }
  }

  virtual void update(void) override {
    audio_block_f32_t *blocka, *blockb, *blockc;
    float Ktot = K;
    bool FCmodActive = true;
    bool QmodActive = true;

    blocka = receiveWritable_f32(0);
    blockb = receiveReadOnly_f32(1);
    blockc = receiveReadOnly_f32(2);
    if (!blocka) {
      if (resonating()) {
        // When no data arrives but the filter is still resonating, we must
        // continue computing the filter with zero input to sustain resonance
        blocka = AudioStream_F32::allocate_f32();
      }
      if (!blocka) {
        if (blockb) AudioStream_F32::release(blockb);
        if (blockc) AudioStream_F32::release(blockc);
        return;
      }
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        blocka->data[i] = 0.0f;
      }
    }
    if (!blockb) FCmodActive = false;
    if (!blockc) QmodActive = false;

    if (polyOn == true) {
      /*----------------------- upsample -------------------------*/
      float blockOS[I_NUM_SAMPLES], blockIn[AUDIO_BLOCK_SAMPLES];
      float blockOutOS[I_NUM_SAMPLES], blockOut[AUDIO_BLOCK_SAMPLES];
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        blockIn[i] = blocka->data[i] * overdrive * (float)INTERPOLATION;
      }
      arm_fir_interpolate_f32(
        &interpolation, blockIn, blockOS, AUDIO_BLOCK_SAMPLES
      );

      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        if (FCmodActive) {
          float FCmod = blockb->data[i] * octaveScale;
          float ftot = Fbase * fast_exp2f(FCmod);
          if (ftot > MAX_FREQUENCY) ftot = MAX_FREQUENCY;
          compute_coeffs(ftot);
        }
        if (QmodActive) {
          float Qmod = blockc->data[i];
          Ktot = K + 4.0f * Qmod;
        }
        if (Ktot > MAX_RESONANCE * 4.0f) {
          Ktot = MAX_RESONANCE * 4.0f;
        } else if (Ktot < 0.0f) {
          Ktot = 0.0f;
        }
        for (int os = 0; os < INTERPOLATION; os++) {
          float input = blockOS[i * 4 + os];
          float u = input - (z1[3] - pbg * input) * Ktot * Qadjust;
          u = fast_tanh(u);
          float stage1 = LPF(u, 0);
          float stage2 = LPF(stage1, 1);
          float stage3 = LPF(stage2, 2);
          float stage4 = LPF(stage3, 3);
          blockOutOS[i * 4 + os] = stage4;
        }
      }
      arm_fir_decimate_f32(&decimation, blockOutOS, blockOut, I_NUM_SAMPLES);
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        blocka->data[i] = blockOut[i];
      }
    } else {
      // linear interpolation
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float input = blocka->data[i] * overdrive;
        if (FCmodActive) {
          float FCmod = blockb->data[i] * octaveScale;
          float ftot = Fbase * fast_exp2f(FCmod);
          if (ftot > MAX_FREQUENCY) ftot = MAX_FREQUENCY;
          compute_coeffs(ftot);
        }
        if (QmodActive) {
          float Qmod = blockc->data[i];
          Ktot = K + 4.0f * Qmod;
        }
        if (Ktot > MAX_RESONANCE * 4.0f) {
          Ktot = MAX_RESONANCE * 4.0f;
        } else if (Ktot < 0.0f) {
          Ktot = 0.0f;
        }
        float total = 0.0f;
        float interp = 0.0f;
        for (int os = 0; os < INTERPOLATION; os++) {
          float u = (interp * oldinput + (1.0f - interp) * input)
            - (z1[3] - pbg * input) * Ktot * Qadjust;
          u = fast_tanh(u);
          float stage1 = LPF(u, 0);
          float stage2 = LPF(stage1, 1);
          float stage3 = LPF(stage2, 2);
          float stage4 = LPF(stage3, 3);
          total += stage4 * (1.0f / (float)INTERPOLATION);
          interp += (1.0f / (float)INTERPOLATION);
        }
        oldinput = input;
        blocka->data[i] = total;
      }
    }
    AudioStream_F32::transmit(blocka);
    AudioStream_F32::release(blocka);
    if (blockb) AudioStream_F32::release(blockb);
    if (blockc) AudioStream_F32::release(blockc);
  }

private:
  static constexpr float MAX_RESONANCE = 1.8f;
  static constexpr float MAX_FREQUENCY = AUDIO_SAMPLE_RATE_EXACT * 0.425f;
  static const int INTERPOLATION = 4;
  static const int I_NUM_SAMPLES = AUDIO_BLOCK_SAMPLES * INTERPOLATION;
  static const int interpolation_taps = 36;

  // same halfband coefficients as the int16 ladder filter
  static inline float interpolation_coeffs[interpolation_taps] = {
    -14.30851541590154240E-6f, 0.001348560352009071f, 0.004029285548698377f, 0.007644563345368599f,
    0.010936856250494802f, 0.011982063548666887f, 0.008882946305001046f, 826.6598116471556070E-6f,
    -0.011008071930708746f,-0.023014151355548934f,-0.029736402750934567f,-0.025405787911977455f,
    -0.006012006772274640f, 0.028729626071574525f, 0.074466890595619062f, 0.122757573409695370f,
    0.163145421379242955f, 0.186152844567746417f, 0.186152844567746417f, 0.163145421379242955f,
    0.122757573409695370f, 0.074466890595619062f, 0.028729626071574525f,-0.006012006772274640f,
    -0.025405787911977455f,-0.029736402750934567f,-0.023014151355548934f,-0.011008071930708746f,
    826.6598116471556070E-6f, 0.008882946305001046f, 0.011982063548666887f, 0.010936856250494802f,
    0.007644563345368599f, 0.004029285548698377f, 0.001348560352009071f,-14.30851541590154240E-6f
  };

  void initpoly() {
    if (arm_fir_interpolate_init_f32(
          &interpolation, INTERPOLATION, interpolation_taps,
          interpolation_coeffs, interpolation_state, AUDIO_BLOCK_SAMPLES
        )) {
      polyCapable = false;
      return;
    }
    if (arm_fir_decimate_init_f32(
          &decimation, interpolation_taps, INTERPOLATION,
          interpolation_coeffs, decimation_state, I_NUM_SAMPLES
        )) {
      polyCapable = false;
      return;
    }
    polyCapable = true;
    polyOn = true;
  }

  float LPF(float s, int i) {
    float ft = s * (1.0f / 1.3f) + (0.3f / 1.3f) * z0[i] - z1[i];
    ft = ft * alpha + z1[i];
    z1[i] = ft;
    z0[i] = s;
    return ft;
  }

  void compute_coeffs(float c) {
    if (c > MAX_FREQUENCY) {
      c = MAX_FREQUENCY;
    } else if (c < 5.0f) {
      c = 5.0f;
    }
    float wc = c
      * (float)(2.0f * 3.14159265358979323846f
                / ((float)INTERPOLATION * AUDIO_SAMPLE_RATE_EXACT));
    float wc2 = wc * wc;
    alpha = 0.9892f * wc - 0.4324f * wc2 + 0.1381f * wc * wc2
      - 0.0202f * wc2 * wc2;
    Qadjust = 1.006f + 0.0536f * wc - 0.095f * wc2 - 0.05f * wc2 * wc2;
  }

  bool resonating() {
    for (int i = 0; i < 4; i++) {
      if (fabsf(z0[i]) > 0.0001f) return true;
      if (fabsf(z1[i]) > 0.0001f) return true;
    }
    return false;
  }

  static inline float fast_exp2f(float x) {
    float i;
    float f = modff(x, &i);
    f *= 0.693147f / 256.0f;
    f += 1.0f;
    f *= f;
    f *= f;
    f *= f;
    f *= f;
    f *= f;
    f *= f;
    f *= f;
    f *= f;
    f = ldexpf(f, i);
    return f;
  }

  static inline float fast_tanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
  }

  float interpolation_state[(AUDIO_BLOCK_SAMPLES - 1)
                            + interpolation_taps / INTERPOLATION];
  arm_fir_interpolate_instance_f32 interpolation;
  float decimation_state[(AUDIO_BLOCK_SAMPLES * INTERPOLATION - 1)
                         + interpolation_taps];
  arm_fir_decimate_instance_f32 decimation;
  bool polyCapable = false;
  bool polyOn = false; // FIR is default after initpoly()
  float alpha = 1.0f;
  float beta[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  float z0[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  float z1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  float K = 1.0f;
  float Fbase = 1000;
  float Qadjust = 1.0f;
  float octaveScale = 1.0f;
  float pbg = 0.5f;
  float overdrive = 0.5f;
  float host_overdrive = 1.0f;
  float oldinput = 0;
  audio_block_f32_t *inputQueueArray[3];
};
