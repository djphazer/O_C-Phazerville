#include <Arduino.h>
#include "OC_core.h"
#include "HemisphereApplet.h"
#include "HSUtils.h"

#ifdef ARDUINO_TEENSY41
#include "AudioSetup.h"
#endif

namespace HS {

  uint32_t popup_tick; // for button feedback
  PopupType popup_type = MENU_POPUP;
  uint8_t qview = 0; // which quantizer's setting is shown in popup
  int q_edit = 0; // edit cursor for quantizer popup, 0 = not editing

  OC::SemitoneQuantizer input_quant[ADC_CHANNEL_LAST];

  braids::Quantizer quantizer[QUANT_CHANNEL_COUNT]; // global shared quantizers
  int quant_scale[QUANT_CHANNEL_COUNT];
  int8_t root_note[QUANT_CHANNEL_COUNT];
  int8_t q_octave[QUANT_CHANNEL_COUNT];
  uint16_t q_mask[QUANT_CHANNEL_COUNT];

  // for Beat Sync'd octave or key switching
  int next_ch = -1;
  int8_t next_octave, next_root_note;

  int octave_max = 5;

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

  OC::menu::ScreenCursor<5> showhide_cursor;

  void Init() {
    for (int i = 0; i < ADC_CHANNEL_LAST; ++i)
      input_quant[i].Init();

    for (int i = 0; i < QUANT_CHANNEL_COUNT; ++i)
      quantizer[i].Init();

    showhide_cursor.Init(0, HEMISPHERE_AVAILABLE_APPLETS - 1);
    showhide_cursor.Scroll(0);
  }

  void PokePopup(PopupType pop) {
    popup_type = pop;
    popup_tick = OC::CORE::ticks;
  }

  void ProcessBeatSync() {
    if (next_ch > -1) {
      q_octave[next_ch] = next_octave;
      root_note[next_ch] = next_root_note;
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
    q_mask[ch] = mask;
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
    if (next_ch < 0) {
      next_ch = ch;
      next_root_note = root_note[ch];
      next_octave = q_octave[ch];
    }
    next_root_note += dir;

    if (next_root_note > 11 && next_octave < 5) {
      ++next_octave;
      next_root_note -= 12;
    }
    if (next_root_note < 0 && next_octave > -5) {
      --next_octave;
      next_root_note += 12;
    }
    CONSTRAIN(next_root_note, 0, 11);

    QueueBeatSync();
  }
  void NudgeOctave(int ch, int dir) {
    if (next_ch < 0) {
      next_ch = ch;
      next_root_note = root_note[ch];
      next_octave = q_octave[ch];
    }
    next_octave += dir;
    CONSTRAIN(next_octave, -5, 5);

    QueueBeatSync();
  }
  void NudgeScale(int ch, int dir) {
    const int max = OC::Scales::NUM_SCALES;
    int &s = quant_scale[ch];

    s+= dir;
    if (s >= max) s = 0;
    if (s < 0) s = max - 1;
    QuantizerConfigure(ch, s, q_mask[ch]);
  }
  void RotateMask(int ch, int dir) {
    const size_t scale_size = OC::Scales::GetScale( quant_scale[ch] ).num_notes;
    uint16_t &mask = q_mask[ch];
    uint16_t used_bits = ~(0xffffU << scale_size);
    mask &= used_bits;

    if (dir < 0) {
      dir = -dir;
      mask = (mask >> dir) | (mask << (scale_size - dir));
    } else {
      mask = (mask << dir) | (mask >> (scale_size - dir));
    }
    mask |= ~used_bits; // fill upper bits

    quantizer[ch].Configure(OC::Scales::GetScale(quant_scale[ch]), mask);
  }
  void QuantizerEdit(int ch) {
    qview = constrain(ch, 0, QUANT_CHANNEL_COUNT - 1);
    q_edit = 1;
  }
  void QEditEncoderMove(bool rightenc, int dir) {
    if (!rightenc) {
      // left encoder moves q_edit cursor
      const int scale_size = OC::Scales::GetScale( quant_scale[qview] ).num_notes;
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
        uint16_t mask = dir>0 ? (q_mask[qview] | (1u << idx)) : (q_mask[qview] & ~(1u << idx));
        QuantizerConfigure(qview, quant_scale[qview], mask);
      }
    }
  }

  void DrawPopup(const int config_cursor, const int preset_id, const bool blink) {

    enum ConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
    };

    if (popup_type == MENU_POPUP) {
      graphics.clearRect(73, 25, 54, 38);
      graphics.drawFrame(74, 26, 52, 36);
    } else if (popup_type == QUANTIZER_POPUP) {
      graphics.clearRect(20, 23, 88, 28);
      graphics.drawFrame(21, 24, 86, 26);
      graphics.setPrintPos(26, 28);
    } else {
      graphics.clearRect(23, 23, 82, 18);
      graphics.drawFrame(24, 24, 80, 16);
      graphics.setPrintPos(28, 28);
    }

    switch (popup_type) {
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
              gfxIcon(114, 40, auto_save_enabled ? CHECK_ON_ICON : CHECK_OFF_ICON );
            break;
          case CONFIG_DUMMY:
            gfxIcon(115, 50, RIGHT_ICON);
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
        const int root = (next_ch > -1) ? next_root_note : root_note[qview];
        const int octave = (next_ch > -1) ? next_octave : q_octave[qview];
        graphics.print("Q");
        graphics.print(qview + 1);
        graphics.print(":");
        graphics.print(OC::scale_names_short[ quant_scale[qview] ]);
        graphics.print(" ");
        graphics.print(OC::Strings::note_names[ root ]);
        if (octave >= 0) graphics.print("+");
        graphics.print(octave);

        // scale mask
        const size_t scale_size = OC::Scales::GetScale( quant_scale[qview] ).num_notes;
        for (size_t i = 0; i < scale_size; ++i) {
          const int x = 24 + i*5;

          if (q_mask[qview] >> i & 1)
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

/* Convert CV value to voltage level and print  to two decimal places */
void gfxPrintVoltage(int cv) {
#ifdef NORTHERNLIGHT
    int v = (cv * 120) / (12 << 7);
#else
    int v = (cv * 100) / (12 << 7);
#endif
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
  gfxLine(0, 11, 127, 11);
}
