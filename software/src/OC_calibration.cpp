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

FLASHMEM void calibration_reset() {
  memcpy(&OC::calibration_data, &kCalibrationDefaults, sizeof(OC::calibration_data));
  for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
    for (int i = 0; i < OCTAVES; ++i) {
      OC::calibration_data.dac.calibrated_octaves[ch][i] += DAC_OFFSET;
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
  { HELLO, "Setup: Calibrate", "Use defaults? ", select_help, start_footer, CALIBRATE_NONE, 0, OC::Strings::no_yes, 0, 1 },
  { CENTER_DISPLAY, "Center Display", "Pixel offset ", default_help_r, default_footer, CALIBRATE_DISPLAY, 0, nullptr, 0, 2 },

  #if defined(NORTHERNLIGHT) && !defined(IO_10V)
    { DAC_A_VOLT_3m, "DAC A 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_5,  "DAC A 9.6 volts", "-> 9.600V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_B_VOLT_3m, "DAC B 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_5,  "DAC B 9.6 volts", "-> 9.600V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_3m, "DAC C 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_5,  "DAC C 9.6 volts", "-> 9.600V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_3m, "DAC D 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_5,  "DAC D 9.6 volts", "-> 9.600V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  #elif defined(IO_10V) && !defined(VOR)
    { DAC_A_VOLT_3m, "DAC A 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_5,  "DAC A 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_B_VOLT_3m, "DAC B 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_5,  "DAC B 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_3m, "DAC C 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_5,  "DAC C 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_3m, "DAC D 0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_5,  "DAC D 8.0 volts", "-> 8.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 8, nullptr, 0, DAC::MAX_VALUE },
  #elif defined(VOR)
    { DAC_A_VOLT_3m, "DAC A  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_6,  "DAC A 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
    
    { DAC_B_VOLT_3m, "DAC B  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_6,  "DAC B 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_3m, "DAC C  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_6,  "DAC C 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_3m, "DAC D  0.0 volts", "-> 0.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, 0, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_6,  "DAC D 9.0 volts", "-> 9.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 9, nullptr, 0, DAC::MAX_VALUE },
  #else
    { DAC_A_VOLT_3m, "DAC A -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_A_VOLT_5,  "DAC A 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_B_VOLT_3m, "DAC B -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_B_VOLT_5,  "DAC B 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_C_VOLT_3m, "DAC C -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_C_VOLT_5,  "DAC C 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },
  
    { DAC_D_VOLT_3m, "DAC D -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_D_VOLT_5,  "DAC D 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },

#ifdef ARDUINO_TEENSY41
    { DAC_E_VOLT_3m, "DAC E -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_E_VOLT_5,  "DAC E 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },

    { DAC_F_VOLT_3m, "DAC F -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_F_VOLT_5,  "DAC F 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },

    { DAC_G_VOLT_3m, "DAC G -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_G_VOLT_5,  "DAC G 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },

    { DAC_H_VOLT_3m, "DAC H -3 volts", "-> -3.000V ", default_help_r, default_footer, CALIBRATE_OCTAVE, -3, nullptr, 0, DAC::MAX_VALUE },
    { DAC_H_VOLT_5,  "DAC H 5 volts", "->  5.000V ", long_press_hint, default_footer, CALIBRATE_OCTAVE, 5, nullptr, 0, DAC::MAX_VALUE },
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
    { ADC_PITCH_C2, "CV Scaling 1V", "CV1: Input 1V (C2)", long_press_hint, default_footer, CALIBRATE_ADC_1V, 0, nullptr, 0, 0 },
    { ADC_PITCH_C4, "CV Scaling 3V", "CV1: Input 3V (C4)", long_press_hint, default_footer, CALIBRATE_ADC_3V, 0, nullptr, 0, 0 },
  #endif
  
  // Changing screensaver to screen blank, and seconds to minutes
  { CALIBRATION_SCREENSAVER_TIMEOUT, "Screen Blank", "(minutes)", default_help_r, default_footer, CALIBRATE_SCREENSAVER, 0, nullptr, (OC::Ui::kLongPressTicks * 2 + 500) / 1000, SCREENSAVER_TIMEOUT_MAX_S },

  { CALIBRATION_EXIT, "Calibration complete", "Save values? ", select_help, end_footer, CALIBRATE_NONE, 0, OC::Strings::no_yes, 0, 1 }
};


FLASHMEM void calibration_draw(const CalibrationState &state) {
  const CalibrationStep *step = state.current_step;

  /*
  graphics.drawLine(0, 10, 127, 10);
  graphics.drawLine(0, 12, 127, 12);
  graphics.setPrintPos(1, 2);
  */
  menu::DefaultTitleBar::Draw();
  graphics.print(step->title);

  weegfx::coord_t y = menu::CalcLineY(0);

  static constexpr weegfx::coord_t kValueX = menu::kDisplayWidth - 30;

  graphics.setPrintPos(menu::kIndentDx, y + 2);
  switch (step->calibration_type) {
    case CALIBRATE_OCTAVE:
      if (state.auto_scale_set[step_to_channel(step->step)]) {
        graphics.drawBitmap8(menu::kDisplayWidth - 10, y + 13, 8, CHECK_ICON);
        graphics.setPrintPos(menu::kIndentDx, y + 2);
      }
    case CALIBRATE_SCREENSAVER:
    #ifdef VOR
    case CALIBRATE_VBIAS_BIPOLAR:
    case CALIBRATE_VBIAS_ASYMMETRIC:
    #endif
      graphics.print(step->message);
      graphics.setPrintPos(kValueX, y + 2);
      graphics.print((int)state.encoder_value, 5);
      menu::DrawEditIcon(kValueX, y, state.encoder_value, step->min, step->max);
      break;
      
    case CALIBRATE_ADC_OFFSET:
      graphics.print(step->message);
      graphics.setPrintPos(kValueX, y + 2);
      graphics.print((int)OC::ADC::value(static_cast<ADC_CHANNEL>(step->index)), 5);
      menu::DrawEditIcon(kValueX, y, state.encoder_value, step->min, step->max);
      break;

    case CALIBRATE_DISPLAY:
      graphics.print(step->message);
      graphics.setPrintPos(kValueX, y + 2);
      graphics.pretty_print((int)state.encoder_value, 2);
      menu::DrawEditIcon(kValueX, y, state.encoder_value, step->min, step->max);
      graphics.drawFrame(0, 0, 128, 64);
      break;

    case CALIBRATE_ADC_1V:
    case CALIBRATE_ADC_3V:
      graphics.setPrintPos(menu::kIndentDx, y + 2);
      graphics.print(step->message);
      y += menu::kMenuLineH;
      graphics.setPrintPos(menu::kIndentDx, y + 2);
      graphics.print((int)OC::ADC::value(ADC_CHANNEL_1), 2);
      if ( (state.adc_1v && step->calibration_type == CALIBRATE_ADC_1V) ||
           (state.adc_3v && step->calibration_type == CALIBRATE_ADC_3V) )
      {
        graphics.print("  (set)");
      }
      break;

    case CALIBRATE_NONE:
    default:
      if (CALIBRATION_EXIT != step->step) {
        graphics.setPrintPos(menu::kIndentDx, y + 2);
        graphics.print(step->message);
        if (step->value_str)
          graphics.print(step->value_str[state.encoder_value]);
      } else {
        graphics.setPrintPos(menu::kIndentDx, y + 2);

        if (calibration_data_loaded && state.used_defaults)
          graphics.print("Overwrite? ");
        else
          graphics.print("Save? ");

        if (step->value_str)
          graphics.print(step->value_str[state.encoder_value]);

      }
      break;
  }

  y += menu::kMenuLineH;
  graphics.setPrintPos(menu::kIndentDx, y + 2);
  if (step->help)
    graphics.print(step->help);
 
  // NJM: display encoder direction config on first and last screens
  if (step->step == HELLO || step->step == CALIBRATION_EXIT) {
      y += menu::kMenuLineH;
      graphics.setPrintPos(menu::kIndentDx, y + 2);
      graphics.print("Encoders: ");
      graphics.print(OC::Strings::encoder_config_strings[ OC::calibration_data.encoder_config() ]);
  }

  weegfx::coord_t x = menu::kDisplayWidth - 22;
  y = 2;
  for (int input = OC::DIGITAL_INPUT_1; input < OC::DIGITAL_INPUT_LAST; ++input) {
    uint8_t state = (digital_input_displays[input].getState() + 3) >> 2;
    if (state)
      graphics.drawBitmap8(x, y, 4, OC::bitmap_gate_indicators_8 + (state << 2));
    x += 5;
  }

  graphics.drawStr(1, menu::kDisplayHeight - menu::kFontHeight - 3, step->footer);

  static constexpr uint16_t step_width = (menu::kDisplayWidth << 8 ) / (CALIBRATION_STEP_LAST - 1);
  graphics.drawRect(0, menu::kDisplayHeight - 2, (state.step * step_width) >> 8, 2);
}

/* DAC output etc */ 

FLASHMEM void calibration_update(CalibrationState &state) {

  CONSTRAIN(state.encoder_value, state.current_step->min, state.current_step->max);
  const CalibrationStep *step = state.current_step;

  switch (step->calibration_type) {
    case CALIBRATE_NONE:
      DAC::set_all_octave(0);
      break;
    case CALIBRATE_OCTAVE:
      OC::calibration_data.dac.calibrated_octaves[step_to_channel(step->step)][step->index + DAC::kOctaveZero] =
        state.encoder_value;
      DAC::set_all_octave(step->index);
      break;
    #ifdef VOR
    case CALIBRATE_VBIAS_BIPOLAR:
      /* set 0V @ bipolar range */
      DAC::set_all_octave(5);
      OC::calibration_data.v_bias = (OC::calibration_data.v_bias & 0xFFFF0000) | state.encoder_value;
      DAC::set_Vbias(0xFFFF & OC::calibration_data.v_bias);
      break;
    case CALIBRATE_VBIAS_ASYMMETRIC:
      /* set 0V @ asym. range */
      DAC::set_all_octave(3);
      OC::calibration_data.v_bias = (OC::calibration_data.v_bias & 0xFFFF) | (state.encoder_value << 16);
      DAC::set_Vbias(OC::calibration_data.v_bias >> 16);
    break;
    #endif
    case CALIBRATE_ADC_OFFSET:
      OC::calibration_data.adc.offset[step->index] = state.encoder_value;
      DAC::set_all_octave(0);
      break;
    case CALIBRATE_ADC_1V:
      DAC::set_all_octave(1);
      break;
    case CALIBRATE_ADC_3V:
      DAC::set_all_octave(3);
      break;
    case CALIBRATE_DISPLAY:
      OC::calibration_data.display_offset = state.encoder_value;
      display::AdjustOffset(OC::calibration_data.display_offset);
      break;
    case CALIBRATE_SCREENSAVER:
      DAC::set_all_octave(0);
      OC::calibration_data.screensaver_timeout = state.encoder_value;
      break;
  }
}

} // namespace OC

/* misc */ 

uint32_t adc_average() {
  delay(OC_CORE_TIMER_RATE + 1);

  return
    OC::ADC::smoothed_raw_value(ADC_CHANNEL_1) + OC::ADC::smoothed_raw_value(ADC_CHANNEL_2) +
    OC::ADC::smoothed_raw_value(ADC_CHANNEL_3) + OC::ADC::smoothed_raw_value(ADC_CHANNEL_4);
}

// end
