#include <Arduino.h>
#include "HSClockManager.h"
#include "OC_core.h"
#include "SegmentDisplay.h"
#include "tideslite.h"
#include "OC_gpio.h"
#include "HSUtils.h"
#include "HSIOFrame.h"
#include "enigma/TuringMachine.h"
#include "vector_osc/HSVectorOscillator.h"

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
  const char* popup_msg;
  ErrMsgIndex msg_idx;

  int preset_id = -1;

  int q_edit = 0; // edit cursor for quantizer popup, 0 = not editing
  uint8_t qview = 0; // which quantizer's setting is shown in popup

  int midi_edit = 0;
  uint8_t mview = 0;

  util::SemitoneQuantizer input_quant[ADC_CHANNEL_COUNT];
  util::TuringShiftRegister* turing_machine_[ADC_CHANNEL_COUNT];
  peaks::MultistageEnvelope env_[DAC_CHANNEL_COUNT];

  // All of the HS:: globals should be instantiated here
  TuringMachine user_turing_machines[TURING_MACHINE_COUNT];
  VOSegment user_waveforms[VO_SEGMENT_COUNT];
  // global shared quantizers
  QuantEngine q_engine[QUANT_CHANNEL_COUNT];

  // for Beat Sync'd octave or key switching
  int next_ch = -1;
  int8_t next_octave, next_root_note;

#if defined(ARDUINO_TEENSY41) || defined(VOR)
  int octave_max = 6;
#endif

  bool midi_thru_enabled = 1;
  bool cursor_wrap = 0;
  bool auto_save_enabled = false;
  DigitalInputMap trigmap[ADC_CHANNEL_COUNT];
  CVInputMap cvmap[ADC_CHANNEL_COUNT];
  uint8_t trig_length = 10; // in ms, multiplier for HEMISPHERE_CLOCK_TICKS
  uint8_t screensaver_mode = SCREEN_BEATS; // ScreensaverMode
  const char * const ssmodes[SCREENSAVER_MODE_COUNT] = {
    "[blank]",
    "Meters", "Scope",
    "Zaps", "Stars", "Zips",
    "Beats",
  };

  OC::menu::ScreenCursor<5> showhide_cursor;

  peaks::MultistageEnvelope& GetEnvelope(int index) {
    return env_[index];
  }
  util::TuringShiftRegister& GetTM(int index) {
    if (!turing_machine_[index]) turing_machine_[index] = new util::TuringShiftRegister();
    return *turing_machine_[index];
  }
  QuantEngine& GetQEngine(int index) {
    return q_engine[index];
  }

  FLASHMEM
  void Init() {
    const int default_scales[8] = {
      OC::Scales::SCALE_SEMI + 0,
      OC::Scales::SCALE_SEMI + 1, // Ionian
      OC::Scales::SCALE_SEMI + 6, // Aeolian
      OC::Scales::SCALE_SEMI + 137, // Harmonic Minor

      OC::Scales::SCALE_SEMI + 5, // Mixolydian
      OC::Scales::SCALE_SEMI + 7, // Locrian
      OC::Scales::SCALE_SEMI + 10, // Pentatonic Major
      OC::Scales::SCALE_SEMI + 11, // Pentatonic Minor
    };

    for (auto &iq : input_quant)
      iq.Init();

    for (int i = 0; i < QUANT_CHANNEL_COUNT; ++i) {
      q_engine[i].quantizer.Init();
      q_engine[i].Configure(default_scales[i], 0xffff);
    }

    ResetMappings();
  }
  FLASHMEM
  void ResetMappings() {
    for (int i = 0; i < APPLET_SLOTS * 2; ++i) {
      trigmap[i].SetGateInput(i % 4);
      trigmap[i].Reset(true);
      cvmap[i].SetInput(i);
      cvmap[i].attenuversion = 60;
      frame.output_slew[i] = 0;
      frame.output_atten[i] = 60;
      frame.clockskip[i] = 0;
      clock_m.SetMultiply(0, i);
    }

  }

  void PokePopup(PopupType pop, const char* msg) {
    popup_msg = msg;
    popup_type = pop;
    popup_tick = OC::CORE::ticks;
  }
  void PokePopup(PopupType pop, ErrMsgIndex err) {
    popup_msg = OC::Strings::err_msg[err];
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
        q_engine[qview].Reconfig();
      }
    }
  }
  // -----
  void MidiMapEdit(int ch) {
    mview = constrain(ch, 0, MIDIMAP_MAX - 1);
    midi_edit = 1;
  }
  enum MEditCursor {
    OFF, CHANNEL,
    MODE,
    VOICE,
    RANGELOW, RANGEHIGH,

    MEDITCURSOR_COUNT
  };
  void MEditEncoderMove(bool rightenc, int dir) {
    if (!rightenc) {
      // left encoder moves midi_edit cursor
      midi_edit = constrain(midi_edit + dir, 1, MEDITCURSOR_COUNT-1);
    } else {
      MIDIMapping &map = frame.MIDIState.mapping[mview];
      // right encoder is delegated
      switch(midi_edit){
        case 1: // chan
          map.AdjustChannel(dir);
          frame.MIDIState.UpdateMidiChannelFilter();
          break;
        case 2: // mode
          map.AdjustFunction(dir);
          frame.MIDIState.UpdateMidiChannelFilter();
          break;
        case 3: // voice (poly only)
          map.AdjustVoice(dir);
          break;
        case 4: // low
          map.AdjustRangeLow(dir);
          break;
        case 5: // high
          map.AdjustRangeHigh(dir);
          break;
      }
    }
  }

  void DrawPopup(const int config_cursor, const int preset_id, const bool blink) {

    enum ConfigCursor {
        DELETE_PRESET,
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
    MIDI_POPUP,
    MESSAGE_POPUP,
    */
    switch (popup_type) {
      case MENU_POPUP:
        px = 73; py = 25;
        pw = 54; ph = 38;
        break;
      case MIDI_POPUP:
      case QUANTIZER_POPUP:
        px = 20; py = 23;
        pw = 88; ph = 28;
        break;
      case MESSAGE_POPUP:
        pw = 6 * strlen(popup_msg) + 10;
        pw = min(pw, 124);
        ph = 18;
        px = 62 - (pw/2);
        py = 23;
        break;
      default:
        px = 23; py = 23;
        pw = 82; ph = 18;
        break;
    }

    graphics.clearRect(px, py, pw, ph);
    graphics.drawFrame(px+1, py+1, pw-2, ph-2);
    graphics.setPrintPos(px+5, py+5);

    switch (popup_type) {
      default:
      case MESSAGE_POPUP:
        gfxPrint(popup_msg);
        break;
      case MENU_POPUP:
        gfxPrint(78, 30, "Load");
        gfxPrint(78, 40, config_cursor == AUTO_SAVE ? "(auto)" : "Save");
        gfxIcon(78, 50, PhzIcons::snowflakeA);
        gfxIcon(86, 50, ZAP_ICON);
        gfxIcon(94, 50, PhzIcons::snowflakeB);
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

          gfxInvert(px, py, pw, ph);

          // context clues at top/bottom of screen
          gfxFooter("L:cursor     R:adjust");
          graphics.clearRect(0, 0, 128, 10);
          gfxHeader("A:Oct-         B:Oct+");
        }

        break;
      }
      case MIDI_POPUP:
      {
        MIDIMapping& map = frame.MIDIState.mapping[mview];
        graphics.printf(
          "Ch:%s  %s", midi_channels[map.channel], midi_fn_name[map.function]
        );
        if (map.function == HEM_MIDI_CC_OUT) gfxPrint(map.function_cc);

        graphics.setPrintPos(px + 5, py + 15);
        graphics.printf(
          "V:%d<%s:%s>",
          map.dac_polyvoice + 1,
          midi_note_numbers[map.range_low],
          midi_note_numbers[map.range_high]
        );

        if (midi_edit) {
          if (midi_edit < 3) // chan or mode
            gfxIcon(px + 5 + 24 * midi_edit, 35, UP_BTN_ICON);
          else // voice, range low, range high
            gfxIcon(px + 17 + 20 * (midi_edit - 3), 45, UP_BTN_ICON);

          // context clues at top/bottom of screen
          gfxFooter("L:cursor     R:adjust");
          graphics.clearRect(0, 0, 128, 10);
          gfxHeader("A:Prev   M     B:Next");
          gfxPrint(61, 1, mview + 1);
        }

        gfxInvert(px, py, pw, ph);
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

  bool applet_is_hidden(const int& index);
  const char * get_applet_name(const int index);
  const uint8_t * get_applet_icon(const int index);

  FLASHMEM
  void DrawAppletList(bool blink) {
    const size_t LineH = 12;

    int y = (64 - (5 * LineH)) / 2;

    for (int current = showhide_cursor.first_visible();
         current <= showhide_cursor.last_visible();
         ++current, y += LineH) {

      if (!applet_is_hidden(current))
        gfxIcon(  12, y + 1, get_applet_icon(current));
      gfxPrint( 23, y + 2, get_applet_name(current));

      if (current == showhide_cursor.cursor_pos()) {
        gfxIcon(1, y + 1, RIGHT_ICON);
        if (blink) gfxInvert(0, y, 10, 10);
      }
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

void gfxPrintIcon(const uint8_t *data, int16_t w) {
    gfxIcon(graphics.getPrintPosX(), graphics.getPrintPosY(), data);
    gfxPos(graphics.getPrintPosX() + w, graphics.getPrintPosY());
}
void gfxPrint(const HS::DigitalInputMap &map) {
  gfxPrintIcon(map.Icon());
  if (map.Gate()) gfxInvert(graphics.getPrintPosX()-8, graphics.getPrintPosY(), 8, 8);
}
void gfxPrint(const HS::CVInputMap &map) {
  gfxPrintIcon(map.Icon());
  const int xpos = graphics.getPrintPosX() - 1;
  const int ypos = graphics.getPrintPosY() + 4;
  const int height = map.InRescaled(24);
  gfxLine(xpos, ypos, xpos, ypos - height);
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
  bool minutes = (swap && int_part > 600);
  if (minutes) {
    denom *= 60;
    int_part = num / denom;
  }
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
    gfxPrint(minutes ? "m" : "s");
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
    gfxIcon(x, 56, icon);
    x += 8;
  }
  gfxPrint(x, 56, str);
}

// --- Phazerville Screensaver Library ---
struct Zap {
    int x = 12800;
    int y = 6400;
    int x_v = 6;
    int y_v = 3;
    uint16_t flip = 0;

    void Flip() {
      flip = random(0xffff);
      Drift();
    }
    void Move(bool stars) {
        if (stars) Move(6100, 2900);
        else Move();
    }
    void Move(int target_x = -1, int target_y = -1) {
        x += x_v;
        y += y_v;
        if (x > 12700 || x < 0 || y > 6300 || y < 0) {
            if (target_x < 0 || target_y < 0) {
                x = random(12700);
                y = 0; // from the top
                y_v = 5 + random(10); // only falling
            } else {
                x = target_x + random(400);
                y = target_y + random(400);
                CONSTRAIN(x, 0, 12700);
                CONSTRAIN(y, 0, 6300);
                y_v = random(31) - 15;
            }

            x_v = random(51) - 25;
            if (x_v == 0) ++x_v;
            if (y_v == 0) ++y_v;
        }
    }
    void Drift() {
      // Snow drifts randomly as it falls
      x_v += random(3) - 1;
      CONSTRAIN(x_v, -5, 5);
    }
};
static constexpr int HOW_MANY_ZAPS = 30;
void ZapScreensaver(const uint8_t stars) {
  static Zap zaps[HOW_MANY_ZAPS];
  static int frame_delay = 0;
  static elapsedMillis timer = 0;
  const uint8_t* flake_icon[] = {
    PhzIcons::snowflakeA,
    PhzIcons::snowflakeB,
    PhzIcons::snowflakeC,
    ZAP_ICON
  };

  if (stars == 0 && timer > 100) {
    for (int i = 0; i < 10; i++) {
      zaps[i].Flip();
    }
    timer = 0;
  }
  for (int i = 0; i < (stars ? HOW_MANY_ZAPS : 10); i++) {
    if (frame_delay & 0x1) {
      if (stars > 1) {
        // Zips respawn from their previous sibling
        if (0 == i) zaps[0].Move();
        else zaps[i].Move(zaps[i-1].x, zaps[i-1].y);
      } else
        zaps[i].Move(stars == 1); // centered starfield
    }

    if (stars && frame_delay == 0) {
      // accel
      zaps[i].x_v *= 2;
      zaps[i].y_v *= 2;
    }

    if (stars)
      gfxPixel(zaps[i].x/100, zaps[i].y/100);
    else {
      const uint8_t idx = (zaps[i].flip < OC::CORE::FreeRam()) ? 3 : (zaps[i].flip % 3);
      gfxIcon(zaps[i].x/100, zaps[i].y/100, flake_icon[idx]);
    }
  }
  if (--frame_delay < 0) frame_delay = 100;
}

void BeatCounterScreensaver() {
  static SegmentDisplay digits{BIG_SEGMENTS};
  const int y = 27;

  if (HS::clock_m.IsRunning()) {
    gfxIcon(60, 10, HS::clock_m.Cycle() ? METRO_L_ICON : METRO_R_ICON);
  }

  // 4-bar phrases
  digits.PrintWhole(12, y, HS::clock_m.beat_count / 16 + 1, 100);
  gfxRect(48, y+8, 3, 3);
  // bars (measures)
  digits.PrintDigit(64, y, HS::clock_m.beat_count / 4 % 4 + 1);
  gfxRect(80, y+8, 3, 3);
  // beats
  digits.PrintDigit(96, y, HS::clock_m.beat_count % 4 + 1);
}
