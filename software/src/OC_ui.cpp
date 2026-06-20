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
  std::fill(button_press_time_, button_press_time_ + 4, 0);
  button_state_ = 0;
  button_ignore_mask_ = 0;
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
    if (button.just_pressed()) {
      button_press_time_[i] = now;
      PushEvent(UI::EVENT_BUTTON_DOWN, control_mask(i), 0, button_state);
    } else if (button.released()) {
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

  if (screensaver_) {
    return UI_MODE_SCREENSAVER;
  } else if (jump_to_menu_) {
    SetButtonIgnoreMask(); // ignore release
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

      const uint8_t *iconroulette[] = {
        PhzIcons::clockDivider, PhzIcons::clockSkip,
        PhzIcons::clock_warp_A, PhzIcons::clock_warp_B,
        PhzIcons::snowflakeB,
        PhzIcons::snowflakeA
      };

      static int pick = 0;
      if (timeout % 50 == 0) pick = random(6);
      // pew pew?
      for (int i = 0; i < 124; i+=8)
        graphics.drawBitmap8(i, 56, 8, iconroulette[pick]);

      // chargin mah lazerrrr
      weegfx::coord_t w = timeout * 128 / SPLASHSCREEN_DELAY_MS;
      w %= 256;
      if (w > 128) w = 256 - w;
      graphics.invertRect(0, 56, w, 8);

      ZapScreensaver();

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
      /*
      const uint8_t *flake_icon[] = { PhzIcons::snowflakeA, PhzIcons::snowflakeB, ZAP_ICON };
      for (int i=0; i<128; ++i) {
        graphics.drawBitmap8(i*8%128 + random(2), i/16*8 + random(2), 8, flake_icon[random(3)]);
      }
      */
      ZapScreensaver();

      graphics.clearRect(27, 22, 74, 22);
      if (reset_settings) {
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
      //graphics.print(OC::Strings::RELEASE_NAME);

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
