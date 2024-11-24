#ifdef ARDUINO_TEENSY41

#include "AudioIO.h"
// #include <Audio.h>
#include "Audio/AudioPassthrough.h"

namespace OC {
  namespace AudioIO {
    AudioInputI2S2 input_stream;
    AudioPassthrough<2> to_mastering;
    AudioEffectDynamics complimit[2];
    AudioOutputI2S2 output_stream;

    AudioConnection leftin{to_mastering, 0, complimit[0], 0};
    AudioConnection rightin{to_mastering, 1, complimit[1], 0};
    AudioConnection leftout{complimit[0], 0, output_stream, 0};
    AudioConnection rightout{complimit[1], 0, output_stream, 1};

    AudioInputI2S2& InputStream() {
      return input_stream;
    }

    AudioStream& OutputStream() {
      return to_mastering;
    }

    void Init() {
      AudioMemory(AUDIO_MEMORY);
      for (int i = 0; i < 2; ++i) {
        complimit[i].gate(-60.0);
        complimit[i].compression(-6.0);
        complimit[i].limit(-3.0);
        complimit[i].makeupGain(0.0);
      }
    }
  }
}
#endif
