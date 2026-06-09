#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
    // total block size including header is 260 bytes
    // 252 * 260 fits nicely into two 32KB pages
    const int AUDIO_MEMORY = 252;
    AudioStream& InputStream(int interface = 0);
    AudioStream& OutputStream();
    void Init();
  }
}
