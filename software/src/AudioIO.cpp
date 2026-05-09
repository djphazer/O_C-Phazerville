#ifdef ARDUINO_TEENSY41

#include "AudioIO.h"
#include "AudioStream.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "PhzConfig.h"
#include "Audio/AudioPassthrough.h"
#include "usb_desc.h"

namespace OC {
  namespace AudioIO {
    AudioInputI2S2 input_stream;

    AudioOutputI2S2* output_stream = nullptr;
    AudioPassthrough<2> output_route;
#ifdef AUDIO_INTERFACE
    AudioInputUSB input_usb;
    AudioOutputUSB output_usb;
    AudioConnection out_conn_usbL{output_route, 0, output_usb, 0};
    AudioConnection out_conn_usbR{output_route, 1, output_usb, 1};
#endif

    AudioConnection out_conn[2];

    AudioStream& InputStream(int interface) {
      switch (interface) {
        default:
        case 0:
          return input_stream;
#ifdef AUDIO_INTERFACE
        case 1:
          return input_usb;
#endif
      }
    }

    AudioStream& OutputStream() {
      // The output stream should be created after all other streams have been
      // created or we get an extra 3ms of latency. Hence, we initialize it
      // lazily. This is pretty hacky; if someone references this before all
      // other streams have been created it won't work. But it was the simplest
      // fix for now.
      if (output_stream == nullptr) {
        output_stream = new AudioOutputI2S2();
        out_conn[0].connect(output_route, 0, *output_stream, 0);
        out_conn[1].connect(output_route, 1, *output_stream, 1);
      }
      return output_route;
    }

    void Init() {
      AudioMemory(AUDIO_MEMORY);
    }
  }
}
#endif
