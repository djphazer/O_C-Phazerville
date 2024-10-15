#ifdef ARDUINO_TEENSY41

#include "AudioIO.h"
// #include <Audio.h>

namespace OC {
  namespace AudioIO {
    AudioInputI2S2 input_stream;
    AudioOutputI2S2 output_stream;
    // AudioConnection left(input_stream, 0, output_stream, 0);
    // AudioConnection right(input_stream, 1, output_stream, 1);

    AudioInputI2S2& InputStream() {
      return input_stream;
    }

    AudioOutputI2S2& OutputStream() {
      return output_stream;
    }

    void Init() {
      AudioMemory(128);
    }
  }
}
#endif
