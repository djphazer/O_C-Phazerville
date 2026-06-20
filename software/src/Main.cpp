// Copyright (c) 2015, 2016 Max Stadler, Patrick Dowling
//
// Original Author : Max Stadler
// Heavily modified: Patrick Dowling (pld@gurkenkiste.com)
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

// Main startup/loop for O&C firmware

#include <Arduino.h>
#include <EEPROM.h>

#include "OC_core.h"
#include "OC_app_switcher.h"
#include "OC_apps.h"
#include "OC_DAC.h"
#include "OC_debug.h"
#include "OC_gpio.h"
#include "OC_global_settings.h"
#include "OC_ADC.h"
#include "OC_calibration.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ui.h"
#include "OC_options.h"
#include "src/drivers/display.h"
#include "src/drivers/ADC/OC_util_ADC.h"
#include "util/util_debugpins.h"
#include "VBiasManager.h"
#include "HSMIDI.h"

#include "PhzConfig.h"

#if defined(ARDUINO_TEENSY41)
USBHost thisUSB;
USBHub hub1(thisUSB);
MIDIDevice_BigBuffer usbHostMIDI[2] {
  MIDIDevice_BigBuffer(thisUSB),
  MIDIDevice_BigBuffer(thisUSB)
};
MIDI_CREATE_INSTANCE(HardwareSerial, Serial8, MIDI1);
#include "AudioIO.h"
#include "usb_desc.h"
#include "Wire.h"
#ifdef MULTIBOOT
#include "util/cachedisable.h"
#endif

FLASHMEM
void ScanI2C() {
  noInterrupts();

  Serial.println("...Scanning i2c addresses...");
  uint8_t error;
  for (uint8_t address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    } //else { Serial.print("Nothing happened at address 0x"); }
  }

  interrupts();
}
#endif // ARDUINO_TEENSY41

uint_fast8_t MENU_REDRAW = true;
static OC::UiMode ui_mode = OC::UI_MODE_MENU;
static OC::IOFrame io_frame;

/*  ------------------------ UI timer ISR ---------------------------   */

IntervalTimer UI_timer;

void FASTRUN UI_timer_ISR() {
  OC_DEBUG_PROFILE_SCOPE(OC::DEBUG::UI_cycles);
  OC::ui.Poll();
  OC_DEBUG_RESET_CYCLES(OC::ui.ticks(), 2048, OC::DEBUG::UI_cycles);
}

/*  ------------------------ core timer ISR ---------------------------   */
IntervalTimer CORE_timer;
volatile bool OC::CORE::app_isr_enabled = false;
volatile bool OC::CORE::display_update_enabled = false;
volatile bool OC::CORE::app_loop_enabled = false;
volatile uint32_t OC::CORE::ticks = 0;

void FASTRUN CORE_timer_ISR() {
  DEBUG_PIN_SCOPE(OC_GPIO_DEBUG_PIN2);
  OC_DEBUG_PROFILE_SCOPE(OC::DEBUG::ISR_cycles);

  using namespace OC;

  // DAC and display share SPI. By first updating the DAC values, then starting
  // a DMA transfer to the display things are fairly nicely interleaved. In the
  // next ISR, the display transfer is finalized (CS update).

  display::Flush();
  DAC::Update();
  display::Update();

  // see OC_ADC.h for details; empirically (with current parameters), Scan_DMA() picks up new samples @ 5.55kHz
  OC::ADC::Scan_DMA();

  // Pin changes are tracked in separate ISRs, so depending on prio it might
  // need extra precautions. Note: This call is required to clear flags
  DigitalInputs::Scan();

  ++CORE::ticks;
  if (CORE::app_isr_enabled) {
    OC::app_switcher.Process(&io_frame);
  }

  OC_DEBUG_RESET_CYCLES(OC::CORE::ticks, 16384, OC::DEBUG::ISR_cycles);
}

/*       ---------------------------------------------------------         */

#ifdef MULTIBOOT
extern "C" {
  static void jump_to_alt(uint32_t choice) {
    const uint32_t JUMP_ADDR = 0x60000000 + (choice * 0x100000);
    uint32_t instptr = JUMP_ADDR + 0x1000 + sizeof(uint32_t);
    uint32_t instaddr = *(uint32_t*)instptr;
    ((void (*)(void))instaddr)();
  }
}

void BootMenu() {
  bool save = false;
  int choice = -1;

  while (true) {
    const bool z_held = OC::ui.read_immediate(OC::CONTROL_BUTTON_Z);
    const bool a_held = OC::ui.read_immediate(OC::CONTROL_BUTTON_A);
    const bool b_held = OC::ui.read_immediate(OC::CONTROL_BUTTON_B);
    const bool x_held = OC::ui.read_immediate(OC::CONTROL_BUTTON_X);
    const bool y_held = OC::ui.read_immediate(OC::CONTROL_BUTTON_Y);
    const bool any_held = (a_held || b_held || z_held || x_held || y_held);

    if (a_held) choice = 0;
    if (b_held) choice = 1;
    if (x_held) choice = 2;
    if (y_held) choice = 3;
    if (choice > -1) {
      if (OC::calibration_data.bootchoice() != choice) {
        OC::calibration_data.set_bootchoice(choice);
        if (z_held) {
          save = true;
        }
      }
      if (!any_held)
        break;
    }

    GRAPHICS_BEGIN_FRAME(true);
    graphics.setPrintPos(1, 5);
    graphics.print("USB Device Mode:");

    graphics.setPrintPos(1, 15);
    graphics.print("A: MIDI + Audio");
    if (any_held && 0 == OC::calibration_data.bootchoice()) {
      graphics.invertRect(1, 15, 127, 9);
    }
    graphics.setPrintPos(1, 25);
    graphics.print("B: MIDI");
    if (any_held && 1 == OC::calibration_data.bootchoice()) {
      graphics.invertRect(1, 25, 127, 9);
    }
    graphics.setPrintPos(1, 35);
    graphics.print("X: MTP + O_C Stock");
    if (any_held && 2 == OC::calibration_data.bootchoice()) {
      graphics.invertRect(1, 35, 127, 9);
    }
    graphics.setPrintPos(1, 45);
    graphics.print("Y: (HW Debug)");
    if (any_held && 3 == OC::calibration_data.bootchoice()) {
      graphics.invertRect(1, 45, 127, 9);
    }

    graphics.setPrintPos(1, 55);
    graphics.print("(hold Z to set)");
    GRAPHICS_END_FRAME();

    delay(10);
  }

  if (save) {
    OC::calibration_save();
  }
}
#endif

void setup() {
  delay(50);
  Serial.begin(9600);

  if (CrashReport) {
    while (!Serial && millis() < 3000) ; // wait
    Serial.println(CrashReport);
    delay(1500);
  }

  #if defined(ARDUINO_TEENSY41)
  OC::Pinout_Detect();
  #endif
#if defined(__MK20DX256__)
  NVIC_SET_PRIORITY(IRQ_PORTB, 0); // TR1 = 0 = PTB16
#endif
  SPI_init();
  SERIAL_PRINTLN("* O&C BOOTING...");
  SERIAL_PRINTLN("* %s", OC::Strings::VERSION);

  OC::DEBUG::Init();
  OC::DigitalInputs::Init();

#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  if (DAC8568_Uses_SPI) {
    // DAC8568 Vref does not turn on by default like DAC8565
    // best to turn on Vref as early as possible for analog
    // circuitry to settle
    OC::DAC::DAC8568_Vref_enable();
  }
  if (ADC33131D_Uses_FlexIO) {
    // ADC33131D wants calibration for Vref, takes ~1150 ms
    OC::ADC::ADC33131D_Vref_calibrate();
  } else {
#endif
    delay(400);
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  }
#endif

  OC::calibration_load();
  OC::SetFlipMode(OC::calibration_data.flipcontrols());

#if defined(ARDUINO_TEENSY41)
  Wire.begin();
  Wire.setClock(100000);
#endif

  OC::ADC::Init(&OC::calibration_data.adc, OC::calibration_data.flipcontrols());
  OC::ADC::Init_DMA();
  OC::DAC::Init(&OC::calibration_data.dac, &OC::global_settings.autotune_calibration_data, OC::calibration_data.flipcontrols());

  display::AdjustOffset(OC::calibration_data.display_offset);
  display::SetFlipMode( OC::calibration_data.flipscreen() );
  display::Init();

  GRAPHICS_BEGIN_FRAME(true);
  GRAPHICS_END_FRAME();

  OC::ui.Init();
  OC::ui.configure_encoders(OC::calibration_data.encoder_config());

  SERIAL_PRINTLN("* CORE ISR @%luus", OC_CORE_TIMER_RATE);
  io_frame.Reset();
  CORE_timer.begin(CORE_timer_ISR, OC_CORE_TIMER_RATE);
  CORE_timer.priority(OC_CORE_TIMER_PRIO);

  // Wait until there's at least some ADC values read
  delay(4);
  uint32_t random_seed =
      OC::ADC::raw_value(ADC_CHANNEL_1) * OC::ADC::raw_value(ADC_CHANNEL_2) +
      OC::ADC::raw_value(ADC_CHANNEL_3) + OC::ADC::raw_value(ADC_CHANNEL_4);
  randomSeed(random_seed);

  SERIAL_PRINTLN("* UI ISR @%luus", OC_UI_TIMER_RATE);
  UI_timer.begin(UI_timer_ISR, OC_UI_TIMER_RATE);
  UI_timer.priority(OC_UI_TIMER_PRIO);

  // first sign of life
  GRAPHICS_BEGIN_FRAME(true);
  graphics.setPrintPos(1, 28);
  graphics.print("*Main Screen Turn On*");
  GRAPHICS_END_FRAME();

#if defined(ARDUINO_TEENSY41)
  // Standard MIDI I/O on Serial8, only for Teensy 4.1
  if (MIDI_Uses_Serial8) {
    Serial8.begin(31250);
    MIDI1.begin(MIDI_CHANNEL_OMNI);
  }
  // USB Host support for 4.1 only
  thisUSB.begin();
#endif

#ifdef MULTIBOOT
  delay(100);
  if (OC::ui.read_immediate(OC::CONTROL_BUTTON_Z)) {
    BootMenu();
  }

  if (OC::calibration_data.bootchoice() == 3) {
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
      // -3V to +4V
      OC::DAC::set_octave(DAC_CHANNEL(i), i-3);
    }
    OC::ui.DebugStats();
  } else if (OC::calibration_data.bootchoice()) {
    GRAPHICS_BEGIN_FRAME(true);
    graphics.setPrintPos(1, 28);
    graphics.print("Switching to alt mode!");
    GRAPHICS_END_FRAME();
    AudioNoInterrupts();
    delay(10);
    disableCache();
    jump_to_alt(OC::calibration_data.bootchoice());
  }
#endif

  // --- more hardware init
#if defined(ARDUINO_TEENSY41)
  // this takes a couple seconds to timeout if no card
  SDcard_Ready = SD.begin(BUILTIN_SDCARD);

  if (I2S2_Audio_ADC && I2S2_Audio_DAC) {
    OC::AudioIO::Init();
  }
#endif

  // initialize LittleFS for config files
  PhzConfig::Init();

  // Display loading splash screen and optional calibration
  bool reset_settings = false;
  ui_mode = OC::ui.Splashscreen(reset_settings, 0);

  bool start_cal = false;
  if (ui_mode == OC::UI_MODE_CALIBRATE) {
    start_cal = true;
    ui_mode = OC::UI_MODE_MENU;
  }
  OC::ui.set_screensaver_timeout(OC::calibration_data.screensaver_timeout);

#ifdef VOR
  VBiasManager *vbias_m = vbias_m->get();
  vbias_m->SetState(VBiasManager::BI);
#endif

  // use default global config file in LFS
  bool firstrun = !PhzConfig::load_config();

  // initialize apps
  OC::app_switcher.Init(reset_settings || firstrun);

  // Welcome splash
  OC::ui.Splashscreen(firstrun, 1);

  if (start_cal)
    OC::start_calibration();

  OC::app_switcher.current_app()->DispatchAppEvent(OC::APP_EVENT_RESUME);

  SERIAL_PRINTLN("[End of setup()]");
}

/*  ---------    main loop  --------  */

void FASTRUN loop() {
  using namespace OC;
  CORE::app_isr_enabled = true;
  CORE::display_update_enabled = true;
  CORE::app_loop_enabled = true;
  uint32_t menu_draw_count = 0;
  uint32_t last_redraw_time = 0;

  while (true) {
#if defined(ARDUINO_TEENSY41)
    thisUSB.Task();
#endif

    // Refresh display
    if (MENU_REDRAW && CORE::display_update_enabled) {
      GRAPHICS_BEGIN_FRAME(false); // Don't busy wait

      if (UI_MODE_APP_SETTINGS == ui_mode) {
        // Only draw the App menu here...
        // Handle events and process state changes elsewhere.
        ui.AppSettings(true);

      } else { // if (UI_MODE_MENU == ui_mode) {
        OC_DEBUG_RESET_CYCLES(menu_draw_count, 512, DEBUG::MENU_draw_cycles);
        OC_DEBUG_PROFILE_SCOPE(DEBUG::MENU_draw_cycles);
        app_switcher.current_app()->Draw(ui_mode);
        ++menu_draw_count;
#ifdef VOR
        // TODO: move this into AppBase
        // only if not screensaver
        VBiasManager *vbias_m = vbias_m->get();
        vbias_m->DrawPopupPerhaps();
#endif
      }

      MENU_REDRAW = 0;
      last_redraw_time = ui.ticks();
      GRAPHICS_END_FRAME();
    }

    // Run current app
    if (CORE::app_loop_enabled)
      app_switcher.current_app()->DispatchLoop();

    // Take care of queued tasks
    OC::CORE::FlushTasks();

    // UI events
    if (UI_MODE_APP_SETTINGS == ui_mode) {
      if (!ui.AppSettings(false)) {
        // exit menu, resume app
        ui_mode = UI_MODE_MENU;
      }
    } else {
      UiMode mode = ui.DispatchEvents(app_switcher.current_slot());

      // State transition for app
      if (mode != ui_mode) {
        if (UI_MODE_SCREENSAVER == mode)
          app_switcher.current_app()->DispatchAppEvent(APP_EVENT_SCREENSAVER_ON);
        else if (UI_MODE_SCREENSAVER == ui_mode)
          app_switcher.current_app()->DispatchAppEvent(APP_EVENT_SCREENSAVER_OFF);
        else if (UI_MODE_APP_SETTINGS == mode)
          app_switcher.current_app()->DispatchAppEvent(APP_EVENT_SUSPEND);

        ui_mode = mode;
      }
    }

    if (ui.ticks() - last_redraw_time > REDRAW_TIMEOUT_MS)
      MENU_REDRAW = 1;

#ifdef MTP_INTERFACE
    // handle MTP Disk requests
    MTP.loop();
#endif

    static size_t cap_idx = 0;
    static elapsedMicros cap_send_time = 0;
    // check for request from PC to capture the screen
    if (Serial && Serial.available() > 0) {
      bool capreq = false;
      do {
        int cmd = Serial.read();
        switch (cmd) {
#ifdef PRINT_DEBUG
          case 'z':
            Serial.println("-=[ PEW PEW NERDS! ]=-");
            Serial.println("Secret Menu Options:");
            Serial.printf("'I' = Toggle App ISR [%s]\n", OC::CORE::app_isr_enabled ? "ON" : "OFF");
            Serial.printf("'D' = Toggle Display Redraw [%s]\n", OC::CORE::display_update_enabled ? "ON" : "OFF");
            Serial.printf("'L' = Toggle App Loop [%s]\n", OC::CORE::app_loop_enabled ? "ON" : "OFF");
#if defined(__IMXRT1062__)
#if defined(ARDUINO_TEENSY41)
            Serial.println("'i' = scan all i2c addresses");
#endif
            Serial.println("'l' = list all files in flash (LittleFS)");
            Serial.println("'s' = list all files on SD card");
            Serial.println("'C' = clear/reset default Config file");
            Serial.println("'F' = format/erase all LittleFS files");
#endif
            break;

          case 'I':
            OC::CORE::app_isr_enabled = !OC::CORE::app_isr_enabled;
            Serial.printf("App ISR = %s\n", OC::CORE::app_isr_enabled ? "ON" : "OFF");
            break;
          case 'D':
            OC::CORE::display_update_enabled = !OC::CORE::display_update_enabled;
            Serial.printf("Display Redraw = %s\n", OC::CORE::display_update_enabled ? "ON" : "OFF");
            break;
          case 'L':
            OC::CORE::app_loop_enabled = !OC::CORE::app_loop_enabled;
            Serial.printf("App Loop = %s\n", OC::CORE::app_loop_enabled ? "ON" : "OFF");
            break;

#if defined(__IMXRT1062__)
#if defined(ARDUINO_TEENSY41)
          case 'i':
            ScanI2C();
            break;
#endif
          case 'C':
            Serial.println("Resetting Config File!!");
            PhzConfig::clear_config();
            PhzConfig::save_config();
          case 'l':
            Serial.println(" -=- LittleFS -=- ");
            PhzConfig::listFiles();
            break;
          case 's':
            Serial.println(" -=- SD Card -=- ");
            PhzConfig::listFiles(SD);
            break;
          case 'F':
            Serial.println("!! ERASING ALL FILES on LittleFS !!");
            PhzConfig::eraseFiles();
            break;
#endif

            // TODO:
          case '+':
          case '-':
            // simulate UP and DOWN buttons
            break;
          case '[':
          case ']':
            // simulate Encoder button press
            break;
          case ',':
          case '.':
            // simulate Left Encoder turn
            break;
          case '<':
          case '>':
            // simulate Right Encoder turn
            break;
#endif
          default:
            capreq = true;
            break;
        }
      } while (Serial.available() > 0);
      if (capreq) {
        display::frame_buffer.capture_request();
        cap_idx = 0;
      }
    }

    // check for frame buffer to have capture data ready
    const uint8_t *capture_data = display::frame_buffer.captured();
    if (capture_data && cap_send_time > 950) {
      cap_send_time = 0;
      capture_data += cap_idx; // start where we left off

      // limit to n bytes every 950 micros
      const size_t chunk_size = 32;
      for (size_t i=0; i < chunk_size; i++) {
        uint8_t n = *capture_data++;
        if (n < 16) Serial.print("0");
        Serial.print(n, HEX);

        if (++cap_idx >= display::frame_buffer.kFrameSize) {
          // we're done sending this one
          Serial.println();
          Serial.flush();
          cap_idx = 0;
          display::frame_buffer.capture_retire();
          break;
        }
      }
    }

  }
}


