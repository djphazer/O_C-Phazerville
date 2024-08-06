#pragma once

#include <Audio.h>
#include <Wire.h>
// --- Audio-related functions accessible from elsewhere in the O_C codebase
namespace OC {
  namespace AudioDSP {

    AudioStream &input_stream();
    AudioStream &output_stream();

    void Init();

    void Process(const int *values);

  } // AudioDSP namespace
} // OC namespace
