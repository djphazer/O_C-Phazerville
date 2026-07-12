// quad_capture_midi.h
//
// USB-MIDI SysEx transport for the Quadrants external display, for hosts that
// can't use USB serial (notably iPadOS/iOS, which exposes USB MIDI to apps but
// not USB CDC serial). It mirrors the serial 'Q'/'A'/'V' commands over SysEx so
// a native iPad app can drive the same views.
//
// Protocol (manufacturer 0x7D "non-commercial", sub-id 0x7A "quad capture"):
//   request  (host -> module):  F0 7D 7A <cmd> F7          cmd = 'Q' | 'A' | 'V'
//   reply Q  (module -> host):  F0 7D 7A 'Q' <4096 nibbles> F7   -> 2048B 128x128
//   reply A  (module -> host):  F0 7D 7A 'A' <2048 nibbles> F7   -> 1024B 128x64
//   reply V  (module -> host):  F0 7D 7A 'V' <l7> <r7> F7        -> L/R 0..127
//
// Each 8-bit framebuffer byte is sent as two 7-bit nibbles (hi, lo) so the data
// stays inside SysEx's 7-bit rule. Requests are captured by a SysEx handler
// that piggybacks on Quadrants' existing usbMIDI.read() pump (musical MIDI is
// untouched — SysEx is already ignored there); the actual render + send happens
// in the main loop via service(), never inside the read callback.

#pragma once

#include <Arduino.h>
#include "quad_capture.h"

namespace QuadMidi {

static constexpr uint8_t MFR = 0x7D;   // non-commercial manufacturer id
static constexpr uint8_t SUB = 0x7A;   // our "quad capture" sub-id

static volatile char pending = 0;                 // request cmd awaiting service
static uint8_t frame[2048] __attribute__((aligned(4)));   // render scratch
static uint8_t out[4 + 4096 + 1];                 // header + max nibbles + F7

// SysEx handler — keep it tiny; just latch the requested command.
inline void onSysEx(uint8_t *data, unsigned size) {
  if (size >= 5 && data[1] == MFR && data[2] == SUB)
    pending = (char)data[3];
}

inline void begin() {
  usbMIDI.setHandleSystemExclusive(onSysEx);
}

inline void sendNibblesFrame(char cmd, const uint8_t *buf, size_t n) {
  size_t k = 0;
  out[k++] = 0xF0; out[k++] = MFR; out[k++] = SUB; out[k++] = (uint8_t)cmd;
  for (size_t i = 0; i < n; i++) {
    out[k++] = (buf[i] >> 4) & 0x0F;
    out[k++] = buf[i] & 0x0F;
  }
  out[k++] = 0xF7;
  usbMIDI.sendSysEx(k, out);
  usbMIDI.send_now();
}

// Call once per main-loop iteration. Renders + sends the pending frame, if any.
inline void service() {
  char c = pending;
  if (!c) return;
  pending = 0;

  if (c == 'Q') {
    if (QuadCapture_Render(frame)) sendNibblesFrame('Q', frame, 2048);
  } else if (c == 'A') {
    if (QuadCapture_RenderAudio(frame)) sendNibblesFrame('A', frame, 1024);
  } else if (c == 'V') {
    float l = 0.f, r = 0.f;
    if (QuadCapture_InputLevels(l, r)) {
      uint8_t v[7] = {
        0xF0, MFR, SUB, 'V',
        (uint8_t)(constrain(l, 0.f, 1.f) * 127.f),
        (uint8_t)(constrain(r, 0.f, 1.f) * 127.f),
        0xF7
      };
      usbMIDI.sendSysEx(7, v);
      usbMIDI.send_now();
    }
  }
}

} // namespace QuadMidi
