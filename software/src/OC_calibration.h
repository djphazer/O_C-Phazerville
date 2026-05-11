#ifndef OC_CALIBRATION_H_
#define OC_CALIBRATION_H_

#include "OC_config.h"
#include "OC_ADC.h"
#include "OC_DAC.h"
#include "util/util_pagestorage.h"

//#define VERBOSE_LUT
#ifdef VERBOSE_LUT
#define LUT_PRINTF(fmt, ...) serial_printf(fmt, ##__VA_ARGS__)
#else
#define LUT_PRINTF(x, ...) do {} while (0)
#endif

namespace OC {

static constexpr uint16_t _ADC_OFFSET_NLM = 4095; // 0V == maximum 12-bit ADC value
#ifdef NORTHERNLIGHT
static constexpr uint16_t _ADC_OFFSET = (uint16_t)((float)pow(2,OC::ADC::kAdcResolution)*1.0f);   // ADC offset @3.3V
#else
static constexpr uint16_t _ADC_OFFSET = (uint16_t)((float)pow(2,OC::ADC::kAdcResolution)*0.6666667f); // ADC offset @2.2V
#endif

static constexpr unsigned kCalibrationAdcSmoothing = 4;

enum CALIBRATION_STEP {
  HELLO,
  CENTER_DISPLAY,

  #ifdef VOR
  DAC_A_VOLT_MIN, DAC_A_VOLT_HIGH,
  DAC_B_VOLT_MIN, DAC_B_VOLT_HIGH,
  DAC_C_VOLT_MIN, DAC_C_VOLT_HIGH,
  DAC_D_VOLT_MIN, DAC_D_VOLT_HIGH,
  V_BIAS_BIPOLAR, V_BIAS_ASYMMETRIC,
  #else
  DAC_A_VOLT_MIN, DAC_A_VOLT_HIGH,
  DAC_B_VOLT_MIN, DAC_B_VOLT_HIGH,
  DAC_C_VOLT_MIN, DAC_C_VOLT_HIGH,
  DAC_D_VOLT_MIN, DAC_D_VOLT_HIGH,
#ifdef ARDUINO_TEENSY41
  DAC_E_VOLT_MIN, DAC_E_VOLT_HIGH,
  DAC_F_VOLT_MIN, DAC_F_VOLT_HIGH,
  DAC_G_VOLT_MIN, DAC_G_VOLT_HIGH,
  DAC_H_VOLT_MIN, DAC_H_VOLT_HIGH,
#endif
  #endif

  ADC_OFFSETS,
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
  if (step >= DAC_H_VOLT_MIN) return DAC_CHANNEL_H;
  if (step >= DAC_G_VOLT_MIN) return DAC_CHANNEL_G;
  if (step >= DAC_F_VOLT_MIN) return DAC_CHANNEL_F;
  if (step >= DAC_E_VOLT_MIN) return DAC_CHANNEL_E;
#endif
  if (step >= DAC_D_VOLT_MIN) return DAC_CHANNEL_D;
  if (step >= DAC_C_VOLT_MIN) return DAC_CHANNEL_C;
  if (step >= DAC_B_VOLT_MIN) return DAC_CHANNEL_B;
  /*if (step >= DAC_A_VOLT_MIN)*/ 
  return DAC_CHANNEL_A;
}

struct CalibrationState {
  CALIBRATION_STEP step;
  const CalibrationStep *current_step;
  int encoder_value;

  uint16_t adc_1v;
  uint16_t adc_3v;

  bool used_defaults;
  bool auto_scale_set[DAC_CHANNEL_COUNT];
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

// bitmasks
enum CalibrationFlags : uint32_t {
  CALIBRATION_FLAG_ENCODER_MASK = 0b0011,
  CALIBRATION_FLAG_FLIP_MASK    = 0b1100,

  CALIBRATION_FLAG_FLIPSCREEN   = 1u << 2,
  CALIBRATION_FLAG_FLIPCONTROLS = 1u << 3,
  CALIBRATION_FLAG_BOOTCHOICE   = (0x3 << 4), // 2 bits, 4 choices
  // a signal for the wizard
  CALIBRATION_FLAG_START        = 1u << 31,
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

  void set_calstart(bool start = true) {
    if (start)
      flags |= CALIBRATION_FLAG_START;
    else
      flags &= ~CALIBRATION_FLAG_START;
  }
  bool get_calstart() const {
    return (flags & CALIBRATION_FLAG_START);
  }
  bool flipcontrols() const {
    return (flags & CALIBRATION_FLAG_FLIPCONTROLS);
  }
  bool flipscreen() const {
    return (flags & CALIBRATION_FLAG_FLIPSCREEN);
  }
  uint32_t bootchoice() const {
    return (flags & CALIBRATION_FLAG_BOOTCHOICE) >> 4;
  }
  void set_bootchoice(uint32_t choice) {
    flags = (flags & ~CALIBRATION_FLAG_BOOTCHOICE) | ((choice & 0x3) << 4);
  }
  // default behavior - flip both screen and I/O
  void toggle_flipmode() {
    flags = (flags & ~CALIBRATION_FLAG_FLIP_MASK) |
      ((flags & CALIBRATION_FLAG_FLIP_MASK) ? 0 : CALIBRATION_FLAG_FLIP_MASK);
  }
  // flip both bits independently
  void cycle_flipmode() {
    flags = (flags & ~CALIBRATION_FLAG_FLIP_MASK) |
      ( ((flags & CALIBRATION_FLAG_FLIP_MASK) + (CALIBRATION_FLAG_FLIPSCREEN))
       & CALIBRATION_FLAG_FLIP_MASK );
  }
  bool toggle_flipscreen() {
    flags ^= CALIBRATION_FLAG_FLIPSCREEN;

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

extern const CalibrationStep calibration_steps[CALIBRATION_STEP_LAST];
extern CalibrationData calibration_data;
extern bool calibration_data_loaded;

void calibration_load();
void calibration_save();
void calibration_reset();

}; // namespace OC

#endif // OC_CALIBRATION_H_
