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

    static constexpr int RAM2_HEADROOM = 10240;

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
      uint32_t now = CORE::ticks;
      uint32_t ticks = now - last_ticks;
      last_ticks = now;
      return ticks;
    }

    void Reset() {
      last_ticks = CORE::ticks;
    }

    uint32_t last_ticks = 0;
  };
}; // namespace OC

template <typename T, size_t max_instances>
struct Factory {
  std::array<T*, max_instances> pool;
  uint16_t mask = 0;

  T* get() {
    for (size_t i = 0; i < max_instances; ++i) {
      if (mask & (1 << i)) continue;

      if (!pool[i]) {
        // use RAM2 first
        void *block = (OC::CORE::FreeRam() > OC::CORE::RAM2_HEADROOM) ? calloc(1, sizeof(T)) : nullptr;
        if (!block) block = extmem_calloc(1, sizeof(T)); // fallback to PSRAM
        if (block) pool[i] = new (block) T(); // place new object
        // else cry about it
      }
      if (pool[i]) {
        mask |= (1 << i);
        return pool[i];
      }
    }
    return nullptr;
  }
  void release(T* instance) {
    for (size_t i = 0; i < max_instances; ++i) {
      if (pool[i] == instance) {
        mask &= ~(1 << i);
      }
    }
  }
};

#endif // OC_CORE_H_
