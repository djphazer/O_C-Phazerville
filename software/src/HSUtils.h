#pragma once

#include "OC_core.h"
#include "OC_ADC.h"
#include "OC_gpio.h"
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
#define ONE_OCTAVE (12 << 7)
#define PULSE_VOLTAGE HS::octave_max
#define HEMISPHERE_MAX_CV (HS::octave_max * ONE_OCTAVE)
#define HEMISPHERE_MIN_CV (-OC::DAC::kOctaveZero * ONE_OCTAVE*(1+DAC_20Vpp))
#define HEMISPHERE_CENTER_CV ((HEMISPHERE_MAX_CV-HEMISPHERE_MIN_CV)/2)
#define HEMISPHERE_3V_CV (3 * ONE_OCTAVE)
#define HEMISPHERE_CENTER_INPUT_CV (NorthernLightModular*HEMISPHERE_MAX_CV/2)
#define HEMISPHERE_MAX_INPUT_CV (6*ONE_OCTAVE + NorthernLightModular*(4*ONE_OCTAVE)) // 6V or 10V
#define HEMISPHERE_CENTER_DETENT 80
#define HEMISPHERE_CLOCK_TICKS 17 // one millisecond
#define HEMISPHERE_CURSOR_TICKS 5000
#define HEMISPHERE_ADC_LAG 33
#define HEMISPHERE_CHANGE_THRESHOLD 32

// Hemisphere-specific macros
#define BottomAlign(h) (62 - (h))
#define ForEachChannel(ch) for(int_fast8_t ch = 0; (ch) < 2; ++(ch))
#define ForAllChannels(ch) for(int_fast8_t ch = 0; (ch) < 4; ++(ch))
#define gfx_offset ((hemisphere & 1) * 64) // Graphics offset, based on the side
#define io_offset (hemisphere * 2) // Input/Output offset, based on the side
#define SPLIT_INT_DEC(x, mul) \
  static_cast<int>(x), \
  static_cast<int>((mul) * ((x) - static_cast<int32_t>(x)))

namespace HS {

  enum HEM_SIDE : uint8_t {
    LEFT_HEMISPHERE = 0,
    RIGHT_HEMISPHERE = 1,
#ifdef ARDUINO_TEENSY41
    LEFT2_HEMISPHERE = 2,
    RIGHT2_HEMISPHERE = 3,
#endif
    GLOBAL_CURSOR,
    GLOBAL_CURSOR_EXTRA,
    AUDIO_SLOT_L,
    AUDIO_SLOT_R,

    APPLET_CURSOR_COUNT,
    APPLET_SLOTS = GLOBAL_CURSOR,
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

  enum PopupType {
    MENU_POPUP,
    CLOCK_POPUP, PRESET_POPUP,
    QUANTIZER_POPUP,
    MESSAGE_POPUP,

    POPUP_TYPE_COUNT
  };

  enum ErrMsgIndex {
    NO_ERROR,
    LFS_WRITE_ERROR,
    PRESET_SAVED,
    MYSTERIOUS_ERROR,
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

  enum ScreensaverMode {
    SCREEN_BLANK,
    SCREEN_METERS,
    SCREEN_SCOPE,
    SCREEN_ZAPS,
    SCREEN_STARS,
    SCREEN_ZIPS,

    SCREENSAVER_MODE_COUNT
  };

  struct QuantEngineSettings {
    int16_t scale; // = OC::Scales::SCALE_SEMI;
    int8_t root_note;
    int8_t octave;
    uint16_t mask; // = 0xffff;
  };
  struct QuantEngine : public QuantEngineSettings {
    braids::Quantizer quantizer;

    QuantEngine() {
      scale = OC::Scales::SCALE_SEMI;
      mask = 0xffff;
      quantizer.Init();
      Reconfig();
    }

    void Reconfig() {
      quantizer.Configure(OC::Scales::GetScale(scale), mask);
    }
    void Configure(int scale_, uint16_t mask_) {
      CONSTRAIN(scale_, 0, OC::Scales::NUM_SCALES - 1);
      scale = scale_;
      if (mask_) mask = mask_;
      Reconfig();
    }
    void EditMask(int idx, bool on) {
      mask = on ? (mask | (1u << idx)) : (mask & ~(1u << idx));
    }
    void NudgeScale(int dir) {
      const int max = OC::Scales::NUM_SCALES;
      scale += dir;
      if (scale >= max) scale = 0;
      if (scale < 0) scale = max - 1;
      Reconfig();
    }
    void RotateMask(int dir) {
      const size_t scale_size = OC::Scales::GetScale( scale ).num_notes;
      uint16_t used_bits = ~(0xffffU << scale_size);
      mask &= used_bits;

      if (dir < 0) {
        dir = -dir;
        mask = (mask >> dir) | (mask << (scale_size - dir));
      } else {
        mask = (mask << dir) | (mask >> (scale_size - dir));
      }
      mask |= ~used_bits; // fill upper bits

      Reconfig();
    }

    int Process(int cv, int root, int transpose) {
      if (root == 0) root = (root_note << 7);
      return quantizer.Process(cv, root, transpose) + (octave * ONE_OCTAVE);
    }
    int Lookup(int note) {
      return quantizer.Lookup(note) + (root_note << 7) + (octave * ONE_OCTAVE);
    }

    const int Size() {
      return OC::Scales::GetScale( scale ).num_notes;
    }
  };

  extern uint32_t popup_tick; // for button feedback
  extern PopupType popup_type;
  extern uint8_t qview; // which quantizer's setting is shown in popup
  extern int q_edit;
  extern ErrMsgIndex msg_idx;

  // input quantizers, because sometimes we need hysteresis
  extern OC::SemitoneQuantizer input_quant[ADC_CHANNEL_LAST];

  extern QuantEngine q_engine[QUANT_CHANNEL_COUNT];

#if defined(ARDUINO_TEENSY41) || defined(VOR)
  extern int octave_max;
#elif defined(NORTHERNLIGHT)
  static constexpr int octave_max = 10;
#else
  static constexpr int octave_max = 6;
#endif

  extern int select_mode;
  extern bool cursor_wrap;
  extern bool auto_save_enabled;
  extern uint8_t trig_length;
  extern uint8_t screensaver_mode;
  extern const char * const ssmodes[];

  extern OC::menu::ScreenCursor<5> showhide_cursor;

  void Init();
  void DrawAppletList(bool blink = false);

  // --- Quantizer helpers
  QuantEngine& GetQuantEngine(int ch);
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
  void PokePopup(PopupType pop, ErrMsgIndex err = NO_ERROR);

} // namespace HS


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

template <typename T>
constexpr std::pair<int, T&&> IndexedInput(int index, T&& input_map) {
  return std::pair<int, T&&>(index, std::forward<T>(input_map));
}

void gfxPos(int x, int y);
void gfxPrint(int x, int y, const char *str);
void gfxPrint(int x, int y, int num);
void gfxPrint(const char *str);
void gfxPrint(int num);
void gfxPrint(int x_adv, int num);
void gfxPrintVoltage(int cv);
void gfxPrintFreqFromPitch(int16_t pitch);
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
void gfxFooter(const char *str, const uint8_t *icon = nullptr);

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
const int ProportionCV(const int cv_value, const int max_pixels, const int max_cv = HEMISPHERE_MAX_CV);
