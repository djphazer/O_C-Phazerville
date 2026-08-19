// 225e-style front-panel preset manager for the 200e bus. See PresetBusUI.h.
#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

#include <Arduino.h>

#include "PresetBusUI.h"
#include "PresetBus.h"
#include "PresetEngine.h"
#include "OC_ui.h"
#include "OC_app_switcher.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_digital_inputs.h"
#include "PhzConfig.h"
#include "HSUtils.h"

extern uint_fast8_t MENU_REDRAW;

namespace OC {
namespace PresetBusUI {

// persistence (PRESETBUS_KEY namespace in GLOBALS.CFG)
static constexpr uint16_t kNextTrigKey = (8 << 8) | 0x11;
static constexpr uint16_t kLastTrigKey = (8 << 8) | 0x12;

static bool active = false;
static uint8_t sel = 0;          // selected preset, 0-29 (shown 1-30)
static int8_t cursor = 0;        // 0 preset, 1 name, 2 next-trig, 3 last-trig

// rename edit mode (UXR spec: recursive grammar, L=position R=character)
static bool edit_mode = false;
static int8_t edit_pos = 0;
static char edit_buf[PresetEngine::kNameLen + 1];
// cycling order: space adjacent to A and (via wrap) to '+'
static const char kCharset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./&+";
static constexpr int kCharsetLen = sizeof(kCharset) - 1;
static uint8_t next_trig = 0;    // 0 = off, 1-4 = TR1-4
static uint8_t last_trig = 0;
static bool assign_dirty = false;

// stored/empty indicator cache (checked on selection change only)
static int8_t sel_stored = -1;

// store/recall feedback: hold-progress while L is held, then a worded
// confirmation banner once the engine finishes the operation
static uint32_t hold_start_ms = 0;        // 0 = not holding
static int8_t pending_slot = -1;          // op we're waiting on
static bool pending_was_save = false;
static uint32_t pending_start_ms = 0;
static uint32_t pending_opcount = 0;
static char banner[14] = "";
static uint32_t banner_until_ms = 0;

// trigger edge detection (runs whether or not the overlay is open)
static bool trig_prev[4] = {false, false, false, false};

bool Active() { return active; }

FLASHMEM void Init() {
  uint64_t v = 0;
  if (PhzConfig::getValue(kNextTrigKey, v) && v <= 4) next_trig = (uint8_t)v;
  v = 0;
  if (PhzConfig::getValue(kLastTrigKey, v) && v <= 4) last_trig = (uint8_t)v;
  const int last = PresetEngine::LastSlot();
  if (last >= 0) sel = (uint8_t)last;
}

FLASHMEM static void persist_assignments() {
  if (!assign_dirty) return;
  assign_dirty = false;
  PhzConfig::load_config();
  PhzConfig::setValue(kNextTrigKey, next_trig);
  PhzConfig::setValue(kLastTrigKey, last_trig);
  PhzConfig::save_config();
  // hand the shared map back to the active app (see persist_cur_slot)
  app_switcher.current_app()->DispatchAppEvent(OC::APP_EVENT_RESUME);
}

FLASHMEM void Enter() {
  const int last = PresetEngine::LastSlot();
  if (last >= 0) sel = (uint8_t)last;
  cursor = 0;
  edit_mode = false;
  sel_stored = -1;
  active = true;
}

FLASHMEM void Exit() {
  active = false;
  persist_assignments();
}

FLASHMEM static void recall_selected() {
  PresetBus::BroadcastRecall(sel);
  // overlay open: the confirmation banner owns feedback; the popup serves
  // trigger-driven recalls that happen with the overlay closed
  if (!active) HS::PokePopup(HS::MESSAGE_POPUP, "Bus recall...");
}

FLASHMEM static void commit_name() {
  // trim trailing spaces; an all-space buffer commits as unnamed
  char out[PresetEngine::kNameLen + 1];
  memcpy(out, edit_buf, sizeof(out));
  for (int i = PresetEngine::kNameLen - 1; i >= 0 && out[i] == ' '; --i)
    out[i] = 0;
  PresetEngine::SetSlotName(sel, out);
}

FLASHMEM bool HandleEvent(const UI::Event &event) {
  if (!active) return false;
  if (sel_stored < 0) sel_stored = PresetEngine::SlotUsed(sel) ? 1 : 0;

  // ---- EDIT mode: the grammar recurses, one char cell is the focus ----
  if (edit_mode) {
    if (event.control == CONTROL_ENCODER_L) {
      edit_pos = constrain(edit_pos + (event.value > 0 ? 1 : -1), 0,
                           (int)PresetEngine::kNameLen - 1);
    } else if (event.control == CONTROL_ENCODER_R) {
      const char *hit = strchr(kCharset, edit_buf[edit_pos]);
      int idx = hit ? (int)(hit - kCharset) : 0;
      idx = (idx + (event.value % kCharsetLen) + kCharsetLen) % kCharsetLen;
      edit_buf[edit_pos] = kCharset[idx];
    } else if (event.control == CONTROL_BUTTON_L &&
               event.type == UI::EVENT_BUTTON_PRESS) {
      commit_name();               // DONE
      edit_mode = false;
    } else if ((event.control == CONTROL_BUTTON_UP ||
                event.control == CONTROL_BUTTON_DOWN) &&
               event.type == UI::EVENT_BUTTON_PRESS) {
      edit_mode = false;           // cancel: back out one tier only
    }
    // R press / L long / everything else: swallowed. A bus-wide recall
    // must never fire from a rename gesture.
    return true;
  }

  // ---- BROWSE ----
  if (event.control == CONTROL_BUTTON_UP || event.control == CONTROL_BUTTON_DOWN) {
    if (event.type == UI::EVENT_BUTTON_PRESS) Exit();
    return true;
  }

  if (event.control == CONTROL_ENCODER_L) {
    cursor = constrain(cursor + (event.value > 0 ? 1 : -1), 0, 3);
    return true;
  }
  if (event.control == CONTROL_ENCODER_R) {
    switch (cursor) {
      case 0:
        sel = (uint8_t)constrain((int)sel + event.value, 0, 29);
        sel_stored = -1;
        break;
      case 1: break;  // nothing directly spinnable; EDIT/click legend explains
      case 2:
        next_trig = (uint8_t)constrain((int)next_trig + event.value, 0, 4);
        assign_dirty = true;
        break;
      case 3:
        last_trig = (uint8_t)constrain((int)last_trig + event.value, 0, 4);
        assign_dirty = true;
        break;
    }
    return true;
  }
  if (event.control == CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_PRESS) {
    if (cursor == 1 && sel_stored == 1) {   // enter EDIT (stored slots only)
      memset(edit_buf, ' ', PresetEngine::kNameLen);
      edit_buf[PresetEngine::kNameLen] = 0;
      const char *n = PresetEngine::SlotName(sel);
      for (int i = 0; n[i] && i < (int)PresetEngine::kNameLen; ++i)
        edit_buf[i] = n[i];
      edit_pos = 0;
      edit_mode = true;
    }
    return true;
  }
  if (event.control == CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) {
    recall_selected();
    pending_slot = sel;
    pending_was_save = false;
    pending_start_ms = millis();
    pending_opcount = PresetEngine::OpCount();
    return true;
  }
  if (event.control == CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_LONG_PRESS) {
    if (cursor == 1 && sel_stored == 1) return true;  // STORE legend hidden here
    PresetBus::BroadcastSave(sel);
    sel_stored = -1;
    pending_slot = sel;
    pending_was_save = true;
    pending_start_ms = millis();
    pending_opcount = PresetEngine::OpCount();
    return true;
  }
  return true;
}

// ---- drawing ---------------------------------------------------------------
// Layout per the Orin_Fun design system review (module border, letterspaced
// legend + rule, 225e-style 7-segment LCD window, banana-jack presence dots,
// inversion = focus and nothing else).

// 7-segment digit: cell 12x22, segment thickness 2. Bits: A B C D E F G.
static const uint8_t kSeg[10] = {
  0b1111110,  // 0: ABCDEF
  0b0110000,  // 1: BC
  0b1101101,  // 2: ABGED
  0b1111001,  // 3: ABGCD
  0b0110011,  // 4: FBGC
  0b1011011,  // 5: AFGCD
  0b1011111,  // 6: AFGEDC
  0b1110000,  // 7: ABC
  0b1111111,  // 8
  0b1111011,  // 9: ABCDFG
};

FLASHMEM static void draw_7seg(int dx, int dy, uint8_t digit) {
  const uint8_t m = kSeg[digit % 10];
  if (m & 0b1000000) graphics.drawRect(dx + 2, dy,      8, 2);  // A
  if (m & 0b0100000) graphics.drawRect(dx + 10, dy + 2, 2, 8);  // B
  if (m & 0b0010000) graphics.drawRect(dx + 10, dy + 12, 2, 8); // C
  if (m & 0b0001000) graphics.drawRect(dx + 2, dy + 20, 8, 2);  // D
  if (m & 0b0000100) graphics.drawRect(dx,     dy + 12, 2, 8);  // E
  if (m & 0b0000010) graphics.drawRect(dx,     dy + 2,  2, 8);  // F
  if (m & 0b0000001) graphics.drawRect(dx + 2, dy + 10, 8, 2);  // G
}

FLASHMEM static void draw_jack(int cx, int cy, bool active) {
  graphics.drawCircle(cx, cy, 3);
  if (active) graphics.drawRect(cx - 1, cy - 1, 3, 3);
}

FLASHMEM void Draw() {
  graphics.drawFrame(0, 0, 128, 64);            // module border
  graphics.setPrintPos(4, 2);
  graphics.print("P R E S E T  B U S");         // letterspaced legend
  graphics.drawHLine(1, 11, 126);               // rule

  // LCD window (v2: shorter; the status word moved to the name row)
  graphics.drawFrame(43, 13, 42, 28);
  if (sel_stored < 0) sel_stored = PresetEngine::SlotUsed(sel) ? 1 : 0;
  const uint8_t shown = sel + 1;                // 01-30, zero-padded
  draw_7seg(50, 16, shown / 10);
  draw_7seg(66, 16, shown % 10);

  // edge legends: contextual (turns are unlabeled, presses are labeled)
  const char *l_top = "STORE", *l_hint = "hold";
  const char *r_top = "RECALL", *r_hint = "press";
  if (edit_mode) {
    l_top = "DONE"; l_hint = "click";
    r_top = "CHAR"; r_hint = "turn";
  } else if (cursor == 1 && sel_stored == 1) {
    l_top = "EDIT"; l_hint = "click";
  }
  graphics.setPrintPos(4, 16);
  graphics.print(l_top);
  graphics.setPrintPos(7, 26);
  graphics.print(l_hint);
  graphics.setPrintPos(88, 16);
  graphics.print(r_top);
  graphics.setPrintPos(91, 26);
  graphics.print(r_hint);

  // WPM presence: banana-jack dot, filled = present
  graphics.drawCircle(92, 38, 2);
  if (PresetBus::WpmPresent()) graphics.drawRect(91, 37, 3, 3);
  graphics.setPrintPos(97, 35);
  graphics.print(PresetBus::WpmPresent() ? "WPM" : "wpm");

  // name row (y=44): edit grid, or the worded state, or the name
  if (edit_mode) {
    for (int i = 0; i < (int)PresetEngine::kNameLen; ++i) {
      graphics.setPrintPos(16 + 6 * i, 44);
      graphics.print(edit_buf[i] == ' ' ? '_' : edit_buf[i]);
    }
  } else {
    const char *nm = PresetEngine::SlotName(sel);
    if (!sel_stored) {
      graphics.setPrintPos(49, 44);
      graphics.print("EMPTY");
    } else if (!nm[0]) {
      graphics.setPrintPos(43, 44);
      graphics.print("unnamed");
    } else {
      graphics.setPrintPos(64 - 3 * (int)strlen(nm), 44);
      graphics.print(nm);
    }
  }

  // 225e last/next pulse jacks
  draw_jack(8, 57, next_trig != 0);
  graphics.setPrintPos(14, 54);
  graphics.print("NEXT");
  graphics.setPrintPos(40, 54);
  if (next_trig) graphics.printf("TR%d", next_trig);
  else graphics.print("off");

  draw_jack(72, 57, last_trig != 0);
  graphics.setPrintPos(78, 54);
  graphics.print("LAST");
  graphics.setPrintPos(104, 54);
  if (last_trig) graphics.printf("TR%d", last_trig);
  else graphics.print("off");

  // hold-progress bar under the STORE column while the left encoder is held
  if (hold_start_ms && !edit_mode) {
    const uint32_t held = millis() - hold_start_ms;
    if (held > 120) {   // don't flicker on ordinary clicks
      graphics.drawFrame(4, 35, 34, 5);
      uint32_t w = (held >= 500) ? 32 : ((held - 120) * 32) / 380;
      if (w) graphics.drawRect(5, 36, w, 3);
    }
  }

  // one focus grammar: invert exactly what the right encoder will change
  if (edit_mode) {
    graphics.invertRect(15 + 6 * edit_pos, 43, 8, 10);
  } else switch (cursor) {
    case 0: graphics.invertRect(48, 14, 32, 26); break;
    case 1: {
      const char *nm = PresetEngine::SlotName(sel);
      int sx, len;
      if (!sel_stored) { sx = 49; len = 5; }
      else if (!nm[0]) { sx = 43; len = 7; }
      else { len = (int)strlen(nm); sx = 64 - 3 * len; }
      graphics.invertRect(sx - 2, 43, 6 * len + 4, 10);
      break;
    }
    case 2: graphics.invertRect(39, 53, 20, 10); break;
    case 3: graphics.invertRect(103, 53, 20, 10); break;
  }

  // confirmation banner, drawn over everything: worded, temporary
  if (banner_until_ms && millis() < banner_until_ms) {
    const int len = (int)strlen(banner);
    const int w = 6 * len + 16;
    const int x = 64 - w / 2;
    graphics.clearRect(x, 20, w, 20);
    graphics.drawFrame(x, 20, w, 20);
    graphics.drawFrame(x + 1, 21, w - 2, 18);   // double frame: this is an event
    graphics.setPrintPos(64 - 3 * len, 26);
    graphics.print(banner);
  } else {
    banner_until_ms = 0;
  }
}

void Task() {
  // follow externally-driven preset changes (WPM/225e recall, boot recall)
  // so trigger cycling always steps from the CURRENT preset, 225e-style
  static uint32_t seen_opcount = 0;
  if (pending_slot < 0 && PresetEngine::OpCount() != seen_opcount) {
    seen_opcount = PresetEngine::OpCount();
    const int last = PresetEngine::LastSlot();
    if (last >= 0 && (uint8_t)last != sel) {
      sel = (uint8_t)last;
      sel_stored = -1;
    }
  }

  // 225e last/next pulse inputs: rising edge cycles + recalls bus-wide.
  // Levels are sampled EVERY pass (assigned or not) so enabling an
  // assignment while a line sits high can never fire a spurious recall.
  for (uint8_t t = 0; t < 4; ++t) {
    const bool level = DigitalInputs::read_immediate((DigitalInput)t);
    const bool rising = level && !trig_prev[t];
    trig_prev[t] = level;
    if (!rising) continue;
    if (next_trig == t + 1) {
      sel = (sel + 1) % 30;
      sel_stored = -1;
      recall_selected();
    } else if (last_trig == t + 1) {
      sel = (sel + 29) % 30;
      sel_stored = -1;
      recall_selected();
    }
  }

  if (active) {
    // hold-progress source: sample the left encoder button directly
    const bool store_context = !edit_mode && !(cursor == 1 && sel_stored == 1);
    if (store_context && ui.read_immediate(CONTROL_BUTTON_L)) {
      if (!hold_start_ms) hold_start_ms = millis() | 1;
    } else {
      hold_start_ms = 0;
    }

    // completion watch: the engine bumps OpCount when the op finishes
    if (pending_slot >= 0) {
      if (PresetEngine::OpCount() != pending_opcount) {
        const uint8_t shown = (uint8_t)pending_slot + 1;
        if (pending_was_save)
          snprintf(banner, sizeof(banner),
                   PresetEngine::LastSaveOk() ? "STORED %d" : "STORE ERR %d", shown);
        else
          snprintf(banner, sizeof(banner), "RECALLED %d", shown);
        banner_until_ms = millis() + 1600;
        pending_slot = -1;
        sel_stored = -1;
      } else if (millis() - pending_start_ms > 4000) {
        snprintf(banner, sizeof(banner),
                 pending_was_save ? "STORE FAILED" : "RECALL FAILED");
        banner_until_ms = millis() + 1600;
        pending_slot = -1;
      }
    }

    // Throttled redraw: forcing MENU_REDRAW every pass makes the renderer
    // race the display DMA and the last-scanned corner visibly tears.
    static uint32_t last_kick = 0;
    if (millis() - last_kick >= 66) {   // ~15Hz; events redraw immediately
      last_kick = millis();
      ::MENU_REDRAW = 1;
    }
    ui.Poke();  // keep the screensaver away
  }
}

}  // namespace PresetBusUI
}  // namespace OC

#endif  // ARDUINO_TEENSY41 && PRESET_BUS
