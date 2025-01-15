#pragma once

#include "OC_scales.h"
#include "PackingUtils.h"

// misc. utility functions extracted from Hemisphere
// -NJM

// Simulated fixed floats by multiplying and dividing by powers of 2
#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
using simfloat = int32_t;
#endif

// Reference Constants
#ifdef NORTHERNLIGHT
#define PULSE_VOLTAGE 9
#define HEMISPHERE_MAX_CV 15360
#define HEMISPHERE_CENTER_CV 7680 // 5V
#define HEMISPHERE_MIN_CV 0
#elif defined(VOR)
#define PULSE_VOLTAGE 8
#define HEMISPHERE_MAX_CV (HS::octave_max * 12 << 7)
#define HEMISPHERE_CENTER_CV 0
#define HEMISPHERE_MIN_CV (HEMISPHERE_MAX_CV - 15360)
#else
#define PULSE_VOLTAGE 5
#define HEMISPHERE_MAX_CV 9216 // 6V
#define HEMISPHERE_CENTER_CV 0
#define HEMISPHERE_MIN_CV -4608 // -3V
#endif
#define HEMISPHERE_3V_CV 4608
#define HEMISPHERE_MAX_INPUT_CV 9216 // 6V
#define HEMISPHERE_CENTER_DETENT 80
#define HEMISPHERE_CLOCK_TICKS 17 // one millisecond
#define HEMISPHERE_CURSOR_TICKS 5000
#define HEMISPHERE_ADC_LAG 33
#define HEMISPHERE_CHANGE_THRESHOLD 32

// Hemisphere-specific macros
#define BottomAlign(h) (62 - (h))
#define ForEachChannel(ch) for(int_fast8_t ch = 0; (ch) < 2; ++(ch))
#define ForAllChannels(ch) for(int_fast8_t ch = 0; (ch) < 4; ++(ch))
// #define gfx_offset ((hemisphere % 2) * 64) // Graphics offset, based on the side
#define io_offset (hemisphere * 2) // Input/Output offset, based on the side
#define SPLIT_INT_DEC(x, mul) \
  static_cast<int>(x), \
  static_cast<int>((mul) * ((x) - static_cast<int32_t>(x)))

namespace HS {

enum HEM_SIDE {
  LEFT_HEMISPHERE = 0,
  RIGHT_HEMISPHERE = 1,
#ifdef ARDUINO_TEENSY41
  LEFT2_HEMISPHERE = 2,
  RIGHT2_HEMISPHERE = 3,
#endif

  APPLET_SLOTS
};

// Codes for help system labels
enum HELP_SECTIONS {
  HELP_DIGITAL1 = 0,
  HELP_DIGITAL2,
  HELP_CV1,
  HELP_CV2,
  HELP_OUT1,
  HELP_OUT2,
  HELP_EXTRA1,
  HELP_EXTRA2,

  HELP_LABEL_COUNT,

  // aliases to ease refactoring
  HEMISPHERE_HELP_DIGITALS = HELP_DIGITAL1,
  HEMISPHERE_HELP_CVS      = HELP_CV1,
  HEMISPHERE_HELP_OUTS     = HELP_OUT1,
  HEMISPHERE_HELP_ENCODER  = HELP_EXTRA1,
};

static constexpr uint32_t MENU_ANIMATION_TIME = 2500; // approx 150ms
static constexpr uint32_t HEMISPHERE_DOUBLE_CLICK_TIME = 8000;
static constexpr uint32_t HEMISPHERE_PULSE_ANIMATION_TIME = 500;
static constexpr uint32_t HEMISPHERE_PULSE_ANIMATION_TIME_LONG = 1200;

extern const int HEMISPHERE_AVAILABLE_APPLETS;

} // namespace HS

//////////////// Calculation methods
////////////////////////////////////////////////////////////////////////////////

/* Proportion method using simfloat, useful for calculating scaled values given
 * a fractional value.
 *
 * Solves this:  numerator        ???
 *              ----------- = -----------
 *              denominator       max
 *
 * For example, to convert a parameter with a range of 1 to 100 into value scaled
 * to HEMISPHERE_MAX_CV, to be sent to the DAC:
 *
 * Out(ch, Proportion(value, 100, HEMISPHERE_MAX_CV));
 *
 */
constexpr int Proportion(const int numerator, const int denominator, const int max_value) {
    simfloat proportion = int2simfloat((int32_t)abs(numerator)) / (int32_t)denominator;
    int scaled = simfloat2int(proportion * max_value);
    return numerator >= 0 ? scaled : -scaled;
}

/* Proportion CV values into pixels for display purposes.
 *
 * Solves this:     cv_value           ???
 *              ----------------- = ----------
 *              HEMISPHERE_MAX_CV   max_pixels
 */
constexpr int ProportionCV(const int cv_value, const int max_pixels) {
    // TODO: variable scaling for VOR?
    int prop = constrain(Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, max_pixels), -max_pixels, max_pixels);
    return prop;
}


// Specifies where data goes in flash storage for each selcted applet, and how big it is
struct PackLocation {
    size_t location;
    size_t size;
};

/* Add value to a 64-bit storage unit at the specified location */
constexpr void Pack(uint64_t &data, const PackLocation p, const uint64_t value) {
    data |= (value << p.location);
}

/* Retrieve value from a 64-bit storage unit at the specified location and of the specified bit size */
constexpr int Unpack(const uint64_t &data, const PackLocation p) {
    uint64_t mask = 1;
    for (size_t i = 1; i < p.size; i++) mask |= (0x01 << i);
    return (data >> p.location) & mask;
}

void gfxPos(int x, int y);
void gfxPrint(int x, int y, const char *str);
void gfxPrint(int x, int y, int num);
void gfxPrint(const char *str);
void gfxPrint(int num);
void gfxPrint(int x_adv, int num);
void gfxPrintVoltage(int cv);
void gfxPixel(int x, int y);
void gfxFrame(int x, int y, int w, int h, bool dotted = false);
void gfxRect(int x, int y, int w, int h);
void gfxInvert(int x, int y, int w, int h);
void gfxLine(int x, int y, int x2, int y2);
void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p = 2);
void gfxCircle(int x, int y, int r);
void gfxBitmap(int x, int y, int w, const uint8_t *data);
void gfxIcon(int x, int y, const uint8_t *data, bool clearfirst = false);
void gfxHeader(const char *str, const uint8_t *icon = nullptr);

static constexpr uint8_t pad(int range, int number) {
    uint8_t padding = 0;
    while (range > 1)
    {
        if (abs(number) < range) padding += 6;
        range = range / 10;
    }
    if (number < 0 && padding > 0) padding -= 6; // Compensate for minus sign
    return padding;
}


namespace HS {
  enum PopupType {
    MENU_POPUP,
    CLOCK_POPUP, PRESET_POPUP,
    QUANTIZER_POPUP,
  };

  enum QUANT_CHANNEL {
    QUANT_CHANNEL_1,
    QUANT_CHANNEL_2,
    QUANT_CHANNEL_3,
    QUANT_CHANNEL_4,
    QUANT_CHANNEL_5,
    QUANT_CHANNEL_6,
    QUANT_CHANNEL_7,
    QUANT_CHANNEL_8,

    QUANT_CHANNEL_COUNT
  };

  extern uint32_t popup_tick; // for button feedback
  extern PopupType popup_type;
  extern uint8_t qview; // which quantizer's setting is shown in popup
  extern int q_edit;

  // input quantizers, because sometimes we need hysteresis
  extern OC::SemitoneQuantizer input_quant[ADC_CHANNEL_LAST];

  extern braids::Quantizer quantizer[QUANT_CHANNEL_COUNT]; // global shared quantizers
  extern int quant_scale[QUANT_CHANNEL_COUNT];
  extern int8_t root_note[QUANT_CHANNEL_COUNT];
  extern int8_t q_octave[QUANT_CHANNEL_COUNT];
  extern uint16_t q_mask[QUANT_CHANNEL_COUNT];

  extern int octave_max;

  extern int select_mode;
  extern bool cursor_wrap;

  extern bool auto_save_enabled;
  extern int trigger_mapping[ADC_CHANNEL_LAST];
  extern int cvmapping[ADC_CHANNEL_LAST];
  extern uint8_t trig_length;
  extern uint8_t screensaver_mode;
  extern bool wavplayer_available;

  extern OC::menu::ScreenCursor<5> showhide_cursor;

  void Init();
  void DrawAppletList(bool blink = false);

  // --- Quantizer helpers
  int GetLatestNoteNumber(int ch);
  int Quantize(int ch, int cv, int root = 0, int transpose = 0);
  int QuantizerLookup(int ch, int note);
  void QuantizerConfigure(int ch, int scale, uint16_t mask = 0xffff);
  int GetScale(int ch);
  int GetRootNote(int ch);
  int SetRootNote(int ch, int root);
  void NudgeRootNote(int ch, int dir);
  void NudgeOctave(int ch, int dir);
  void NudgeScale(int ch, int dir);
  void QuantizerEdit(int ch);
  void QEditEncoderMove(bool rightenc, int dir);
  void DrawPopup(const int config_cursor = 0, const int preset_id = 0, const bool blink = 0);
  void ToggleClockRun();
  void PokePopup(PopupType pop);

} // namespace HS
