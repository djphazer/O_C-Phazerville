#include "tideslite.h"

uint32_t WarpPhase(uint16_t phase, uint16_t curve) {
  int32_t c = (curve - 32767) >> 8;
  bool flip = c < 0;
  if (flip) phase = max_16 - phase;
  uint32_t a = 128 * c * c;
  phase = (max_8 + a / max_8) * phase
    / ((max_16 + a / max_8 * phase / max_8) / max_8);
  if (flip) phase = max_16 - phase;
  return phase;
}

uint16_t ShapePhase(
  uint16_t phase, uint16_t attack_curve, uint16_t decay_curve
) {
  return phase < (1UL << 15) ? WarpPhase(phase << 1, attack_curve)
                             : WarpPhase((0xffff - phase) << 1, decay_curve);
}

uint16_t ShapePhase(uint16_t phase, uint16_t shape) {
  uint32_t att = 0;
  uint32_t dec = 0;
  if (shape < 1 * 65536 / 4) {
    shape *= 4;
    att = 0;
    dec = 65535 - shape;
  } else if (shape < 2 * 65536 / 4) {
    // shape between -24576 and 81
    shape = (shape - 65536 / 4) * 4;
    att = shape;
    dec = shape;
  } else if (shape < 3 * 65536 / 4) {
    shape = (shape - 2 * 65536 / 4) * 4;
    att = 65535;
    dec = 65535 - shape;
  } else {
    shape = (shape - 3 * 65536 / 4) * 4;
    att = 65535 - shape;
    dec = shape;
  }
  return ShapePhase(phase, att, dec);
}

void ProcessSample(
  uint16_t slope,
  uint16_t shape,
  int16_t fold,
  uint32_t phase,
  TidesLiteSample& sample
) {
  uint32_t eoa = slope << 16;
  // uint32_t skewed_phase = phase;
  slope = slope ? slope : 1;
  uint32_t decay_factor = (32768 << kSlopeBits) / slope;
  uint32_t attack_factor = (32768 << kSlopeBits) / (65536 - slope);

  uint32_t skewed_phase = phase;
  if (phase <= eoa) {
    skewed_phase = (phase >> kSlopeBits) * decay_factor;
  } else {
    skewed_phase = ((phase - eoa) >> kSlopeBits) * attack_factor;
    skewed_phase += 1L << 31;
  }

  sample.unipolar = ShapePhase(skewed_phase >> 16, shape);
  sample.bipolar = ShapePhase(skewed_phase >> 15, shape) >> 1;
  if (skewed_phase >= (1UL << 31)) {
    sample.bipolar = -sample.bipolar;
  }

  sample.flags = 0;
  if (phase <= eoa) {
    sample.flags |= FLAG_EOR;
  } else {
    sample.flags |= FLAG_EOA;
  }

  if (fold > 0) {
    int32_t wf_gain = 2048;
    wf_gain += fold * (32767 - 1024) >> 14;
    int32_t wf_balance = fold;

    int32_t original = sample.unipolar;
    int32_t folded = Interpolate824(wav_unipolar_fold, original * wf_gain) << 1;
    sample.unipolar = original + ((folded - original) * wf_balance >> 15);

    original = sample.bipolar;
    folded = Interpolate824(wav_bipolar_fold, original * wf_gain + (1UL << 31));
    sample.bipolar = original + ((folded - original) * wf_balance >> 15);
  }
}
