#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
    const int AUDIO_MEMORY = 128;
    AudioInputI2S2& InputStream();
    AudioStream& OutputStream();
    void Init();
  }
}
