#pragma once

#include "HemisphereApplet.h"
#include <AudioStream.h>

struct ParamInput {
  char const* name;
  int8_t source = 0;

  virtual int8_t NumInputs() const = 0;
  virtual char const* InputName() const = 0;

  void ChangeSource(int direction) {
    source = constrain(source + direction, 0, NumInputs() - 1);
  }
};

struct CVInput : public ParamInput {
  CVInput(char const* n) {
    name = n;
  }

  int In() {
    if (!source) return 0;
    return source <= ADC_CHANNEL_LAST
      ? frame.inputs[source - 1]
      : frame.outputs[source - 1 - ADC_CHANNEL_LAST];
  }

  int8_t NumInputs() const {
    return ADC_CHANNEL_LAST * 2 + 1;
  }

  char const* InputName() const {
    return OC::Strings::cv_input_names_none[source];
  }

  uint8_t const* Icon() const {
    return PARAM_MAP_ICONS + 8 * source;
  }
};

struct TrigInput {
  char const* name;
  uint8_t source = 0;
};

enum AudioChannels {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;

  virtual inline size_t NumInputParams() const { return 0; }
  virtual ParamInput* GetInputParam(size_t i) { return nullptr; }

  void DisplayInputMap(ParamInput& input, int index, int cursor) {
    int y = 10 * index + 15;
    gfxPrint(1, y, input.name);
    int px = 63 - 6 * 3;
    gfxPrint(px, y, input.InputName());
    if (cursor == index) {
      gfxCursor(px, y + 9, 6 * 3);
    }
  }

  // void ViewInputMappings() {
  //   for (size_t i = 0; i < NumInputParams(); i++) {
  //     auto p = GetInputParam(i);
  //     int y = 10 * i + 15;
  //     gfxPrint(1, y, p->name);
  //     int px = 63 - 6 * 3;
  //     gfxPrint(px, y, p->InputName());
  //     if (mapping_cursor == static_cast<int>(i)) {
  //       gfxCursor(px, y + 9, 6 * 3);
  //     }
  //   }
  // };
  //
private:
  int8_t mapping_cursor = 0;
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
