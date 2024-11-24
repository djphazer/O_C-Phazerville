#pragma once

#include <Audio.h>
#include "Audio/effect_dynamics.h"

namespace OC {
  namespace AudioIO {
    const int AUDIO_MEMORY = 128;
    AudioInputI2S2& InputStream();
    AudioStream& OutputStream();
    void Init();

    extern AudioEffectDynamics complimit[2];
  }
}
