// quad_scope.h
//
// Basic dual oscilloscope capture for the external Big Display viewer
// ('O' command). Two independent channels (slots A/B), each with a ring of
// recent waveform samples from ONE selected source:
//
//   CV outs 1..8  : the raw DAC codes written each ioframe tick, converted to
//                   millivolts via the channel's calibration (zero offset +
//                   one-volt step). Logical outs A..H go through the
//                   runtime-remapped DAC_CHANNEL_A..H globals (on ORN8/T41
//                   hardware A..D are physical 4..7).
//   Audio out L/R : an AudioStream tap on whatever stream currently feeds the
//                   main audio output.
//
// Time base: each slot has a window setting 0..3 that scales the decimation
// by 1/2/4/8. 256-sample window lengths:
//   CV    (base 16.67 kHz / 16 ~= 1042 Hz): ~0.25 s / 0.5 s / 1 s / 2 s
//   audio (base 44.1 kHz  /  2 ~= 22 kHz):  ~11.6 / 23 / 46 / 93 ms
//
// Wire format (see PROTOCOL.md): host sends 'O' <slot 'A'|'B'> <src> <win>;
// the snapshot streams via QuadCapture's chunked hex sender as 512 bytes =
// 256 big-endian int16 samples. CV samples are millivolts; audio raw PCM.
//
// This header is included ONLY from apps/Quadrants.h (single TU via
// OC_apps.cpp) and only when QUAD_CAPTURE is defined.

#pragma once

#include <Arduino.h>
#include <Audio.h>
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "OC_digital_inputs.h"
#include "OC_io.h"
#include "AudioIO.h"

namespace QuadScope {

static constexpr size_t kSamples = 256;     // ring size = one wire snapshot
static constexpr uint32_t kCVDecim = 16;    // base decim, window 0
static constexpr uint32_t kAudioDecim = 2;  // base decim, window 0

// Source codes: 0..7 = CV outs A..H (logical), 8/9 = audio out L/R,
// 10..17 = CV ins 1..8, 18..21 = trigger ins 1..4, 22/23 = audio in L/R,
// -1 = none yet.
static constexpr int8_t AUDIO_L = 8, AUDIO_R = 9;
static constexpr int8_t CVIN_BASE = 10, TR_BASE = 18;
static constexpr int8_t AUDIO_IN_L = 22, AUDIO_IN_R = 23;

// True only for the four audio-tap sources. NOTE: a plain `>= AUDIO_L` test
// here is wrong — CV-in/trigger codes are higher than AUDIO_L, and a stale
// audio tap would flood their rings with 22 kHz silence (that bug made CV-in
// traces read dead flat).
static inline bool is_audio_source(int8_t s) {
  return s == AUDIO_L || s == AUDIO_R || s == AUDIO_IN_L || s == AUDIO_IN_R;
}

static inline DAC_CHANNEL physical(int logical) {
  const DAC_CHANNEL map[8] = {
    DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_C, DAC_CHANNEL_D,
    DAC_CHANNEL_E, DAC_CHANNEL_F, DAC_CHANNEL_G, DAC_CHANNEL_H,
  };
  return map[logical & 7];
}

static inline ADC_CHANNEL physical_adc(int logical) {
  const ADC_CHANNEL map[8] = {
    ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4,
    ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8,
  };
  return map[logical & 7];
}

struct Chan {
  volatile int8_t source = -1;   // -1 forces a Select on the first request
  int8_t phys = 0;               // physical DAC index for the CV source
  uint8_t win = 0;               // time window 0..3 -> decim << win
  int32_t cv_zero = 0, cv_step = 1;
  int16_t ring[kSamples];
  volatile uint16_t widx = 0;
  uint32_t phase = 0;

  void push(int16_t v) {
    uint16_t w = widx;
    ring[w] = v;
    widx = (uint16_t)((w + 1) % kSamples);
  }

  // Everything below except Sample()/push() is a cold path (host requests,
  // a few Hz) — FLASHMEM keeps it out of ITCM (32 KB-block allocated).
  FLASHMEM void clear() {
    for (size_t i = 0; i < kSamples; i++) ring[i] = 0;
    widx = 0;
    phase = 0;
  }

  // Copy the ring oldest-first into dst. Unsynchronized with the writer;
  // worst case is a one-sample seam, fine for a monitoring scope.
  FLASHMEM void snapshot(int16_t *dst) {
    uint16_t w = widx;
    for (size_t i = 0; i < kSamples; i++) dst[i] = ring[(w + i) % kSamples];
  }

  // Millivolt conversion cached at select time: mv = (dac-zero)*1000/step.
  FLASHMEM void SelectCV(int ch, uint8_t w) {
    const DAC_CHANNEL pch = physical(ch);
    cv_zero = (int32_t)OC::DAC::get_zero_offset(pch);
    cv_step = (int32_t)OC::DAC::get_octave_offset(pch, 1) - cv_zero;
    if (cv_step == 0) cv_step = 1;   // uncalibrated safety
    phys = (int8_t)pch;
    win = w;
    source = (int8_t)ch;
    clear();
  }

  FLASHMEM void SelectAudioSrc(int8_t code, uint8_t w) { // AUDIO_L/R or AUDIO_IN_L/R
    if (source == code && win == w) return;
    win = w;
    source = code;
    clear();
  }

  FLASHMEM void SelectCVIn(int ch, uint8_t w) {    // 0..7 = CV in 1..8
    phys = (int8_t)physical_adc(ch);
    win = w;
    source = (int8_t)(CVIN_BASE + ch);
    clear();
  }

  FLASHMEM void SelectTR(int ch, uint8_t w) {      // 0..3 = trigger in 1..4
    win = w;
    source = (int8_t)(TR_BASE + ch);
    clear();
  }

  // Called every ioframe tick (~16.67 kHz) from AppQuadrants::Process.
  // CV outs read the DAC code actually written to the jack (post
  // pitch/gate/uni conversion) so every output mode displays correctly;
  // CV ins use the frame's calibrated pitch value (1536 per volt); trigger
  // ins render as a 0 / 5 V gate trace.
  void Sample(OC::IOFrame *ioframe) {
    const int8_t src = source;
    if (src < 0 || is_audio_source(src)) return;
    if (++phase < (kCVDecim << win)) return;
    phase = 0;
    int32_t mv;
    if (src < AUDIO_L) {          // CV out A..H
      mv = ((int32_t)OC::DAC::value(phys) - cv_zero) * 1000 / cv_step;
    } else if (src < TR_BASE) {   // CV in 1..8
      mv = ioframe->cv.pitch_values[phys] * 1000 / 1536;
    } else {                      // trigger in 1..4
      mv = ioframe->digital_inputs.raised((OC::DigitalInput)(src - TR_BASE)) ? 5000 : 0;
    }
    if (mv > 32767) mv = 32767; else if (mv < -32768) mv = -32768;
    push((int16_t)mv);
  }
};

static Chan chans[2];

static inline void Sample(OC::IOFrame *ioframe) {
  chans[0].Sample(ioframe);
  chans[1].Sample(ioframe);
}

// --- audio outs ----------------------------------------------------------------
// A passive tap per slot: receives blocks from the stream feeding the main
// audio output and copies decimated samples into its channel's ring. Runs in
// the audio ISR.
class ScopeTap : public AudioStream {
public:
  explicit ScopeTap(Chan *c) : AudioStream(1, in_queue), owner(c) {}
  void update() override {
    audio_block_t *b = receiveReadOnly(0);
    if (!b) return;
    Chan *c = owner;
    if (c && is_audio_source(c->source)) {
      const int step = (int)(kAudioDecim << c->win);
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i += step) c->push(b->data[i]);
    }
    release(b);
  }
private:
  Chan *owner;
  audio_block_t *in_queue[1];
};

static ScopeTap taps[2] = { ScopeTap(&chans[0]), ScopeTap(&chans[1]) };
static AudioConnection tap_conns[2];
static AudioStream *tap_srcs[2] = { nullptr, nullptr };
static int tap_chs[2] = { -1, -1 };

// Keep a slot's tap on whatever stream currently feeds the main output.
// Called on every audio-scope read (a few Hz), so it follows applet changes.
FLASHMEM static void RetargetAudio(int slot, AudioStream *stream, int channel) {
  if (!stream) return;
  if (stream == tap_srcs[slot] && channel == tap_chs[slot]) return;
  // Re-patching the audio graph must never race the audio ISR while it walks
  // the connection lists — that's a hard fault (module freezes, USB drops).
  AudioNoInterrupts();
  if (tap_srcs[slot]) tap_conns[slot].disconnect();  // never disconnect a virgin connection
  tap_conns[slot].connect(*stream, channel, taps[slot], 0);
  AudioInterrupts();
  tap_srcs[slot] = stream;
  tap_chs[slot] = channel;
}

FLASHMEM static void Untap(int slot) {
  if (!tap_srcs[slot]) return;
  AudioNoInterrupts();
  tap_conns[slot].disconnect();
  AudioInterrupts();
  tap_srcs[slot] = nullptr;
  tap_chs[slot] = -1;
}

// The stream currently feeding each side of the main audio output, kept fresh
// by QuadScopeTapOutput() below every time the audio stack re-patches.
static AudioStream *out_stream[2] = { nullptr, nullptr };
static int out_ch[2] = { 0, 0 };

} // namespace QuadScope

// Called by AudioAppletSubapp::Connect{Mono,Stereo}ToNext whenever the final
// stage of the audio stack is (re)wired — i.e., at audio init and on every
// applet/preset change. Applet output streams come and go, so a tap left
// connected to an old stream sits as a stale entry in connection lists being
// rebuilt (that was crashing the module on applet changes). This hook moves
// live audio-out taps to the new stream and releases stale ones, in the same
// breath as the stack re-patches its own meters. Audio-IN taps sit on the
// permanent input object and are never stale.
FLASHMEM void QuadScopeTapOutput(int side, AudioStream *stream, int channel) {
  using namespace QuadScope;
  out_stream[side] = stream;
  out_ch[side] = channel;
  AudioStream *input = &OC::AudioIO::InputStream();
  for (int s = 0; s < 2; s++) {
    const int8_t src = chans[s].source;
    if (src == AUDIO_L + side) {
      RetargetAudio(s, stream, channel);                 // follow the new stream
    } else if (tap_srcs[s] && tap_srcs[s] != input       // connected to an applet stream…
               && src != AUDIO_L + (1 - side)) {         // …and not the other side's live tap
      Untap(s);                                          // stale — release it now
    }
  }
}
