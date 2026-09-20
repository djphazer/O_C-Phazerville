// quad_capture_midi.h
//
// USB-MIDI SysEx transport for the Quadrants external display, for hosts that
// can't use USB serial: iPadOS/iOS (native app) and Android (Chrome Web MIDI —
// Web Serial doesn't exist on mobile Chrome). Mirrors the serial commands over
// SysEx so the same views work everywhere.
//
// Protocol (manufacturer 0x7D "non-commercial", sub-id 0x7A "quad capture"):
//   request  (host -> module):  F0 7D 7A <cmd> [args...] F7
//   replies  (module -> host):  F0 7D 7A <cmd> <payload> F7
//
//   cmd 'Q' : no args    -> 4096 nibbles (2048 B, 128x128 quad view)
//   cmd 'A' : no args    -> 2048 nibbles (1024 B, 128x64 audio stack)
//   cmd 'V' : no args    -> <l7> <r7> (input levels 0..127)
//   cmd 'T' : no args    -> <left> <right> <full+1> <preset+1> (0 = none)
//   cmd 'O' : slot,src,win (ASCII, same as serial) -> 1024 nibbles (512 B scope)
//   cmd 'N' : no args    -> 384 nibbles (192 B = 32 MIDI-monitor events)
//   cmd '~' : selector   -> no reply (remote control inject, same selectors)
//
// Each payload byte is sent as two 7-bit nibbles (hi, lo) to respect SysEx's
// 7-bit rule; short numeric replies ('V','T') send 7-bit values directly.
// Requests are latched by the SysEx handler (piggybacking on Quadrants'
// usbMIDI.read() pump — musical MIDI untouched); render + send happens in the
// main loop via service(), never inside the read callback.

#pragma once

#include <Arduino.h>
#include "quad_capture.h"

namespace QuadMidi {

static constexpr uint8_t MFR = 0x7D;   // non-commercial manufacturer id
static constexpr uint8_t SUB = 0x7A;   // our "quad capture" sub-id

static volatile char pending = 0;                 // request cmd awaiting service
static volatile uint8_t args[3] = { 0, 0, 0 };    // request args ('O' slot/src/win, '~' sel)
static DMAMEM uint8_t frame[2048] __attribute__((aligned(4)));   // render scratch
static DMAMEM uint8_t out[4 + 4096 + 1];                 // header + max nibbles + F7

// SysEx handler — keep it tiny; just latch the request.
inline void onSysEx(uint8_t *data, unsigned size) {
  if (size >= 5 && data[1] == MFR && data[2] == SUB) {
    args[0] = size > 5 ? data[4] : 0;
    args[1] = size > 6 ? data[5] : 0;
    args[2] = size > 7 ? data[6] : 0;
    pending = (char)data[3];
  }
}

inline void begin() {
  usbMIDI.setHandleSystemExclusive(onSysEx);
}

FLASHMEM __attribute__((noinline))
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

// Cold body (a few requests/s at most): FLASHMEM + noinline keeps it out of
// ITCM, which is allocated in 32 KB blocks and sits right at the RAM1 limit.
FLASHMEM __attribute__((noinline))
static void servicePending(void (*remote)(int)) {
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
  } else if (c == 'T') {
    int l = 0, r = 0, f = -1, pre = -1;
    if (QuadCapture_ControlState(l, r, f, pre)) {
      uint8_t v[9] = {
        0xF0, MFR, SUB, 'T',
        (uint8_t)(l & 0x7F), (uint8_t)(r & 0x7F),
        (uint8_t)((f + 1) & 0x7F), (uint8_t)((pre + 1) & 0x7F),
        0xF7
      };
      usbMIDI.sendSysEx(9, v);
      usbMIDI.send_now();
    }
  } else if (c == 'O') {
    if (QuadCapture_ScopeCapture((char)args[0], (char)args[1], (char)args[2], frame))
      sendNibblesFrame('O', frame, 512);
  } else if (c == 'N') {
    if (QuadCapture_MidiLogPack(frame)) sendNibblesFrame('N', frame, 192);
  } else if (c == '~') {
    if (remote) remote((int)args[0]);
  }
}

// Call once per main-loop iteration; the hot path is just this check.
inline void service(void (*remote)(int) = nullptr) {
  if (pending) servicePending(remote);
}

} // namespace QuadMidi
