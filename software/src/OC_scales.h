#ifndef OS_SCALES_H_
#define OS_SCALES_H_

#include <Arduino.h>
#include "FS.h"
#include "src/extern/braids_quantizer.h"
#include "src/extern/braids_quantizer_scales.h"

// Common scales and stuff
namespace OC {

using Scale = braids::Scale;

static constexpr int kMaxScaleLength = 16;
static constexpr int kMinScaleLength = 4;

class Scales {
public:

  enum {
    SCALE_USER_0,
    SCALE_USER_1,
    SCALE_USER_2,
    SCALE_USER_3,
    SCALE_USER_COUNT, // index 0 in braids::scales
    SCALE_SEMI,
    SCALE_NONE = SCALE_USER_COUNT,
  };

  static void Init();
  static void Validate();
  static const Scale &GetScale(int index);
  static constexpr int NUM_SCALES = SCALE_USER_COUNT + braids::kNumScales;

  static void SaveToScala(Scale &scale, File &file);
  static void LoadScala(Scale &scale, File &file);
};

extern const char *const scale_names[];
extern const char *const scale_names_short[];
extern Scale user_scales[OC::Scales::SCALE_USER_COUNT];
extern Scale dummy_scale;
extern const char *const voltage_scalings[];

};

#endif // OS_SCALES_H_
