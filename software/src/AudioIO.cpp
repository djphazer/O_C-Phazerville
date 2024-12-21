#pragma once
#ifdef ARDUINO_TEENSY41

  #include "AudioIO.h"

namespace OC {
  namespace AudioIO {
    AudioInputI2S2 input_stream;
    AudioOutputI2S2* output_stream = nullptr;

    AudioInputI2S2& InputStream() {
      return input_stream;
    }

    AudioStream& OutputStream() {
      // The output stream should be created after all other streams have been
      // created or we get an extra 3ms of latency. Hence, we initialize it
      // lazily. This is pretty hacky; if someone references this before all
      // other streams have been created it won't work. But it was the simplest
      // fix for now.
      if (output_stream == nullptr) {
        output_stream = new AudioOutputI2S2();
      }
      return *output_stream;
    }

    void Init() {
      AudioMemory(AUDIO_MEMORY);
    }
  }
}
#endif
