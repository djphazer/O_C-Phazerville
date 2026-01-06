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
#include <functional>
#include <queue>

using Task = std::function<void()>;

namespace OC {
  namespace CORE {
    extern volatile uint32_t ticks;
    extern volatile bool app_isr_enabled;
    extern volatile bool display_update_enabled;
    extern volatile bool app_loop_enabled;

    void DeferTask(Task func);
    void FlushTasks();
    int FreeRam();
  }; // namespace CORE

  struct TickCount {
    TickCount() { }
    void Init() {
      last_ticks = 0;
    }

    uint32_t Update() {
      uint32_t now = OC::CORE::ticks;
      uint32_t ticks = now - last_ticks;
      last_ticks = now;
      return ticks;
    }

    uint32_t last_ticks;
  };
}; // namespace OC

template <typename T, size_t max_instances>
struct Factory {
  std::array<T*, max_instances> pool;
  uint16_t mask = 0;

  T* get() {
    for (int i = 0; i < max_instances; ++i) {
      if (mask & (1 << i)) continue;

      if (!pool[i] && OC::CORE::FreeRam() > (int)sizeof(T) + 1000) { // 1KB of padding to be safe
        pool[i] = new T();
      }
      if (pool[i]) {
        mask |= (1 << i);
        return pool[i];
      }
    }
    return nullptr;
  }
  void release(T* instance) {
    for (int i = 0; i < max_instances; ++i) {
      if (pool[i] == instance) {
        mask &= ~(1 << i);
      }
    }
  }
};

#endif // OC_CORE_H_
