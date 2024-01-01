#include <Arduino.h>
#include <algorithm>

#include "OC_strings.h"
#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "OC_calibration.h"
#include "OC_config.h"
#include "OC_core.h"
#include "OC_gpio.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_options.h"
#include "src/drivers/display.h"

#ifdef VOR
#include "VBiasManager.h"
VBiasManager *VBiasManager::instance = 0;
#endif

extern uint_fast8_t MENU_REDRAW;

namespace OC {

Ui ui;

void Ui::Init() {
  ticks_ = 0;
  set_screensaver_timeout(SCREENSAVER_TIMEOUT_S);

#if defined(VOR)
  static const int button_pins[] = { but_top, but_bot, butL, butR, but_mid };
#else
  static const int button_pins[] = { but_top, but_bot, butL, butR };
#endif

  for (size_t i = 0; i < CONTROL_BUTTON_LAST; ++i) {
    buttons_[i].Init(button_pins[i], OC_GPIO_BUTTON_PINMODE);
  }
  std::fill(button_press_time_, button_press_time_ + 4, 0);
  button_state_ = 0;
  button_ignore_mask_ = 0;
  screensaver_ = false;
  preempt_screensaver_ = false;

  encoder_right_.Init(OC_GPIO_ENC_PINMODE);
  encoder_left_.Init(OC_GPIO_ENC_PINMODE);

  event_queue_.Init();
}

void Ui::configure_encoders(EncoderConfig encoder_config) {
  SERIAL_PRINTLN("Configuring encoders: %s (%x)", OC::Strings::encoder_config_strings[encoder_config], encoder_config);

  encoder_right_.reverse(encoder_config & ENCODER_CONFIG_R_REVERSED);
  encoder_left_.reverse(encoder_config & ENCODER_CONFIG_L_REVERSED);
}

void Ui::set_screensaver_timeout(uint32_t seconds) {
  uint32_t timeout = seconds * 1000U;
  if (timeout < kLongPressTicks * 2)
    timeout = kLongPressTicks * 2;

  screensaver_timeout_ = timeout;
  SERIAL_PRINTLN("Set screensaver timeout to %lu", timeout);
  event_queue_.Poke();
}

void FASTRUN Ui::_Poke() {
  event_queue_.Poke();
}

void Ui::_preemptScreensaver(bool v) {
  preempt_screensaver_ = v;
}

void FASTRUN Ui::Poll() {

  uint32_t now = ++ticks_;
  uint16_t button_state = 0;

  for (size_t i = 0; i < CONTROL_BUTTON_LAST; ++i) {
    if (buttons_[i].Poll())
      button_state |= control_mask(i);
  }

  for (size_t i = 0; i < CONTROL_BUTTON_LAST; ++i) {
    auto &button = buttons_[i];
    if (button.just_pressed()) {
      button_press_time_[i] = now;
      PushEvent(UI::EVENT_BUTTON_DOWN, control_mask(i), 0, button_state);
    } else if (button.released()) {
      if (now - button_press_time_[i] < kLongPressTicks)
        PushEvent(UI::EVENT_BUTTON_PRESS, control_mask(i), 0, button_state);
      button_press_time_[i] = 0;
    } else if (button.pressed() && (now - button_press_time_[i] == kLongPressTicks)) {
      button_state &= ~control_mask(i);
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

UiMode Ui::DispatchEvents(const App *app) {

  while (event_queue_.available()) {
    const UI::Event event = event_queue_.PullEvent();
    if (IgnoreEvent(event))
      continue;

    switch (event.type) {
      case UI::EVENT_BUTTON_PRESS:
#ifdef VOR
        if (OC::CONTROL_BUTTON_M == event.control) {
            VBiasManager *vbias_m = vbias_m->get();
            vbias_m->AdvanceBias();
        } else
#endif
        app->HandleButtonEvent(event);
        break;
      case UI::EVENT_BUTTON_DOWN:
#ifdef VOR
        // dual encoder press
        if ( ((OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R) == event.mask) )
        {
            VBiasManager *vbias_m = vbias_m->get();
            vbias_m->AdvanceBias();
            SetButtonIgnoreMask(); // ignore release and long-press
        }
        else
#endif
            app->HandleButtonEvent(event);
        break;
      case UI::EVENT_BUTTON_LONG_PRESS:
        if (OC::CONTROL_BUTTON_UP == event.control) {
            if (!preempt_screensaver_) screensaver_ = true;
        }
        else if (OC::CONTROL_BUTTON_R == event.control)
          return UI_MODE_APP_SETTINGS;
        else
          app->HandleButtonEvent(event);
        break;
      case UI::EVENT_ENCODER:
        app->HandleEncoderEvent(event);
        break;
      default:
        break;
    }
    MENU_REDRAW = 1;
  }

  // Turning screensaver seconds into screen-blanking minutes with the * 60 (chysn 9/2/2018)
  if (idle_time() > (screensaver_timeout() * 60))
    screensaver_ = true;

  if (screensaver_)
    return UI_MODE_SCREENSAVER;
  else
    return UI_MODE_MENU;
}

UiMode Ui::Splashscreen(bool &reset_settings) {

  UiMode mode = UI_MODE_MENU;

  unsigned long start = millis();
  unsigned long now = start;
  do {

    mode = UI_MODE_MENU;
    if (read_immediate(CONTROL_BUTTON_L))
      mode = UI_MODE_CALIBRATE;
    if (read_immediate(CONTROL_BUTTON_R))
      mode = UI_MODE_APP_SETTINGS;

    reset_settings =
    #if defined(BUCHLA_4U) && !defined(IO_10V)
       read_immediate(CONTROL_BUTTON_UP) && read_immediate(CONTROL_BUTTON_R);
    #else
       read_immediate(CONTROL_BUTTON_UP) && read_immediate(CONTROL_BUTTON_DOWN);
    #endif

    now = millis();

    // This graphics frame is necessary for the keys to be read. I don't know why this is the case,
    // since the keys are read above, but I don't have time to figure that out right now. --jj
    GRAPHICS_BEGIN_FRAME(true);

    /* fixes spurious button presses when booting ? */
    while (event_queue_.available())
      (void)event_queue_.PullEvent();

    GRAPHICS_END_FRAME();

  } while (now - start < SPLASHSCREEN_DELAY_MS);

  SetButtonIgnoreMask();
  return mode;
}

} // namespace OC
