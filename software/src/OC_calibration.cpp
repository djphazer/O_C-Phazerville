/*
*
* calibration menu:
*
* enter by pressing left encoder button during start up; use encoder switches to navigate.
*
*/

#include "OC_core.h"
#include "OC_apps.h"
#include "OC_DAC.h"
#include "OC_debug.h"
#include "OC_gpio.h"
#include "OC_ADC.h"
#include "OC_calibration.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ui.h"
#include "OC_options.h"
#include "HSicons.h"
#include "src/drivers/display.h"
#include "src/drivers/ADC/OC_util_ADC.h"
#include "util/util_debugpins.h"
#include "OC_calibration.h"
#include "VBiasManager.h"
namespace menu = OC::menu;

using OC::DAC;

namespace OC {

DMAMEM CalibrationStorage calibration_storage;
CalibrationData calibration_data;

bool calibration_data_loaded = false;

const CalibrationData kCalibrationDefaults = {
  // DAC
  { {
#ifdef NORTHERNLIGHT
    // T3.2 only
    {197, 6634, 13083, 19517, 25966, 32417, 38850, 45301, 51733, 58180, 64400},
    {197, 6634, 13083, 19517, 25966, 32417, 38850, 45301, 51733, 58180, 64400},
    {197, 6634, 13083, 19517, 25966, 32417, 38850, 45301, 51733, 58180, 64400},
    {197, 6634, 13083, 19517, 25966, 32417, 38850, 45301, 51733, 58180, 64400}
#elif defined(VOR)
    {1285, 7580, 13876, 20171, 26468, 32764, 39061, 45357, 51655, 57952, 64248},
    {1285, 7580, 13876, 20171, 26468, 32764, 39061, 45357, 51655, 57952, 64248},
    {1285, 7580, 13876, 20171, 26468, 32764, 39061, 45357, 51655, 57952, 64248},
    {1285, 7580, 13876, 20171, 26468, 32764, 39061, 45357, 51655, 57952, 64248}
#else
  #ifdef ARDUINO_TEENSY41
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
  #endif
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535},
    {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535}
#endif
    },
  },
  // ADC
  { {
#ifdef ARDUINO_TEENSY41
      _ADC_OFFSET, _ADC_OFFSET, _ADC_OFFSET, _ADC_OFFSET,
#endif
      _ADC_OFFSET, _ADC_OFFSET, _ADC_OFFSET, _ADC_OFFSET
    },
    0,  // pitch_cv_scale
    0   // pitch_cv_offset : unused
  },
  // display_offset
  SH1106_128x64_Driver::kDefaultOffset,
  OC_CALIBRATION_DEFAULT_FLAGS,
  SCREENSAVER_TIMEOUT_S,
  { 0, 0, 0 }, // reserved0
  #ifdef VOR
  DAC::VBiasBipolar | (DAC::VBiasAsymmetric << 16) // default v_bias values
  #else
  0 // reserved1
  #endif
};

const CalibrationData kNLMCalibrationDefaults = {
  // DAC
  { {
#ifdef ARDUINO_TEENSY41
    // Xenomorpher
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100},
    {200, 6500, 12900, 19200, 25500, 32700, 38000, 44300, 50600, 56900, 63100}
#else
    // cardOC or hOC with T40
    {390, 6800, 13200, 19650, 26100, 32500, 38900, 45400, 51800, 58200, 64600},
    {390, 6800, 13200, 19650, 26100, 32500, 38900, 45400, 51800, 58200, 64600},
    {390, 6800, 13200, 19650, 26100, 32500, 38900, 45400, 51800, 58200, 64600},
    {390, 6800, 13200, 19650, 26100, 32500, 38900, 45400, 51800, 58200, 64600}
#endif
    },
  },
  // ADC
  { {
#ifdef ARDUINO_TEENSY41
      _ADC_OFFSET_NLM, _ADC_OFFSET_NLM, _ADC_OFFSET_NLM, _ADC_OFFSET_NLM,
#endif
      _ADC_OFFSET_NLM, _ADC_OFFSET_NLM, _ADC_OFFSET_NLM, _ADC_OFFSET_NLM
    },
    0,  // pitch_cv_scale
    0   // pitch_cv_offset : unused
  },
  // display_offset
  SH1106_128x64_Driver::kDefaultOffset,
  OC_CALIBRATION_DEFAULT_FLAGS,
  SCREENSAVER_TIMEOUT_S,
  { 0, 0, 0 }, // reserved0
  0 // reserved1
};

const DAC::CalibrationData kDAC20VppDefaults = {
  {
  #ifdef ARDUINO_TEENSY41
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
  #endif
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610},
    {936, 7303, 13670, 20037, 26404, 32771, 39138, 45505, 51872, 58239, 64610}
  }
};

#if defined(NORTHERNLIGHT) || defined(VOR)
static constexpr uint16_t DAC_OFFSET = 0;  // DAC offset, initial approx., ish (Easel card)
#else
static constexpr uint16_t DAC_OFFSET = 4890; // DAC offset, initial approx., ish --> -3.5V to 6V
#endif

FLASHMEM void calibration_reset() {
  if (NorthernLightModular) {
    memcpy(&OC::calibration_data, &kNLMCalibrationDefaults, sizeof(OC::calibration_data));
  } else {
    memcpy(&OC::calibration_data, &kCalibrationDefaults, sizeof(OC::calibration_data));
    if (DAC_20Vpp) {
      memcpy(&OC::calibration_data.dac, &kDAC20VppDefaults, sizeof(OC::calibration_data.dac));
      return;
    }

    for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
      for (int i = 0; i < OCTAVES; ++i) {
        OC::calibration_data.dac.calibrated_octaves[ch][i] += DAC_OFFSET;
      }
    }
  }
}

FLASHMEM void calibration_load() {
  SERIAL_PRINTLN("Cal.Storage: PAGESIZE=%u, PAGES=%u, LENGTH=%u",
                 OC::CalibrationStorage::PAGESIZE, OC::CalibrationStorage::PAGES, OC::CalibrationStorage::LENGTH);

  calibration_reset();
  calibration_data_loaded = OC::calibration_storage.Load(OC::calibration_data);
  if (!calibration_data_loaded) {
    SERIAL_PRINTLN("No calibration data, using defaults");
  } else {
    SERIAL_PRINTLN("Calibration data loaded...");
  }

  // Fix-up left-overs from development
  if (!OC::calibration_data.adc.pitch_cv_scale) {
    SERIAL_PRINTLN("CV scale not set, using default");
    OC::calibration_data.adc.pitch_cv_scale = OC::ADC::kDefaultPitchCVScale;
  }

  if (!OC::calibration_data.screensaver_timeout)
    OC::calibration_data.screensaver_timeout = SCREENSAVER_TIMEOUT_S;
}

FLASHMEM void calibration_save() {
  SERIAL_PRINTLN("Saving calibration data");
  OC::calibration_storage.Save(OC::calibration_data);

  uint32_t start = millis();
  while(millis() < start + SETTINGS_SAVE_TIMEOUT_MS) {
    GRAPHICS_BEGIN_FRAME(true);
    graphics.setPrintPos(13, 18);
    graphics.print("Calibration saved");
    graphics.setPrintPos(31, 27);
    graphics.print("to EEPROM!");
    GRAPHICS_END_FRAME();
  }
}

DigitalInputDisplay digital_input_displays[4];

//        128/6=21                  |                     |
const char * const start_footer   = "[CANCEL]         [OK]";
const char * const end_footer     = "[PREV]         [EXIT]";
const char * const default_footer = "[PREV]         [NEXT]";
const char * const default_help_r = "[R] => Adjust";
#ifdef ARDUINO_TEENSY41
const char * const long_press_hint = "Hold [B] to set";
#else
const char * const long_press_hint = "Hold [DOWN] to set";
#endif
const char * const select_help    = "[R] => Select";

const CalibrationStep calibration_steps[CALIBRATION_STEP_LAST] = {
  { HELLO, "Setup: Calibrate", "Start fresh? ", select_help, start_footer, CALIBRATE_NONE, 0, OC::Strings::no_yes, 0, 1 },
  { CENTER_DISPLAY, "Center Display", "Pixel offset ", default_help_r, default_footer, CALIBRATE_DISPLAY, 0, nullptr, 0, 2 },

  #if defined(IO_10V) && !defined(VOR)
    { DAC_A_VOLT_MIN, "DAC A 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_HIGH,  "DAC A 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_B_VOLT_MIN, "DAC B 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_HIGH,  "DAC B 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_MIN, "DAC C 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_HIGH,  "DAC C 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_MIN, "DAC D 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_HIGH,  "DAC D 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  #elif defined(VOR)
    { DAC_A_VOLT_MIN, "DAC A  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_HIGH,  "DAC A 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
    
    { DAC_B_VOLT_MIN, "DAC B  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_HIGH,  "DAC B 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_MIN, "DAC C  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_HIGH,  "DAC C 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_MIN, "DAC D  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_HIGH,  "DAC D 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  #else
    { DAC_A_VOLT_MIN, "DAC A (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_HIGH,  "DAC A (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_B_VOLT_MIN, "DAC B (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_HIGH,  "DAC B (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_MIN, "DAC C (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_HIGH,  "DAC C (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_MIN, "DAC D (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_HIGH,  "DAC D (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },

#ifdef ARDUINO_TEENSY41
    { DAC_E_VOLT_MIN, "DAC E (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_E_VOLT_HIGH,  "DAC E (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },

    { DAC_F_VOLT_MIN, "DAC F (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_F_VOLT_HIGH,  "DAC F (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },

    { DAC_G_VOLT_MIN, "DAC G (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_G_VOLT_HIGH,  "DAC G (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },

    { DAC_H_VOLT_MIN, "DAC H (min)", "", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_H_VOLT_HIGH,  "DAC H (high)", "", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
#endif
  #endif

  #ifdef VOR
    { V_BIAS_BIPOLAR, "0.000V: bipolar", "--> 0.000V", default_help_r, default_footer, CALIBRATE_VBIAS_BIPOLAR, 0, nullptr, 0, 4095 },
    { V_BIAS_ASYMMETRIC, "0.000V: asym.", "--> 0.000V", default_help_r, default_footer, CALIBRATE_VBIAS_ASYMMETRIC, 0, nullptr, 0, 4095 },
  #endif

  { CV_OFFSET_0, "ADC CV1", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_1, nullptr, 0, 4095 },
  { CV_OFFSET_1, "ADC CV2", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_2, nullptr, 0, 4095 },
  { CV_OFFSET_2, "ADC CV3", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_3, nullptr, 0, 4095 },
  { CV_OFFSET_3, "ADC CV4", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_4, nullptr, 0, 4095 },
#ifdef ARDUINO_TEENSY41
  { CV_OFFSET_4, "ADC CV5", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_5, nullptr, 0, 4095 },
  { CV_OFFSET_5, "ADC CV6", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_6, nullptr, 0, 4095 },
  { CV_OFFSET_6, "ADC CV7", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_7, nullptr, 0, 4095 },
  { CV_OFFSET_7, "ADC CV8", "ADC value at 0V", default_help_r, default_footer, CALIBRATE_ADC_OFFSET, ADC_CHANNEL_8, nullptr, 0, 4095 },
#endif

  #if defined(NORTHERNLIGHT) && !defined(IO_10V)
    { ADC_PITCH_C2, "ADC cal. octave #1", "CV1: Input 1.2V", long_press_hint, default_footer, CALIBRATE_ADC_1V, 0, nullptr, 0, 0 },
    { ADC_PITCH_C4, "ADC cal. octave #3", "CV1: Input 3.6V", long_press_hint, default_footer, CALIBRATE_ADC_3V, 0, nullptr, 0, 0 },
  #else
    { ADC_PITCH_C2, "ADC octave +1", "CV1: Input 1.%dV", long_press_hint, default_footer, CALIBRATE_ADC_1V, 1, nullptr, 0, 0 },
    { ADC_PITCH_C4, "ADC octave +3", "CV1: Input 3.%dV", long_press_hint, default_footer, CALIBRATE_ADC_3V, 3, nullptr, 0, 0 },
  #endif

  { CALIBRATION_SCREENSAVER_TIMEOUT, "Screen Blank", "(minutes)", default_help_r, default_footer, CALIBRATE_SCREENSAVER, 0, nullptr, (OC::Ui::kLongPressTicks * 2 + 500) / 1000, SCREENSAVER_TIMEOUT_MAX_S },

  { CALIBRATION_EXIT, "Calibration complete", "Save values? ", select_help, end_footer, CALIBRATE_NONE, 0, OC::Strings::no_yes, 0, 1 }
};


} // namespace OC
