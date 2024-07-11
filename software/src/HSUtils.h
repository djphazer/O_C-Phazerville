#pragma once

// misc. utility functions extracted from Hemisphere
// -NJM

// Simulated fixed floats by multiplying and dividing by powers of 2
#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
typedef int32_t simfloat;
#endif

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
typedef struct PackLocation {
    size_t location;
    size_t size;
} PackLocation;

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
void gfxFrame(int x, int y, int w, int h);
void gfxRect(int x, int y, int w, int h);
void gfxInvert(int x, int y, int w, int h);
void gfxLine(int x, int y, int x2, int y2);
void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p = 2);
void gfxCircle(int x, int y, int r);
void gfxBitmap(int x, int y, int w, const uint8_t *data);
void gfxIcon(int x, int y, const uint8_t *data);

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
  extern bool q_edit;

  static inline void PokePopup(PopupType pop) {
    popup_type = pop;
    popup_tick = OC::CORE::ticks;
  }

  extern braids::Quantizer quantizer[QUANT_CHANNEL_COUNT]; // global shared quantizers
  extern int quant_scale[QUANT_CHANNEL_COUNT];
  extern int8_t root_note[QUANT_CHANNEL_COUNT];
  extern int8_t q_octave[QUANT_CHANNEL_COUNT];

  extern int octave_max;

  extern int select_mode;
  extern bool cursor_wrap;

  extern bool auto_save_enabled;
  extern int trigger_mapping[ADC_CHANNEL_LAST];
  extern int cvmapping[ADC_CHANNEL_LAST];
  extern uint8_t trig_length;
  extern uint8_t screensaver_mode;

  // --- Quantizer helpers
  int GetLatestNoteNumber(int ch);
  int Quantize(int ch, int cv, int root = 0, int transpose = 0);
  int QuantizerLookup(int ch, int note);
  void QuantizerConfigure(int ch, int scale, uint16_t mask = 0xffff);
  int GetScale(int ch);
  int GetRootNote(int ch);
  int SetRootNote(int ch, int root);
  void NudgeRootNote(int ch, int dir);
  void NudgeScale(int ch, int dir);
  void QuantizerEdit(int ch);
  void DrawPopup(const int config_cursor = 0, const int preset_id = 0, const bool blink = 0);
  void ToggleClockRun();

}
