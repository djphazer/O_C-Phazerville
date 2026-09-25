#ifndef OC_UI_H_
#define OC_UI_H_

#include <stdint.h>
#include "OC_config.h"
#include "OC_options.h"
#include "OC_debug.h"
#include "src/UI/ui_button.h"
#include "src/UI/ui_encoder.h"
#include "src/UI/ui_event_queue.h"

struct RuntimeSlot;

namespace OC {

enum EncoderConfig : uint32_t;
class AppBase;

// UI::Event::control is uint16_t, but we only have 6 controls anyway.
// So we can helpfully make things into bitmasks, which seems useful.
enum UiControl : uint16_t {
  CONTROL_BUTTON_UP   = 1 << 0,
  CONTROL_BUTTON_DOWN = 1 << 1,
  /* Reverse the left and right buttons if Hemisphere Suite is installed on the left-hand
   * side of a Northern Light 2OC 4U module.
   */
#ifdef NORTHERNLIGHT_2OC_LEFTSIDE
  CONTROL_BUTTON_L    = 1 << 3,
  CONTROL_BUTTON_R    = 1 << 2,
#else
  CONTROL_BUTTON_L    = 1 << 2,
  CONTROL_BUTTON_R    = 1 << 3,
#endif

  // not all of these are present on all hardware...
  // but it probably doesn't hurt to include in the enum
  CONTROL_BUTTON_M     = 1 << 4,
  CONTROL_BUTTON_UP2   = 1 << 5,
  CONTROL_BUTTON_DOWN2 = 1 << 6,

  CONTROL_ENCODER_L   = 1 << 8,
  CONTROL_ENCODER_R   = 1 << 9,

#if defined(VOR)
  CONTROL_BUTTON_LAST = 5,
#elif defined(ARDUINO_TEENSY41)
  CONTROL_BUTTON_LAST = 7,
#else
  CONTROL_BUTTON_LAST = 4,
#endif

  // aliases for T41
  CONTROL_BUTTON_A = CONTROL_BUTTON_UP,
  CONTROL_BUTTON_B = CONTROL_BUTTON_DOWN,
  CONTROL_BUTTON_X = CONTROL_BUTTON_UP2,
  CONTROL_BUTTON_Y = CONTROL_BUTTON_DOWN2,
  CONTROL_BUTTON_Z = CONTROL_BUTTON_M,
};

static inline uint16_t control_mask(unsigned i) {
  return 1 << i;
}

enum UiMode {
  UI_MODE_SCREENSAVER,
  UI_MODE_MENU,
  UI_MODE_APP_SETTINGS,
  UI_MODE_CALIBRATE
};

class Ui {
public:
  static const size_t kEventQueueDepth = 16;
  static const uint32_t kLongPressTicks = 500;

  Ui() { }

  void Init();

  UiMode Splashscreen(bool &reset_settings, uint8_t phase = 0);
  bool ConfirmReset();
  void DebugStats();
  bool AppSettings(bool drawmenu);
  UiMode DispatchEvents(const RuntimeSlot &appslot);

  // Bench: queue a synthetic panel event from the console, exactly as
  // Poll() would queue the physical one, so it lands wherever a real press
  // would -- app, overlay or menu. Poll() runs from the 1 kHz UI timer ISR
  // and is the queue's only other producer, so the push is bracketed
  // against it; the consumer (DispatchEvents) is loop context like the
  // console and cannot interleave. Callers send a tap as DOWN then PRESS,
  // the way the panel reports one.
  // `held` names buttons to report as still down in the event's mask, which
  // is how every global chord is recognised (see z_hold/a_hold in
  // OC_ui.cpp). Injecting a chord needs it; injecting a plain press does
  // not, so it defaults to none and existing callers are unchanged.
  void Inject(UI::EventType type, uint16_t control, int16_t value,
              uint16_t held = 0);

  void Poll();
  void Poke();
  void preempt_screensaver(bool v);

  // The raw, undebounced pin state as of the last Poll(). Note "raw": a
  // bouncing switch reads released here for a poll or two mid-hold.
  inline bool read_immediate(UiControl control) const {
    return button_state_ & control;
  }

  // read_immediate() with the release-first rule (below) applied: false while
  // `control` is still owed a release from the chord that opened this screen.
  //
  // A press-and-hold gesture that times ITSELF by sampling the pin -- rather
  // than by waiting for an event -- must use this one. The event path cannot
  // help it: PresetBusUI's RECALL bar is timed in Task() against
  // read_immediate(), so no amount of event filtering stopped the entry
  // chord's still-held right encoder from filling that bar and firing a
  // bus-wide recall. This is the read that stops it.
  inline bool read_deliberate(UiControl control) const {
    return (button_state_ & control) && !(chord_hold_ & control);
  }

  // True while `control` has not yet been released since the guard was armed.
  // For a screen that wants to SAY so ("release to continue"); the guard needs
  // no help from the screen to do its job.
  inline bool awaiting_release(UiControl control) const {
    return chord_hold_ & control;
  }

  inline void encoders_enable_acceleration(bool enable) {
    encoder_left_.enable_acceleration(enable);
    encoder_right_.enable_acceleration(enable);
  }

  inline void encoder_enable_acceleration(UiControl encoder, bool enable) {
    switch (encoder) {
    case CONTROL_ENCODER_L:
      encoder_left_.enable_acceleration(enable);
      break;
    case CONTROL_ENCODER_R:
      encoder_right_.enable_acceleration(enable);
      break;
    default: break;
    }
  }

  void configure_encoders(EncoderConfig encoder_config);

  inline uint32_t idle_time() const {
    return event_queue_.idle_time();
  }

  inline uint32_t ticks() const {
    return ticks_;
  }

  // --- the release-first rule for chord-opened screens ---------------------
  //
  // This panel has six controls, no labels and no menu key, so every global
  // navigation gesture is a held-button chord: hold A + press encR for the app
  // switcher, hold A + press encL for I/O settings, both encoder buttons for
  // the preset-bus overlay, hold Z + press A for the screensaver, A+B for the
  // per-app clock/flip screens. A chord is still physically held at the moment
  // the screen it opened starts accepting input, so the tail of the ENTRY
  // gesture leaks into the screen -- as a release event, a long-press event, or
  // (worst) as a hold that a timer on the new screen is already counting.
  //
  // That is not a hypothetical. The both-encoder chord left the right encoder
  // down; the preset overlay's 250 ms RECALL hold timer started counting on it
  // and fired. A bus-wide recall of every 200e module in the case, from a
  // gesture that was only ever meant to open a screen.
  //
  // IgnoreUntilRelease() states the fix once, for every such screen: the
  // buttons named here are dead until each has been physically released. Not
  // "for N milliseconds" and not "for one event" -- until released, which is
  // the one condition a user satisfies on purpose and a fumbled chord cannot
  // satisfy by accident.
  //
  //   ui.IgnoreUntilRelease(CONTROL_BUTTON_L | CONTROL_BUTTON_R);
  //
  // CONTRACT, for each button b named in `buttons`:
  //
  //  * If b is down: every event carrying control == b is dropped -- DOWN,
  //    PRESS, LONG_PRESS and LONG_RELEASE alike -- until b's debounced release,
  //    and that release event is dropped too. The next event after it is
  //    delivered normally.
  //  * If b is already up: the ONE release event that may still be in the queue
  //    for it is dropped. (Releases are reported seven polls after the pin
  //    rises, so a chord button let go a few ms early has a release still in
  //    flight when the screen opens -- and event.mask, being the raw pin state,
  //    no longer mentions it. This is the fumble that a mask test at release
  //    time cannot see.) If no such release arrives, the guard costs nothing:
  //    a DOWN clears it and is delivered.
  //  * read_deliberate(b) is false from the arming until b's debounced release
  //    -- the held half only; it goes true again the moment b is pressed anew,
  //    without waiting for the swallowed release event to be dispatched.
  //    read_immediate(b) is NOT affected: it stays the raw pin, because callers
  //    that mean the pin (Main.cpp's console modifiers) exist.
  //  * A DOWN event is never dropped once b has been released. A press the user
  //    meant therefore cannot be refused: it requires b up first, which ends
  //    the guard.
  //  * ENCODER TURNS are untouched. CONTROL_ENCODER_L/R are different bits from
  //    CONTROL_BUTTON_L/R, so a screen opened with an encoder held is still
  //    navigable by turning that same encoder -- which the preset overlay
  //    relies on. Only the seven button bits are guarded.
  //
  // Arm it BEFORE opening the screen, and name the WHOLE chord including
  // modifiers you are not certain were still down: over-naming a button costs
  // one swallowed release that never comes, under-naming one is the bug.
  inline void IgnoreUntilRelease(uint16_t buttons) {
    buttons &= kGuardableButtons;
    // "Down" here is the raw pin OR the debounced down-state, and it has to be
    // both because the two disagree at opposite edges -- each disagreement
    // being a way this guard can be handed a button and file it wrongly:
    //
    //  * On the PRESS edge the raw pin leads: just_pressed() wants seven low
    //    reads, but the chord that opens a screen is recognised from
    //    event.mask, which IS the raw pin. Both encoders pressed a millisecond
    //    apart open the overlay on the first one's DOWN, at which point the
    //    second is raw-low but not yet debounced-down. Filing it as "already
    //    up" left that encoder unguarded, and a hold sampled through
    //    read_deliberate() fired on the entry chord anyway -- the original bug,
    //    reproduced in the simulator while this guard was being written.
    //  * On the RELEASE edge the debounced state leads: the pin is high for
    //    seven polls before the release is reported, and a switch that bounces
    //    reads high mid-hold without being released at all.
    //
    // Whichever says "down" wins, so a button is only ever filed as up when it
    // is up by both measures -- which is precisely the case where its release
    // event is already queued and the other half of the guard must eat it.
    const uint16_t down = button_down_ | button_state_;
    chord_hold_    |= buttons &  down;
    chord_release_ |= buttons & ~down;
  }

  // Legacy spelling of "ignore whatever is held right now". Kept because ~30
  // call sites across the apps use it; it is now one way to ARM the rule above,
  // not a second mechanism. The release-first rule is authoritative.
  //
  // What it used to do differently -- and got wrong in ways four call sites'
  // own comments already denied: it dropped exactly ONE event per button, so a
  // button held past kLongPressTicks spent the drop on its LONG_PRESS and let
  // the LONG_RELEASE straight through, next to a comment reading "ignore
  // release and long-press"; and it snapshotted the raw pin, so it never
  // covered a chord button released a few ms early.
  //
  // Prefer IgnoreUntilRelease() with the chord spelled out wherever the chord
  // is known -- this form can only guess from what happens to be down.
  inline void SetButtonIgnoreMask() {
    IgnoreUntilRelease(button_down_ | button_state_);
  }

  // Single-button form of the same rule. Every caller uses it on a button it
  // has just established is held (as a modifier), which is exactly the case
  // IgnoreUntilRelease() handles; the one behaviour it no longer has is eating
  // a future DOWN of a button that was already up, which no caller wants.
  inline void IgnoreButton(UiControl control) {
    IgnoreUntilRelease(control);
  }

  // True once the module has idled past kDisplaySleepMs. AppBase::Draw() draws
  // nothing at all while this holds, which is what stops the panel ageing.
  bool display_asleep() const { return display_asleep_; }

  uint32_t screensaver_timeout() const {
    return screensaver_timeout_;
  }

  void set_screensaver_timeout(uint32_t seconds);

  void JumpToMenu() {
    jump_to_menu_ = true;
  }

private:

  // Controls 0..6 are buttons; the encoders live at bits 8 and 9 and are
  // deliberately outside the guard (see IgnoreUntilRelease).
  static const uint16_t kGuardableButtons = 0x7f;

  uint32_t ticks_ = 0;
  uint32_t screensaver_timeout_ = 120;

  // Panel sleep. The screensaver replaces what is drawn; this turns the OLED's
  // drive off entirely, because a screensaver still lights pixels and an OLED
  // ages the pixels it lights. Nothing in this firmware ever stopped drawing:
  // AppBase::Draw() calls DrawScreensaver() for as long as the module idles, so
  // before this a module left powered held a static image indefinitely -- and
  // gfxHeader() draws a 128-pixel full-width rule, which is the worst shape for
  // burn-in there is.
  //
  // Ten minutes, in milliseconds, matched against event_queue_.idle_time().
  // Note this fires BEFORE the default screensaver (25 minutes, because the
  // stored seconds get a * 60 at the comparison), which is deliberate: a dark
  // panel protects the display better than any animation can.
  static constexpr uint32_t kDisplaySleepMs = 10UL * 60UL * 1000UL;
  bool display_asleep_ = false;

  UI::Button buttons_[CONTROL_BUTTON_LAST];
  uint32_t button_press_time_[CONTROL_BUTTON_LAST];
  uint16_t button_state_ = 0;
  // Debounced down-state: set on just_pressed(), cleared on released(), i.e.
  // it tracks what the EVENT stream believes rather than what the pin reads.
  uint16_t button_down_ = 0;
  // The release-first guard, in two halves. chord_hold_ is the buttons still
  // held from the entry chord; chord_release_ is the buttons that have since
  // been let go but whose release event has not been dispatched yet. A button
  // moves from the first to the second on its debounced release and out of the
  // second when that release is consumed.
  uint16_t chord_hold_ = 0;
  uint16_t chord_release_ = 0;
  bool screensaver_ = 0;
  bool preempt_screensaver_ = 0;
  bool jump_to_menu_ = 0;

  /* Reverse the left and right encoders if Hemisphere Suite is installed on the left-hand
   * side of a Northern Light 2OC 4U module.
   */
#ifdef NORTHERNLIGHT_2OC_LEFTSIDE
  UI::Encoder<encR1, encR2> encoder_left_;
  UI::Encoder<encL1, encL2> encoder_right_;
#else
  UI::Encoder<encR1, encR2> encoder_right_;
  UI::Encoder<encL1, encL2> encoder_left_;
#endif

  UI::EventQueue<kEventQueueDepth> event_queue_;

  inline void PushEvent(UI::EventType t, uint16_t c, int16_t v, uint16_t m) {
#ifdef OC_DEBUG_UI
    if (!event_queue_.writable())
      ++DEBUG::UI_queue_overflow;
    ++DEBUG::UI_event_count;
#endif
    event_queue_.PushEvent(t, c, v, m);
  }

  bool IgnoreEvent(const UI::Event &event) {
    // Release-first rule, held half: while a chord button is still down,
    // NOTHING it produces reaches the screen it opened. DOWN included, and
    // that is load-bearing: the both-encoder chord is recognised on whichever
    // encoder debounces first, so the OTHER encoder's DOWN is already in the
    // queue behind it. The old one-event-per-button budget was spent right
    // there, which left that encoder's LONG_PRESS unguarded 500 ms later --
    // and in the preset overlay a left-encoder LONG_PRESS is STORE. Pressing
    // the right encoder a millisecond before the left and holding wrote preset
    // slot 1, from a gesture that only meant to open the screen.
    if (chord_hold_ & event.control)
      return true;

    // ...released half: exactly one event is owed, the release the button was
    // in the middle of when the guard armed.
    if (chord_release_ & event.control) {
      chord_release_ &= ~event.control;
      if (UI::EVENT_BUTTON_PRESS == event.type ||
          UI::EVENT_BUTTON_LONG_RELEASE == event.type)
        return true;
      // Anything else means that release never arrived -- a dropped queue
      // entry, or a button named in the chord that was not actually down. Fall
      // through and honour the event: swallowing a DOWN the user meant is the
      // exact failure this rule exists to prevent, so it must not be the
      // failure mode of the rule itself.
    }

    if (screensaver_) {
      screensaver_ = false;
      // The press that woke us is consumed by the waking, and so is the rest
      // of it: whatever is down stays ignored until released.
      SetButtonIgnoreMask();
      return true;
    }

    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

extern Ui ui;

}; // namespace OC

#endif // OC_UI_H_
