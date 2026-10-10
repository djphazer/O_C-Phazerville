#pragma once

#include "HemisphereApplet.h"
#include "dsputils.h"
#include "Audio/effect_reverb_schroeder_F32.h"
#include "Audio/effect_freeverb_F32.h"
#include "Audio/effect_dynamics_F32.h"
#include "src/Audio/effect_dynamics.h"
#include <AudioStream.h>

enum AudioChannels : uint8_t {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:

  // F32 reverbs: only the F32 Reverb/Bungverb applets draw from these pools
  static Factory<AudioEffectReverbSchroederF32, 8> bung_factory;
  static Factory<AudioEffectFreeverbF32, 8> verb_factory;
  // int16 pool stays for ThreeBandz; the F32 Dynamics applet has its own pool
  // (10 = every slot running stereo Dynamics). Factory allocates lazily, so
  // the second pool costs nothing until instances are actually used.
  static Factory<AudioEffectDynamics, 20> compressor_factory;
  static Factory<AudioEffectDynamicsF32, 10> compressor_f32_factory;

  static const uint_fast8_t CONFIG_SIZE = 4;
  static const uint_fast8_t MAX_CABLES = 32;

  // -90 = 15bits of depth so no point in going lower
  static const int LVL_MIN_DB = -90;
  static const int LVL_MAX_DB = 90;

  AudioConnection* cables = nullptr;
  size_t cable_count;
  size_t slot_index = 0;
  void SetSlot(size_t slot) {
    slot_index = slot;
  }

  // If applet_name() can return different things at different times, you
  // *must* override this or saving and loading won't work!
  virtual const uint64_t applet_id() {
    return strhash(applet_name());
  };
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;
  virtual void mainloop() {}

  virtual void OnDataReceive(uint64_t data) {
    Serial.println(
      "Warning: default OnDataReceive() called; either override this or OnDataReceive(const std::<array<uint64_t, CONFIG_SIZE>& data)"
    );
  }
  virtual uint64_t OnDataRequest() {
    Serial.println(
      "Warning: default OnDataRequest() called; either override this or OnDataRequest(std::<array<uint64_t, CONFIG_SIZE>& data)"
    );
    return 0;
  }
  virtual void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) {
    OnDataReceive(data[0]);
  }
  virtual void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) {
    data[0] = OnDataRequest();
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
  }

  virtual void Unload() override {
    // always all restart, so virtual PatchCable's can be reconnected
    AllowRestart();
  }

  // call this from Start() to connect objects together
  void PatchCable(AudioStream &source, uint8_t s_ch, AudioStream &dest, uint8_t d_ch) {
    if (!cables) cables = new AudioConnection[MAX_CABLES];

    // TODO: we need a static_assert, if possible... or use a vector instead
    if (cable_count >= MAX_CABLES) {
      HS::PokePopup(HS::MESSAGE_POPUP, "AUDIO MAXED!!");
      return;
    }

    cables[cable_count++].connect(source, s_ch, dest, d_ch);
  }

  AudioEffectReverbSchroederF32 *GetBungverb() {
    return bung_factory.get();
  }
  void ReleaseBungverb(AudioEffectReverbSchroederF32* verb) {
    bung_factory.release(verb);
  }

  AudioEffectFreeverbF32 *GetFreeverb() {
    return verb_factory.get();
  }
  void ReleaseFreeverb(AudioEffectFreeverbF32* verb) {
    verb_factory.release(verb);
  }

  AudioEffectDynamics *GetComp() {
    return compressor_factory.get();
  }
  void ReleaseComp(AudioEffectDynamics* comp) {
    compressor_factory.release(comp);
  }

  AudioEffectDynamicsF32 *GetCompF32() {
    return compressor_f32_factory.get();
  }
  void ReleaseCompF32(AudioEffectDynamicsF32* comp) {
    compressor_f32_factory.release(comp);
  }

  virtual void Disconnect() {
    for (size_t i = 0; i < cable_count; ++i) {
      cables[i].disconnect();
    }
    cable_count = 0;
  }

  void gfxPrintTuningIndicator(int16_t pitch) {
    // TODO this assumes pitch = C, which might not be true for some applets
    int semitone = pitch / 128;
    int offset = pitch - semitone * 128;
    if (offset >= 64) {
      offset = offset - 128;
      semitone++;
    }
    semitone = ((semitone % 12) + 12) % 12;
    int y = gfxGetPrintPosY();
    int x = gfxGetPrintPosX();
    gfxPos(x + 1, y);
    gfxPrintIcon(NOTE_NAMES + semitone * 8, 9);
    int pxOffset = 7 - (offset / 16 + 4);
    if (offset == 0) gfxInvert(x, y, 10, 8);
    else gfxDottedLine(x, y + pxOffset, x + 10, y + pxOffset);
  }

  void gfxPrintPitchHz(int16_t pitch, float base_freq = C3) {
    float freq = PitchToRatio(pitch) * base_freq;
    int shiftedFreq = static_cast<int>(roundf(freq * 10));
    int int_part = shiftedFreq / 10;
    int dec = shiftedFreq % 10;
    if (int_part > 9999) graphics.printf("%6d", int_part);
    else graphics.printf("%4d.%01d", int_part, dec);
    gfxPrintIcon(HZ_ICON);
  }

  void gfxPrintDb(int db) {
    if (db < LVL_MIN_DB) gfxPrint("   - ");
    else graphics.printf("%3ddB", db);
  }
};
