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
// The slot a rename was opened on. `sel` is not it: a NEXT/LAST pulse or a
// bus recall can move sel while the name is being typed (both are
// performance events and cannot wait for a menu), and the name must land on
// the slot the performer was looking at when they clicked EDIT.
static uint8_t edit_slot = 0;
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
// RECALL used to fire on a bare press, which made it far too easy to trigger
// by accident while chording both encoders to open this overlay -- and a
// stray recall is bus-wide and immediate. It now wants a hold too, half the
// length STORE asks for: deliberate, but still clearly the lighter gesture.
// Derived from kLongPressTicks so the two can never drift apart.
static const uint32_t kStoreHoldMs = OC::Ui::kLongPressTicks;
static const uint32_t kRecallHoldMs = OC::Ui::kLongPressTicks / 2;
static uint32_t recall_hold_ms = 0;       // 0 = not holding
static bool recall_fired = false;         // one shot per hold
static bool store_fired = false;          // this hold's STORE has gone out
// Set on Enter(): the opening chord holds both encoders, so a hold that was
// already in progress when the overlay appeared must not count. Cleared once
// the button in question has actually been let go.
static int8_t pending_slot = -1;          // op we're waiting on
static bool pending_was_save = false;
static uint32_t pending_start_ms = 0;
static uint32_t pending_opcount = 0;
static char banner[20] = "";
static uint32_t banner_until_ms = 0;
// The banner's own words, before the follower count is appended. Modules
// that act on a broadcast answer with a poll reply over the ~500 ms AFTER
// the engine has finished our copy of the op, so the count cannot be known
// when the banner is first drawn. The banner is never made to wait for the
// bus: it appears at once and grows a "+N" as the case reports in.
static char banner_base[16] = "";
static uint32_t banner_follow_since = 0;

// `follow_since` is the millis() stamp our broadcast went out, or 0 for a
// banner that is about this module alone.
FLASHMEM static void set_banner(const char *text, uint32_t follow_since) {
  strncpy(banner_base, text, sizeof(banner_base) - 1);
  banner_base[sizeof(banner_base) - 1] = 0;
  banner_follow_since = follow_since;
  snprintf(banner, sizeof(banner), "%s", banner_base);
  banner_until_ms = millis() + 1600;
}

bool Active() { return active; }

FLASHMEM void Init() {
  uint64_t v = 0;
  if (PhzConfig::getValue(kNextTrigKey, v) && v <= 4) next_trig = (uint8_t)v;
  v = 0;
  if (PhzConfig::getValue(kLastTrigKey, v) && v <= 4) last_trig = (uint8_t)v;
  const int last = PresetEngine::BusSlot();
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
  const int last = PresetEngine::BusSlot();
  if (last >= 0) sel = (uint8_t)last;
  cursor = 0;
  edit_mode = false;
  sel_stored = -1;
  active = true;
  // This overlay is opened by holding BOTH encoders, so on the way in both
  // buttons are already down -- and the STORE/RECALL hold timers would start
  // counting immediately and fire the moment their thresholds pass. That is
  // the accidental recall Oren reported, and adding a hold to RECALL only
  // delayed it rather than preventing it.
  //
  // The local store_needs_release/recall_needs_release flags that used to live
  // here were NOT the fix they were documented as being. They gated the
  // progress BARS, which are drawn from these timers -- but STORE does not
  // fire from a timer. It fires from the LONG_PRESS event in HandleEvent, so
  // it went straight past them, and the entry chord could commit a store on
  // its own: press encR a millisecond before encL, hold, and the overlay
  // opened and wrote slot 1. RECALL, which IS timer-driven, was genuinely
  // protected -- which is why an attack that released the encoders in the
  // other order found nothing wrong.
  hold_start_ms = 0;
  recall_hold_ms = 0;
  recall_fired = false;
  // The chord that opened this overlay is still physically held. Arming the
  // UI layer's release-first rule is what stops it leaking in -- see
  // Ui::IgnoreUntilRelease. Both encoder buttons are named because either may
  // still be down, and over-naming costs only a swallowed release.
  ui.IgnoreUntilRelease(CONTROL_BUTTON_L | CONTROL_BUTTON_R);
}

FLASHMEM void Exit() {
  active = false;
  // The hold timers are cleared by the `if (active)` block in Task(), which
  // stops running here. Leaving the overlay with a button still down would
  // otherwise freeze one at non-zero, and a stale hold blocks the follow of
  // bus recalls (above) for as long as the overlay stays closed.
  hold_start_ms = 0;
  recall_hold_ms = 0;
  recall_fired = false;
  store_fired = false;
  // Same for the completion watch: it only runs while the overlay is open,
  // and the bus-follow above is gated on it. A STORE fired and the overlay
  // closed before the engine finished left pending_slot set, and with it
  // every WPM/225e recall un-followed until the overlay was next opened.
  pending_slot = -1;
  banner_until_ms = 0;
  edit_mode = false;
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
  PresetEngine::SetSlotName(edit_slot, out);
}

// noinline so FLASHMEM sticks (free-function LTO rule -- same treatment as
// Main.cpp's BootMenu/SelfTest/loop and PresetBus.cpp's bus_stuck_recover).
// Without it LTO is free to inline all ~800 bytes of this overlay handler
// into OC::Ui::DispatchEvents, which lives in ITCM: the FLASHMEM annotation
// is silently defeated and, on the tightest build (T41_audio_dbg, which sits
// ~1KB under a 32KB ITCM block boundary), that inline costs a whole extra
// 32KB bank and overflows RAM1. Whether LTO takes that inline was, until
// now, decided by unrelated code churn elsewhere in the image.
FLASHMEM __attribute__((noinline)) bool HandleEvent(const UI::Event &event) {
  if (!active) return false;
  if (sel_stored < 0) sel_stored = PresetEngine::SlotUsed(sel) ? 1 : 0;

  // ---- chords: the two that mean something everywhere else keep meaning
  // it here, and neither may leave a hold running behind it ----
  if (event.type == UI::EVENT_BUTTON_DOWN &&
      (event.control == CONTROL_BUTTON_L || event.control == CONTROL_BUTTON_R)) {
    // A/Z + encR is the app menu and A/Z + encL the IO settings, module-wide.
    // Every R event used to be swallowed below, so the menu chord did nothing
    // visible -- and the R it left held reached the 250 ms RECALL threshold
    // in Task() and recalled bus-wide, from a gesture that meant "menu". The
    // overlay closes and the event goes on to the global hotkeys untouched,
    // which claim the chord themselves (OC_ui.cpp).
    if (event.mask & (CONTROL_BUTTON_A | CONTROL_BUTTON_Z)) {
      Exit();
      return false;
    }
    // Both encoder buttons is the chord that opened this screen; held again
    // inside it, it closes it. It used to mean nothing here, so both hold
    // bars ran, RECALL fired at 250 ms, and that op's completion cancelled
    // the STORE hold: two bars, a bus-wide recall, then STORE OFF. Claimed on
    // the way out like on the way in, so the release cannot leak.
    if ((event.mask & (CONTROL_BUTTON_L | CONTROL_BUTTON_R))
            == (CONTROL_BUTTON_L | CONTROL_BUTTON_R)) {
      Exit();
      ui.IgnoreUntilRelease(CONTROL_BUTTON_L | CONTROL_BUTTON_R);
      return true;
    }
  }

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
      edit_slot = sel;
      edit_mode = true;
    }
    return true;
  }
  // RECALL is NOT fired here any more -- a bare press was too easy to trigger
  // by accident. The hold is timed in Task() against kRecallHoldMs; this only
  // swallows the event so nothing else acts on it.
  if (event.control == CONTROL_BUTTON_R) return true;
  if (event.control == CONTROL_BUTTON_L && event.type == UI::EVENT_BUTTON_LONG_PRESS) {
    if (cursor == 1 && sel_stored == 1) return true;  // STORE legend hidden here
    PresetBus::BroadcastSave(sel);
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
  // 01-30, zero-padded. A rename shows the slot it will land on, which is
  // not sel once a pulse or a bus recall has moved on underneath the edit.
  const uint8_t shown = (edit_mode ? edit_slot : sel) + 1;
  draw_7seg(50, 16, shown / 10);
  draw_7seg(66, 16, shown % 10);

  // edge legends: contextual (turns are unlabeled, presses are labeled)
  //
  // The hint row has SIX columns a side: x=7..43 up to the LCD window and
  // x=91..127 to the screen edge. The hints briefly stated their durations
  // ("hold .5s" / "hold .25s") to make the STORE/RECALL asymmetry visible --
  // and at 8 and 9 columns they did not fit: the "5s" was drawn INSIDE the
  // LCD window over the tens digit, and the "25s" ran off the right edge,
  // leaving "hold ." on both sides (Oren's photo: "strange glitch in the
  // overlay"). The edge check could not see it -- the '2' clipped at a blank
  // glyph column -- so the simulator now counts clipped glyphs directly. The
  // durations are shown by the bars themselves: each fills at its own rate.
  const char *l_top = "STORE", *l_hint = "hold";
  const char *r_top = "RECALL", *r_hint = "hold";
  if (edit_mode) {
    l_top = "DONE"; l_hint = "click";
    r_top = "CHAR"; r_hint = "turn";
  } else if (cursor == 1 && sel_stored == 1) {
    l_top = "EDIT"; l_hint = "click";
  }
  graphics.setPrintPos(4, 16);
  graphics.print(l_top);
  graphics.setPrintPos(88, 16);
  graphics.print(r_top);
  // The hint row doubles as the progress row: while a hold is running the
  // bar is drawn HERE, in place of the word, so it never has to be squeezed
  // in beside the label. The RECALL bar used to sit at y=29 and was drawn
  // straight through the bottom five rows of its own "hold" text.
  if (!(hold_start_ms && !edit_mode)) {
    graphics.setPrintPos(7, 26);
    graphics.print(l_hint);
  }
  if (!(recall_hold_ms && !edit_mode)) {
    graphics.setPrintPos(91, 26);
    graphics.print(r_hint);
  }

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

  // Hold-progress bars. Both live on the hint row (y=26) in place of the
  // word they replace, both are 34x5, and both start filling IMMEDIATELY.
  //
  // The 120 ms flicker guard the STORE bar used to carry is gone: it meant
  // the first 24% of a 500 ms hold gave no feedback at all, so the gesture
  // that most needs to feel like it registered was the one that stayed dark
  // longest. RECALL never had the guard, so the two also disagreed about
  // when feedback begins. They now behave identically and differ only in how
  // fast they fill -- which is the one thing that IS different about them.
  if (hold_start_ms && !edit_mode) {
    const uint32_t held = millis() - hold_start_ms;
    graphics.drawFrame(4, 26, 34, 5);
    const uint32_t w = (held >= kStoreHoldMs) ? 32 : (held * 32) / kStoreHoldMs;
    if (w) graphics.drawRect(5, 27, w, 3);
  }
  if (recall_hold_ms && !edit_mode) {
    const uint32_t held = millis() - recall_hold_ms;
    graphics.drawFrame(90, 26, 34, 5);
    const uint32_t w = (held >= kRecallHoldMs) ? 32 : (held * 32) / kRecallHoldMs;
    if (w) graphics.drawRect(91, 27, w, 3);
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

// A STORE hold that has not fired yet is abandoned: the bar resets and the
// release is swallowed, so the user sees it and simply holds again. A STORE
// already sent by this hold is left alone (callers check store_fired).
FLASHMEM static void cancel_store_hold() {
  hold_start_ms = 0;
  ui.IgnoreUntilRelease(CONTROL_BUTTON_L);
  set_banner("STORE OFF", 0);
}

// A RECALL hold that has not fired yet is abandoned, quietly: the event
// that abandoned it has already recalled the slot the panel now shows, and
// letting the hold fire would recall it a second time -- another ~280 ms
// of interrupts-off flash reads, a second dropout, for nothing. The release
// is swallowed so the bar cannot simply restart under the same finger.
FLASHMEM static void cancel_recall_hold() {
  recall_hold_ms = 0;
  recall_fired = false;
  ui.IgnoreUntilRelease(CONTROL_BUTTON_R);
}

void Task() {
  // follow externally-driven preset changes (WPM/225e recall, boot recall)
  // so trigger cycling always steps from the CURRENT preset, 225e-style
  //
  // A preset op that lands from the bus during an un-fired STORE hold
  // cancels the hold, for the same reason a pulse does (below): the engine
  // has already applied the recall -- the case moved, so it must -- and the
  // state under the user's hand is no longer the one they held STORE over.
  // Letting the hold fire wrote preset 5's freshly-recalled state into the
  // slot under the cursor (0), and the mid-hold gate that used to keep `sel`
  // from following only made the write land on the wrong slot instead of
  // the right one. Nothing is written; the panel then follows.
  //
  // A RECALL hold keeps its gate until it fires: it is a statement about the
  // slot that was showing when the button went down, and following the bus
  // mid-hold would recall the bus's slot instead. The op count is left
  // unconsumed, so the catch-up happens the moment the hold fires or the
  // button lifts -- a fired hold is spent (one shot, re-armed by release),
  // so there is nothing left to protect while the finger stays down. A
  // rename needs no gate: it is bound to edit_slot, not sel, and the digit
  // shows edit_slot while it runs.
  static uint32_t seen_opcount = 0;
  if (PresetEngine::OpCount() != seen_opcount && pending_slot < 0 && hold_start_ms && !store_fired)
    cancel_store_hold();
  const bool mid_gesture = hold_start_ms || (recall_hold_ms && !recall_fired);
  if (pending_slot < 0 && !mid_gesture && PresetEngine::OpCount() != seen_opcount) {
    seen_opcount = PresetEngine::OpCount();
    // BusSlot, not LastSlot: a recall the engine refused (EMPTY SLOT) still
    // moved every other module in the case. Following LastSlot snapped sel
    // back to the slot still loaded here, so a NEXT pulse into an empty slot
    // bounced -- 0 -> 1, refused, back to 0, and the next pulse tried 1
    // again. The panel could never step past an empty slot, while the case
    // had already gone on without it.
    const int last = PresetEngine::BusSlot();
    if (last >= 0 && (uint8_t)last != sel) {
      sel = (uint8_t)last;
      sel_stored = -1;
    }
  }

  // 225e last/next pulse inputs: rising edge cycles + recalls bus-wide.
  // Edges come from the driver's latch, not from comparing pin levels
  // between loop passes: a pulse narrower than one pass -- and every pulse
  // that lands during a 250 ms flash write, when loop() is not running at
  // all -- was invisible to a level sample and simply dropped, while the
  // rest of the case stepped. The latch is drained EVERY pass (assigned or
  // not) so enabling an assignment can never fire a spurious recall on an
  // edge that happened before it.
  const uint32_t edges = DigitalInputs::take_latched_edges();
  for (uint8_t t = 0; t < 4; ++t) {
    if (!(edges & DIGITAL_INPUT_MASK(t))) continue;
    if (next_trig != t + 1 && last_trig != t + 1) continue;
    // The pulse wins over a STORE hold in progress, and the hold is
    // cancelled rather than left to fire: a pulse is a performance event
    // that cannot wait for a button, but it moves `sel` under the user's
    // hand, and a STORE that fires after it writes the slot the case just
    // stepped TO -- not the one that was showing when the hold began. That
    // was reproducible: a NEXT pulse 480 ms into the hold stored slot 2
    // when the panel had said 1. A cancelled write is the safe outcome;
    // the bar resets and the release is swallowed, so the user sees it and
    // simply holds again. A STORE already sent by this hold is left alone.
    if (hold_start_ms && !store_fired) cancel_store_hold();   // holds only exist while active
    // The same pulse also steps the slot a RECALL hold would recall: a hold
    // 100 ms in went on to recall the stepped slot again, right after the
    // pulse had. Not a data hazard, but a second dropout for nothing.
    if (recall_hold_ms && !recall_fired) cancel_recall_hold();
    sel = (next_trig == t + 1) ? (sel + 1) % 30 : (sel + 29) % 30;
    sel_stored = -1;
    recall_selected();
  }

  if (active) {
    // hold-progress source: sample the left encoder button directly
    const bool store_context = !edit_mode && !(cursor == 1 && sel_stored == 1);
    // read_deliberate, not read_immediate: the UI layer's release-first rule
    // is authoritative now, and it covers what the local flags could not.
    // store_needs_release only ever gated the PROGRESS BAR -- STORE itself
    // fires from the LONG_PRESS event in HandleEvent, so the entry chord could
    // and did commit a store on its own (r-down, l-down, hold: "STORED 1").
    if (store_context && ui.read_deliberate(CONTROL_BUTTON_L)) {
      if (!hold_start_ms) hold_start_ms = PresetEngine::StampMs();
    } else {
      hold_start_ms = 0;
      store_fired = false;
    }

    // RECALL: same idea, half the hold, and it fires the moment the
    // threshold is reached rather than on release -- so the gesture ends
    // when the bar fills, matching how STORE feels. Never in edit_mode:
    // a bus-wide recall must not fall out of a rename gesture.
    if (!edit_mode && ui.read_deliberate(CONTROL_BUTTON_R)) {
      if (!recall_hold_ms) {
        recall_hold_ms = PresetEngine::StampMs();
        recall_fired = false;
      } else if (!recall_fired &&
                 (millis() - recall_hold_ms) >= kRecallHoldMs) {
        recall_fired = true;              // one shot; released to re-arm
        recall_selected();
        pending_slot = sel;
        pending_was_save = false;
        pending_start_ms = millis();
        pending_opcount = PresetEngine::OpCount();
      }
    } else {
      recall_hold_ms = 0;
      recall_fired = false;
    }

    // completion watch: the engine bumps OpCount when the op finishes
    if (pending_slot >= 0) {
      // Which finished op answers the gesture? A STORE only its own: the
      // engine queues, so a bus recall already in line lands before our
      // SAVE, and announcing "STORED" on its behalf would be a lie. A RECALL
      // is answered by ANY recall: the engine merges a recall queued behind
      // ours over it (the case applies the last one anyway), so a manager's
      // recall 5 arriving while a SAVE held ours up is what the case did --
      // waiting for "recall 3" then meant 4 s of a panel stuck on 3, the
      // follow blocked, and "RECALL FAILED" for a recall that did happen.
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
          // A refused recall reports its reason at once ("EMPTY SLOT 7"),
          // which is a different sentence from RECALL FAILED: the first
          // means the slot has nothing stored, the second means the op
          // never finished. A night was lost to conflating them.
          const char *err = PresetEngine::LastRecallError();
          if (err) snprintf(text, sizeof(text), "%s %d", err, shown);
          else snprintf(text, sizeof(text), "RECALLED %d", shown);
        }
        // Count followers from the moment the frame went out. A refused
        // recall still broadcast, so the case still moved: the count is
        // about the bus, not about this module's own result.
        set_banner(text, pending_start_ms);
        pending_slot = -1;
        sel_stored = -1;
      } else if (PresetBus::BroadcastQueued()) {
        // still waiting for a quiet wire: nothing has reached the engine,
        // so the 4 s runs from the moment the frame goes out, not from the
        // press. A busy bus used to earn "STORE FAILED" for a save that then
        // went out and completed a moment later. A broadcast the transport
        // gives up on (50 tries) leaves the queue without a dispatch, and
        // the timeout below is then the true verdict.
        pending_start_ms = millis();
      } else if (millis() - pending_start_ms > 4000) {
        set_banner(pending_was_save ? "STORE FAILED" : "RECALL FAILED", 0);
        pending_slot = -1;
      }
    }

    // Followers report in after the engine has finished, so the count is
    // refreshed while the banner is up rather than waited for. A case with
    // no polling module answers nothing and the banner simply stays as it
    // was: the absence of a count is not a claim that nothing followed.
    if (banner_until_ms && banner_follow_since) {
      const int n = PresetBus::LoadAckCountSince(banner_follow_since);
      if (n > 0) snprintf(banner, sizeof(banner), "%s +%d", banner_base, n);
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
