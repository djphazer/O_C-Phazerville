#ifndef OC_CONFIG_H_
#define OC_CONFIG_H_

#if defined(__MK20DX256__) && F_CPU != 120000000
#error "Please compile O&C firmware for Teensy 3.2 with CPU speed 120MHz"
#endif

// --- Hardware details --- //
static constexpr int DIGITAL_INPUT_COUNT = 4;
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
static constexpr int DAC_CHANNEL_COUNT = 8;
static constexpr int ADC_CHANNEL_COUNT = 8;
#else
static constexpr int DAC_CHANNEL_COUNT = 4;
static constexpr int ADC_CHANNEL_COUNT = 4;
#endif

// --- Timing --- //
// 60us = 16.666...kHz : Works, SPI transfer ends 2uS before next ISR
// 66us = 15.1515...kHz
// 72us = 13.888...kHz
// 100us = 10Khz
static constexpr uint32_t OC_CORE_ISR_FREQ = 16666U;
static constexpr uint32_t OC_CORE_TIMER_RATE = (1000000UL / OC_CORE_ISR_FREQ);
static constexpr uint32_t OC_UI_TIMER_RATE   = 1000UL;

// From kinetis.h
// Cortex-M4: 0,16,32,48,64,80,96,112,128,144,160,176,192,208,224,240
static constexpr int OC_CORE_TIMER_PRIO = 80;  // yet higher
static constexpr int OC_GPIO_ISR_PRIO   = 112; // higher
static constexpr int OC_UI_TIMER_PRIO   = 128; // default

static constexpr unsigned long REDRAW_TIMEOUT_MS = 1;
static constexpr uint32_t SCREENSAVER_TIMEOUT_S = 25; // default time out menu (in s)
static constexpr uint32_t SCREENSAVER_TIMEOUT_MAX_S = 120;

namespace OC {
static constexpr size_t kMaxTriggerDelayTicks = 96;
};

#define OCTAVES 10      // # octaves
#define SEMITONES (OCTAVES * 12)

static constexpr unsigned long SPLASHSCREEN_DELAY_MS = 2000;

static constexpr unsigned long APP_SELECTION_TIMEOUT_MS = 25000;
static constexpr unsigned long SETTINGS_SAVE_TIMEOUT_MS = 1000;

#define EEPROM_CALIBRATIONDATA_START 0

#if defined(ARDUINO_TEENSY41)
// T4.1 - 8 channels, 11 octaves
#define EEPROM_CALIBRATIONDATA_END 224
#define EEPROM_GLOBALSETTINGS_END 224
#elif defined(__IMXRT1062__)
// T4.0 - 4 channels, 11 octaves
#define EEPROM_CALIBRATIONDATA_END 128
#define EEPROM_GLOBALSETTINGS_END 128
#else
// T3.2 - 4 channels, 11 octaves
#define EEPROM_CALIBRATIONDATA_END 128

#ifdef NO_HEMISPHERE
#define EEPROM_GLOBALSETTINGS_END 960
#else
#define EEPROM_GLOBALSETTINGS_END 1060
#endif

#endif

#define EEPROM_GLOBALSETTINGS_START EEPROM_CALIBRATIONDATA_END

#define EEPROM_APPDATA_START EEPROM_GLOBALSETTINGS_END

#ifdef ARDUINO_TEENSY41
// The top 8 bytes of the emulated EEPROM hold what the module was doing at
// power-down: the current preset-bus slot and the app on screen
// (PresetEngine.cpp: the "current record"). It lives here and not in
// GLOBALS.CFG because a LittleFS write is a flash erase with interrupts off
// (~270 ms of dead audio, USB and bus under the core's 64 KB blocks when this
// was measured; ~45 ms per sector under XenoFS's 4 KB ones) three seconds
// after every preset change, or right inside a cross-app recall -- while an
// emulated-EEPROM byte is one 2-byte program (eeprom.c), skipped outright
// when the value is unchanged. Carved off the app-data region explicitly; the 160-byte reserve
// became 152 so BINARY_SIZE stays 3900 and app data stored by earlier
// firmware still validates. (The record was 4 bytes before it carried the
// app; the reserve shrank in step, 156 -> 152.)
#define EEPROM_PRESETBUS_SIZE 8
#define EEPROM_PRESETBUS_START (EEPROMStorage::LENGTH - EEPROM_PRESETBUS_SIZE)
#define EEPROM_APPDATA_END EEPROM_PRESETBUS_START
#define EEPROM_APPDATA_BINARY_SIZE (EEPROM_APPDATA_END - EEPROM_APPDATA_START - 152)
#else
#define EEPROM_APPDATA_END EEPROMStorage::LENGTH
// This is the available space for all apps' settings (\sa OC_apps.ino)
#define EEPROM_APPDATA_BINARY_SIZE (EEPROM_APPDATA_END - EEPROM_APPDATA_START - 32)
#endif

#define OC_ENCODERS_ENABLE_ACCELERATION_DEFAULT true

// Alternate encoder handling - processes both rising and falling edge on pins
//#define TINRS_ENCODERS

#define OC_CALIBRATION_DEFAULT_FLAGS (0)

/* ------------ uncomment line below to print boot-up and settings saving/restore info to serial ----- */
//#define PRINT_DEBUG
/* ------------ uncomment line below to enable ASR debug page ---------------------------------------- */
//#define ASR_DEBUG
/* ------------ uncomment line below to enable POLYLFO debug page ------------------------------------ */
//#define POLYLFO_DEBUG
/* ------------ uncomment line below to enable BBGEN debug page -------------------------------------- */
//#define BBGEN_DEBUG
/* ------------ uncomment line below to enable ENVGEN debug page ------------------------------------- */
//#define ENVGEN_DEBUG
//#define ENVGEN_DEBUG_SCREENSAVER
/* ------------ uncomment line below to enable BYTEBEATGEN debug page -------------------------------- */
//#define BYTEBEATGEN_DEBUG
/* ------------ uncomment line below to enable H1200 debug page -------------------------------------- */
//#define H1200_DEBUG
/* ------------ uncomment line below to enable QQ debug page ----------------------------------------- */
//#define QQ_DEBUG
//#define QQ_DEBUG_SCREENSAVER
/* ------------ Extra ADC debug stats ---------------------------------------------------------------  */
//#define OC_DEBUG_ADC_STATS
/* ------------ Debug for app load/save -------------------------------------------------------------  */
//#define OC_DEBUG_APPS
/* ------------ Debug for UI events/stats -----------------------------------------------------------  */
//#define OC_DEBUG_UI
/* ------------ Enable debug trace pins -------------------------------------------------------------  */
//#define OC_DEBUG_ENABLE_PINS

#endif // OC_CONFIG_H_
