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
#include "PresetEngine.h"
#include "PresetBus.h"
#include "PresetBusUI.h"

#if defined(ARDUINO_TEENSY41)
USBHost thisUSB;
USBHub hub1(thisUSB);
// These MUST stay in DTCM: their member arrays are the EHCI DMA buffers,
// and USBHost_t36's midi/ehci paths do NO cache maintenance - DTCM is
// non-cacheable (EHCI reaches it via the CM7 AHBS backdoor), while RAM2 is
// write-back cached and silently corrupts host MIDI both directions.
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
volatile uint32_t loop_counter = 0;   // main-loop rate, read by DebugDump
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

// boot-time only; noinline so FLASHMEM sticks (free-function LTO rule)
FLASHMEM __attribute__((noinline)) void BootMenu() {
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

// ---------------------------------------------------------------------------
// Crash forensics + hardware watchdog (T4.x)
// ---------------------------------------------------------------------------
#if defined(__IMXRT1062__)
// The core never zeroes .bss.dma (DMAMEM is documented as uninitialized), so
// C++ objects placed there boot with garbage in any member their constructor
// doesn't touch - USBHost_t36 state machines rely on bss-zero and lock up.
// Zero the section here: ResetHandler calls this hook after the DTCM bss
// clear and BEFORE __libc_init_array (C++ ctors). .bss.dma is the first
// section in RAM2 (origin 0x20200000) and ends at _heap_start per the .ld.
extern unsigned long _heap_start;
extern "C" FLASHMEM void startup_middle_hook(void) {
  memset((void *)0x20200000, 0, (uint32_t)&_heap_start - 0x20200000u);
}

// Capture CrashReport text at boot so it can be appended to CRASH.LOG once
// LittleFS is mounted (the Serial print is gone if nobody was watching).
// buffer lives in RAM2 (DMAMEM) - DTCM stack headroom is precious
static DMAMEM char crash_buf[1024];
class BufferPrint : public Print {
public:
  char *buf = crash_buf;
  size_t len = 0;
  size_t write(uint8_t c) override {
    if (len < sizeof(crash_buf) - 1) { buf[len++] = c; buf[len] = 0; return 1; }
    return 0;
  }
};
static BufferPrint crash_capture;

// Reset cause, captured then cleared at boot: SRSR bits are sticky w1c and
// would otherwise show every cause since the last power-on, forever.
static uint32_t boot_srsr = 0;

// WDOG1: 128s timeout, fed only from loop(). Long enough that the slowest
// legitimate blocking op (a full LittleFS format) rarely trips it; a hard
// loop() hang reboots instead of bricking the module until power-cycle.
// Armed at the END of setup() so interactive boot flows (BootMenu,
// ConfirmReset) can block indefinitely.
static bool watchdog_armed = false;
FLASHMEM static void watchdog_arm() {
  // the core never ungates WDOG1's clock ("WDOG1 requires CCM_CCGR3_WDOG1"
  // per imxrt.h) - touching its registers without this bus-faults
  CCM_CCGR3 |= CCM_CCGR3_WDOG1(CCM_CCGR_ON);
  asm volatile("dsb");
  // PDE (set at reset) is a one-shot 16s power-down counter that asserts
  // reset unless cleared - with the clock just ungated it would fire ~16s
  // after arming and masquerade as a watchdog timeout
  WDOG1_WMCR = 0;
  WDOG1_WCR = (uint16_t)(255u << 8)  // WT: (255+1)*0.5s = 128s
            | WDOG_WCR_WDE | WDOG_WCR_SRS | WDOG_WCR_WDA
            | WDOG_WCR_WDBG | WDOG_WCR_WDZST;
  watchdog_armed = true;
}
static inline void watchdog_feed() {
  WDOG1_WSR = 0x5555;
  WDOG1_WSR = 0xAAAA;
}

// A full LittleFS format can exceed 128s worst-case (flash sector erase is
// 400ms MAX) - the one legitimate loop-blocking op longer than the
// watchdog. Feed from a timer for its duration only.
static IntervalTimer wdog_format_feeder;
static void watchdog_feed_isr() { watchdog_feed(); }  // ISR: stays in ITCM
FLASHMEM static void watchdog_feed_during(void (*op)()) {
  wdog_format_feeder.begin(watchdog_feed_isr, 1000000);  // 1s
  op();
  wdog_format_feeder.end();
}
#endif

FLASHMEM void setup() {
  delay(50);
#if defined(__IMXRT1062__)
  boot_srsr = SRC_SRSR;
  SRC_SRSR = boot_srsr;  // w1c: next boot reports only its own cause
#endif
  Serial.begin(9600);

  if (CrashReport) {
    while (!Serial && millis() < 3000) ; // wait
    Serial.println(CrashReport);
#if defined(__IMXRT1062__)
    // stash the report so it can be appended to CRASH.LOG once the
    // filesystem is up (console prints vanish when nobody is watching)
    crash_capture.print(CrashReport);
#endif
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

  bool firstrun = false;
#ifdef __IMXRT1062__
  // use default global config file in LFS
  firstrun = !PhzConfig::load_config();
  if (firstrun) {
    // GLOBALS.CFG missing/corrupt: try the boot-time backup before the
    // ConfirmReset flow gets a chance to threaten a factory wipe.
    if (PhzConfig::load_config(PhzConfig::BACKUP_FILENAME)) {
      Serial.println("CONFIG: GLOBALS.CFG bad; restored from GLOBALS.BAK");
      PhzConfig::save_config();  // re-materialize the primary from memory
      firstrun = false;
    }
  } else {
    PhzConfig::backup_config();  // known-good primary: refresh the backup
  }

  // append any captured crash report to CRASH.LOG (rotate at 8KB)
  if (crash_capture.len) {
    File cl = PhzConfig::myfs.open("CRASH.LOG", FILE_READ);
    const bool rotate = cl && cl.size() > 8192;
    if (cl) cl.close();
    if (rotate) {  // keep one generation of history instead of deleting
      PhzConfig::myfs.remove("CRASH.OLD");
      PhzConfig::myfs.rename("CRASH.LOG", "CRASH.OLD");
    }
    cl = PhzConfig::myfs.open("CRASH.LOG", FILE_WRITE);  // append
    if (cl) {
      cl.printf("--- boot @ %lu ms ---\n", millis());
      cl.write((const uint8_t *)crash_capture.buf, crash_capture.len);
      cl.close();
      Serial.println("CrashReport appended to CRASH.LOG");
    }
  }
#endif

  // initialize apps (on T3.x firstrun is detected by the EEPROM load inside)
  firstrun |= !OC::app_switcher.Init(reset_settings || firstrun);
#if defined(ARDUINO_TEENSY41) && defined(AUDIO_INTERFACE)
  // Force the audio output path (I2S codec out + host-playback monitor mix)
  // into existence. It is lazily built and was only ever constructed when an
  // audio applet wired up the chain - an appletless boot had DEAD panel outs
  // and no USB monitoring. Called here so it is created after every other
  // stream (its documented ordering requirement).
  OC::AudioIO::OutputStream();
#endif

  OC::PresetEngine::Init();
  OC::PresetBus::Init();
  OC::PresetBusUI::Init();
  // restores the last bus preset on any T4.1 (bench units included);
  // no bus traffic is emitted, so non-bus hardware is unaffected
  OC::PresetEngine::BootRecall();  // gated on I2C_Expansion inside

  // Welcome splash
  OC::ui.Splashscreen(firstrun, 1);

  if (start_cal)
    OC::start_calibration();

  OC::app_switcher.current_app()->DispatchAppEvent(OC::APP_EVENT_RESUME);

#if defined(__IMXRT1062__)
  // last: everything interactive that can legitimately block forever is
  // behind us, and loop() takes over feeding from here
  watchdog_arm();
  SERIAL_PRINTLN("* WDOG1 armed (128s, fed from loop)");
#endif

  SERIAL_PRINTLN("[End of setup()]");
}

/*  ---------    main loop  --------  */

#if defined(__IMXRT1062__)
// console 't': one-shot system health report
extern char _heap_end[], *__brkval;
FLASHMEM __attribute__((noinline)) static void SelfTest() {
  Serial.println("=== selftest ===");
  // bit 4 = wdog_rst_b (bit 5 is JTAG). Captured+cleared at boot.
  Serial.printf("uptime=%lus  reset_cause(SRC_SRSR@boot)=%08lX%s\n",
                millis() / 1000, boot_srsr,
                (boot_srsr & (1 << 4)) ? " [WDOG]" : "");
  Serial.printf("watchdog: %s (128s, fed from loop)\n",
                watchdog_armed ? "armed" : "OFF");
  {
    static uint32_t last_lc = 0, last_ms = 0;
    const uint32_t lc = loop_counter, ms = millis();
    if (last_ms && ms != last_ms)
      Serial.printf("loop rate ~%lu Hz\n", (lc - last_lc) * 1000 / (ms - last_ms));
    last_lc = lc; last_ms = ms;
    const uint32_t t0 = OC::CORE::ticks;
    delay(5);
    Serial.printf("core delta5ms=%lu (expect ~83)\n", OC::CORE::ticks - t0);
  }
  Serial.printf("heap free: %lu bytes (RAM2)\n",
                (unsigned long)(_heap_end - __brkval));
#if defined(ARDUINO_TEENSY41)
  // integer tenths: %f would drag the float-printf tables into DTCM
  Serial.printf("audio pool: %u now / %u max   cpu: %lu.%lu%% now / %lu.%lu%% max\n",
                AudioMemoryUsage(), AudioMemoryUsageMax(),
                (unsigned long)(AudioProcessorUsage() * 10) / 10,
                (unsigned long)(AudioProcessorUsage() * 10) % 10,
                (unsigned long)(AudioProcessorUsageMax() * 10) / 10,
                (unsigned long)(AudioProcessorUsageMax() * 10) % 10);
#endif
  {
    // %llu is unsupported by Print::printf (prints literal "lu")
    Serial.printf("littlefs: %luKB/%luKB used\n",
                  (unsigned long)(PhzConfig::myfs.usedSize() >> 10),
                  (unsigned long)(PhzConfig::myfs.totalSize() >> 10));
    // write-verify: the failure mode where writes "succeed" as 0-byte files
    const char *tf = "SELFTST.TMP";
    uint8_t pat[64], chk[64];
    for (unsigned i = 0; i < sizeof(pat); ++i) pat[i] = (uint8_t)(i * 37 + 5);
    PhzConfig::myfs.remove(tf);
    File f = PhzConfig::myfs.open(tf, FILE_WRITE_BEGIN);
    bool ok = f && f.write(pat, sizeof(pat)) == sizeof(pat);
    if (f) f.close();
    if (ok) {
      f = PhzConfig::myfs.open(tf, FILE_READ);
      ok = f && f.read(chk, sizeof(chk)) == sizeof(chk)
             && memcmp(pat, chk, sizeof(pat)) == 0;
      if (f) f.close();
    }
    PhzConfig::myfs.remove(tf);
    Serial.printf("fs write-verify: %s\n", ok ? "PASS" : "FAIL");
    f = PhzConfig::myfs.open("CRASH.LOG", FILE_READ);
    if (f) {
      Serial.printf("CRASH.LOG present: %lu bytes (crashes recorded)\n",
                    (unsigned long)f.size());
      f.close();
    } else {
      Serial.println("CRASH.LOG: none (no crashes recorded)");
    }
  }
  Serial.println("=== selftest done ===");
}
#endif

// loop() is the slow path (drawing, UI events, deferred work) — all the
// real-time work happens in the CORE/UI timer ISRs. Run it from cached
// flash instead of burning ~4KB of ITCM (it gets inlined into main()).
FLASHMEM __attribute__((noinline)) void loop() {
  using namespace OC;
  CORE::app_isr_enabled = true;
  CORE::display_update_enabled = true;
  CORE::app_loop_enabled = true;
  uint32_t menu_draw_count = 0;
  uint32_t last_redraw_time = 0;

  while (true) {
    ++loop_counter;
#if defined(__IMXRT1062__)
    watchdog_feed();  // a wedged loop() now reboots instead of bricking
#endif
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

      } else if (OC::PresetBusUI::Active()) {
        OC::PresetBusUI::Draw();
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
    OC::PresetEngine::Process();
    OC::PresetBus::Task();
    OC::PresetBusUI::Task();

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
      // Console lock: hosts like the Jetson's ModemManager AT/MBIM-probe every
      // new CDC port, and that byte soup has hit real commands ('D' froze the
      // display, '(' fired preset saves, 'i'/'C'/'F' are worse). Ignore all
      // input until the literal sequence "pew!" arrives.
      static bool console_unlocked = false;
      static uint32_t unlock_shift = 0;
      static uint32_t destructive_arm_ms = 0;  // C/F double-press confirm
      static char destructive_arm_key = 0;
      do {
        int cmd = Serial.read();
        if (!console_unlocked) {
          unlock_shift = (unlock_shift << 8) | (uint8_t)cmd;
          if (unlock_shift == 0x70657721) {  // "pew!"
            console_unlocked = true;
            Serial.println("-=[ console unlocked ]=-");
          }
          continue;
        }
        switch (cmd) {
#ifdef PRINT_DEBUG
          case 'z':
            Serial.println("-=[ PEW PEW NERDS! ]=-");
            Serial.println("-- system --");
#if defined(__IMXRT1062__)
            Serial.println("t selftest   a activate default app");
#endif
            Serial.printf("I app ISR [%s]   D display [%s]   L app loop [%s]\n",
                          OC::CORE::app_isr_enabled ? "on" : "OFF",
                          OC::CORE::display_update_enabled ? "on" : "OFF",
                          OC::CORE::app_loop_enabled ? "on" : "OFF");
#if defined(__IMXRT1062__)
#if defined(ARDUINO_TEENSY41)
            Serial.println("i i2c scan");
#endif
            Serial.println("-- files --");
            Serial.println("l list LittleFS   s list SD");
            Serial.println("-- DANGER --");
            Serial.println("C RESET config file   F ERASE ALL LittleFS files");
#endif
            Serial.println("(any other key = screen capture)");
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
          // preset-engine bench triggers: [ = save, ] = recall (slot 0);
          // { and } use slot 1
          case '(': OC::PresetEngine::RequestSave(0); break;
          case ')': OC::PresetEngine::RequestRecall(0); break;
          case '{': OC::PresetEngine::RequestSave(1); break;
          case '}': OC::PresetEngine::RequestRecall(1); break;
          case 'g':
            Serial.println("Saving global settings + app data...");
            OC::SaveAppData();
            break;
          case 'p':  // toggle the preset-bus overlay (remote UI inspection)
            if (OC::PresetBusUI::Active()) OC::PresetBusUI::Exit();
            else OC::PresetBusUI::Enter();
            Serial.printf("PresetBusUI %s\n",
                          OC::PresetBusUI::Active() ? "open" : "closed");
            break;
          case 'b': OC::PresetBus::DebugDump(); break;
          case 'B':
            OC::PresetBus::SetVerbose(!OC::PresetBus::Verbose());
            Serial.printf("PresetBus verbose = %d\n", OC::PresetBus::Verbose());
            break;
#endif
          // destructive keys need a second press within 3s ('pew!' stops
          // robots typing garbage; this stops human typos)
          case 'C':
            if (millis() - destructive_arm_ms < 3000 && destructive_arm_key == 'C') {
              destructive_arm_ms = 0;
              Serial.println("Resetting Config File!!");
              PhzConfig::clear_config();
              PhzConfig::save_config();
            } else {
              destructive_arm_ms = millis();
              destructive_arm_key = 'C';
              Serial.println("'C' RESETS the config - press 'C' again within 3s");
            }
            break;
          case 't': SelfTest(); break;  // one-shot system health report
          case 'a': OC::SwitchToDefaultApp(); break;  // remote: default app
          case 'l':
            Serial.println(" -=- LittleFS -=- ");
            PhzConfig::listFiles();
            break;
          case 's':
            Serial.println(" -=- SD Card -=- ");
            PhzConfig::listFiles(SD);
            break;
          case 'F':
            if (millis() - destructive_arm_ms >= 3000 || destructive_arm_key != 'F') {
              destructive_arm_ms = millis();
              destructive_arm_key = 'F';
              Serial.println("'F' ERASES ALL LittleFS files - press 'F' again within 3s");
              break;
            }
            destructive_arm_ms = 0;
            Serial.println("!! ERASING ALL FILES on LittleFS !!");
            // worst-case format outlives the 128s watchdog: timer-fed
            watchdog_feed_during([] { PhzConfig::eraseFiles(); });
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
#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)
          // commander mode: bus-wide preset ops (every module + local engine)
          case '<': OC::PresetBus::BroadcastRecall(0); break;
          case '>': OC::PresetBus::BroadcastSave(0); break;
          case ',': OC::PresetBus::BroadcastRecall(1); break;
          case '.': OC::PresetBus::BroadcastSave(1); break;
#else
          case ',':
          case '.':
            // simulate Left Encoder turn
            break;
          case '<':
          case '>':
            // simulate Right Encoder turn
#endif
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


