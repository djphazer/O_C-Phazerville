#pragma once
// Float32-native audio applets. An F32 applet processes float internally
// (full precision in feedback paths, no int16 truncation between its stages)
// while still presenting the int16 InputStream()/OutputStream() interface to
// the chain via edge converters, so AudioAppletSubapp's wiring is unchanged.
// Cross-applet float linking (skipping the edge converters between adjacent
// F32 applets) is a later chain-level optimization.

#ifdef ARDUINO_TEENSY41

#include "HemisphereAudioApplet.h"
#include "extern/f32/AudioStream_F32.h"
#include "extern/f32/AudioConvert_F32.h"

// N-channel int16 -> float32 edge adapter (one object, N lanes)
template <uint8_t N>
class AudioConvertI16toF32Multi : public AudioStream_F32 {
public:
  AudioConvertI16toF32Multi() : AudioStream_F32(0, nullptr, N, iq) {}
  void update(void) override {
    for (uint8_t i = 0; i < N; i++) {
      audio_block_t* in = AudioStream::receiveReadOnly(i);
      if (!in) continue;
      audio_block_f32_t* out = AudioStream_F32::allocate_f32();
      if (out) {
        AudioConvert_I16toF32::convertAudio_I16toF32(in, out, out->length);
        AudioStream_F32::transmit(out, i);
        AudioStream_F32::release(out);
      }
      AudioStream::release(in);
    }
  }
private:
  audio_block_t* iq[N];
};

// N-channel float32 -> int16 edge adapter
template <uint8_t N>
class AudioConvertF32toI16Multi : public AudioStream_F32 {
public:
  AudioConvertF32toI16Multi() : AudioStream_F32(N, fq) {}
  void update(void) override {
    for (uint8_t i = 0; i < N; i++) {
      audio_block_f32_t* in = receiveReadOnly_f32(i);
      if (!in) continue;
      audio_block_t* out = AudioStream::allocate();
      if (out) {
        AudioConvert_F32toI16::convertAudio_F32toI16(in, out, in->length);
        AudioStream::transmit(out, i);
        AudioStream::release(out);
      }
      AudioStream_F32::release(in);
    }
  }
private:
  audio_block_f32_t* fq[N];
};

template <AudioChannels Channels>
class HemisphereAudioAppletF32 : public HemisphereAudioApplet {
public:
  // Matches the int16 base's MAX_CABLES; the stereo Delay applet alone needs
  // 28 F32 cables. Pool is heap-allocated on first PatchCableF32().
  static const uint_fast8_t MAX_CABLES_F32 = 32;

  AudioStream* InputStream() override {
    return &input_adapter;
  }
  AudioStream* OutputStream() override {
    return &output_adapter;
  }
  // F32-side endpoints for the applet's internal graph: patch the internal
  // processing from InputF32() (channels 0..Channels-1) into OutputF32().
  AudioStream_F32& InputF32() {
    return input_adapter;
  }
  AudioStream_F32& OutputF32() {
    return output_adapter;
  }

  void PatchCableF32(AudioStream_F32& source, uint8_t s_ch, AudioStream_F32& dest, uint8_t d_ch) {
    if (!cables_f32) cables_f32 = new AudioConnection_F32[MAX_CABLES_F32];
    if (cable_f32_count >= MAX_CABLES_F32) {
      HS::PokePopup(HS::MESSAGE_POPUP, "AUDIO MAXED!!");
      return;
    }
    cables_f32[cable_f32_count++].connect(source, s_ch, dest, d_ch);
  }

  void Disconnect() override {
    HemisphereAudioApplet::Disconnect();
    for (size_t i = 0; i < cable_f32_count; ++i) {
      cables_f32[i].disconnect();
    }
    cable_f32_count = 0;
  }

private:
  AudioConvertI16toF32Multi<Channels> input_adapter;
  AudioConvertF32toI16Multi<Channels> output_adapter;
  AudioConnection_F32* cables_f32 = nullptr;
  size_t cable_f32_count = 0;
};

#endif // ARDUINO_TEENSY41
