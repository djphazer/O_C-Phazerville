// Host tests for the bus-MIDI transmit ring (src/MidiTxRing.h): continuous-
// controller coalescing, stream identity, event messages that must NEVER be
// folded, head-entry protection, wraparound and overflow.
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_miditxring test_miditxring.cpp && ./build/test_miditxring
#include <cstdio>
#include <cstring>

#include "../src/MidiTxRing.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

static MidiTxRing ring;

// status bytes carry the 200e bus-line mask in the low nibble
static const uint8_t CC_A    = 0xB8;   // control change, bus A
static const uint8_t CC_B    = 0xB4;   // control change, bus B
static const uint8_t CHAT_A  = 0xD8;   // channel aftertouch, bus A
static const uint8_t BEND_A  = 0xE8;   // pitch bend, bus A
static const uint8_t POLY_A  = 0xA8;   // poly aftertouch, bus A
static const uint8_t NOTEON  = 0x98;   // note on, bus A
static const uint8_t NOTEOFF = 0x88;
static const uint8_t PROG    = 0xC8;   // program change
static const uint8_t CLOCK   = 0xF8;   // realtime

static uint8_t d1_at(uint8_t depth) {  // data1 of the nth pending entry
  uint32_t v = ring.q[(uint8_t)(ring.r + depth) & (MidiTxRing::kSize - 1)];
  return (uint8_t)((v >> 8) & 0xFF);
}
static uint8_t d2_at(uint8_t depth) {
  uint32_t v = ring.q[(uint8_t)(ring.r + depth) & (MidiTxRing::kSize - 1)];
  return (uint8_t)((v >> 16) & 0xFF);
}

static void test_basic_queue() {
  ring.reset();
  CHECK(ring.pending() == 0);
  uint32_t v = 0;
  CHECK(!ring.peek(v));                 // empty
  CHECK(ring.push(NOTEON, 60, 100));
  CHECK(ring.pending() == 1);
  CHECK(ring.peek(v));
  CHECK((v & 0xFF) == NOTEON && ((v >> 8) & 0xFF) == 60);
  ring.pop();
  CHECK(ring.pending() == 0);
  CHECK(ring.merged == 0 && ring.dropped == 0);
}

static void test_events_never_merge() {
  ring.reset();
  // notes, program change and clock are EVENTS: every one must survive
  ring.push(NOTEON, 60, 100);
  ring.push(NOTEON, 60, 100);           // same note twice = two hits
  ring.push(NOTEOFF, 60, 0);
  ring.push(PROG, 5, 0);
  ring.push(PROG, 6, 0);
  ring.push(CLOCK, 0, 0);
  ring.push(CLOCK, 0, 0);               // every tick matters
  CHECK(ring.pending() == 7);
  CHECK(ring.merged == 0);
}

static void test_aftertouch_coalesces() {
  ring.reset();
  ring.push(NOTEON, 60, 100);           // head: protected from merging
  ring.push(CHAT_A, 0, 10);
  CHECK(ring.pending() == 2);
  for (uint8_t v = 11; v <= 60; ++v) ring.push(CHAT_A, 0, v);
  CHECK(ring.pending() == 2);           // 50 more pressures, still 2 entries
  CHECK(ring.merged == 50);
  CHECK(ring.dropped == 0);
  CHECK(d2_at(1) == 60);                // newest value won
}

static void test_bend_and_streams_are_distinct() {
  ring.reset();
  ring.push(NOTEON, 60, 100);           // head
  ring.push(CHAT_A, 0, 20);
  ring.push(BEND_A, 0, 64);
  ring.push(CC_A, 74, 5);
  ring.push(CC_A, 71, 5);               // different CC number = own stream
  ring.push(CC_B, 74, 5);               // same CC, different bus line
  CHECK(ring.pending() == 6);
  CHECK(ring.merged == 0);
  // now update each stream once; nothing new should be appended
  ring.push(CHAT_A, 0, 21);
  ring.push(BEND_A, 0, 100);
  ring.push(CC_A, 74, 9);
  ring.push(CC_A, 71, 9);
  ring.push(CC_B, 74, 9);
  CHECK(ring.pending() == 6);
  CHECK(ring.merged == 5);
}

static void test_poly_aftertouch_keys_on_note() {
  ring.reset();
  ring.push(NOTEON, 60, 100);           // head
  ring.push(POLY_A, 60, 10);
  ring.push(POLY_A, 64, 10);            // different note = own stream
  CHECK(ring.pending() == 3);
  ring.push(POLY_A, 60, 90);
  ring.push(POLY_A, 64, 91);
  CHECK(ring.pending() == 3);
  CHECK(ring.merged == 2);
  CHECK(d1_at(1) == 60 && d2_at(1) == 90);
  CHECK(d1_at(2) == 64 && d2_at(2) == 91);
}

static void test_head_entry_is_never_rewritten() {
  ring.reset();
  // the transport has already copied the head out to put it on the wire;
  // rewriting it there would lose the update entirely
  ring.push(CHAT_A, 0, 5);
  ring.push(CHAT_A, 0, 6);              // head protected -> appends
  CHECK(ring.pending() == 2);
  CHECK(ring.merged == 0);
  uint32_t v = 0;
  CHECK(ring.peek(v));
  CHECK(((v >> 16) & 0xFF) == 5);       // head still the value being sent
  ring.push(CHAT_A, 0, 7);              // now merges into entry 1
  CHECK(ring.pending() == 2);
  CHECK(ring.merged == 1);
  CHECK(d2_at(1) == 7);
}

static void test_overflow_only_for_events() {
  ring.reset();
  // fill with distinct events
  for (uint8_t i = 0; i < MidiTxRing::kSize; ++i)
    CHECK(ring.push(NOTEON, (uint8_t)(i & 0x7F), 100) == true);
  CHECK(ring.pending() == MidiTxRing::kSize);
  CHECK(ring.high_water == MidiTxRing::kSize);
  CHECK(ring.push(NOTEON, 1, 1) == false);   // full: dropped
  CHECK(ring.dropped == 1);
  // a continuous controller with no pending match also has nowhere to go
  CHECK(ring.push(CHAT_A, 0, 1) == false);
  CHECK(ring.dropped == 2);
  // but once its stream IS pending, updates are free forever
  ring.reset();
  ring.push(NOTEON, 60, 100);
  ring.push(CHAT_A, 0, 1);
  for (int i = 0; i < 10000; ++i) CHECK(ring.push(CHAT_A, 0, 42) == true);
  CHECK(ring.pending() == 2);
  CHECK(ring.dropped == 0);
  CHECK(ring.merged == 10000);
}

static void test_wraparound() {
  ring.reset();
  // drive the indices several times around the 8-bit space
  for (int cycle = 0; cycle < 8; ++cycle) {
    for (uint8_t i = 0; i < 100; ++i) CHECK(ring.push(NOTEON, i, 1));
    for (uint8_t i = 0; i < 100; ++i) {
      uint32_t v = 0;
      CHECK(ring.peek(v));
      CHECK(((v >> 8) & 0xFF) == i);    // FIFO order preserved across wrap
      ring.pop();
    }
    CHECK(ring.pending() == 0);
  }
  CHECK(ring.dropped == 0);
  // coalescing still works after the indices have wrapped
  ring.push(NOTEON, 60, 100);
  ring.push(CC_A, 74, 1);
  ring.push(CC_A, 74, 2);
  CHECK(ring.pending() == 2);
  CHECK(d2_at(1) == 2);
}

static void test_drain_order_after_merge() {
  ring.reset();
  ring.push(NOTEON, 60, 100);
  ring.push(CC_A, 74, 1);
  ring.push(NOTEOFF, 60, 0);
  ring.push(CC_A, 74, 99);              // merges into slot 1, keeps position
  CHECK(ring.pending() == 3);
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == NOTEON); ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == CC_A && ((v >> 16) & 0xFF) == 99); ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == NOTEOFF); ring.pop();
  CHECK(ring.pending() == 0);
}

// ---- realtime priority: clock never waits behind a controller backlog ----

static const uint8_t START = 0xFA;

static void test_promote_brings_oldest_realtime_to_head() {
  ring.reset();
  ring.push(CC_A, 1, 10);
  ring.push(CC_B, 2, 20);
  ring.push(CLOCK, 0, 0);
  CHECK(ring.promote_realtime());
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == CLOCK); ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == CC_A);  ring.pop();   // passed entries keep
  ring.peek(v); CHECK((v & 0xFF) == CC_B);  ring.pop();   // their relative order
  CHECK(ring.pending() == 0);
  CHECK(ring.promoted == 1);
}

static void test_promote_preserves_note_on_off_order() {
  // A swap instead of a rotation would put note-off before note-on: a
  // stuck note. The entries the clock passes must stay in order.
  ring.reset();
  ring.push(NOTEON, 60, 100);
  ring.push(NOTEOFF, 60, 0);
  ring.push(CLOCK, 0, 0);
  CHECK(ring.promote_realtime());
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == CLOCK);   ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == NOTEON);  ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == NOTEOFF); ring.pop();
}

static void test_promote_keeps_realtime_in_order() {
  ring.reset();
  ring.push(CC_A, 1, 1);
  ring.push(START, 0, 0);
  ring.push(CC_A, 2, 2);
  ring.push(CLOCK, 0, 0);
  CHECK(ring.promote_realtime());
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == START); ring.pop();   // the older one first
  CHECK(ring.promote_realtime());
  ring.peek(v); CHECK((v & 0xFF) == CLOCK); ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == CC_A && ((v >> 8) & 0xFF) == 1); ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == CC_A && ((v >> 8) & 0xFF) == 2); ring.pop();
}

static void test_promote_is_a_no_op_without_realtime() {
  ring.reset();
  CHECK(!ring.promote_realtime());        // empty
  ring.push(CC_A, 1, 1);
  ring.push(NOTEON, 60, 100);
  CHECK(!ring.promote_realtime());        // nothing to bring forward
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == CC_A);
  CHECK(ring.promoted == 0);
  ring.push(CLOCK, 0, 0);
  ring.pop(); ring.pop();
  CHECK(ring.promote_realtime());         // already at the head: true, no move
  CHECK(ring.promoted == 0);
}

static void test_promote_across_wraparound() {
  ring.reset();
  // walk the indices near the wrap point, then queue a backlog that spans it
  for (int i = 0; i < MidiTxRing::kSize - 2; ++i) { ring.push(NOTEON, 1, 1); ring.pop(); }
  ring.push(PROG, 7, 0);
  ring.push(NOTEON, 61, 1);
  ring.push(NOTEOFF, 61, 0);
  ring.push(CLOCK, 0, 0);
  CHECK(ring.promote_realtime());
  uint32_t v = 0;
  ring.peek(v); CHECK((v & 0xFF) == CLOCK);   ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == PROG);    ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == NOTEON);  ring.pop();
  ring.peek(v); CHECK((v & 0xFF) == NOTEOFF); ring.pop();
  CHECK(ring.pending() == 0);
}

int main() {
  test_promote_brings_oldest_realtime_to_head();
  test_promote_preserves_note_on_off_order();
  test_promote_keeps_realtime_in_order();
  test_promote_is_a_no_op_without_realtime();
  test_promote_across_wraparound();
  test_basic_queue();
  test_events_never_merge();
  test_aftertouch_coalesces();
  test_bend_and_streams_are_distinct();
  test_poly_aftertouch_keys_on_note();
  test_head_entry_is_never_rewritten();
  test_overflow_only_for_events();
  test_wraparound();
  test_drain_order_after_merge();
  printf("test_miditxring: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
