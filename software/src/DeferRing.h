#ifndef DEFERRING_H_
#define DEFERRING_H_

#include <stdint.h>

struct DeferRing {
  static const uint8_t kSize = 16;

  typedef void (*Fn0)();
  typedef void (*Fn1)(void *);

  struct Entry {
    Fn0 fn0;
    Fn1 fn1;
    void *ctx;
  };

  Entry q[kSize];
  volatile uint8_t w, r;
  uint32_t dropped, high_water;

  void reset() {
    w = r = 0;
    dropped = high_water = 0;
    for (uint8_t i = 0; i < kSize; ++i) q[i].fn0 = 0, q[i].fn1 = 0, q[i].ctx = 0;
  }

  uint8_t pending() const { return (uint8_t)(w - r); }

  static void barrier() {
#if defined(__arm__)
    __asm__ volatile("dmb" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
  }

  bool push(Fn0 fn) { return push_entry(fn, 0, 0); }
  bool push(Fn1 fn, void *ctx) { return push_entry(0, fn, ctx); }

  uint32_t run_all() {
    uint32_t n = 0;
    uint8_t stop = w;
    while (r != stop) {
      const Entry &e = q[r & (kSize - 1)];
      if (e.fn0) e.fn0();
      else if (e.fn1) e.fn1(e.ctx);
      barrier();
      r = (uint8_t)(r + 1);
      ++n;
    }
    return n;
  }

 private:
  bool push_entry(Fn0 f0, Fn1 f1, void *ctx) {
    const uint8_t depth = pending();
    if (depth >= kSize) {
      dropped++;
      return false;
    }
    Entry &e = q[w & (kSize - 1)];
    e.fn0 = f0;
    e.fn1 = f1;
    e.ctx = ctx;
    barrier();
    w = (uint8_t)(w + 1);
    const uint32_t depth_now = (uint32_t)depth + 1u;
    if (depth_now > high_water) high_water = depth_now;
    return true;
  }
};

#endif
