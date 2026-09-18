#ifndef MIDITXRING_H_
#define MIDITXRING_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Bus-MIDI transmit ring with continuous-controller coalescing.
//
// BSP-free and host-testable (see test/test_miditxring.cpp); the transport
// (PresetBus.cpp) owns the IRQ masking around it and the actual bus writes.
//
// The 200e bus is a 100kHz quiet-gated wire: a nine-byte frame plus its gate
// costs on the order of a millisecond, so two controllers playing aftertouch
// produce bursts far faster than it drains. Continuous controllers are LEVEL,
// not events - only the newest value of a stream matters - so a queued but
// unsent one is replaced in place instead of queued behind. Note on/off,
// program change and realtime/clock are events and are never coalesced.
// ---------------------------------------------------------------------------
struct MidiTxRing {
  static const uint8_t kSize = 128;   // power of two, <= 128 for uint8 indices

  uint32_t q[kSize];
  uint8_t w, r;
  uint32_t merged, dropped, high_water, promoted;

  void reset() {
    w = r = 0;
    merged = dropped = high_water = promoted = 0;
    for (uint8_t i = 0; i < kSize; ++i) q[i] = 0;
  }

  uint8_t pending() const { return (uint8_t)(w - r); }

  // A stream is identified by its status byte (which already carries the
  // 200e bus-line mask in the low nibble); CC and poly aftertouch key on
  // data1 as well, since the controller/note number selects the stream.
  static bool is_continuous(uint8_t status) {
    if (status >= 0xF0) return false;          // realtime/system: events
    const uint8_t kind = status & 0xF0;
    return kind == 0xA0 || kind == 0xB0 || kind == 0xD0 || kind == 0xE0;
  }
  static bool is_keyed(uint8_t status) {
    const uint8_t kind = status & 0xF0;
    return kind == 0xA0 || kind == 0xB0;
  }

  // Returns true if the message was accepted (queued or merged), false if
  // the ring was full and it had to be dropped.
  bool push(uint8_t status, uint8_t d1, uint8_t d2) {
    const uint32_t packed =
        (uint32_t)status | ((uint32_t)d1 << 8) | ((uint32_t)d2 << 16);

    if (is_continuous(status) && w != r) {
      // Skip the head: the transport has already copied that entry out to
      // put it on the wire, so an update there would be lost.
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

  // Bring the oldest realtime byte (0xF8-0xFF: clock, start, continue, stop)
  // to the head, keeping the relative order of everything it passes.
  //
  // Coalescing stops a controller stream from GROWING the queue, but not
  // from sitting in front of a clock tick that was queued after it: each
  // frame ahead costs about a millisecond of quiet-gated bus, so a burst of
  // note and program traffic delays the tick by as many milliseconds as it
  // has entries. The tick is the one message whose whole value is WHEN it
  // arrives, so it goes first. Rotating rather than swapping matters: a
  // swap would move a note-off in front of its note-on and leave the voice
  // stuck on.
  //
  // True when a realtime entry is at the head afterwards (including when it
  // already was, which counts as no promotion); false when none is queued.
  // The caller must mask interrupts: push()'s coalescing writes into the
  // ring body from ISR context and would tear a rotation in progress.
  bool promote_realtime() {
    uint8_t i = r;
    while (i != w && (q[i & (kSize - 1)] & 0xFF) < 0xF8) i = (uint8_t)(i + 1);
    if (i == w) return false;   // nothing realtime queued
    if (i == r) return true;    // already next out
    const uint32_t rt = q[i & (kSize - 1)];
    while (i != r) {            // shift the entries it passes up one slot
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

#endif  // MIDITXRING_H_
