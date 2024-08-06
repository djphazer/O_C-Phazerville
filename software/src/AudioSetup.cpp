#ifdef ARDUINO_TEENSY41
#include "AudioSetup.h"

AudioInputI2S2           i2s1;           //xy=58,274
AudioOutputI2S2          i2s2;           //xy=1329,266
namespace OC {
  namespace AudioDSP {
    AudioStream &input_stream() { return i2s1; }
    AudioStream &output_stream() { return i2s2; }

    void Init() {
      AudioMemory(128);
    }
    void Process(const int *values) {
    }
  }
}
#endif
