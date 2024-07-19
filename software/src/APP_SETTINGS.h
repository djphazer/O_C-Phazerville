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
#include "OC_apps.h"
#include "OC_ui.h"
#include "HSApplication.h"
#include "OC_strings.h"

class Settings : public HSApplication {
public:
  bool calibration_mode = false;
  bool calibration_complete = true;
  OC::DigitalInputDisplay digital_input_displays[4];
  OC::TickCount tick_count;

  OC::CalibrationState calibration_state = {
    OC::HELLO,
    &OC::calibration_steps[OC::HELLO],
    0, // "use defaults: no"
  };

  void Start() { }
  void Resume() { }

  void Controller()
  {
    using namespace OC;
    if (calibration_mode && !calibration_complete)
    {
      uint32_t ticks = tick_count.Update();
      digital_input_displays[0].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_1>());
      digital_input_displays[1].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_2>());
      digital_input_displays[2].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_3>());
      digital_input_displays[3].Update(ticks, DigitalInputs::read_immediate<DIGITAL_INPUT_4>());

      const CalibrationStep *next_step = &calibration_steps[calibration_state.step];
      if (next_step != calibration_state.current_step) {
        #ifdef PRINT_DEBUG
          SERIAL_PRINTLN("%s (%d)", next_step->title, step_to_channel(next_step->step));
        #else
          step_to_channel(next_step->step);
        #endif
        // Special cases on exit current step
        switch (calibration_state.current_step->step) {
          case HELLO:
            if (calibration_state.encoder_value) {
              SERIAL_PRINTLN("Reset to defaults...");
              OC::calibration_reset();
              calibration_state.used_defaults = true;
            }
            break;
#ifdef VOR
          case DAC_A_VOLT_7:
#else
          case DAC_A_VOLT_6:
#endif
            if (calibration_state.used_defaults) {
              // copy DAC A to the rest of them, to make life easier
              for (int ch = 1; ch < DAC_CHANNEL_LAST; ++ch) {
                for (int i = 0; i < OCTAVES; ++i) {
                  OC::calibration_data.dac.calibrated_octaves[ch][i] = OC::calibration_data.dac.calibrated_octaves[0][i];
                }
              }
            }
            break;
          case ADC_PITCH_C4:
            if (calibration_state.adc_1v && calibration_state.adc_3v) {
              OC::ADC::CalibratePitch(calibration_state.adc_1v, calibration_state.adc_3v);
              SERIAL_PRINTLN("ADC SCALE 1V=%d, 3V=%d -> %d",
                             calibration_state.adc_1v, calibration_state.adc_3v,
                             OC::calibration_data.adc.pitch_cv_scale);
            }
            break;

          default: break;
        }

        // Setup next step
        switch (next_step->calibration_type) {
        case CALIBRATE_OCTAVE:
          calibration_state.encoder_value =
              OC::calibration_data.dac.calibrated_octaves[step_to_channel(next_step->step)][next_step->index + DAC::kOctaveZero];
            #ifdef VOR
            /* set 0V @ unipolar range */
            DAC::set_Vbias(DAC::VBiasUnipolar);
            #endif
          break;

        #ifdef VOR
        case CALIBRATE_VBIAS_BIPOLAR:
          calibration_state.encoder_value = (0xFFFF & OC::calibration_data.v_bias); // bipolar = lower 2 bytes
        break;
        case CALIBRATE_VBIAS_ASYMMETRIC:
          calibration_state.encoder_value = (OC::calibration_data.v_bias >> 16);  // asymmetric = upper 2 bytes
        break;
        #endif
        
        case CALIBRATE_ADC_OFFSET:
          calibration_state.encoder_value = OC::calibration_data.adc.offset[next_step->index];
            #ifdef VOR
            DAC::set_Vbias(DAC::VBiasUnipolar);
            #endif
          break;
        case CALIBRATE_DISPLAY:
          calibration_state.encoder_value = OC::calibration_data.display_offset;
          break;

        case CALIBRATE_ADC_1V:
        case CALIBRATE_ADC_3V:
          SERIAL_PRINTLN("offset=%d", OC::calibration_data.adc.offset[ADC_CHANNEL_1]);
          break;

        case CALIBRATE_SCREENSAVER:
          calibration_state.encoder_value = OC::calibration_data.screensaver_timeout;
          SERIAL_PRINTLN("timeout=%d", calibration_state.encoder_value);
          break;

        case CALIBRATE_NONE:
        default:
          if (CALIBRATION_EXIT != next_step->step) {
            calibration_state.encoder_value = 0;
          } else {
            // Make the default "Save: no" if the calibration data was reset
            // manually, but only if calibration data was actually loaded from
            // EEPROM
            if (calibration_state.used_defaults && OC::calibration_data_loaded)
              calibration_state.encoder_value = 0;
            else
              calibration_state.encoder_value = 1;
          }
        }
        calibration_state.current_step = next_step;
      }

      OC::calibration_update(calibration_state);
      return;
    }

    if (calibration_mode && calibration_complete) {
    }

    #ifdef PEWPEWPEW
        HS::frame.Load();PewPewTime.PEWPEW(Clock(3)<<1|Clock(0));}
        struct{bool go=0;int idx=0;struct{uint8_t x,y;int x_v,y_v;}pewpews[8];
        void PEWPEW(uint8_t mask){uint32_t t=OC::CORE::ticks;for(int i=0;i<8;++i){auto &p=pewpews[i];
          if(mask>>i&0x01){auto &pp=pewpews[idx++];pp.x=0+120*i;pp.y=55;pp.x_v=(6+random(3))*(i?-1:1);pp.y_v=-9;idx%=8;}
          if(t%500==0){p.x+=p.x_v;p.y+=p.y_v;if(p.y>=55&&p.y_v>0)p.y_v=-p.y_v;else ++p.y_v;}
          if(t%10000==0){p.x_v=p.x_v*100/101;p.y_v=p.y_v*10/11;}}}}PewPewTime;
        void PEWPEW(){for(int i=0;i<8;++i){auto &p=PewPewTime.pewpews[i];gfxIcon(p.x%128,p.y%64,ZAP_ICON);}
    #endif
  }

    void View() {
      if (calibration_mode) {
        OC::calibration_draw(calibration_state);
        return;
      }

        gfxHeader("Setup / About");

        #if defined(ARDUINO_TEENSY40)
        gfxPrint(100, 0, "T4.0");
        //gfxPrint(0, 45, "E2END="); gfxPrint(E2END);
        #elif defined(ARDUINO_TEENSY41)
        gfxPrint(100, 0, "T4.1");
        #else
        gfxPrint(100, 0, "T3.2");
        #endif

        gfxIcon(0, 15, ZAP_ICON);
        gfxIcon(120, 15, ZAP_ICON);
        #ifdef PEWPEWPEW
        gfxPrint(21, 15, "PEW! PEW! PEW!");
        #else
        gfxPrint(12, 15, "Phazerville Suite");
        #endif
        gfxPrint(0, 25, OC::Strings::VERSION);
        gfxPrint(0, 35, "github.com/djphazer");
        gfxPrint(0, 55, "[CALIBRATE]   [RESET]");
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////

    void HandleUiEvent(const UI::Event &event) {
      using namespace OC;

      if (!calibration_mode) {
        if (event.control == OC::CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_PRESS) Calibration();
        if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) FactoryReset();
        return;
      }

      // event handling from Ui::Calibrate()
      if (event.type == UI::EVENT_BUTTON_DOWN)
        return;

      switch (event.control) {
        case CONTROL_BUTTON_L:
          if (calibration_state.step == HELLO) calibration_complete = 1; // Way out --jj
          if (calibration_state.step > CENTER_DISPLAY)
            calibration_state.step = static_cast<CALIBRATION_STEP>(calibration_state.step - 1);
          break;
        case CONTROL_BUTTON_R:
          // Special case these values to read, before moving to next step
          if (calibration_state.step < CALIBRATION_EXIT)
            calibration_state.step = static_cast<CALIBRATION_STEP>(calibration_state.step + 1);
          else
            calibration_complete = true;
          break;
        case CONTROL_ENCODER_L:
          if (calibration_state.step > HELLO) {
            calibration_state.step = static_cast<CALIBRATION_STEP>(calibration_state.step + event.value);
            CONSTRAIN(calibration_state.step, CENTER_DISPLAY, CALIBRATION_EXIT);
          }
          break;
        case CONTROL_ENCODER_R:
          calibration_state.encoder_value += event.value;
          break;
        case CONTROL_BUTTON_UP:
        case CONTROL_BUTTON_DOWN:
          if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
            const CalibrationStep *step = calibration_state.current_step;

            // long-press DOWN to measure ADC points
            switch (step->step) {
              case ADC_PITCH_C2:
                calibration_state.adc_1v = OC::ADC::value(ADC_CHANNEL_1);
                break;
              case ADC_PITCH_C4:
                calibration_state.adc_3v = OC::ADC::value(ADC_CHANNEL_1);
                break;
              default: break;
            }

            // long-press DOWN to auto-scale DAC values on current channel
            int volts = step->index + DAC::kOctaveZero;
            if (step->calibration_type == CALIBRATE_OCTAVE && volts > 0) {
              int ch = step_to_channel(step->step);
              uint16_t first = OC::calibration_data.dac.calibrated_octaves[ch][0];
              uint16_t second = OC::calibration_data.dac.calibrated_octaves[ch][volts];
              int interval = (second - first) / volts;

              for (int i = 1; i < OCTAVES; ++i) {
                first += interval;
                OC::calibration_data.dac.calibrated_octaves[ch][i] = first;
              }
            }
            break;
          }

          if (calibration_state.step == HELLO || calibration_state.step == CALIBRATION_EXIT)
            OC::ui.configure_encoders(calibration_data.next_encoder_config());
          else if (calibration_state.step > CENTER_DISPLAY)
            calibration_state.encoder_value += (event.control == CONTROL_BUTTON_UP ? 128 : -128);

          break;
        default:
          break;
      }

      if (calibration_complete) {
        if (calibration_state.encoder_value) {
          SERIAL_PRINTLN("Calibration complete");
          OC::calibration_save();
        } else {
          SERIAL_PRINTLN("Calibration complete (but don't save)");
        }

        OC::ui.set_screensaver_timeout(OC::calibration_data.screensaver_timeout);
        calibration_mode = false;
      }
    }
    void Calibration() {
        // migrated from OC::ui.Calibrate();

        calibration_state = {
          OC::HELLO,
          &OC::calibration_steps[OC::HELLO],
          OC::calibration_data_loaded ? 0 : 1, // "use defaults: no" if data loaded
        };
        calibration_state.adc_sum.set(OC::_ADC_OFFSET);
        calibration_state.used_defaults = false;

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
#ifdef PEWPEWPEW
  Settings_instance.Controller();
#endif
// skip the Controller to avoid I/O conflict with Calibration
  return;
}

void Settings_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        Settings_instance.Resume();
    }
}

void Settings_loop() {} // Deprecated

void Settings_menu() {
    Settings_instance.BaseView();
}

void Settings_screensaver() {
#ifdef PEWPEWPEW
    Settings_instance.PEWPEW();
#endif
}

void Settings_handleButtonEvent(const UI::Event &event) {
  Settings_instance.HandleUiEvent(event);
}

void Settings_handleEncoderEvent(const UI::Event &event) {
  Settings_instance.HandleUiEvent(event);
}
