#ifndef UTIL_DEBUGPINS_H_
#define UTIL_DEBUGPINS_H_

#include "../OC_config.h"

namespace util {

struct scoped_debug_pin {
  scoped_debug_pin(int pin) : pin_(pin) {
    digitalWriteFast(pin, HIGH);
  }
  ~scoped_debug_pin() {
    digitalWriteFast(pin_, LOW);
  }
  int pin_;
};

}; // namespace util

#ifdef OC_DEBUG_ENABLE_PINS
#define DEBUG_PIN_SCOPE(pin) \
  util::scoped_debug_pin debug_pin(pin)
#else
#define DEBUG_PIN_SCOPE(pin) \
  do {} while (0)
#endif

#endif // UTIL_DEBUGPINS_H_
