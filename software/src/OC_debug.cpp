#include <Arduino.h>
#include "OC_ADC.h"
#include "OC_digital_inputs.h"
#include "OC_app_switcher.h"
#include "OC_config.h"
#include "OC_core.h"
#include "OC_debug.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_strings.h"
#include "util/util_misc.h"
#include "src/extern/dspinst.h"

#ifdef ARDUINO_TEENSY41
#include <Audio.h>
#endif

#ifdef POLYLFO_DEBUG  
extern void POLYLFO_debug();
#endif // POLYLFO_DEBUG

#ifdef BBGEN_DEBUG  
extern void BBGEN_debug();
#endif // BBGEN_DEBUG

#ifdef ENVGEN_DEBUG  
extern void ENVGEN_debug();
#endif // ENVGEN_DEBUG

#ifdef BYTEBEATGEN_DEBUG  
extern void BYTEBEATGEN_debug();
#endif // BYTEBEATGEN_DEBUG

#ifdef H1200_DEBUG  
extern void H1200_debug();
#endif // H1200_DEBUG

#ifdef QQ_DEBUG  
extern void QQ_debug();
#endif // QQ_DEBUG

#ifdef ASR_DEBUG
extern void ASR_debug();
#endif // ASR_DEBUG

namespace OC {

namespace DEBUG {
  debug::AveragedCycles ISR_cycles;
  debug::AveragedCycles UI_cycles;
  debug::AveragedCycles MENU_draw_cycles;
  uint32_t UI_event_count;
  uint32_t UI_max_queue_depth;
  uint32_t UI_queue_overflow;

  void Init() {
    debug::CycleMeasurement::Init();
    DebugPins::Init();
  }
}; // namespace DEBUG

static void debug_menu_core() {

  graphics.setPrintPos(2, 12);
  graphics.printf("%uMHz %luus+%luus", F_CPU / 1000 / 1000, OC_CORE_TIMER_RATE, OC_UI_TIMER_RATE);
  
  graphics.setPrintPos(2, 22);
  uint32_t isr_us = debug::cycles_to_us(DEBUG::ISR_cycles.value());
  graphics.printf("CORE%3lu/%3lu/%3lu %2lu%%",
                  debug::cycles_to_us(DEBUG::ISR_cycles.min_value()),
                  isr_us,
                  debug::cycles_to_us(DEBUG::ISR_cycles.max_value()),
                  (isr_us * 100) /  OC_CORE_TIMER_RATE);

  graphics.setPrintPos(2, 32);
  graphics.printf("POLL%3lu/%3lu/%3lu",
                  debug::cycles_to_us(DEBUG::UI_cycles.min_value()),
                  debug::cycles_to_us(DEBUG::UI_cycles.value()),
                  debug::cycles_to_us(DEBUG::UI_cycles.max_value()));

#ifdef OC_DEBUG_UI
  graphics.setPrintPos(2, 42);
  graphics.printf("UI   !%lu #%lu", DEBUG::UI_queue_overflow, DEBUG::UI_event_count);
  graphics.setPrintPos(2, 52);
#endif
}

static void debug_menu_version()
{
  graphics.setPrintPos(2, 12);
  graphics.print(Strings::NAME);
  graphics.setPrintPos(2, 22);
  graphics.print(Strings::VERSION);

  weegfx::coord_t y = 32;
  graphics.setPrintPos(2, y); y += 10;
#ifdef OC_DEV
  graphics.print("DEV");
#else
  graphics.print("PROD");
#endif

#ifdef IO_10V
  graphics.drawStr(2, y, "IO_1OV"); y += 10;
#endif
#ifdef BUCHLA_SUPPORT
  graphics.drawStr(2, y, "BUCHLA_SUPPORT"); y += 10;
#endif
#ifdef BUCHLA_cOC
  graphics.drawStr(2, y, "BUCHLA_cOC"); y += 10;
#endif
#ifdef BUCHLA_4U
  graphics.drawStr(2, y, "BUCHLA_4U"); y += 10;
#endif

#ifdef USB_SERIAL
  graphics.setPrintPos(2, y); y += 10;
  graphics.print("USB_SERIAL");
#endif
}

static void debug_menu_gfx() {
  graphics.drawFrame(0, 0, 128, 64);

  graphics.setPrintPos(0, 12);
  graphics.print("W");

  graphics.setPrintPos(2, 22);
  graphics.printf("MENU %3lu/%3lu/%3lu",
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.min_value()),
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.value()),
                  debug::cycles_to_us(DEBUG::MENU_draw_cycles.max_value()));
}

static void debug_menu_adc() {
#ifdef ARDUINO_TEENSY41
  graphics.setPrintPos(2, 12);
  graphics.printf("C1 %5lu C5 %5lu", ADC::raw_value(ADC_CHANNEL_1), ADC::raw_value(ADC_CHANNEL_5));

  graphics.setPrintPos(2, 22);
  graphics.printf("C2 %5lu C6 %5lu", ADC::raw_value(ADC_CHANNEL_2), ADC::raw_value(ADC_CHANNEL_6));

  graphics.setPrintPos(2, 32);
  graphics.printf("C3 %5lu C7 %5lu", ADC::raw_value(ADC_CHANNEL_3), ADC::raw_value(ADC_CHANNEL_7));

  graphics.setPrintPos(2, 42);
  graphics.printf("C4 %5lu C8 %5lu", ADC::raw_value(ADC_CHANNEL_4), ADC::raw_value(ADC_CHANNEL_8));
#else
  graphics.setPrintPos(2, 12);
  graphics.printf("C1 %5ld %5lu", ADC::value<ADC_CHANNEL_1>(), ADC::raw_value(ADC_CHANNEL_1));

  graphics.setPrintPos(2, 22);
  graphics.printf("C2 %5ld %5lu", ADC::value<ADC_CHANNEL_2>(), ADC::raw_value(ADC_CHANNEL_2));

  graphics.setPrintPos(2, 32);
  graphics.printf("C3 %5ld %5lu", ADC::value<ADC_CHANNEL_3>(), ADC::raw_value(ADC_CHANNEL_3));

  graphics.setPrintPos(2, 42);
  graphics.printf("C4 %5ld %5lu", ADC::value<ADC_CHANNEL_4>(), ADC::raw_value(ADC_CHANNEL_4));
#endif
  const uint8_t trigz[] = {
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_1>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_2>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_3>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_4>()
  };
  graphics.setPrintPos(2, 52);
  graphics.printf("T1=%u T2=%u T3=%u T4=%u", trigz[0], trigz[1], trigz[2], trigz[3]);
}

#ifdef ARDUINO_TEENSY41
static void debug_menu_adc_value() {
  graphics.setPrintPos(2, 12);
  graphics.printf("C1 %5ld C5 %5ld", ADC::value(ADC_CHANNEL_1), ADC::value(ADC_CHANNEL_5));

  graphics.setPrintPos(2, 22);
  graphics.printf("C2 %5ld C6 %5ld", ADC::value(ADC_CHANNEL_2), ADC::value(ADC_CHANNEL_6));

  graphics.setPrintPos(2, 32);
  graphics.printf("C3 %5ld C7 %5ld", ADC::value(ADC_CHANNEL_3), ADC::value(ADC_CHANNEL_7));

  graphics.setPrintPos(2, 42);
  graphics.printf("C4 %5ld C8 %5ld", ADC::value(ADC_CHANNEL_4), ADC::value(ADC_CHANNEL_8));

  const uint8_t trigz[] = {
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_1>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_2>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_3>(),
    OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_4>()
  };
  graphics.setPrintPos(2, 52);
  graphics.printf("T1=%u T2=%u T3=%u T4=%u", trigz[0], trigz[1], trigz[2], trigz[3]);
}

static void debug_menu_audio() {
  float whole = AudioProcessorUsage();
  int part = int(whole * 100) % 100;
  graphics.setPrintPos(2, 12);
  graphics.printf("Total CPU %2d.%2d%%", int(whole), part);

  whole = AudioProcessorUsageMax();
  part = int(whole * 100) % 100;
  graphics.setPrintPos(2, 22);
  graphics.printf("Max CPU %2d.%2d%%", int(whole), part);
}
#endif

#ifdef OC_DEBUG_ADC_STATS
static void debug_menu_adc2() {
  weegfx::coord_t y = 12;
  for (int channel = ADC_CHANNEL_1; channel < ADC_CHANNEL_LAST; ++channel) {
    auto &stats = ADC::get_channel_stats(static_cast<ADC_CHANNEL>(channel));
    graphics.setPrintPos(2, y);
    graphics.printf("CV%d %5d %5d", channel + 1, stats.min, stats.max);
    y += 10;
  }
}
#endif

static void debug_menu_app() {
  auto app = app_switcher.current_app();
  if (app) {
    graphics.print(app->name());
    app->DrawDebugInfo();
  } else {
    graphics.print("?");
  }
}

struct DebugMenu {
  const char *title;
  void (*display_fn)();
};

static const DebugMenu debug_menus[] = {
  { " CORE", debug_menu_core },
  { " VERS", debug_menu_version },
  { " GFX", debug_menu_gfx },
  { " ADC", debug_menu_adc },
#ifdef OC_DEBUG_ADC_STATS
  { " ADC min/max", debug_menu_adc2 },
#endif
#ifdef ARDUINO_TEENSY41
  { " ADC (value)", debug_menu_adc_value },
  { " AUDIO", debug_menu_audio },
#endif
#ifdef POLYLFO_DEBUG  
  { " POLYLFO", POLYLFO_debug },
#endif // POLYLFO_DEBUG
#ifdef ENVGEN_DEBUG  
  { " ENVGEN", ENVGEN_debug },
#endif // ENVGEN_DEBUG
#ifdef BBGEN_DEBUG  
  { " BBGEN", BBGEN_debug },
#endif // BBGEN_DEBUG
#ifdef BYTEBEATGEN_DEBUG  
  { " BYTEBEATGEN", BYTEBEATGEN_debug },
#endif // BYTEBEATGEN_DEBUG
#ifdef H1200_DEBUG  
  { " H1200", H1200_debug },
#endif // H1200_DEBUG
#ifdef QQ_DEBUG  
  { " QQ", QQ_debug },
#endif // QQ_DEBUG
#ifdef ASR_DEBUG  
  { " ASR", ASR_debug },
#endif // ASR_DEBUG
  { " ", debug_menu_app },
};

void Ui::DebugStats() {
  SERIAL_PRINTLN("DEBUG/STATS MENU");

  int current_menu_index = 0;
  bool exit_loop = false;
  while (!exit_loop) {
    const auto &current_menu = debug_menus[current_menu_index];

    GRAPHICS_BEGIN_FRAME(false);
      graphics.setPrintPos(2, 2);
      graphics.printf("%d/%u", current_menu_index + 1, ARRAY_SIZE(debug_menus));
      graphics.print(current_menu.title);
      current_menu.display_fn();
    GRAPHICS_END_FRAME();

    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (UI::EVENT_ENCODER == event.type && CONTROL_ENCODER_L == event.control) {
        current_menu_index = current_menu_index + event.value;
      } else if (UI::EVENT_BUTTON_PRESS == event.type) {
        if (CONTROL_BUTTON_R == event.control)
          exit_loop = true;
        if (CONTROL_BUTTON_L == event.control)
          ++current_menu_index;
      }
    }
    CONSTRAIN(current_menu_index, 0, (int)ARRAY_SIZE(debug_menus) - 1);
  }

  event_queue_.Flush();
  event_queue_.Poke();
}

} // namespace OC
