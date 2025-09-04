#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
    const int AUDIO_MEMORY = 128;
    AudioStream& InputStream(int interface = 0);
    AudioStream& OutputStream();
    void Init();
    void RecordStart();
    void RecordStop();
    size_t RecordFlush(File &file);
    int GetRecordQueueSize();
  }
}
