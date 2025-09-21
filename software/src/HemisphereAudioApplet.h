#pragma once

#include "HemisphereApplet.h"
#include "dsputils.h"
#include <AudioStream.h>

// For ascii strings of 9 characters or less, will just be the ascii bits
// concatenated together. More characters than that and the xor plus misaligned
// shifting should avoid collisions.
constexpr uint64_t strhash(const char* str) {
  uint64_t id = 0;
  for (const char* c = str; *c != '\0'; c++) {
    id = (id << 7) | (id >> (64 - 7));
    id ^= (*c);
  }
  return id;
}

enum AudioChannels : uint8_t {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  static const uint_fast8_t CONFIG_SIZE = 4;
  static const uint_fast8_t MAX_CABLES = 32;

  // -90 = 15bits of depth so no point in going lower
  static const int LVL_MIN_DB = -90;
  static const int LVL_MAX_DB = 90;

  AudioConnection* cables = nullptr;
  size_t cable_count;

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
      HS::PokePopup(HS::MESSAGE_POPUP, HS::MYSTERIOUS_ERROR);
      return;
    }

    cables[cable_count++].connect(source, s_ch, dest, d_ch);
  }

  void Disconnect() {
    for (int i = 0; i < cable_count; ++i) {
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
    gfxPrintIcon(HZ);
  }

  void gfxPrintDb(int db) {
    if (db < LVL_MIN_DB) gfxPrint("    - ");
    else graphics.printf("%3ddB", db);
  }
};
