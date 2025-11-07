// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <Arduino.h>
#include <EEPROM.h>
#include "HSUtils.h"
#include "OC_apps.h"
#include "OC_calibration.h"
#include "OC_core.h"
#include "OC_ui.h"
#include "HSApplication.h"
#include "OC_strings.h"
#include "src/drivers/display.h"
#ifdef PEWPEWPEW
#include "util/pewpewsplash.h"
#endif

extern "C" void _reboot_Teensyduino_();
using namespace OC;

class Settings : public HSApplication {
public:
  bool reflash = false;
  bool calibration_mode = false;
  bool calibration_complete = true;
  bool cal_save_q = false;
  int current_octave = 0; // for fine-tuning DAC points
  OC::DigitalInputDisplay digital_input_displays[4];
  OC::TickCount tick_count;

  OC::CalibrationState calstate = {
    OC::HELLO,
    &OC::calibration_steps[OC::HELLO],
    0, // "use defaults: no"
  };

  void Start() { }
  void Resume() {
    if (calibration_mode && !calibration_complete) {
      // restart calibration if you exit and come back
      StartCalibration();
    }
  }
  void Suspend() {
    if (cal_save_q) {
      OC::calibration_save();
      cal_save_q = false;
    }
  }

  void SwitchToStep(int direction) {

      if (calstate.current_step->calibration_type == CALIBRATE_OCTAVE && !calstate.used_defaults) {
        // fine-tuning for CALIBRATE_DAC
        int octave = current_octave + direction;
        if ( !(octave < 0 || octave > min(OCTAVES, calstate.current_step->index + 7)) ) {
          current_octave = octave;
          calstate.encoder_value =
              OC::calibration_data.dac.calibrated_octaves[step_to_channel(calstate.step)][current_octave];
          return;
        }
      }

      CALIBRATION_STEP index = static_cast<CALIBRATION_STEP>(calstate.step + direction);
      CONSTRAIN(index, CENTER_DISPLAY, CALIBRATION_EXIT);
      const CalibrationStep *next_step = &calibration_steps[index];
      if (next_step != calstate.current_step)
      {
        calstate.step = index;
        const DAC_CHANNEL chan = step_to_channel(next_step->step);
        SERIAL_PRINTLN("%s (%d)", next_step->title, chan);

        // Special cases on exit current step
        if (calstate.used_defaults
          && CALIBRATE_OCTAVE == calstate.current_step->calibration_type
          && calstate.current_step->index > 5) {
          // always apply interpolation when leaving a high DAC point
          InterpolateChannel(step_to_channel(calstate.current_step->step));
        }
        switch (calstate.current_step->step) {
          case HELLO:
            if (calstate.encoder_value) {
              SERIAL_PRINTLN("Reset to defaults...");
              uint32_t flags = OC::calibration_data.flags & CALIBRATION_FLAG_ENCODER_MASK;
              OC::calibration_reset();
              OC::calibration_data.flags |= flags; // preserve encoder config
              calstate.used_defaults = true;
            }
            break;
          case DAC_A_VOLT_HIGH:
            if (calstate.used_defaults) {
              // copy DAC A to the rest of them, to make life easier
              // WARNING: this doesn't work in flipped mode!
              for (int ch = 1; ch < DAC_CHANNEL_LAST; ++ch) {
                for (int i = 0; i < OCTAVES; ++i) {
                  OC::calibration_data.dac.calibrated_octaves[ch][i] = OC::calibration_data.dac.calibrated_octaves[0][i];
                }
              }
            }
            break;
          case ADC_PITCH_C4:
            if (calstate.adc_1v && calstate.adc_3v) {
              OC::ADC::CalibratePitch(calstate.adc_1v, calstate.adc_3v);
              SERIAL_PRINTLN("ADC SCALE 1V=%d, 3V=%d -> %d",
                             calstate.adc_1v, calstate.adc_3v,
                             OC::calibration_data.adc.pitch_cv_scale);
            }
            break;

          default: break;
        }

        // Setup next step
        switch (next_step->calibration_type) {
        case CALIBRATE_OCTAVE:
          current_octave = next_step->index;
          calstate.encoder_value =
              OC::calibration_data.dac.calibrated_octaves[chan][current_octave];
            #ifdef VOR
            /* set 0V @ unipolar range */
            DAC::set_Vbias(DAC::VBiasUnipolar);
            #endif
          break;

        #ifdef VOR
        case CALIBRATE_VBIAS_BIPOLAR:
          calstate.encoder_value = (0xFFFF & OC::calibration_data.v_bias); // bipolar = lower 2 bytes
        break;
        case CALIBRATE_VBIAS_ASYMMETRIC:
          calstate.encoder_value = (OC::calibration_data.v_bias >> 16);  // asymmetric = upper 2 bytes
        break;
        #endif

        case CALIBRATE_ADC_OFFSET: // set ADC zero-point offset
          if (calstate.used_defaults) // start fresh? auto-cal
            calstate.encoder_value = OC::ADC::smoothed_raw_value(static_cast<ADC_CHANNEL>(next_step->index));
          else
            calstate.encoder_value = OC::calibration_data.adc.offset[next_step->index];

          #ifdef VOR
          DAC::set_Vbias(DAC::VBiasUnipolar);
          #endif
          break;
        case CALIBRATE_DISPLAY:
          calstate.encoder_value = OC::calibration_data.display_offset;
          break;

        case CALIBRATE_ADC_1V:
        case CALIBRATE_ADC_3V:
          SERIAL_PRINTLN("offset=%d", OC::calibration_data.adc.offset[ADC_CHANNEL_1]);
          break;

        case CALIBRATE_SCREENSAVER:
          calstate.encoder_value = OC::calibration_data.screensaver_timeout;
          SERIAL_PRINTLN("timeout=%d", calstate.encoder_value);
          break;

        case CALIBRATE_NONE:
        default:
          if (CALIBRATION_EXIT != next_step->step) {
            calstate.encoder_value = 0;
          } else {
            // Make the default "Save: no" if the calibration data was reset
            // manually, but only if calibration data was actually loaded from
            // EEPROM
            if (calstate.used_defaults && OC::calibration_data_loaded)
              calstate.encoder_value = 0;
            else
              calstate.encoder_value = 1;
          }
        }
        calstate.current_step = next_step;
      }
  }

  void InterpolateChannel(int ch) {
    const int idxlow = 0;
    const int idxhigh = current_octave;
    uint32_t value = OC::calibration_data.dac.calibrated_octaves[ch][idxlow];
    const uint16_t second = OC::calibration_data.dac.calibrated_octaves[ch][idxhigh];
    int interval = (second - value) / (idxhigh - idxlow);

    for (int i = idxlow+1; i < OCTAVES + 1; ++i) {
      value += interval;
      if (value > 0xFFFF) value = 0xFFFF;
      OC::calibration_data.dac.calibrated_octaves[ch][i] = value;
    }

    calstate.auto_scale_set[ch] = true;
  }

  void Controller()
  {
    if (calibration_mode && !calibration_complete)
    {
      uint32_t ticks = tick_count.Update();
      digital_input_displays[0].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_1>());
      digital_input_displays[1].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_2>());
      digital_input_displays[2].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_3>());
      digital_input_displays[3].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_4>());

      // moved from calibration_update()
      const CalibrationStep *step = calstate.current_step;

      switch (step->calibration_type) {
        case CALIBRATE_NONE:
          DAC::set_all_octave(0);
          break;
        case CALIBRATE_OCTAVE:
          OC::calibration_data.dac.calibrated_octaves[step_to_channel(step->step)][current_octave] =
            calstate.encoder_value;
          DAC::set_all_octave((current_octave - DAC::kOctaveZero)*(1+DAC_20Vpp));
          break;
        #ifdef VOR
        case CALIBRATE_VBIAS_BIPOLAR:
          /* set 0V @ bipolar range */
          DAC::set_all_octave(5);
          OC::calibration_data.v_bias = (OC::calibration_data.v_bias & 0xFFFF0000) | calstate.encoder_value;
          DAC::set_Vbias(0xFFFF & OC::calibration_data.v_bias);
          break;
        case CALIBRATE_VBIAS_ASYMMETRIC:
          /* set 0V @ asym. range */
          DAC::set_all_octave(3);
          OC::calibration_data.v_bias = (OC::calibration_data.v_bias & 0xFFFF) | (calstate.encoder_value << 16);
          DAC::set_Vbias(OC::calibration_data.v_bias >> 16);
        break;
        #endif
        case CALIBRATE_ADC_OFFSET:
          OC::calibration_data.adc.offset[step->index] = calstate.encoder_value;
          DAC::set_all_octave(0);
          break;
        case CALIBRATE_ADC_1V:
          DAC::set_all_octave(1);
          break;
        case CALIBRATE_ADC_3V:
          DAC::set_all_octave(3);
          break;
        case CALIBRATE_DISPLAY:
          OC::calibration_data.display_offset = calstate.encoder_value;
          display::AdjustOffset(OC::calibration_data.display_offset);
          break;
        case CALIBRATE_SCREENSAVER:
          DAC::set_all_octave(0);
          OC::calibration_data.screensaver_timeout = calstate.encoder_value;
          break;
      }

      return;
    }

    if (CORE::ticks % 3200 == 0) {
      pick_left = random(8);
      pick_right = random(8);
    }

  }

  const uint8_t *iconography[8] = {
    PhzIcons::voltage, ZAP_ICON,
    PhzIcons::pigeons, PhzIcons::camels,
    PhzIcons::legoFace, PhzIcons::tb3P0,
    PhzIcons::drLoFi, PhzIcons::umbrella,
  };
  int pick_left = 0, pick_right = 0;

  void View() {
      if (calibration_mode) {
        DrawCalibration();
        return;
      }

      gfxHeader("Setup/About");
      gfxIcon(80, 0, OC::calibration_data.flipscreen() ? DOWN_ICON : UP_ICON);
      gfxIcon(90, 0, OC::calibration_data.flipcontrols() ? LEFT_ICON : RIGHT_ICON);

      #if defined(ARDUINO_TEENSY40)
      gfxPrint(100, 0, "T4.0");
      //gfxPrint(0, 45, "E2END="); gfxPrint(E2END);
      #elif defined(ARDUINO_TEENSY41)
      gfxPrint(100, 0, "T4.1");
      #else
      gfxPrint(100, 0, "T3.2");
      #endif

      gfxIcon(0, 15, iconography[pick_left]);
      gfxIcon(120, 15, iconography[pick_right]);
      #ifdef PEWPEWPEW
      gfxPrint(21, 15, "PEW! PEW! PEW!");
      #else
      gfxPrint(12, 15, OC::Strings::RELEASE_NAME);
      #endif
      gfxIcon(0, 25, PhzIcons::full_book);
      gfxPrint(10, 25, OC::Strings::VERSION);
      gfxIcon(0, 35, PhzIcons::runglBook);
      gfxPrint(10, 35, OC::Strings::BUILD_TAG);
      gfxIcon(0, 45, PhzIcons::frontBack);
      gfxPrint(10, 45, "github.com/djphazer");
      gfxPrint(0, 55, reflash ? "[Reflash]" : "[CALIBRATE]   [RESET]");
  }

  void DrawCalibration() {
    const OC::CalibrationStep *step = calstate.current_step;

    menu::DefaultTitleBar::Draw();
    graphics.print(step->title);

    weegfx::coord_t y = menu::CalcLineY(0);

    static constexpr weegfx::coord_t kValueX = menu::kDisplayWidth - 30;

    gfxPos(menu::kIndentDx, y + 2);
    switch (step->calibration_type) {
      case CALIBRATE_OCTAVE:
      {
        if (calstate.auto_scale_set[step_to_channel(step->step)]) {
          graphics.drawBitmap8(menu::kDisplayWidth - 10, y + 13, 8, CHECK_ICON);
          gfxPos(menu::kIndentDx, y + 2);
        }
        int voltage = (current_octave - DAC::kOctaveZero) * (NorthernLightModular? 12: 10) * (1+DAC_20Vpp);
        graphics.printf("-> %d.%d00V", voltage / 10, voltage % 10);
        gfxPos(kValueX, y + 2);
        graphics.print((int)calstate.encoder_value, 5);
        menu::DrawEditIcon(kValueX, y, calstate.encoder_value, step->min, step->max);
        break;
      }

      case CALIBRATE_SCREENSAVER:
      #ifdef VOR
      case CALIBRATE_VBIAS_BIPOLAR:
      case CALIBRATE_VBIAS_ASYMMETRIC:
      #endif
        graphics.print(step->message);
        gfxPos(kValueX, y + 2);
        graphics.print((int)calstate.encoder_value, 5);
        menu::DrawEditIcon(kValueX, y, calstate.encoder_value, step->min, step->max);
        break;

      case CALIBRATE_ADC_OFFSET:
        graphics.print(step->message);
        gfxPos(kValueX, y + 2);
        graphics.print((int)OC::ADC::value(static_cast<ADC_CHANNEL>(step->index)), 5);
        menu::DrawEditIcon(kValueX, y, calstate.encoder_value, step->min, step->max);
        break;

      case CALIBRATE_DISPLAY:
        graphics.print(step->message);
        gfxPos(kValueX, y + 2);
        graphics.pretty_print((int)calstate.encoder_value, 2);
        menu::DrawEditIcon(kValueX, y, calstate.encoder_value, step->min, step->max);
        graphics.drawFrame(0, 0, 128, 64);
        break;

      case CALIBRATE_ADC_1V:
      case CALIBRATE_ADC_3V:
        gfxPos(menu::kIndentDx, y + 2);
        graphics.printf(step->message, (NorthernLightModular*2*step->index));
        y += menu::kMenuLineH;
        gfxPos(menu::kIndentDx, y + 2);
        graphics.print((int)OC::ADC::value(ADC_CHANNEL_1), 2);
        if ( (calstate.adc_1v && step->calibration_type == CALIBRATE_ADC_1V) ||
             (calstate.adc_3v && step->calibration_type == CALIBRATE_ADC_3V) )
        {
          graphics.print("  (set)");
        }
        break;

      case CALIBRATE_NONE:
      default:
        if (CALIBRATION_EXIT != step->step) {
          gfxPos(menu::kIndentDx, y + 2);
          graphics.print(step->message);
          if (step->value_str)
            graphics.print(step->value_str[calstate.encoder_value]);
        } else {
          gfxPos(menu::kIndentDx, y + 2);

          if (calibration_data_loaded && calstate.used_defaults)
            graphics.print("Overwrite? ");
          else
            graphics.print("Save? ");

          if (step->value_str)
            graphics.print(step->value_str[calstate.encoder_value]);

        }
        break;
    }

    y += menu::kMenuLineH;
    gfxPos(menu::kIndentDx, y + 2);
    if (step->help)
      graphics.print(step->help);

    // NJM: display encoder direction config on first and last screens
    if (step->step == HELLO || step->step == CALIBRATION_EXIT) {
        y += menu::kMenuLineH;
        gfxPos(menu::kIndentDx, y + 2);
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
    graphics.drawRect(0, menu::kDisplayHeight - 2, (calstate.step * step_width) >> 8, 2);

  }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////

    void HandleUiEvent(const UI::Event &event) {
      if (!calibration_mode) {
        if (event.control == OC::CONTROL_ENCODER_L) {
          reflash = (event.value > 0);
        }
        if (event.control == OC::CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_PRESS) {
          if (reflash)
            Reflash();
          else
            StartCalibration();
        }
        if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) FactoryReset();

        // dual-press UP+DOWN / A+B to flip screen
        if ( event.type == UI::EVENT_BUTTON_DOWN &&
            (event.mask == (OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) ) {
          OC::calibration_data.toggle_flipmode();
          display::SetFlipMode(OC::calibration_data.flipscreen());
          cal_save_q = true;
        }

        return;
      }

      // event handling from Ui::Calibrate()
      if (event.type == UI::EVENT_BUTTON_DOWN) {
        // act-on-press for right encoder
        if (event.control == CONTROL_BUTTON_R) {
          if (calstate.step < CALIBRATION_EXIT) {
            SwitchToStep(1); // step forward
          } else
            calibration_complete = true;

          // ignore release and long-press during calibration
          OC::ui.SetButtonIgnoreMask();
        }
      } else {
        // press, long-press, or encoder movements
        switch (event.control) {
          case CONTROL_BUTTON_L:
            if (calstate.step == HELLO) calibration_complete = 1; // Way out --jj
            if (calstate.step > CENTER_DISPLAY)
              SwitchToStep(-1); // step backward
            break;
          case CONTROL_BUTTON_R:
            break;

          case CONTROL_ENCODER_L:
            if (calstate.step > HELLO) {
              SwitchToStep(event.value);
            }
            break;
          case CONTROL_ENCODER_R: {
            int delta = event.value;
            if (event.mask & OC::CONTROL_BUTTON_X) delta *= 16;
            else if (event.mask & OC::CONTROL_BUTTON_Y) delta *= 32;
            calstate.encoder_value = constrain(
              calstate.encoder_value + delta,
              calstate.current_step->min,
              calstate.current_step->max
            );
            break;
          }

          case CONTROL_BUTTON_UP:
          case CONTROL_BUTTON_DOWN:
            if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
              const CalibrationStep *step = calstate.current_step;

              // long-press B/DOWN to measure ADC points
              switch (step->step) {
                case ADC_PITCH_C2:
                  calstate.adc_1v = OC::ADC::value(ADC_CHANNEL_1);
                  break;
                case ADC_PITCH_C4:
                  calstate.adc_3v = OC::ADC::value(ADC_CHANNEL_1);
                  break;
                default: break;
              }

              // long-press to set ADC zero-point offset
              if (CALIBRATE_ADC_OFFSET == step->calibration_type) {
                calstate.encoder_value = OC::ADC::smoothed_raw_value(static_cast<ADC_CHANNEL>(step->index));
              }

              // long-press DOWN to auto-scale DAC values on current channel
              if (step->calibration_type == CALIBRATE_OCTAVE && current_octave > 0) {
                InterpolateChannel(step_to_channel(step->step));
              }
              break;
            }

            // regular press cycles thru encoder orientations on first/last screen
            if (calstate.step == HELLO || calstate.step == CALIBRATION_EXIT)
            {
              OC::ui.configure_encoders(calibration_data.next_encoder_config());
              cal_save_q = true;
            }

            break;
          default:
            break;
        }
      }

      if (calibration_complete) {
        if (calstate.encoder_value) {
          SERIAL_PRINTLN("Calibration complete");
          OC::calibration_save();
          cal_save_q = false;
        } else {
          SERIAL_PRINTLN("Calibration complete (but don't save)");
        }

        OC::ui.set_screensaver_timeout(OC::calibration_data.screensaver_timeout);
        calibration_mode = false;
      }
    }
    void StartCalibration() {
        // migrated from OC::ui.Calibrate();

        calstate = {
          OC::HELLO,
          &OC::calibration_steps[OC::HELLO],
          OC::calibration_data_loaded ? 0 : 1, // "use defaults: no" if data loaded
        };
        calstate.used_defaults = false;

        for (auto &did : digital_input_displays)
          did.Init();

        tick_count.Init();

        OC::ui.encoder_enable_acceleration(OC::CONTROL_ENCODER_R, true);
        #ifdef VOR
        {
          VBiasManager *vb = vb->get();
          vb->SetState(VBiasManager::UNI);
        }
        #endif

        calibration_complete = false;
        calibration_mode = true;
    }
    void Reflash() {
      uint32_t start = millis();
      while(millis() < start + SETTINGS_SAVE_TIMEOUT_MS) {
        GRAPHICS_BEGIN_FRAME(true);
        gfxPos(5, 10);
        graphics.print("Flash Upgrade Mode");
        gfxPos(5, 19);
        graphics.print("(use Teensy Loader)");
        GRAPHICS_END_FRAME();
      }

      // special teensy_reboot command
      _reboot_Teensyduino_();
    }

    void FactoryReset() {
        OC::apps::Init(1);
    }

};

DMAMEM Settings Settings_instance;

// App stubs
void Settings_init() {
    Settings_instance.BaseStart();
}

// Not using O_C Storage
static constexpr size_t Settings_storageSize() {return 0;}
static size_t Settings_save(void *storage) {return 0;}
static size_t Settings_restore(const void *storage) {return 0;}

void Settings_isr() {
  // Usually you would call BaseController, but Calibration is a special case.
  // We don't want the automatic frame Load() and Send() calls from this App.
  Settings_instance.Controller();
  return;
}

void Settings_handleAppEvent(OC::AppEvent event) {
  if (event == OC::APP_EVENT_RESUME) {
    Settings_instance.Resume();
  }
  if (event == OC::APP_EVENT_SUSPEND) {
    Settings_instance.Suspend();
  }
}

void Settings_loop() {
} // Deprecated

void Settings_menu() {
    Settings_instance.BaseView();
}

void Settings_screensaver() {
#ifdef PEWPEWPEW
    for (int i = 0; i < (pewpew_width * pewpew_height / 64); ++i) {
      // TODO: the problem here is that one byte in XBM is a row of 8 pixels,
      //       while one byte in the framebuffer is a column of 8 pixels
      gfxBitmap((i & 0x1)*64, (i>>1)*8, 64, pewpew_bits + i*64);
    }
#endif
}

void Settings_handleButtonEvent(const UI::Event &event) {
  Settings_instance.HandleUiEvent(event);
}

void Settings_handleEncoderEvent(const UI::Event &event) {
  Settings_instance.HandleUiEvent(event);
}
