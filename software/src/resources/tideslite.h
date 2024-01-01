#include <stdint.h>

#define LUT_INCREMENTS_SIZE 97

const int16_t kOctave = 12 * 128;
const uint16_t kSlopeBits = 12;

enum TidesLiteFlagBitMask { FLAG_EOA = 1, FLAG_EOR = 2 };

struct TidesLiteSample {
  uint16_t unipolar;
  int16_t bipolar;
  uint8_t flags;
};

/*
import numpy as np

sample_rate = 16666

excursion = float(1 << 32)

a4_midi = 69
a4_pitch = 440.0
notes = np.arange(0 * 128.0, 12 * 128.0 + 16, 16) / 128.0
pitches = a4_pitch * 2 **((notes - a4_midi) / 12)
increments = excursion / sample_rate * pitches

increments.astype(int)
*/
const uint32_t lut_increments[] = {
    2106971, 2122239, 2137618, 2153108, 2168710, 2184425, 2200255, 2216199,
    2232258, 2248434, 2264727, 2281138, 2297668, 2314318, 2331089, 2347981,
    2364995, 2382133, 2399395, 2416782, 2434295, 2451935, 2469702, 2487599,
    2505625, 2523782, 2542070, 2560491, 2579046, 2597734, 2616559, 2635519,
    2654617, 2673854, 2693230, 2712746, 2732404, 2752204, 2772147, 2792235,
    2812469, 2832850, 2853377, 2874054, 2894881, 2915858, 2936988, 2958270,
    2979707, 3001300, 3023048, 3044954, 3067019, 3089244, 3111630, 3134178,
    3156890, 3179766, 3202808, 3226017, 3249394, 3272940, 3296657, 3320546,
    3344608, 3368845, 3393257, 3417846, 3442613, 3467560, 3492687, 3517996,
    3543489, 3569167, 3595031, 3621082, 3647321, 3673751, 3700373, 3727187,
    3754196, 3781401, 3808802, 3836402, 3864202, 3892204, 3920409, 3948818,
    3977432, 4006254, 4035285, 4064527, 4093980, 4123647, 4153528, 4183626,
    4213943};

/*
import numpy

WAVESHAPER_SIZE = 256

x = numpy.arange(0, WAVESHAPER_SIZE + 1) / (WAVESHAPER_SIZE / 2.0) - 1.0
x[-1] = x[-2]
sine = numpy.sin(8 * numpy.pi * x)
window = numpy.exp(-x * x * 4) ** 2
bipolar_fold = sine * window + numpy.arctan(3 * x) * (1 - window)
bipolar_fold /= numpy.abs(bipolar_fold).max()
(numpy.round(32767) * bipolar_fold).astype(int)

x = numpy.arange(0, WAVESHAPER_SIZE + 1) / float(WAVESHAPER_SIZE)
x[-1] = x[-2]
sine = numpy.sin(8 * numpy.pi * x)
window = numpy.exp(-x * x * 4) ** 2
unipolar_fold = (0.5 * sine + 2 * x) * window + numpy.arctan(4 * x) * (1 -
window) unipolar_fold /= numpy.abs(unipolar_fold).max() (numpy.round(32767 *
unipolar_fold)).astype(int)
*/

const int16_t wav_bipolar_fold[] = {
    -32767, -32701, -32634, -32566, -32496, -32425, -32353, -32279, -32204,
    -32129, -32053, -31977, -31901, -31825, -31749, -31674, -31600, -31526,
    -31453, -31379, -31305, -31229, -31150, -31068, -30980, -30886, -30783,
    -30671, -30546, -30409, -30258, -30091, -29910, -29713, -29503, -29281,
    -29049, -28812, -28572, -28335, -28106, -27890, -27693, -27520, -27374,
    -27261, -27180, -27133, -27117, -27129, -27160, -27203, -27246, -27274,
    -27271, -27222, -27108, -26911, -26617, -26212, -25683, -25026, -24239,
    -23327, -22300, -21176, -19978, -18738, -17489, -16273, -15131, -14106,
    -13240, -12572, -12135, -11953, -12041, -12403, -13028, -13893, -14960,
    -16179, -17485, -18805, -20059, -21161, -22027, -22574, -22730, -22433,
    -21639, -20321, -18479, -16132, -13326, -10133, -6644,  -2973,  752,
    4393,   7806,   10850,  13391,  15310,  16512,  16925,  16512,  15267,
    13223,  10446,  7040,   3136,   -1106,  -5510,  -9885,  -14039, -17784,
    -20947, -23377, -24951, -25584, -25230, -23886, -21592, -18430, -14523,
    -10025, -5117,  0,      5117,   10025,  14523,  18430,  21592,  23886,
    25230,  25584,  24951,  23377,  20947,  17784,  14039,  9885,   5510,
    1106,   -3136,  -7040,  -10446, -13223, -15267, -16512, -16925, -16512,
    -15310, -13391, -10850, -7806,  -4393,  -752,   2973,   6644,   10133,
    13326,  16132,  18479,  20321,  21639,  22433,  22730,  22574,  22027,
    21161,  20059,  18805,  17485,  16179,  14960,  13893,  13028,  12403,
    12041,  11953,  12135,  12572,  13240,  14106,  15131,  16273,  17489,
    18738,  19978,  21176,  22300,  23327,  24239,  25026,  25683,  26212,
    26617,  26911,  27108,  27222,  27271,  27274,  27246,  27203,  27160,
    27129,  27117,  27133,  27180,  27261,  27374,  27520,  27693,  27890,
    28106,  28335,  28572,  28812,  29049,  29281,  29503,  29713,  29910,
    30091,  30258,  30409,  30546,  30671,  30783,  30886,  30980,  31068,
    31150,  31229,  31305,  31379,  31453,  31526,  31600,  31674,  31749,
    31825,  31901,  31977,  32053,  32129,  32204,  32279,  32353,  32425,
    32496,  32566,  32634,  32701,  32701};

const int16_t wav_unipolar_fold[] = {
    0,     1405,  2797,  4165,  5496,  6779,  8003,  9157,  10232, 11219, 12110,
    12900, 13581, 14151, 14606, 14944, 15166, 15271, 15261, 15141, 14915, 14588,
    14166, 13659, 13073, 12419, 11706, 10944, 10146, 9321,  8481,  7638,  6803,
    5986,  5198,  4451,  3752,  3111,  2537,  2036,  1616,  1281,  1035,  883,
    827,   868,   1006,  1240,  1568,  1988,  2495,  3085,  3753,  4493,  5297,
    6158,  7069,  8021,  9007,  10017, 11043, 12077, 13110, 14133, 15141, 16124,
    17076, 17990, 18861, 19683, 20453, 21166, 21818, 22408, 22935, 23396, 23793,
    24125, 24394, 24601, 24749, 24841, 24881, 24871, 24816, 24722, 24592, 24431,
    24245, 24039, 23817, 23585, 23347, 23109, 22874, 22648, 22433, 22234, 22054,
    21896, 21762, 21654, 21574, 21523, 21502, 21511, 21551, 21621, 21720, 21847,
    22002, 22182, 22386, 22612, 22858, 23120, 23398, 23688, 23988, 24296, 24608,
    24923, 25238, 25552, 25861, 26164, 26459, 26744, 27019, 27281, 27529, 27764,
    27983, 28187, 28375, 28547, 28703, 28843, 28968, 29079, 29174, 29256, 29326,
    29383, 29430, 29466, 29494, 29514, 29527, 29534, 29536, 29535, 29531, 29525,
    29518, 29511, 29505, 29500, 29497, 29496, 29498, 29503, 29512, 29524, 29541,
    29561, 29586, 29614, 29646, 29682, 29722, 29765, 29811, 29860, 29912, 29966,
    30022, 30080, 30139, 30199, 30260, 30321, 30382, 30444, 30505, 30565, 30624,
    30683, 30740, 30796, 30850, 30903, 30955, 31005, 31053, 31099, 31144, 31187,
    31228, 31268, 31307, 31343, 31379, 31413, 31446, 31478, 31509, 31539, 31568,
    31597, 31624, 31652, 31678, 31705, 31731, 31757, 31782, 31808, 31833, 31858,
    31884, 31909, 31934, 31960, 31985, 32011, 32036, 32062, 32088, 32114, 32140,
    32166, 32192, 32218, 32244, 32270, 32296, 32322, 32348, 32374, 32399, 32425,
    32451, 32476, 32501, 32526, 32551, 32576, 32601, 32625, 32649, 32673, 32697,
    32720, 32744, 32767, 32767};

inline int16_t Interpolate1022(const int16_t *table, uint32_t phase) {
  int32_t a = table[phase >> 22];
  int32_t b = table[(phase >> 22) + 1];
  return a + ((b - a) * static_cast<int32_t>((phase >> 6) & 0xffff) >> 16);
}

inline int16_t Interpolate824(const int16_t *table, uint32_t phase) {
  int32_t a = table[phase >> 24];
  int32_t b = table[(phase >> 24) + 1];
  return a + ((b - a) * static_cast<int32_t>((phase >> 8) & 0xffff) >> 16);
}

uint32_t ComputePhaseIncrement(int16_t pitch) {
  int16_t num_shifts = 0;
  while (pitch < 0) {
    pitch += kOctave;
    --num_shifts;
  }
  while (pitch >= kOctave) {
    pitch -= kOctave;
    ++num_shifts;
  }
  // Lookup phase increment
  uint32_t a = lut_increments[pitch >> 4];
  uint32_t b = lut_increments[(pitch >> 4) + 1];
  uint32_t phase_increment = a + ((b - a) * (pitch & 0xf) >> 4);
  // Compensate for downsampling
  return num_shifts >= 0 ? phase_increment << num_shifts
                         : phase_increment >> -num_shifts;
}

int16_t ComputePitch(uint32_t phase_increment) {
  uint32_t first = lut_increments[0];
  uint32_t last = lut_increments[LUT_INCREMENTS_SIZE - 2];
  int16_t pitch = 0;

  if (phase_increment == 0) {
    phase_increment = 1;
  }

  while (phase_increment > last) {
    phase_increment >>= 1;
    pitch += kOctave;
  }

  while (phase_increment < first) {
    phase_increment <<= 1;
    pitch -= kOctave;
  }

  int i = 0;
  int j = LUT_INCREMENTS_SIZE - 1;
  while (j - i > 1) {
    int k = i + (j - i) / 2;
    uint32_t mid = lut_increments[k];
    if (phase_increment < mid) {
      j = k;
    } else {
      i = k;
    }
  }
  pitch += (i << 4);
  return pitch;
}


const uint32_t max_16 = 0xffff;
const uint32_t max_8 = 0xff;

uint32_t WarpPhase(uint16_t phase, uint16_t curve) {
  int32_t c = (curve - 32767) >> 8;
  bool flip = c < 0;
  if (flip)
    phase = max_16 - phase;
  uint32_t a = 128 * c * c;
  phase = (max_8 + a / max_8) * phase /
          ((max_16 + a / max_8 * phase / max_8) / max_8);
  if (flip)
    phase = max_16 - phase;
  return phase;
}

uint16_t ShapePhase(uint16_t phase, uint16_t attack_curve,
                    uint16_t decay_curve) {
  return phase < (1UL << 15)
             ? WarpPhase(phase << 1, attack_curve)
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

void ProcessSample(uint16_t slope, uint16_t shape, int16_t fold, uint32_t phase,
                   TidesLiteSample &sample) {
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
