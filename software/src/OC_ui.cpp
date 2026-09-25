#include <Arduino.h>
#include <algorithm>

#include "OC_strings.h"
#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "icons.h"
#include "OC_calibration.h"
#include "OC_config.h"
#include "OC_core.h"
#include "OC_gpio.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_options.h"
#include "OC_app_switcher.h"
#include "src/drivers/display.h"
#include "HSUtils.h"

#include "PresetBusUI.h"

#ifdef VOR
#include "VBiasManager.h"
VBiasManager *VBiasManager::instance = 0;
#endif

extern uint_fast8_t MENU_REDRAW;

namespace OC {

Ui ui;

FLASHMEM
void Ui::Init() {
  ticks_ = 0;
  set_screensaver_timeout(SCREENSAVER_TIMEOUT_S);

#if defined(VOR)
  static const int button_pins[] = { but_top, but_bot, butL, butR, but_mid };
#elif defined(ARDUINO_TEENSY41)
  static const int button_pins[] = { but_top, but_bot, butL, butR, but_mid, but_top2, but_bot2 };
#else
  static const int button_pins[] = { but_top, but_bot, butL, butR };
#endif

#if defined(ARDUINO_TEENSY41)
  const size_t count = (but_mid == 0xFF)? 4 : CONTROL_BUTTON_LAST;
#else
  const size_t count = CONTROL_BUTTON_LAST;
#endif
  for (size_t i = 0; i < count; ++i) {
    buttons_[i].Init(button_pins[i], OC_GPIO_BUTTON_PINMODE);
  }
  // ...+ CONTROL_BUTTON_LAST, not + 4: on T4.1 the array is seven long, and
  // the three tail entries (Z/X/Y) were left uninitialised, so the first
  // long-press decision for those buttons compared `now` against garbage.
  std::fill(button_press_time_, button_press_time_ + CONTROL_BUTTON_LAST, 0);
  button_state_ = 0;
  button_down_ = 0;
  chord_hold_ = 0;
  chord_release_ = 0;
  screensaver_ = false;
  preempt_screensaver_ = false;
  jump_to_menu_ = false;

  encoder_right_.Init(OC_GPIO_ENC_PINMODE);
  encoder_left_.Init(OC_GPIO_ENC_PINMODE);

  event_queue_.Init();
}

FLASHMEM
void Ui::configure_encoders(EncoderConfig encoder_config) {
  SERIAL_PRINTLN("Configuring encoders: %s (%x)", OC::Strings::encoder_config_strings[encoder_config], encoder_config);

  encoder_right_.reverse(encoder_config & ENCODER_CONFIG_R_REVERSED);
  encoder_left_.reverse(encoder_config & ENCODER_CONFIG_L_REVERSED);
}

FLASHMEM
void Ui::set_screensaver_timeout(uint32_t seconds) {
  uint32_t timeout = seconds * 1000U;
  if (timeout < kLongPressTicks * 2)
    timeout = kLongPressTicks * 2;

  screensaver_timeout_ = timeout;
  SERIAL_PRINTLN("Set screensaver timeout to %lu", timeout);
  event_queue_.Poke();
}

void FASTRUN Ui::Poke() {
  screensaver_ = false;
  event_queue_.Poke();
}

void Ui::preempt_screensaver(bool v) {
  preempt_screensaver_ = v;
}

void FASTRUN Ui::Poll() {

  uint32_t now = ++ticks_;
  uint16_t button_state = 0;

#if defined(ARDUINO_TEENSY41)
  const size_t count = (but_mid == 0xFF)? 4 : CONTROL_BUTTON_LAST;
#else
  const size_t count = CONTROL_BUTTON_LAST;
#endif
  for (size_t i = 0; i < count; ++i) {
    if (buttons_[i].Poll())
      button_state |= control_mask(i);
  }

  for (size_t i = 0; i < count; ++i) {
    auto &button = buttons_[i];
    const uint16_t control = control_mask(i);
    if (button.just_pressed()) {
      button_down_ |= control;
      button_press_time_[i] = now;
      PushEvent(UI::EVENT_BUTTON_DOWN, control_mask(i), 0, button_state);
    } else if (button.released()) {
      button_down_ &= ~control;
      // Release-first rule: the held half of the guard ends HERE, on the
      // debounced release -- seven consecutive high reads -- and never on
      // button_state, which is the raw pin. A switch that bounces reads high
      // for a poll or two while the user is still holding, and clearing the
      // guard on that would hand the screen the very press the guard exists to
      // absorb. The release event being pushed on this same tick is still the
      // chord's own, so it goes to the other half rather than out.
      //
      // This is also why the guard cannot stick and leave a button dead: it is
      // only ever armed on a pin that is (or has just been) low, and every low
      // excursion ends here. UI::Button::state_ reaches 0x7f seven high reads
      // after ANY low read, whether or not the press was long enough to have
      // reached 0x80 and reported a press in the first place.
      if (chord_hold_ & control) {
        chord_hold_ &= ~control;
        chord_release_ |= control;
      }
      if (now - button_press_time_[i] < kLongPressTicks)
        PushEvent(UI::EVENT_BUTTON_PRESS, control_mask(i), 0, button_state);
      else
        PushEvent(UI::EVENT_BUTTON_LONG_RELEASE, control_mask(i), 0, button_state);

      button_press_time_[i] = 0;
    } else if (button.pressed() && (now - button_press_time_[i] == kLongPressTicks)) {
      PushEvent(UI::EVENT_BUTTON_LONG_PRESS, control_mask(i), 0, button_state);
    }
  }

  encoder_right_.Poll();
  encoder_left_.Poll();

  int32_t increment;
  increment = encoder_right_.Read();
  if (increment)
    PushEvent(UI::EVENT_ENCODER, CONTROL_ENCODER_R, increment, button_state);

  increment = encoder_left_.Read();
  if (increment)
    PushEvent(UI::EVENT_ENCODER, CONTROL_ENCODER_L, increment, button_state);

  button_state_ = button_state;
}

FLASHMEM void Ui::Inject(UI::EventType type, uint16_t control, int16_t value,
                         uint16_t held) {
  // Only the DOWN of a tap carries its button in the mask, as the raw pin
  // would; by the PRESS the pin is high again.
  //
  // `held` adds modifier buttons to that mask, which is what lets a chord be
  // injected at all -- every global gesture is recognised by testing
  // event.mask for A or Z on another control's DOWN. Nothing is synthesized
  // implicitly: a caller asking for a chord has to name the modifier.
  //
  // The screens these chords open arm IgnoreUntilRelease() on the whole chord.
  // A modifier that was never physically down is simply "already up" to that
  // guard, so it swallows one release that never arrives and costs nothing --
  // the injected press that follows is delivered normally.
  const uint16_t mask = ((type == UI::EVENT_BUTTON_DOWN) ? control : 0) | held;
  noInterrupts();
  PushEvent(type, control, value, mask);
  interrupts();
}

// Loop context only, and only ever from Main.cpp's loop() -- which is itself
// FLASHMEM. The ISR half of the UI is Ui::Poll() above (it fills the event
// queue); this is the half that DRAINS it, so it does nothing at all unless a
// human moved an encoder or pressed a button. Flash is the right home for it.
//
// FLASHMEM + noinline for the reason PresetBusUI.cpp's HandleEvent already
// documents, seen from the other side: without an explicit placement here,
// whether these ~1.5KB land in ITCM was decided by LTO's inlining mood. When
// LTO folded it into FLASHMEM loop() the ITCM cost was zero; when unrelated
// churn elsewhere in the image (a bigger Bus200e bridge, say) pushed it back
// out of line, it reappeared as a 1544-byte ITCM function -- which on
// T41_audio_dbg, sitting ~1KB under a 32KB ITCM bank boundary, cost a whole
// extra bank and overflowed RAM1. Pinning it makes that deterministic.
FLASHMEM __attribute__((noinline))
UiMode Ui::DispatchEvents(const RuntimeSlot &appslot) {
  AppBase* app = static_cast<AppBase*>(appslot.instance);
  if (!app) return UiMode::UI_MODE_APP_SETTINGS;

  while (event_queue_.available()) {
    const UI::Event event = event_queue_.PullEvent();
    if (screensaver_ && UI::EVENT_BUTTON_LONG_RELEASE == event.type)
      continue; // saves some headaches
    if (IgnoreEvent(event))
      continue;

    MENU_REDRAW = 1;

    // 200e preset-bus overlay: it owns all input while open, and holding
    // BOTH encoder buttons opens it (unused gesture; menu is A/Z + R).
    //
    // Compile-gated to the targets that can actually have the bus. VOR
    // hardware binds the same L+R chord to VBiasManager::AdvanceBias()
    // (OC_app_base.cpp, EVENT_BUTTON_DOWN), and DispatchEvents runs before
    // the app's handler. The PresetBusUI stubs are inert off-target, but
    // SetButtonIgnoreMask() and the `continue` are not -- ungated, this
    // block swallows the chord on every build and VBias cycling dies.
#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)
    if (OC::PresetBusUI::Active()) {
      if (OC::PresetBusUI::HandleEvent(event)) continue;
    }
    if (UI::EVENT_BUTTON_DOWN == event.type &&
        (CONTROL_BUTTON_L == event.control || CONTROL_BUTTON_R == event.control) &&
        (event.mask & (CONTROL_BUTTON_L | CONTROL_BUTTON_R))
            == (CONTROL_BUTTON_L | CONTROL_BUTTON_R)) {
      OC::PresetBusUI::Enter();
      SetButtonIgnoreMask();  // swallow the releases
      continue;
    }
#endif

    const bool z_hold = (event.mask & CONTROL_BUTTON_Z);
    const bool a_hold = (event.mask & CONTROL_BUTTON_A);

    // --- Handle global hotkeys
    if (UI::EVENT_BUTTON_DOWN == event.type) {
      // Hold Z or A and push right encoder for main menu
      if (CONTROL_BUTTON_R == event.control && (z_hold || a_hold)) {
        jump_to_menu_ = true;
        break;
      }
      // Hold Z or A and push left encoder for IO settings menu
      if (CONTROL_BUTTON_L == event.control && (z_hold || a_hold)) {
        app->EditIOSettings();
        SetButtonIgnoreMask();
        continue;
      }
      // Hold Z and push A for screensaver (not available on O_C without VOR button)
      if (CONTROL_BUTTON_A == event.control && z_hold) {
        screensaver_ = true;
        SetButtonIgnoreMask();
        break;
      }
    }

    if (UI_MODE_SCREENSAVER == app->DispatchEvent(event)) {
      screensaver_ = true;
      // Break to handle screensaver; queued events will be handled next call
      break;
    }
  }

  // Turning screensaver seconds into screen-blanking minutes with the * 60 (chysn 9/2/2018)
  if (idle_time() > (screensaver_timeout() * 60) && !preempt_screensaver_)
    screensaver_ = true;

  // Panel sleep, evaluated from idle_time() every pass rather than latched on
  // an edge: idle_time() is millis() - last_event_time_, so it is already reset
  // by ANY event from ANY control (ui_event_queue.h). Deriving the state
  // instead of hooking a wake-up path means there is no gesture that can leave
  // the panel dark and no path back that can be missed -- if the module is
  // being touched, the display is drawing, by construction.
  //
  // THIS DELIBERATELY DOES NOT SEND 0xAE, AND MUST NOT.
  //
  // The first version of this called display::SetDisplayOn(), which does an
  // SPI.beginTransaction()/transfer()/endTransaction() from LOOP context. On
  // Teensy 4.1 that LPSPI bus is SHARED WITH THE DAC: the page transfer is
  // chained onto the DAC's completion interrupt (see spi_sendpage_isr and
  // SendPage's `sendpage_state` guard), and the core ISR writes the DAC every
  // 60us. Reconfiguring LPSPI4_TCR from loop, unsynchronised, races that ISR.
  // It is a race rather than a certainty, which is the worst kind: it survived
  // a bench pass, then hung a module hard enough to drop it off USB entirely,
  // where it stayed until the program button was pressed. SetInverted() has
  // the same shape and has simply been lucky, being rare and user-initiated.
  //
  // Blanking costs nothing and buys the same thing. OLED pixels age when they
  // are LIT; an all-black frame lights none of them, so the burn-in this
  // exists to prevent is prevented either way. True display-off would save a
  // little power on top of that, and it is not worth touching a bus the audio
  // ISR is using.
  display_asleep_ = idle_time() > kDisplaySleepMs;

  if (screensaver_) {
    return UI_MODE_SCREENSAVER;
  } else if (jump_to_menu_) {
    // The A/Z + encR chord already claimed itself above; this covers the OTHER
    // way in, JumpToMenu() from an app (Hemisphere's encR), whose button is
    // likewise still down. Re-arming for a chord already armed is a no-op.
    SetButtonIgnoreMask();
    jump_to_menu_ = false;
    return UI_MODE_APP_SETTINGS;
  } else {
    return UI_MODE_MENU;
  }
}

FLASHMEM
UiMode Ui::Splashscreen(bool &reset_settings, uint8_t phase) {

  UiMode mode = UI_MODE_MENU;

  elapsedMillis timeout = 0;

  switch (phase) {
  case 0:
    do {
      mode = UI_MODE_MENU;
      if (read_immediate(CONTROL_BUTTON_L))
        mode = UI_MODE_CALIBRATE;
      if (read_immediate(CONTROL_BUTTON_R))
        mode = UI_MODE_APP_SETTINGS;

      reset_settings =
      #if defined(NORTHERNLIGHT) && !defined(IO_10V)
         read_immediate(CONTROL_BUTTON_UP) && read_immediate(CONTROL_BUTTON_R);
      #else
         read_immediate(CONTROL_BUTTON_UP) && read_immediate(CONTROL_BUTTON_DOWN);
      #endif

      GRAPHICS_BEGIN_FRAME(true);

      menu::DefaultTitleBar::Draw();
      graphics.print( DAC_is_inverted? OC::Strings::NAME_NLM : OC::Strings::NAME);
      weegfx::coord_t y = menu::CalcLineY(0);

      graphics.setPrintPos(menu::kIndentDx, y + menu::kTextDy);
      graphics.print("[L] => Calibrate");
      if (UI_MODE_CALIBRATE == mode)
        graphics.invertRect(menu::kIndentDx, y, 128, menu::kMenuLineH);

      y += menu::kMenuLineH;
      graphics.setPrintPos(menu::kIndentDx, y + menu::kTextDy);
      graphics.print("[R] => Main Menu");
      if (UI_MODE_APP_SETTINGS == mode)
        graphics.invertRect(menu::kIndentDx, y, 128, menu::kMenuLineH);

      y += menu::kMenuLineH;
      graphics.setPrintPos(menu::kIndentDx, y + menu::kTextDy);
      if (reset_settings) {
        graphics.print("!! RESET EEPROM !!");
        y += menu::kMenuLineH;
        graphics.setPrintPos(menu::kIndentDx, y + menu::kTextDy);
      }
      graphics.print(OC::Strings::VERSION);
      graphics.print(" ");
      graphics.print(OC::Strings::BUILD_TAG);

      // Plain progress bar -- was a cycling row of clock/snowflake icons
      // ("chargin mah lazerrrr"); the character animation below is where
      // this splash's personality lives now, so this stays a simple,
      // silent "still booting" indicator instead of competing with it.
      weegfx::coord_t w = timeout * 128 / SPLASHSCREEN_DELAY_MS;
      w %= 256;
      if (w > 128) w = 256 - w;
      graphics.invertRect(0, 56, w, 8);

      /* fixes spurious button presses when booting ? */
      while (event_queue_.available())
        (void)event_queue_.PullEvent();

      GRAPHICS_END_FRAME();

    } while (timeout < SPLASHSCREEN_DELAY_MS);
      break;

  case 1:
  default:
    do {
      GRAPHICS_BEGIN_FRAME(true);

      if (reset_settings) {
        // Safety-relevant confirmation text -- unchanged, no animation
        // competing with it.
        graphics.clearRect(27, 22, 74, 22);
        graphics.setPrintPos(28, 23);
        graphics.print("Time for a ");
        graphics.setPrintPos(28, 33);
        graphics.print("Fresh Start!");
      } else {
        graphics.setPrintPos(28, 23);
        graphics.print(" Welcome to");
        graphics.setPrintPos(28, 33);
        graphics.print("Phazerville!");
      }

      while (event_queue_.available())
        (void)event_queue_.PullEvent();
      GRAPHICS_END_FRAME();
      delay(5);
    } while (timeout < SPLASHSCREEN_DELAY_MS/2);
    break;
  }

  SetButtonIgnoreMask();
  return mode;
}

} // namespace OC
