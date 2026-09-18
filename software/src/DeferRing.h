#ifndef DEFERRING_H_
#define DEFERRING_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// ISR-to-loop deferred-call ring.
//
// Single producer (the CORE timer ISR, 16.667 kHz), single consumer
// (loop() via OC::CORE::FlushTasks). Fixed capacity, plain function
// pointers, no allocation: the std::queue<std::function> it replaces did a
// heap allocation inside the ISR, racing loop()'s own malloc, and the two
// ends shared the queue with no lock at all.
//
// BSP-free and host-testable (test/test_defer_ring.cpp), same shape as
// MidiTxRing.h: power-of-two size, uint8 indices, a full ring refuses the
// newest entry and counts the drop. A dropped entry is a missed clock tick
// on the wire, so the drop counter is a budget row, not a curiosity.
//
// Ordering: the producer writes the entry, then publishes `w`; the
// consumer reads `r`'s entry, then publishes `r`. On the single-core M7 the
// indices are volatile and a data-memory barrier separates payload from
// index so the compiler cannot reorder them either.
// ---------------------------------------------------------------------------
struct DeferRing {
  static const uint8_t kSize = 16;   // power of two, <= 128 for uint8 indices

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

  // Producer side. False = full, the call is dropped and counted.
  bool push(Fn0 fn) { return push_entry(fn, 0, 0); }
  bool push(Fn1 fn, void *ctx) { return push_entry(0, fn, ctx); }

  // Consumer side: run everything queued at entry, in order. Returns the
  // number of calls made. Entries pushed while running are left for the
  // next pass so a task that re-defers cannot spin this loop forever.
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
    // depth is uint8_t, so depth + 1 promotes to int and comparing that
    // against a uint32_t is a sign-compare. Harmless at these values, but
    // GCC puts -Wsign-compare in -Wall for C++ and CI builds -Werror.
    const uint32_t depth_now = (uint32_t)depth + 1u;
    if (depth_now > high_water) high_water = depth_now;
    return true;
  }
};

#endif  // DEFERRING_H_
