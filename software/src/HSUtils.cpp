#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"
#include "HSUtils.h"

namespace HS {

  uint32_t popup_tick; // for button feedback
  PopupType popup_type = MENU_POPUP;
  uint8_t qview = 0; // which quantizer's setting is shown in popup
  bool q_edit = false; // flag to edit current quantizer

  braids::Quantizer quantizer[QUANT_CHANNEL_COUNT]; // global shared quantizers
  int quant_scale[QUANT_CHANNEL_COUNT];
  int8_t root_note[QUANT_CHANNEL_COUNT];
  int8_t q_octave[QUANT_CHANNEL_COUNT];

  int octave_max = 5;

  int select_mode = -1;
  bool cursor_wrap = 0;
  bool auto_save_enabled = false;
#ifdef ARDUINO_TEENSY41
  int trigger_mapping[] = { 1, 2, 3, 4, 1, 2, 3, 4 };
  int cvmapping[] = { 5, 6, 7, 8, 5, 6, 7, 8 };
#else
  int trigger_mapping[] = { 1, 2, 3, 4 };
  int cvmapping[] = { 1, 2, 3, 4 };
#endif
  uint8_t trig_length = 10; // in ms, multiplier for HEMISPHERE_CLOCK_TICKS
  uint8_t screensaver_mode = 3; // 0 = blank, 1 = Meters, 2 = Scope/Zaps, 3 = Zips/Stars

  // --- Quantizer helpers
  int GetLatestNoteNumber(int ch) {
    return quantizer[ch].GetLatestNoteNumber();
  }
  int Quantize(int ch, int cv, int root, int transpose) {
    if (root == 0) root = (root_note[ch] << 7);
    return quantizer[ch].Process(cv, root, transpose) + (q_octave[ch] * 12 << 7);
  }
  int QuantizerLookup(int ch, int note) {
    return quantizer[ch].Lookup(note) + (root_note[ch] << 7) + (q_octave[ch] * 12 << 7);
  }
  void QuantizerConfigure(int ch, int scale, uint16_t mask) {
    CONSTRAIN(scale, 0, OC::Scales::NUM_SCALES - 1);
    quant_scale[ch] = scale;
    quantizer[ch].Configure(OC::Scales::GetScale(scale), mask);
  }
  int GetScale(int ch) {
    return quant_scale[ch];
  }
  int GetRootNote(int ch) {
    return root_note[ch];
  }
  int SetRootNote(int ch, int root) {
    CONSTRAIN(root, 0, 11);
    return (root_note[ch] = root);
  }
  void NudgeRootNote(int ch, int dir) {
    int8_t &r = root_note[ch];
    r += dir;
    if (r > 11 && q_octave[ch] < 5) {
      ++q_octave[ch];
      r -= 12;
    }
    if (r < 0 && q_octave[ch] > -5) {
      --q_octave[ch];
      r += 12;
    }
    CONSTRAIN(r, 0, 11);
  }
  void NudgeScale(int ch, int dir) {
    const int max = OC::Scales::NUM_SCALES;
    int &s = quant_scale[ch];

    s+= dir;
    if (s >= max) s = 0;
    if (s < 0) s = max - 1;
    QuantizerConfigure(ch, s);
  }
  void QuantizerEdit(int ch) {
    qview = constrain(ch, 0, QUANT_CHANNEL_COUNT - 1);
    q_edit = true;
  }

  void DrawPopup(const int config_cursor, const int preset_id, const bool blink) {

    enum ConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
    };

    if (HS::popup_type == MENU_POPUP) {
      graphics.clearRect(73, 25, 54, 38);
      graphics.drawFrame(74, 26, 52, 36);
    } else {
      graphics.clearRect(23, 23, 82, 18);
      graphics.drawFrame(24, 24, 80, 16);
      graphics.setPrintPos(28, 28);
    }

    switch (HS::popup_type) {
      case MENU_POPUP:
        gfxPrint(78, 30, "Load");
        gfxPrint(78, 40, config_cursor == AUTO_SAVE ? "(auto)" : "Save");
        gfxPrint(78, 50, "Config");

        switch (config_cursor) {
          case LOAD_PRESET:
          case SAVE_PRESET:
            gfxIcon(104, 30 + (config_cursor-LOAD_PRESET)*10, LEFT_ICON);
            break;
          case AUTO_SAVE:
            if (blink)
              gfxIcon(114, 40, HS::auto_save_enabled ? CHECK_ON_ICON : CHECK_OFF_ICON );
            break;
          case CONFIG_DUMMY:
            gfxIcon(115, 50, RIGHT_ICON);
            break;
          default: break;
        }

        break;
      case CLOCK_POPUP:
        graphics.print("Clock ");
        if (HS::clock_m.IsRunning())
          graphics.print("Start");
        else
          graphics.print(HS::clock_m.IsPaused() ? "Armed" : "Stop");
        break;

      case PRESET_POPUP:
        graphics.print("> Preset ");
        graphics.print(OC::Strings::capital_letters[preset_id]);
        break;
      case QUANTIZER_POPUP:
        graphics.print("Q");
        graphics.print(HS::qview + 1);
        graphics.print(":");
        graphics.print(OC::scale_names_short[ HS::quant_scale[HS::qview] ]);
        graphics.print(" ");
        graphics.print(OC::Strings::note_names[ HS::root_note[HS::qview] ]);
        if (HS::q_octave[HS::qview] >= 0) graphics.print("+");
        graphics.print(HS::q_octave[HS::qview]);
        if (HS::q_edit) {
          gfxInvert(23, 23, 82, 18);
        }
        break;
    }
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

/* Convert CV value to voltage level and print  to two decimal places */
void gfxPrintVoltage(int cv) {
    int v = (cv * 100) / (12 << 7);
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

void gfxPixel(int x, int y) {
    graphics.setPixel(x, y);
}

void gfxFrame(int x, int y, int w, int h) {
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
void gfxIcon(int x, int y, const uint8_t *data) {
    gfxBitmap(x, y, 8, data);
}

