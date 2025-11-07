#include <Arduino.h>
#include "OC_core.h"
#include "tideslite.h"
#include "OC_gpio.h"
#include "HSUtils.h"
#include "HSIOFrame.h"

#ifdef ARDUINO_TEENSY41
#include "SD.h"
#endif

const int ProportionCV(const int cv_value, const int max_pixels, const int max_cv) {
    int prop = constrain(Proportion(cv_value, max_cv, max_pixels), -max_pixels, max_pixels);
    return prop;
}

namespace HS {

  uint32_t popup_tick; // for button feedback
  PopupType popup_type = MENU_POPUP;
  int q_edit = 0; // edit cursor for quantizer popup, 0 = not editing
  uint8_t qview = 0; // which quantizer's setting is shown in popup
  ErrMsgIndex msg_idx;

  OC::SemitoneQuantizer input_quant[ADC_CHANNEL_LAST];

  // global shared quantizers
  QuantEngine q_engine[QUANT_CHANNEL_COUNT];

  // for Beat Sync'd octave or key switching
  int next_ch = -1;
  int8_t next_octave, next_root_note;

#if defined(ARDUINO_TEENSY41) || defined(VOR)
  int octave_max = 6;
#endif

  bool cursor_wrap = 0;
  bool auto_save_enabled = false;
  DigitalInputMap trigmap[ADC_CHANNEL_LAST];
  CVInputMap cvmap[ADC_CHANNEL_LAST];
  uint8_t trig_length = 10; // in ms, multiplier for HEMISPHERE_CLOCK_TICKS
  uint8_t screensaver_mode = SCREEN_STARS; // ScreensaverMode
  const char * const ssmodes[SCREENSAVER_MODE_COUNT] = {
    "[blank]",
    "Meters", "Scope",
    "Zaps", "Stars", "Zips",
  };

  OC::menu::ScreenCursor<5> showhide_cursor;

  FLASHMEM
  void Init() {
    for (auto &iq : input_quant)
      iq.Init();

    for (auto &q : q_engine)
      q.quantizer.Init();

    for (int i = 0; i < APPLET_SLOTS * 2; ++i) {
      trigmap[i].source = (i%4) + 1;
      cvmap[i].source = i + 1;
      clock_m.SetMultiply(0, i);
    }
  }

  void PokePopup(PopupType pop, ErrMsgIndex err) {
    msg_idx = err;
    popup_type = pop;
    popup_tick = OC::CORE::ticks;
  }

  void ProcessBeatSync() {
    if (next_ch > -1) {
      q_engine[next_ch].octave = next_octave;
      q_engine[next_ch].root_note = next_root_note;
      next_ch = -1;
    }
  }
  void QueueBeatSync() {
    if (clock_m.IsRunning())
      clock_m.BeatSync( &ProcessBeatSync );
    else
      ProcessBeatSync();
  }

  // --- Quantizer helpers
  QuantEngine& GetQuantEngine(int ch) {
    return q_engine[ch];
  }
  int GetLatestNoteNumber(int ch) {
    return q_engine[ch].quantizer.GetLatestNoteNumber();
  }
  int Quantize(int ch, int cv, int root, int transpose) {
    return q_engine[ch].Process(cv, root, transpose);
  }
  int QuantizerLookup(int ch, int note) {
    return q_engine[ch].Lookup(note);
  }
  void QuantizerConfigure(int ch, int scale, uint16_t mask) {
    q_engine[ch].Configure(scale, mask);
  }
  int GetScale(int ch) {
    return q_engine[ch].scale;
  }
  int GetRootNote(int ch) {
    return q_engine[ch].root_note;
  }
  int SetRootNote(int ch, int root) {
    CONSTRAIN(root, 0, 11);
    return (q_engine[ch].root_note = root);
  }
  void NudgeRootNote(int ch, int dir) {
    if (next_ch < 0) {
      next_ch = ch;
      next_root_note = q_engine[ch].root_note;
      next_octave = q_engine[ch].octave;
    }
    next_root_note += dir;

    if (next_root_note > 11 && next_octave < octave_max) {
      ++next_octave;
      next_root_note -= 12;
    }
    if (next_root_note < 0 && next_octave > -octave_max) {
      --next_octave;
      next_root_note += 12;
    }
    CONSTRAIN(next_root_note, 0, 11);

    QueueBeatSync();
  }
  void NudgeOctave(int ch, int dir) {
    if (next_ch < 0) {
      next_ch = ch;
      next_root_note = q_engine[ch].root_note;
      next_octave = q_engine[ch].octave;
    }
    next_octave += dir;
    CONSTRAIN(next_octave, -octave_max, octave_max);

    QueueBeatSync();
  }
  void NudgeScale(int ch, int dir) {
    q_engine[ch].NudgeScale(dir);
  }
  void RotateMask(int ch, int dir) {
    q_engine[ch].RotateMask(dir);
  }
  void QuantizerEdit(int ch) {
    qview = constrain(ch, 0, QUANT_CHANNEL_COUNT - 1);
    q_edit = 1;
  }
  void QEditEncoderMove(bool rightenc, int dir) {
    if (!rightenc) {
      // left encoder moves q_edit cursor
      const int scale_size = q_engine[qview].Size();
      q_edit = constrain(q_edit + dir, 1, 3 + scale_size);
    } else {
      // right encoder is delegated
      if (q_edit == 1) // scale
        NudgeScale(qview, dir);
      else if (q_edit == 2) // root
        NudgeRootNote(qview, dir);
      else if (q_edit == 3) { // mask rotate
        RotateMask(qview, dir);
      } else { // edit mask bits
        const int idx = q_edit - 4;
        q_engine[qview].EditMask(idx, dir>0);
      }
    }
  }

  void DrawPopup(const int config_cursor, const int preset_id, const bool blink) {

    enum ConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
    };

    int px, py, pw, ph;

    /*
    MENU_POPUP,
    CLOCK_POPUP,
    PRESET_POPUP,
    QUANTIZER_POPUP,
    MESSAGE_POPUP,
    */
    if (popup_type == MENU_POPUP) {
      px = 73;
      py = 25;
      pw = 54;
      ph = 38;
    } else if (popup_type == QUANTIZER_POPUP) {
      px = 20;
      py = 23;
      pw = 88;
      ph = 28;
    } else if (popup_type == MESSAGE_POPUP) {
      px = 16;
      py = 23;
      pw = 96;
      ph = 18;
    } else {
      px = 23;
      py = 23;
      pw = 82;
      ph = 18;
    }

    graphics.clearRect(px, py, pw, ph);
    graphics.drawFrame(px+1, py+1, pw-2, ph-2);
    graphics.setPrintPos(px+5, py+5);

    switch (popup_type) {
      default:
      case MESSAGE_POPUP:
        gfxPrint(OC::Strings::err_msg[msg_idx]);
        break;
      case MENU_POPUP:
        gfxPrint(78, 30, "Load");
        gfxPrint(78, 40, config_cursor == AUTO_SAVE ? "(auto)" : "Save");
        gfxIcon(78, 50, ZAP_ICON);
        gfxIcon(86, 50, ZAP_ICON);
        gfxIcon(94, 50, ZAP_ICON);
        //gfxPrint(78, 50, "????");

        switch (config_cursor) {
          case LOAD_PRESET:
          case SAVE_PRESET:
            gfxIcon(104, 30 + (config_cursor-LOAD_PRESET)*10, LEFT_ICON);
            break;
          case AUTO_SAVE:
            if (auto_save_enabled)
              gfxInvert(78, 40, 37, 8);
            gfxIcon(116, 40, LEFT_ICON);
            break;
          case CONFIG_DUMMY:
            gfxIcon(104, 50, LEFT_ICON);
            break;
          default: break;
        }

        break;
      case CLOCK_POPUP:
        graphics.print("Clock ");
        if (clock_m.IsRunning())
          graphics.print("Start");
        else
          graphics.print(clock_m.IsPaused() ? "Armed" : "Stop");
        break;

      case PRESET_POPUP:
        graphics.print("> Preset ");
        graphics.print(OC::Strings::capital_letters[preset_id]);
        break;
      case QUANTIZER_POPUP:
      {
        auto &q = q_engine[qview];
        const int root = (next_ch > -1) ? next_root_note : q.root_note;
        const int octave = (next_ch > -1) ? next_octave : q.octave;
        graphics.print("Q");
        graphics.print(qview + 1);
        graphics.print(":");
        graphics.print(OC::scale_names_short[ q.scale ]);
        graphics.print(" ");
        graphics.print(OC::Strings::note_names[ root ]);
        if (octave >= 0) graphics.print("+");
        graphics.print(octave);

        // scale mask
        const size_t scale_size = q.Size();
        for (size_t i = 0; i < scale_size; ++i) {
          const int x = 24 + i*5;

          if (q.mask >> i & 1)
            gfxRect(x, 40, 4, 4);
          else
            gfxFrame(x, 40, 4, 4);
        }

        if (q_edit) {
          if (q_edit < 3) // scale or root
            gfxIcon(26 + 26*q_edit, 35, UP_BTN_ICON);
          else if (q_edit == 3) // mask rotate
            gfxFrame(23, 39, 81, 6, true);
          else
            gfxIcon(22 + (q_edit-4)*5, 44, UP_BTN_ICON);

          gfxInvert(20, 23, 88, 28);

          // context clues at top/bottom of screen
          gfxFooter("L:cursor     R:adjust");
          graphics.clearRect(0, 0, 128, 10);
          gfxHeader("A:Oct+         B:Oct-");
        }

        break;
      }
    }
  }

  void ToggleClockRun() {
    if (clock_m.IsRunning()) {
      clock_m.Stop();
    } else {
      bool p = clock_m.IsPaused();
      clock_m.Start( !p );
    }
    PokePopup(CLOCK_POPUP);
  }

} // namespace HS

//////////////// Hemisphere-like graphics methods for easy porting
////////////////////////////////////////////////////////////////////////////////
void gfxPos(int x, int y) {
    graphics.setPrintPos(x, y);
}

void gfxPrint(int x, int y, const char *str) {
    graphics.setPrintPos(x, y);
    graphics.print(str);
}

void gfxPrint(int x, int y, int num) {
    graphics.setPrintPos(x, y);
    graphics.print(num);
}

void gfxPrint(const char *str) {
    graphics.print(str);
}

void gfxPrint(int num) {
    graphics.print(num);
}

void gfxPrint(int x_adv, int num) { // Print number with character padding
    for (int c = 0; c < (x_adv / 6); c++) gfxPrint(" ");
    gfxPrint(num);
}

void gfxPrint(int x, int y, HS::QuantEngine &q_eng, bool overlay = true) {
  if (overlay) {
    graphics.clearRect(x - 2, y - 2, 29, 22);
    gfxFrame(x - 1, y - 2, 27, 21, true);
  }

  gfxPrint(x, y, OC::scale_names_short[q_eng.scale]);
  gfxPrint(
    (q_eng.octave == 0 ? x + 6 : x),
    y + 10,
    OC::Strings::note_names_unpadded[q_eng.root_note]
  );
  if (q_eng.octave != 0) {
    gfxPrint(x + 12, y + 10, q_eng.octave);
  }
}
void gfxPrintScale(int x, int y, int qsel) {
  gfxPrint(x, y, HS::q_engine[qsel]);
}

/* Convert CV value to voltage level and print  to two decimal places */
void gfxPrintVoltage(int cv) {
    int v = (cv * (NorthernLightModular? 120 : 100)) / (12 << 7);
    bool neg = v < 0 ? 1 : 0;
    if (v < 0) v = -v;
    int wv = v / 100; // whole volts
    int dv = v - (wv * 100); // decimal
    gfxPrint(neg ? "-" : "+");
    gfxPrint(wv);
    gfxPrint(".");
    if (dv < 10) gfxPrint("0");
    gfxPrint(dv);
    gfxPrint("V");
}

void gfxPrintFreqFromPitch(int16_t pitch) {
  uint32_t num = ComputePhaseIncrement(pitch);
  uint32_t denom = 0xffffffff / 16666;
  bool swap = num < denom;
  if (swap) {
    uint32_t t = num;
    num = denom;
    denom = t;
  }
  int int_part = num / denom;
  int digits = 0;
  if (int_part < 10)
    digits = 1;
  else if (int_part < 100)
    digits = 2;
  else if (int_part < 1000)
    digits = 3;
  else
    digits = 4;

  gfxPrint(int_part);
  gfxPrint(".");

  num %= denom;
  while (digits < 4) {
    num *= 10;
    gfxPrint(num / denom);
    num %= denom;
    digits++;
  }
  if (swap) {
    gfxPrint("s");
  } else {
    gfxPrint("Hz");
  }
}

void gfxPixel(int x, int y) {
    graphics.setPixel(x, y);
}

void gfxFrame(int x, int y, int w, int h, bool dotted) {
  if (dotted) {
    gfxDottedLine(x, y, x + w - 1, y); // top
    gfxDottedLine(x, y + 1, x, y + h - 1); // vert left
    gfxDottedLine(x + w - 1, y + 1, x + w - 1, y + h - 1); // vert right
    gfxDottedLine(x, y + h - 1, x + w - 1, y + h - 1); // bottom
  } else
    graphics.drawFrame(x, y, w, h);
}

void gfxRect(int x, int y, int w, int h) {
    graphics.drawRect(x, y, w, h);
}

void gfxInvert(int x, int y, int w, int h) {
    graphics.invertRect(x, y, w, h);
}

void gfxLine(int x, int y, int x2, int y2) {
    graphics.drawLine(x, y, x2, y2);
}

void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p) {
#ifdef HS_GFX_MOD
    graphics.drawLine(x, y, x2, y2, p);
#else
    graphics.drawLine(x, y, x2, y2);
#endif
}

void gfxCircle(int x, int y, int r) {
    graphics.drawCircle(x, y, r);
}

void gfxBitmap(int x, int y, int w, const uint8_t *data) {
    graphics.drawBitmap8(x, y, w, data);
}

// Like gfxBitmap, but always 8x8
void gfxIcon(int x, int y, const uint8_t *data, bool clearfirst) {
  if (clearfirst) graphics.clearRect(x, y, 8, 8);
  gfxBitmap(x, y, 8, data);
}

void gfxHeader(const char *str, const uint8_t *icon) {
  int x = 1;
  if (icon) {
    gfxIcon(x, 1, icon);
    x += 8;
  }
  gfxPrint(x, 1, str);
  gfxLine(0, 10, 127, 10);
  //gfxLine(0, 11, 127, 11);
}

void gfxFooter(const char *str, const uint8_t *icon) {
  graphics.clearRect(0, 54, 128, 10);
  gfxLine(0, 54, 127, 54);
  int x = 1;
  if (icon) {
    gfxIcon(x, 1, icon);
    x += 8;
  }
  gfxPrint(x, 56, str);
}
