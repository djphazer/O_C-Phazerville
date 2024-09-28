#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
    AudioInputI2S2& InputStream();
    AudioOutputI2S2& OutputStream();
    void Init();
  }
}
