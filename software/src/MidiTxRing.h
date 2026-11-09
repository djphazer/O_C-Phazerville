#ifndef MIDITXRING_H_
#define MIDITXRING_H_

#include <stdint.h>

struct MidiTxRing {
  static const uint8_t kSize = 128;

  uint32_t q[kSize];
  uint8_t w, r;
  uint32_t merged, dropped, high_water, promoted;

  void reset() {
    w = r = 0;
    merged = dropped = high_water = promoted = 0;
    for (uint8_t i = 0; i < kSize; ++i) q[i] = 0;
  }

  uint8_t pending() const { return (uint8_t)(w - r); }

  static bool is_continuous(uint8_t status) {
    if (status >= 0xF0) return false;
    const uint8_t kind = status & 0xF0;
    return kind == 0xA0 || kind == 0xB0 || kind == 0xD0 || kind == 0xE0;
  }
  static bool is_keyed(uint8_t status) {
    const uint8_t kind = status & 0xF0;
    return kind == 0xA0 || kind == 0xB0;
  }

  bool push(uint8_t status, uint8_t d1, uint8_t d2) {
    const uint32_t packed =
        (uint32_t)status | ((uint32_t)d1 << 8) | ((uint32_t)d2 << 16);

    if (is_continuous(status) && w != r) {
      const bool keyed = is_keyed(status);
      for (uint8_t i = (uint8_t)(r + 1); i != w; ++i) {
        const uint32_t v = q[i & (kSize - 1)];
        if ((v & 0xFF) != status) continue;
        if (keyed && ((v >> 8) & 0xFF) != d1) continue;
        q[i & (kSize - 1)] = packed;
        ++merged;
        return true;
      }
    }

    if (pending() >= kSize) {
      ++dropped;
      return false;
    }
    q[w & (kSize - 1)] = packed;
    w = (uint8_t)(w + 1);
    const uint8_t d = pending();
    if (d > high_water) high_water = d;
    return true;
  }

  bool promote_realtime() {
    uint8_t i = r;
    while (i != w && (q[i & (kSize - 1)] & 0xFF) < 0xF8) i = (uint8_t)(i + 1);
    if (i == w) return false;
    if (i == r) return true;
    const uint32_t rt = q[i & (kSize - 1)];
    while (i != r) {
      const uint8_t prev = (uint8_t)(i - 1);
      q[i & (kSize - 1)] = q[prev & (kSize - 1)];
      i = prev;
    }
    q[r & (kSize - 1)] = rt;
    ++promoted;
    return true;
  }

  bool peek(uint32_t &out) const {
    if (r == w) return false;
    out = q[r & (kSize - 1)];
    return true;
  }
  void pop() { if (r != w) r = (uint8_t)(r + 1); }
};

#endif
