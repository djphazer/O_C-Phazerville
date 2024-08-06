#pragma once

#include "HemisphereApplet.h"
#include "AudioPassthrough.h"

template<AudioChannels N>
class HemisphereAudioApplet : public HemisphereApplet {
public:
  optional_ref<AudioStream> InputStream() override { return input_stream; }
  optional_ref<AudioStream> OutputStream() override { return output_stream; }
  AudioChannels NumAudioChannels() override { return N; }

protected:
  AudioPassthrough<N> input_stream;
  AudioPassthrough<N> output_stream;
};

