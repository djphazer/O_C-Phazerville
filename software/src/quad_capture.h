// quad_capture.h
//
// External screen capture for Phazerville Quadrants (Teensy 4.1 / ORN8) over
// USB Serial. Extends the stock capture with three extra views that render
// WITHOUT disturbing the live OLED, then stream in the stock ASCII-hex format:
//
//   'Q'  -> 128x128 (2048-byte) frame: all four CV applets, NW|NE over SW|SE
//   'A'  -> 128x64  (1024-byte) frame: the audio DSP stack (audio_app.View)
//   'M'  -> 128x64  (1024-byte) frame: the MIDI map page   (DrawMidiMaps)
//
// Any other byte still triggers the stock 1024-byte live-OLED capture, so
// Paul Stoffregen's tool / the two-up viewer keep working unchanged.
//
// Capture is USB *Serial* (CDC), not HID. Requires a Serial-inclusive USB type
// (-DUSB_MIDI_SERIAL or -DUSB_MIDI_AUDIO_SERIAL).

#pragma once

#include <Arduino.h>

// Implemented in apps/Quadrants.h (they need the live app / audio instances).
// Each renders into the supplied buffer and returns true, or returns false and
// leaves the buffer untouched if Quadrants is not the current app.
bool QuadCapture_Render(uint8_t *frame2048);       // 2048 bytes (128x128)
bool QuadCapture_RenderAudio(uint8_t *frame1024);  // 1024 bytes (128x64)
bool QuadCapture_RenderMidi(uint8_t *frame1024);   // 1024 bytes (128x64)
bool QuadCapture_InputLevels(float &l, float &r);  // L/R input peaks (0..1)
bool QuadCapture_SwitchToSlot(int slot);           // activate quadrant 0..3
bool QuadCapture_ControlState(int &l, int &r, int &full, int &preset); // + current preset
bool QuadCapture_ChangeApplet(int dir);            // focused slot prev/next applet
bool QuadCapture_ChangePreset(int dir);            // prev/next preset
bool QuadCapture_SetAudioView(bool on);            // enter/leave Audio Setup view
bool QuadCapture_SavePreset(int slot);             // save current state to slot 0..31
bool QuadCapture_BackupBank();                     // stream the whole bank to host
bool QuadCapture_ScopeCapture(char slot, char sel, char win, uint8_t *dst512); // scope snapshot ('O')
void QuadCapture_ScopeDebug();                     // 'O' <any> '?' <any>: print DBG state line

namespace QuadCapture {

static constexpr size_t kMaxFrame = 128 * 128 / 8; // 2048 (largest we send)

static uint8_t buffer[kMaxFrame] __attribute__((aligned(4)));
static bool    sending    = false;
static size_t  idx        = 0;
static size_t  frame_len  = 0;   // bytes to stream for the current frame

// --- request helpers: render now and arm the streamer -----------------------
// If a frame is still streaming, IGNORE the new request instead of re-arming:
// re-arming mid-frame would splice two replies into one endless malformed line
// and permanently desync the host. Dropping lets the in-flight line finish
// cleanly (host times out once, then realigns on the next poll).

static inline void request() {           // 'Q' : 4-up 128x128
  if (sending) return;
  if (QuadCapture_Render(buffer)) { idx = 0; frame_len = 2048; sending = true; }
}

static inline void requestAudio() {      // 'A' : audio DSP stack 128x64
  if (sending) return;
  if (QuadCapture_RenderAudio(buffer)) { idx = 0; frame_len = 1024; sending = true; }
}

static inline void requestMidi() {       // 'M' : MIDI map 128x64
  if (sending) return;
  if (QuadCapture_RenderMidi(buffer)) { idx = 0; frame_len = 1024; sending = true; }
}

static inline void requestScope(char slot, char sel, char win) {
  // 'O' <slot 'A'|'B'> <sel> <win> : scope snapshot, 512 B.
  // sel: '1'..'8' = CV out A..H, 'L'/'R' = audio out; win: '0'..'3' time base.
  // Reply is 256 big-endian int16 samples (CV in millivolts, audio raw PCM)
  // via the same hex streamer.
  if (sending) return;
  if (QuadCapture_ScopeCapture(slot, sel, win, buffer)) { idx = 0; frame_len = 512; sending = true; }
}

// Emit up to chunk_size bytes of the armed frame as ASCII hex, matching the
// stock wire format (two upper-case hex chars/byte, newline + flush at end).
static inline void service(size_t chunk_size = 32) {
  if (!sending) return;

  const uint8_t *p = buffer + idx;
  for (size_t i = 0; i < chunk_size; i++) {
    uint8_t n = *p++;
    if (n < 16) Serial.print("0");
    Serial.print(n, HEX);

    if (++idx >= frame_len) {
      Serial.println();
      Serial.flush();
      idx = 0;
      sending = false;
      break;
    }
  }
}

static inline bool busy() { return sending; }

} // namespace QuadCapture
