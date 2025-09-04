#ifdef ARDUINO_TEENSY41

#include "AudioIO.h"
#include "AudioStream.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "PhzConfig.h"

namespace OC {
  namespace AudioIO {
    AudioInputI2S2 input_stream;
    AudioOutputI2S2* output_stream = nullptr;
    AudioRecordQueue record_queue[2];
    AudioPassthrough<2> output_route;
    AudioConnection rec_conn_L{output_route, 0, record_queue[0], 0};
    AudioConnection rec_conn_R{output_route, 1, record_queue[1], 0};

    AudioConnection out_conn[2];

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
        out_conn[0].connect(output_route, 0, *output_stream, 0);
        out_conn[1].connect(output_route, 1, *output_stream, 1);
      }
      return output_route;
    }

    void Init() {
      AudioMemory(AUDIO_MEMORY);
    }

    int GetRecordQueueSize() {
      return min(record_queue[0].available(), record_queue[1].available());
    }
    void RecordStart() {
      record_queue[0].begin();
      record_queue[1].begin();
    }
    void RecordStop() {
      record_queue[0].end();
      record_queue[0].clear();
      record_queue[1].end();
      record_queue[1].clear();
    }

    size_t RecordFlush(File &file) {
      size_t written = 0;

      noInterrupts();
      // check number of packets; each packet is 128 x 16-bit samples
      if (record_queue[0].available() && record_queue[1].available()) {
        int16_t *packet_left = record_queue[0].readBuffer();
        int16_t *packet_right = record_queue[1].readBuffer();
        //if (!packet_left || !packet_right) break;
        for (int i = 0; i < 128; ++i) {
          written += file.write((uint8_t*)packet_left, 2);
          written += file.write((uint8_t*)packet_right, 2);
          ++packet_left;
          ++packet_right;
        }
        record_queue[0].freeBuffer();
        record_queue[1].freeBuffer();
      }
      interrupts();

      return written;
    }

  }
}
#endif
