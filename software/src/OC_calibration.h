#ifndef OC_CALIBRATION_H_
#define OC_CALIBRATION_H_

#include "OC_ADC.h"
#include "OC_config.h"
#include "OC_DAC.h"
#include "util/util_pagestorage.h"
#include "util/EEPROMStorage.h"

//#define VERBOSE_LUT
#ifdef VERBOSE_LUT
#define LUT_PRINTF(fmt, ...) serial_printf(fmt, ##__VA_ARGS__)
#else
#define LUT_PRINTF(x, ...) do {} while (0)
#endif

namespace OC {

#if defined(NORTHERNLIGHT) || defined(VOR)
static constexpr uint16_t DAC_OFFSET = 0;  // DAC offset, initial approx., ish (Easel card)
#else
static constexpr uint16_t DAC_OFFSET = 4890; // DAC offset, initial approx., ish --> -3.5V to 6V
#endif

#ifdef NORTHERNLIGHT
  static constexpr uint16_t _ADC_OFFSET = (uint16_t)((float)pow(2,OC::ADC::kAdcResolution)*1.0f);       // ADC offset @3.3V
#else
  static constexpr uint16_t _ADC_OFFSET = (uint16_t)((float)pow(2,OC::ADC::kAdcResolution)*0.6666667f); // ADC offset @2.2V
#endif

static constexpr unsigned kCalibrationAdcSmoothing = 4;

enum CALIBRATION_STEP {
  HELLO,
  CENTER_DISPLAY,

  #ifdef VOR
  DAC_A_VOLT_3m,  DAC_A_VOLT_6,
  DAC_B_VOLT_3m,  DAC_B_VOLT_6,
  DAC_C_VOLT_3m,  DAC_C_VOLT_6,
  DAC_D_VOLT_3m,  DAC_D_VOLT_6,
  V_BIAS_BIPOLAR, V_BIAS_ASYMMETRIC,
  #else
  DAC_A_VOLT_3m,  DAC_A_VOLT_5,
  DAC_B_VOLT_3m,  DAC_B_VOLT_5,
  DAC_C_VOLT_3m,  DAC_C_VOLT_5,
  DAC_D_VOLT_3m,  DAC_D_VOLT_5,
#ifdef ARDUINO_TEENSY41
  DAC_E_VOLT_3m,  DAC_E_VOLT_5,
  DAC_F_VOLT_3m,  DAC_F_VOLT_5,
  DAC_G_VOLT_3m,  DAC_G_VOLT_5,
  DAC_H_VOLT_3m,  DAC_H_VOLT_5,
#endif
  #endif

  CV_OFFSET_0, CV_OFFSET_1, CV_OFFSET_2, CV_OFFSET_3,
#ifdef ARDUINO_TEENSY41
  CV_OFFSET_4, CV_OFFSET_5, CV_OFFSET_6, CV_OFFSET_7,
#endif
  ADC_PITCH_C2, ADC_PITCH_C4,
  CALIBRATION_SCREENSAVER_TIMEOUT,
  CALIBRATION_EXIT,
  CALIBRATION_STEP_LAST,
  CALIBRATION_STEP_FINAL = ADC_PITCH_C4
};

enum CALIBRATION_TYPE {
  CALIBRATE_NONE,
  CALIBRATE_OCTAVE,
  #ifdef VOR
  CALIBRATE_VBIAS_BIPOLAR,
  CALIBRATE_VBIAS_ASYMMETRIC,
  #endif
  CALIBRATE_ADC_OFFSET,
  CALIBRATE_ADC_1V,
  CALIBRATE_ADC_3V,
  CALIBRATE_DISPLAY,
  CALIBRATE_SCREENSAVER,
};

struct CalibrationStep {
  CALIBRATION_STEP step;
  const char *title;
  const char *message;
  const char *help; // optional
  const char *footer;

  CALIBRATION_TYPE calibration_type;
  int index;

  const char * const *value_str; // if non-null, use these instead of encoder value
  int min, max;
};

static constexpr DAC_CHANNEL &step_to_channel(const int step) {
#ifdef ARDUINO_TEENSY41
  if (step >= DAC_H_VOLT_3m) return DAC_CHANNEL_H;
  if (step >= DAC_G_VOLT_3m) return DAC_CHANNEL_G;
  if (step >= DAC_F_VOLT_3m) return DAC_CHANNEL_F;
  if (step >= DAC_E_VOLT_3m) return DAC_CHANNEL_E;
#endif
  if (step >= DAC_D_VOLT_3m) return DAC_CHANNEL_D;
  if (step >= DAC_C_VOLT_3m) return DAC_CHANNEL_C;
  if (step >= DAC_B_VOLT_3m) return DAC_CHANNEL_B;
  /*if (step >= DAC_A_VOLT_3m)*/ 
  return DAC_CHANNEL_A;
}

struct CalibrationState {
  CALIBRATION_STEP step;
  const CalibrationStep *current_step;
  int encoder_value;

  SmoothedValue<uint32_t, kCalibrationAdcSmoothing> adc_sum;

  uint16_t adc_1v;
  uint16_t adc_3v;

  bool used_defaults;
  bool auto_scale_set[DAC_CHANNEL_LAST];
};

// Originally, this was a single bit that would reverse both encoders.
// With board revisisions >= 2c however, the pins of the right encoder are
// swapped, so additional configurations were added, but existing data
// calibration might have be updated.
enum EncoderConfig : uint32_t {
  ENCODER_CONFIG_NORMAL,
  ENCODER_CONFIG_R_REVERSED,
  ENCODER_CONFIG_L_REVERSED,
  ENCODER_CONFIG_LR_REVERSED,
  ENCODER_CONFIG_LAST
};

enum CalibrationFlags : uint32_t {
  CALIBRATION_FLAG_ENCODER_MASK = 0x3, // mask for the first two bits
  CALIBRATION_FLAG_FLIP_MASK = 0xc, // mask
  CALIBRATION_FLAG_FLIPSCREEN = 2, // bit index
  CALIBRATION_FLAG_FLIPCONTROLS = 3, // bit index
};

struct CalibrationData {
  static constexpr uint32_t FOURCC = FOURCC<'C', 'A', 'L', 1>::value;

  DAC::CalibrationData dac;
  ADC::CalibrationData adc;

  uint8_t display_offset;
  uint32_t flags;
  uint8_t screensaver_timeout; // 0: default, else seconds
  uint8_t reserved0[3];
#ifdef VOR
  /* less complicated this way than adding it to DAC::CalibrationData... */
  uint32_t v_bias;
#else
  uint32_t reserved1;
#endif

  bool flipcontrols() const {
    return (flags >> CALIBRATION_FLAG_FLIPCONTROLS) & 1;
  }
  bool flipscreen() const {
    return (flags >> CALIBRATION_FLAG_FLIPSCREEN) & 1;
  }
  uint32_t flipmode() const {
    return (flags & CALIBRATION_FLAG_FLIP_MASK) >> CALIBRATION_FLAG_FLIPSCREEN;
  }
  void cycle_flipmode() {
    flags = (flags & ~CALIBRATION_FLAG_FLIP_MASK) |
      ( ((flags & CALIBRATION_FLAG_FLIP_MASK) + (1 << CALIBRATION_FLAG_FLIPSCREEN))
       & CALIBRATION_FLAG_FLIP_MASK );
  }
  bool toggle_flipscreen() {
    flags ^= (1 << CALIBRATION_FLAG_FLIPSCREEN);

    return flipscreen();
  }
  EncoderConfig encoder_config() const {
  	return static_cast<EncoderConfig>(flags & CALIBRATION_FLAG_ENCODER_MASK);
  }

  EncoderConfig next_encoder_config() {
    uint32_t raw_config = ((flags & CALIBRATION_FLAG_ENCODER_MASK) + 1) % ENCODER_CONFIG_LAST;
    flags = (flags & ~CALIBRATION_FLAG_ENCODER_MASK) | raw_config;
    return static_cast<EncoderConfig>(raw_config);
  }
};

#ifndef ARDUINO_TEENSY41
// 4 channels of I/O
static_assert(sizeof(DAC::CalibrationData) == 88, "DAC::CalibrationData size changed!");
static_assert(sizeof(ADC::CalibrationData) == 12, "ADC::CalibrationData size changed!");
static_assert(sizeof(CalibrationData) == 116, "Calibration data size changed!");
#else
// 8 channels of I/O
static_assert(sizeof(DAC::CalibrationData) == 176, "DAC::CalibrationData size changed!");
static_assert(sizeof(ADC::CalibrationData) == 20, "ADC::CalibrationData size changed!");
static_assert(sizeof(CalibrationData) == 212, "Calibration data size changed!");
#endif

using CalibrationStorage = PageStorage<EEPROMStorage, EEPROM_CALIBRATIONDATA_START, EEPROM_CALIBRATIONDATA_END, CalibrationData>;

extern const CalibrationStep calibration_steps[CALIBRATION_STEP_LAST];
extern CalibrationData calibration_data;
extern bool calibration_data_loaded;

void calibration_load();
void calibration_save();
void calibration_update(CalibrationState &state);
void calibration_reset();
void calibration_draw(const CalibrationState &state);


}; // namespace OC

#endif // OC_CALIBRATION_H_
