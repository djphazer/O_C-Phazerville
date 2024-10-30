#include <Arduino.h>
#include "OC_ADC.h"
#include "OC_digital_inputs.h"
#include "OC_config.h"
#include "OC_core.h"
#include "OC_debug.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_strings.h"
#include "util/util_misc.h"
#include "extern/dspinst.h"

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

#ifdef OC_UI_DEBUG
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

//      graphics.setPrintPos(2, 42);
//      graphics.print((long)ADC::busy_waits());
//      graphics.setPrintPos(2, 42); graphics.print(ADC::fail_flag0());
//      graphics.setPrintPos(2, 52); graphics.print(ADC::fail_flag1());
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

struct DebugMenu {
  const char *title;
  void (*display_fn)();
};

static const DebugMenu debug_menus[] = {
  { " CORE", debug_menu_core },
  { " VERS", debug_menu_version },
  { " GFX", debug_menu_gfx },
  { " ADC (raw)", debug_menu_adc },
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
 { nullptr, nullptr }
};

void Ui::DebugStats() {
  SERIAL_PRINTLN("DEBUG/STATS MENU");

  const DebugMenu *current_menu = &debug_menus[0];
  bool exit_loop = false;
  while (!exit_loop) {

    GRAPHICS_BEGIN_FRAME(false);
      graphics.setPrintPos(2, 2);
      graphics.printf("%d/%u", (int)(current_menu - &debug_menus[0]) + 1, ARRAY_SIZE(debug_menus) - 1);
      graphics.print(current_menu->title);
      current_menu->display_fn();
    GRAPHICS_END_FRAME();

    while (event_queue_.available()) {
      UI::Event event = event_queue_.PullEvent();
      if (CONTROL_BUTTON_R == event.control && UI::EVENT_BUTTON_PRESS == event.type) {
        exit_loop = true;
      } else if (CONTROL_BUTTON_L == event.control && UI::EVENT_BUTTON_PRESS == event.type) {
        ++current_menu;
        if (!current_menu->title || !current_menu->display_fn)
          current_menu = &debug_menus[0];
      }
    }
  }

  event_queue_.Flush();
  event_queue_.Poke();
}

}; // namespace OC
