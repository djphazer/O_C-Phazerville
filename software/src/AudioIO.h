#pragma once

#include <Audio.h>
#include "extern/f32/AudioStream_F32.h"

namespace OC {
  namespace AudioIO {
    // total block size including header is 260 bytes
    // 252 * 260 fits nicely into two 32KB pages
    const int AUDIO_MEMORY = 252;
    // float32 audio pool ~528 bytes/block
    // The Delay applet alone can hold ~10 blocks per channel in flight (8 taps + I/O).
    const int F32_AUDIO_MEMORY = 128;

    constexpr uint8_t kOutputRouteChannels = 2;
    constexpr uint8_t kOutputRouteSources = 4;
    constexpr uint8_t kOutputRouteTweightySlot = 1;
    constexpr uint8_t kOutputRouteSamplerSlot = 2;
    constexpr uint8_t kOutputRouteEffectSlot = 3;

    AudioStream_F32& InputStream(int interface = 0);
    AudioStream_F32& OutputStream();
    void Init();
  }
}
