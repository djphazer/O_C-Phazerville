#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
#ifdef USB_AUDIO
    const int AUDIO_MEMORY = 384;
#else
    const int AUDIO_MEMORY = 512;
#endif
    AudioStream& InputStream(int interface = 0);
    AudioStream& OutputStream();
    void Init();
  }
}
