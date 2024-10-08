#ifndef OC_CORE_H_
#define OC_CORE_H_

#include <Arduino.h>
#include <stdint.h>
#include "OC_config.h"
#include "OC_strings.h"
#include "OC_ui.h"
#include "OC_menus.h"
#include "util/util_debugpins.h"
#include "src/drivers/display.h"

namespace OC {
  namespace CORE {
  extern volatile uint32_t ticks;
  extern volatile bool app_isr_enabled;

  }; // namespace CORE


  struct TickCount {
    TickCount() { }
    void Init() {
      last_ticks = 0;
    }

    uint32_t Update() {
      uint32_t now = CORE::ticks;
      uint32_t ticks = now - last_ticks;
      last_ticks = now;
      return ticks;
    }

    void Reset() {
      last_ticks = CORE::ticks;
    }

    uint32_t last_ticks;
  };
}; // namespace OC

#endif // OC_CORE_H_
