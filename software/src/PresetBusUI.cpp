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

static constexpr uint16_t kNextTrigKey = (8 << 8) | 0x11;
static constexpr uint16_t kLastTrigKey = (8 << 8) | 0x12;
static constexpr uint16_t kActiveModeKey = (8 << 8) | 0x15;

static bool active = false;
static uint8_t sel = 0;
static int8_t cursor = 0;
static constexpr int8_t kLastCursor = 5;

// MANAGER page stays hidden until A+B+X+Y are held for 3 s; not saved
static bool manager_unlocked = false;
static constexpr uint16_t kUnlockButtons =
    CONTROL_BUTTON_A | CONTROL_BUTTON_B | CONTROL_BUTTON_X | CONTROL_BUTTON_Y;
static constexpr uint32_t kUnlockHoldMs = 3000;
static uint32_t unlock_hold_ms = 0;
static bool unlock_fired = false;
FLASHMEM static int8_t last_cursor() { return manager_unlocked ? kLastCursor : kLastCursor - 1; }

static bool active_mode = false;
static bool mode_dirty = false;

static bool edit_mode = false;
static int8_t edit_pos = 0;
static char edit_buf[PresetEngine::kNameLen + 1];
static uint8_t edit_slot = 0;
static const char kCharset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./&+";
static constexpr int kCharsetLen = sizeof(kCharset) - 1;
static uint8_t next_trig = 0;
static uint8_t last_trig = 0;
static bool assign_dirty = false;

static uint8_t link_bank_edit = 0;
static bool link_bank_dirty = false;

static int8_t sel_stored = -1;

static uint32_t hold_start_ms = 0;
static const uint32_t kStoreHoldMs = OC::Ui::kLongPressTicks;
static const uint32_t kRecallHoldMs = OC::Ui::kLongPressTicks / 2;
static const uint32_t kBankHoldMs = kStoreHoldMs * 3;
static const uint32_t kManagerHoldMs = 3000;
static uint32_t recall_hold_ms = 0;
static bool recall_fired = false;
static bool store_fired = false;
static int8_t pending_slot = -1;
static bool pending_was_save = false;
static uint32_t pending_start_ms = 0;
static uint32_t pending_opcount = 0;
static char banner[24] = "";
static char banner2[20] = "";
static uint32_t banner_until_ms = 0;
static char banner_base[20] = "";
static uint32_t banner_follow_since = 0;

FLASHMEM static void set_banner(const char *text, uint32_t follow_since) {
  strncpy(banner_base, text, sizeof(banner_base) - 1);
  banner_base[sizeof(banner_base) - 1] = 0;
  banner_follow_since = follow_since;
  snprintf(banner, sizeof(banner), "%s", banner_base);
  banner2[0] = 0;
  banner_until_ms = millis() + 1600;
}

FLASHMEM static void set_banner2(const char *line1, const char *line2) {
  set_banner(line1, 0);
  strncpy(banner2, line2, sizeof(banner2) - 1);
  banner2[sizeof(banner2) - 1] = 0;
  banner_until_ms = millis() + 2500;
}

FLASHMEM static void bank_copy_banner(bool import, int n, uint8_t bank) {
  char l1[20], l2[20];
  if (n < 0) {
    snprintf(l1, sizeof(l1), import ? "IMPORT FAILED" : "EXPORT FAILED");
    snprintf(l2, sizeof(l2), "BANK %02u", bank);
  } else {
    snprintf(l1, sizeof(l1), "%s %d PRESET%s", import ? "IMPORTED" : "EXPORTED",
             n, n == 1 ? "" : "S");
    snprintf(l2, sizeof(l2), import ? "FROM BANK %02u" : "TO BANK %02u", bank);
  }
  set_banner2(l1, l2);
}

static const uint32_t kIdleTimeoutMs = 60000;
static uint32_t last_activity_ms = 0;
FLASHMEM static void touch_activity() { last_activity_ms = millis(); }

bool Active() { return active; }
bool ActiveMode() { return active_mode; }

FLASHMEM void Init() {
  uint64_t v = 0;
  if (PhzConfig::getValue(kNextTrigKey, v) && v <= 4) next_trig = (uint8_t)v;
  v = 0;
  if (PhzConfig::getValue(kLastTrigKey, v) && v <= 4) last_trig = (uint8_t)v;
  // passive unless active mode was chosen and saved; active keeps MANAGER visible
  v = 0;
  active_mode = PhzConfig::getValue(kActiveModeKey, v) && v == 1;
  manager_unlocked = active_mode;
  const int last = PresetEngine::BusSlot();
  if (last >= 0) sel = (uint8_t)last;
  link_bank_edit = PresetEngine::QuadLinkBank();
}

FLASHMEM static void persist_assignments() {
  if (!assign_dirty && !link_bank_dirty && !mode_dirty) return;
  assign_dirty = false;
  mode_dirty = false;
  PhzConfig::load_config();
  PhzConfig::setValue(kNextTrigKey, next_trig);
  PhzConfig::setValue(kLastTrigKey, last_trig);
  PhzConfig::setValue(kActiveModeKey, active_mode ? 1 : 0);
  PhzConfig::save_config();
  app_switcher.current_app()->DispatchAppEvent(OC::APP_EVENT_RESUME);
  if (link_bank_dirty) {
    link_bank_dirty = false;
    PresetEngine::SetQuadLinkBank(link_bank_edit);
  }
}

FLASHMEM void Enter() {
  if (!Buchla200eHardware()) return;
  const int last = PresetEngine::BusSlot();
  if (last >= 0) sel = (uint8_t)last;
  cursor = 0;
  edit_mode = false;
  sel_stored = -1;
  active = true;
  touch_activity();
  if (!link_bank_dirty) link_bank_edit = PresetEngine::QuadLinkBank();
  hold_start_ms = 0;
  recall_hold_ms = 0;
  recall_fired = false;
  ui.IgnoreUntilRelease(CONTROL_BUTTON_L | CONTROL_BUTTON_R);
}

FLASHMEM void Exit() {
  active = false;
  hold_start_ms = 0;
  recall_hold_ms = 0;
  recall_fired = false;
  store_fired = false;
  pending_slot = -1;
  banner_until_ms = 0;
  edit_mode = false;
  persist_assignments();
}

FLASHMEM static uint8_t step_trig_avoiding(int current, int delta, uint8_t avoid) {
  int nv = constrain(current + delta, 0, 4);
  if (avoid && nv == (int)avoid) {
    nv = constrain(nv + (delta > 0 ? 1 : -1), 0, 4);
    if (nv == (int)avoid) nv += (delta > 0) ? -1 : 1;
  }
  return (uint8_t)nv;
}

FLASHMEM static void recall_selected() {
  if (active_mode) PresetBus::BroadcastRecall(sel);
  else PresetEngine::RequestRecall(sel);
  if (!active) HS::PokePopup(HS::MESSAGE_POPUP, active_mode ? "Bus recall..." : "Recall...");
}

FLASHMEM static void store_selected() {
  if (active_mode) PresetBus::BroadcastSave(sel);
  else PresetEngine::RequestSave(sel);
}

FLASHMEM static void commit_name() {
  char out[PresetEngine::kNameLen + 1];
  memcpy(out, edit_buf, sizeof(out));
  for (int i = PresetEngine::kNameLen - 1; i >= 0 && out[i] == ' '; --i)
    out[i] = 0;
  PresetEngine::SetSlotName(edit_slot, out);
}

FLASHMEM __attribute__((noinline)) bool HandleEvent(const UI::Event &event) {
  if (!active) return false;
  touch_activity();
  if (sel_stored < 0) sel_stored = PresetEngine::SlotUsed(sel) ? 1 : 0;

  if (event.type == UI::EVENT_BUTTON_DOWN &&
      (event.control == CONTROL_BUTTON_L || event.control == CONTROL_BUTTON_R)) {
    if (event.mask & (CONTROL_BUTTON_A | CONTROL_BUTTON_Z)) {
      Exit();
      return false;
    }
    if ((event.mask & (CONTROL_BUTTON_L | CONTROL_BUTTON_R))
            == (CONTROL_BUTTON_L | CONTROL_BUTTON_R)) {
      Exit();
      ui.IgnoreUntilRelease(CONTROL_BUTTON_L | CONTROL_BUTTON_R);
      return true;
    }
  }

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
      commit_name();
      edit_mode = false;
    } else if ((event.control == CONTROL_BUTTON_UP ||
                event.control == CONTROL_BUTTON_DOWN) &&
               event.type == UI::EVENT_BUTTON_PRESS) {
      edit_mode = false;
    }
    return true;
  }

  // all four held: swallow their releases so A/B don't close the screen
  if (event.type == UI::EVENT_BUTTON_DOWN &&
      (event.mask & kUnlockButtons) == kUnlockButtons) {
    ui.IgnoreUntilRelease(kUnlockButtons);
    return true;
  }

  if (event.control == CONTROL_BUTTON_UP || event.control == CONTROL_BUTTON_DOWN) {
    if (event.type == UI::EVENT_BUTTON_PRESS) Exit();
    return true;
  }

  if (event.control == CONTROL_ENCODER_L) {
    cursor = constrain(cursor + (event.value > 0 ? 1 : -1), 0, last_cursor());
    return true;
  }
  if (event.control == CONTROL_ENCODER_R) {
    switch (cursor) {
      case 0:
        sel = (uint8_t)constrain((int)sel + event.value, 0, 29);
        sel_stored = -1;
        break;
      case 1: break;
      case 2:
        next_trig = step_trig_avoiding(next_trig, event.value, last_trig);
        assign_dirty = true;
        break;
      case 3:
        last_trig = step_trig_avoiding(last_trig, event.value, next_trig);
        assign_dirty = true;
        break;
      case 4:
        link_bank_edit = (uint8_t)constrain((int)link_bank_edit + event.value,
                                            0, PresetEngine::kQuadBankCount - 1);
        link_bank_dirty = true;
        break;
    }
    return true;
  }
  if (event.control == CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_PRESS) {
    if (cursor == 1 && sel_stored == 1) {
      memset(edit_buf, ' ', PresetEngine::kNameLen);
      edit_buf[PresetEngine::kNameLen] = 0;
      const char *n = PresetEngine::SlotName(sel);
      for (int i = 0; n[i] && i < (int)PresetEngine::kNameLen; ++i)
        edit_buf[i] = n[i];
      edit_pos = 0;
      edit_slot = sel;
      edit_mode = true;
    } else if (cursor == 4) {
      PresetEngine::SetQuadLinkEnabled(!PresetEngine::QuadLinkEnabled());
    }
    return true;
  }
  if (event.control == CONTROL_BUTTON_R) return true;
  if (event.control == CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_LONG_PRESS) {
    if (cursor == 1 && sel_stored == 1) return true;
    if (cursor == 5) return true;
    if (cursor == 4) return true;
    store_selected();
    store_fired = true;
    sel_stored = -1;
    pending_slot = sel;
    pending_was_save = true;
    pending_start_ms = millis();
    pending_opcount = PresetEngine::OpCount();
    return true;
  }
  return true;
}

static const uint8_t kSeg[10] = {
  0b1111110,
  0b0110000,
  0b1101101,
  0b1111001,
  0b0110011,
  0b1011011,
  0b1011111,
  0b1110000,
  0b1111111,
  0b1111011,
};

// w = digit width in pixels (12 normal, narrower to fit three letters)
FLASHMEM static void draw_7seg_mask(int dx, int dy, uint8_t m, int w = 12) {
  const int h = w - 4, r = dx + w - 2;
  if (m & 0b1000000) graphics.drawRect(dx + 2, dy,      h, 2);
  if (m & 0b0100000) graphics.drawRect(r, dy + 2, 2, 8);
  if (m & 0b0010000) graphics.drawRect(r, dy + 12, 2, 8);
  if (m & 0b0001000) graphics.drawRect(dx + 2, dy + 20, h, 2);
  if (m & 0b0000100) graphics.drawRect(dx,     dy + 12, 2, 8);
  if (m & 0b0000010) graphics.drawRect(dx,     dy + 2,  2, 8);
  if (m & 0b0000001) graphics.drawRect(dx + 2, dy + 10, h, 2);
}

FLASHMEM static void draw_7seg(int dx, int dy, uint8_t digit) {
  draw_7seg_mask(dx, dy, kSeg[digit % 10]);
}

FLASHMEM static const char *bank_row_label(int &sx) {
  const char *lbl = PresetEngine::QuadLinkEnabled() ? "LINKED BANK" : "UNLINKED";
  sx = max(23, 64 - 3 * (int)strlen(lbl));
  return lbl;
}

enum Page : uint8_t { PAGE_PRESETS, PAGE_BANKS, PAGE_MANAGER };
FLASHMEM static Page page_of(int8_t c) {
  return c <= 3 ? PAGE_PRESETS : (c == 4 ? PAGE_BANKS : PAGE_MANAGER);
}

static constexpr uint8_t kSegO = 0b1111110;
static constexpr uint8_t kSegN = 0b1110110;
static constexpr uint8_t kSegF = 0b1000111;


FLASHMEM static void draw_jack(int cx, int cy, bool active) {
  graphics.drawCircle(cx, cy, 3);
  if (active) graphics.drawRect(cx - 1, cy - 1, 3, 3);
}

FLASHMEM __attribute__((noinline)) void Draw() {
  const Page page = edit_mode ? PAGE_PRESETS : page_of(cursor);

  graphics.drawFrame(0, 0, 128, 64);
  graphics.setPrintPos(4, 2);
  static const char *const kTitles[] = { "P R E S E T S", "B A N K S", "M A N A G E R" };
  graphics.print(kTitles[page]);
  for (int i = 0; i < (manager_unlocked ? 3 : 2); ++i) {
    graphics.drawCircle(110 + 6 * i, 5, 2);
    if (i == page) graphics.drawRect(109 + 6 * i, 4, 3, 3);
  }
  graphics.drawHLine(1, 11, 126);

  if (sel_stored < 0) sel_stored = PresetEngine::SlotUsed(sel) ? 1 : 0;
  graphics.drawFrame(43, 13, 42, 28);
  if (page == PAGE_PRESETS) {
    const uint8_t shown = (edit_mode ? edit_slot : sel) + 1;
    // unsaved changes: number blinks slowly
    const bool modified = !edit_mode && app_switcher.current_app()->PresetModified();
    if (!modified || (millis() / 700) % 2 == 0) {
      draw_7seg(50, 16, shown / 10);
      draw_7seg(66, 16, shown % 10);
    }
  } else if (page == PAGE_BANKS) {
    draw_7seg(50, 16, link_bank_edit / 10);
    draw_7seg(66, 16, link_bank_edit % 10);
  } else {
    if (active_mode) {
      draw_7seg_mask(50, 16, kSegO);
      draw_7seg_mask(66, 16, kSegN);
    } else {
      draw_7seg_mask(48, 16, kSegO, 9);
      draw_7seg_mask(60, 16, kSegF, 9);
      draw_7seg_mask(72, 16, kSegF, 9);
    }
  }

  const char *l_top = "STORE", *l_hint = "hold";
  const char *r_top = "RECALL", *r_hint = "hold";
  if (edit_mode) {
    l_top = "DONE"; l_hint = "click";
    r_top = "CHAR"; r_hint = "turn";
  } else if (cursor == 1 && sel_stored == 1) {
    l_top = "EDIT"; l_hint = "click";
  } else if (page == PAGE_BANKS) {
    l_top = "EXPORT"; r_top = "IMPORT";
  }
  // every page: three lines centered in the columns left and right of the box;
  // the middle line gives way to the progress bar while holding
  auto column = [](int cx, const char *const *lines, bool holding) {
    static const int ys[] = { 16, 26, 35 };
    for (int i = 0; i < 3; ++i) {
      if (!lines[i] || !lines[i][0] || (holding && i == 1)) continue;
      graphics.setPrintPos(cx - 3 * (int)strlen(lines[i]), ys[i]);
      graphics.print(lines[i]);
    }
  };
  auto is_action = [](const char *h) {
    return !strcmp(h, "hold") || !strcmp(h, "click") || !strcmp(h, "turn");
  };
  const bool l_holding = hold_start_ms && !edit_mode;
  const bool r_holding = recall_hold_ms && !edit_mode;
  if (page == PAGE_MANAGER && !edit_mode) {
    static const char *const kLeft[] = { "MODE", "hold", "L enc" };
    static const char *const kRight[] = { "EXIT", "hold", "R enc" };
    column(22, kLeft, l_holding);
    column(106, kRight, r_holding);
    const char *l_line = active_mode ? "L to deactivate" : "L to activate";
    graphics.setPrintPos(64 - 3 * (int)strlen(l_line), 44);
    graphics.print(l_line);
    graphics.setPrintPos(64 - 3 * 13, 54);
    graphics.print("R to exit XPM");
  } else {
    const char *const left[] = { l_top, l_hint, is_action(l_hint) ? "L enc" : nullptr };
    const char *const right[] = { r_top, r_hint, is_action(r_hint) ? "R enc" : nullptr };
    column(22, left, l_holding);
    column(106, right, r_holding);
  }

  if (edit_mode) {
    for (int i = 0; i < (int)PresetEngine::kNameLen; ++i) {
      graphics.setPrintPos(16 + 6 * i, 44);
      graphics.print(edit_buf[i] == ' ' ? '_' : edit_buf[i]);
    }
  } else if (page == PAGE_BANKS) {
    draw_jack(15, 47, PresetEngine::QuadLinkEnabled());
    int sx;
    const char *lbl = bank_row_label(sx);
    graphics.setPrintPos(sx, 44);
    graphics.print(lbl);
  } else if (page == PAGE_MANAGER) {
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

  if (page == PAGE_PRESETS) {
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
  } else if (page == PAGE_BANKS) {
    const char *hint = PresetEngine::QuadLinkEnabled() ? "click to unlink" : "click to link";
    graphics.setPrintPos(64 - 3 * (int)strlen(hint), 54);
    graphics.print(hint);
  }

  if (hold_start_ms && !edit_mode) {
    const uint32_t held = millis() - hold_start_ms;
    const uint32_t full = (page == PAGE_BANKS) ? kBankHoldMs
                        : (page == PAGE_MANAGER) ? kManagerHoldMs : kStoreHoldMs;
    graphics.drawFrame(5, 26, 34, 5);
    const uint32_t w = (held >= full) ? 32 : (held * 32) / full;
    if (w) graphics.drawRect(6, 27, w, 3);
  }
  if (recall_hold_ms && !edit_mode) {
    const uint32_t held = millis() - recall_hold_ms;
    graphics.drawFrame(89, 26, 34, 5);
    const uint32_t full = (page == PAGE_BANKS) ? kBankHoldMs
                        : (page == PAGE_MANAGER) ? kManagerHoldMs : kRecallHoldMs;
    const uint32_t w = (held >= full) ? 32 : (held * 32) / full;
    if (w) graphics.drawRect(90, 27, w, 3);
  }

  if (edit_mode) {
    graphics.invertRect(15 + 6 * edit_pos, 43, 8, 10);
  } else switch (cursor) {
    case 0:
    case 4:
      graphics.invertRect(48, 14, 32, 26);
      break;
    case 5:
      if (active_mode) graphics.invertRect(48, 14, 32, 26);
      break;
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

  if (banner_until_ms && millis() < banner_until_ms) {
    const bool two = banner2[0];
    const int len1 = (int)strlen(banner), len2 = (int)strlen(banner2);
    const int w = 6 * max(len1, len2) + (two ? 10 : 16);
    const int x = 64 - w / 2;
    const int y = two ? 16 : 20, h = two ? 30 : 20;
    graphics.clearRect(x, y, w, h);
    graphics.drawFrame(x, y, w, h);
    graphics.drawFrame(x + 1, y + 1, w - 2, h - 2);
    graphics.setPrintPos(64 - 3 * len1, two ? 21 : 26);
    graphics.print(banner);
    if (two) {
      graphics.setPrintPos(64 - 3 * len2, 32);
      graphics.print(banner2);
    }
  } else {
    banner_until_ms = 0;
  }
}

FLASHMEM static void cancel_store_hold() {
  hold_start_ms = 0;
  ui.IgnoreUntilRelease(CONTROL_BUTTON_L);
  set_banner("STORE OFF", 0);
}

FLASHMEM static void cancel_recall_hold() {
  recall_hold_ms = 0;
  recall_fired = false;
  ui.IgnoreUntilRelease(CONTROL_BUTTON_R);
}

FLASHMEM __attribute__((noinline)) static void toggle_active_mode() {
  if (active_mode) {
    active_mode = false;
    set_banner("MANAGER OFF", 0);
  } else {
    active_mode = true;
    set_banner("MANAGER ON", 0);
  }
  mode_dirty = true;
  persist_assignments();
}

// right hold on MANAGER: passive, hide the page, save, back to presets
FLASHMEM __attribute__((noinline)) static void exit_manager() {
  active_mode = false;
  manager_unlocked = false;
  mode_dirty = true;
  persist_assignments();
  cursor = 0;
  sel_stored = -1;
  set_banner("MANAGER OFF", 0);
}

FLASHMEM __attribute__((noinline)) static void toggle_manager_page() {
  // another manager on the bus: the page stays hidden
  if (!manager_unlocked && PresetBus::WpmPresent()) {
    set_banner("MANAGER FOUND", 0);
    return;
  }
  manager_unlocked = !manager_unlocked;
  if (manager_unlocked) cursor = kLastCursor;  // straight to the MANAGER page
  if (!manager_unlocked) {
    if (active_mode) { active_mode = false; mode_dirty = true; }
    if (cursor > last_cursor()) cursor = last_cursor();
  }
  set_banner(manager_unlocked ? "MANAGER UNLOCKED" : "MANAGER HIDDEN", 0);
}

FLASHMEM __attribute__((noinline)) static void poll_unlock_combo() {
  const bool held = active &&
      ui.read_immediate(CONTROL_BUTTON_A) && ui.read_immediate(CONTROL_BUTTON_B) &&
      ui.read_immediate(CONTROL_BUTTON_X) && ui.read_immediate(CONTROL_BUTTON_Y);
  if (!held) {
    unlock_hold_ms = 0;
    unlock_fired = false;
    return;
  }
  touch_activity();
  if (!unlock_hold_ms) unlock_hold_ms = millis();
  else if (!unlock_fired && millis() - unlock_hold_ms >= kUnlockHoldMs) {
    unlock_fired = true;
    toggle_manager_page();
  }
}

FLASHMEM __attribute__((noinline)) void Task() {
  if (!Buchla200eHardware()) return;
  // bus address: 22 (the manager's) when active, default when passive
  static int8_t addr_mode = -1;
  if (addr_mode != (int8_t)active_mode) {
    addr_mode = active_mode;
    PresetBus::SetModuleAddressRuntime(active_mode ? 0x22 : 0x3C);
  }
  poll_unlock_combo();

  if ((active_mode || manager_unlocked) && PresetBus::WpmPresent()) {
    if (active_mode) mode_dirty = true;
    active_mode = false;
    manager_unlocked = false;
    if (cursor > last_cursor()) cursor = 0;
    if (active) set_banner2("MANAGER FOUND", "NOW PASSIVE");
  }
  static uint32_t seen_opcount = 0;
  if (PresetEngine::OpCount() != seen_opcount && pending_slot < 0 && hold_start_ms && !store_fired)
    cancel_store_hold();
  const bool mid_gesture = hold_start_ms || (recall_hold_ms && !recall_fired);
  if (pending_slot < 0 && !mid_gesture && PresetEngine::OpCount() != seen_opcount) {
    seen_opcount = PresetEngine::OpCount();
    const int last = PresetEngine::BusSlot();
    if (last >= 0 && (uint8_t)last != sel) {
      sel = (uint8_t)last;
      sel_stored = -1;
      if (active) {
        const char *err = PresetEngine::LastRecallError();
        if (err) {
          char text[16];
          snprintf(text, sizeof(text), "%s %d", err, (uint8_t)last + 1);
          set_banner(text, 0);
        }
      }
    }
  }

  const uint32_t edges = DigitalInputs::take_latched_edges();
  for (uint8_t t = 0; t < 4; ++t) {
    if (!(edges & DIGITAL_INPUT_MASK(t))) continue;
    if (next_trig != t + 1 && last_trig != t + 1) continue;
    if (hold_start_ms && !store_fired) cancel_store_hold();
    if (recall_hold_ms && !recall_fired) cancel_recall_hold();
    sel = (next_trig == t + 1) ? (sel + 1) % 30 : (sel + 29) % 30;
    sel_stored = -1;
    recall_selected();
    if (active) touch_activity();
  }

  if (active && millis() - last_activity_ms >= kIdleTimeoutMs) {
    Exit();
    return;
  }

  if (active) {
    const bool store_context = !edit_mode && !(cursor == 1 && sel_stored == 1);
    if (store_context && ui.read_deliberate(CONTROL_BUTTON_L)) {
      if (!hold_start_ms) {
        hold_start_ms = PresetEngine::StampMs();
      } else if (cursor == 4 && !store_fired &&
                 millis() - hold_start_ms >= kBankHoldMs) {
        store_fired = true;
        const int n = PresetEngine::CopySlotsToBank(link_bank_edit);
        bank_copy_banner(false, n, link_bank_edit);
      } else if (cursor == 5 && !store_fired &&
                 millis() - hold_start_ms >= kManagerHoldMs) {
        store_fired = true;
        toggle_active_mode();
      }
    } else {
      hold_start_ms = 0;
      store_fired = false;
    }

    if (!edit_mode && ui.read_deliberate(CONTROL_BUTTON_R)) {
      const uint32_t threshold = (cursor == 5) ? kManagerHoldMs
                               : (cursor == 4) ? kBankHoldMs : kRecallHoldMs;
      if (!recall_hold_ms) {
        recall_hold_ms = PresetEngine::StampMs();
        recall_fired = false;
      } else if (!recall_fired &&
                 (millis() - recall_hold_ms) >= threshold) {
        recall_fired = true;
        if (cursor == 5) {
          exit_manager();
        } else if (cursor == 4) {
          const int n = PresetEngine::CopyBankToSlots(link_bank_edit);
          bank_copy_banner(true, n, link_bank_edit);
          if (n >= 0) {
            cursor = 0;
            sel = 0;
            sel_stored = -1;
          }
        } else {
          recall_selected();
          pending_slot = sel;
          pending_was_save = false;
          pending_start_ms = millis();
          pending_opcount = PresetEngine::OpCount();
        }
      }
    } else {
      recall_hold_ms = 0;
      recall_fired = false;
    }

    if (pending_slot >= 0) {
      const bool other_op = PresetEngine::LastWasSave() != pending_was_save ||
                            (pending_was_save && PresetEngine::BusSlot() != pending_slot);
      if (PresetEngine::OpCount() != pending_opcount && other_op) {
        pending_opcount = PresetEngine::OpCount();
      } else if (PresetEngine::OpCount() != pending_opcount) {
        const uint8_t shown = (uint8_t)PresetEngine::BusSlot() + 1;
        char text[16];
        if (pending_was_save) {
          snprintf(text, sizeof(text),
                   PresetEngine::LastSaveOk() ? "STORED %d" : "STORE ERR %d", shown);
        } else {
          const char *err = PresetEngine::LastRecallError();
          if (err) snprintf(text, sizeof(text), "%s %d", err, shown);
          else snprintf(text, sizeof(text), "RECALLED %d", shown);
        }
        set_banner(text, pending_start_ms);
        pending_slot = -1;
        sel_stored = -1;
      } else if (PresetBus::BroadcastQueued()) {
        pending_start_ms = millis();
      } else if (millis() - pending_start_ms > 4000) {
        set_banner(pending_was_save ? "STORE FAILED" : "RECALL FAILED", 0);
        pending_slot = -1;
      }
    }

    if (banner_until_ms && banner_follow_since) {
      const int n = PresetBus::LoadAckCountSince(banner_follow_since);
      if (n > 0) snprintf(banner, sizeof(banner), "%s +%d", banner_base, n);
    }

    static uint32_t last_kick = 0;
    if (millis() - last_kick >= 66) {
      last_kick = millis();
      ::MENU_REDRAW = 1;
    }
    ui.Poke();
  }
}

}
}

#endif
