#pragma once

#include <AudioStream.h>
#include "HemisphereApplet.h"

enum AudioChannels {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet{
public:
  virtual AudioStream *InputStream() = 0;
  virtual AudioStream *OutputStream() = 0;
  virtual AudioChannels NumChannels() = 0;
};

// template <AudioChannels NumIn, AudioChannels NumOut>
// class HemisphereAudioApplet : public HemisphereApplet {
// public:
//   AudioStream &InputStream() override { return input_stream; }
//   AudioStream &OutputStream() override { return output_stream; }
//   inline AudioChannels NumInputs() { return NumIn; }
//   inline AudioChannels NumOutputs() { return NumOut; }
//
// protected:
//   AudioPassthrough<NumIn> input_stream;
//   AudioPassthrough<NumOut> output_stream;
// };
