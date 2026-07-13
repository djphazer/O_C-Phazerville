// quad_midilog.h
//
// MIDI monitor ring for the external Big Display viewer ('N' command).
// Logs both directions:
//   IN  — every message the Quadrants MIDI pump receives (all devices:
//         USB device, both USB hosts, TRS serial), except Clock / Active
//         Sensing / SysEx (SysEx is the display transport; Clock would flood).
//   OUT — everything sent through the MIDIFrame Send* wrappers (note on/off,
//         CC, pitch bend, aftertouch) plus MIDI-thru traffic, except Clock.
//
// Wire format ('N', Quadrants only): one line
//   "N:" + 32 events x 12 uppercase hex chars + "\n"
// Each event: seq(4) message(2) dirchan(2) d1(2) d2(2). seq is a rolling
// 16-bit counter (0 = empty slot; the host tracks the highest seq seen and
// renders only newer events, wraparound-aware). dirchan: bit 7 = direction
// (1 = out), low 7 bits = MIDI channel 1..16.
//
// push() runs in hot paths (MIDI pump in mainloop, Send* in the app ISR), so
// it stays in ITCM and is branch-light. The printer is FLASHMEM (Quadrants.h).
//
// This header is included ONLY from apps/Quadrants.h (single TU) and only
// when QUAD_CAPTURE is defined. HSIOFrame.h calls QuadMidiLog_Push via a
// prototype; the definition below has external linkage.

#pragma once

#include <Arduino.h>

namespace QuadMidiLog {

static constexpr size_t kEvents = 32;

struct Ev {
  uint16_t seq;       // rolling counter, 0 = empty
  uint8_t message;    // raw MIDI status (0x80/0x90/0xB0/0xC0/0xD0/0xE0/0xFx)
  uint8_t dirchan;    // bit7 = out, low 7 = channel 1..16 (0 for system msgs)
  uint8_t d1, d2;
};

static Ev ring[kEvents];
static volatile uint16_t seq = 0;

} // namespace QuadMidiLog

// External-linkage push so HSIOFrame.h's Send* wrappers can call it via a
// bare prototype without including this header. Deliberately NOT inline: the
// definition must be emitted exactly once (this header is single-include via
// Quadrants.h) so other translation units (HSIOFrame.cpp) can link to it.
void QuadMidiLog_Push(bool out, uint8_t message, uint8_t channel,
                      uint8_t d1, uint8_t d2) {
  using namespace QuadMidiLog;
  uint16_t s = (uint16_t)(seq + 1);
  if (s == 0) s = 1;             // skip 0 on wrap (0 marks empty slots)
  seq = s;
  Ev &e = ring[s % kEvents];
  e.message = message;
  e.dirchan = (uint8_t)((out ? 0x80 : 0) | (channel & 0x7F));
  e.d1 = d1;
  e.d2 = d2;
  e.seq = s;                     // seq written last: slot valid once visible
}
