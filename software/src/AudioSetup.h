#pragma once

#include <Audio.h>
#include <Wire.h>

// --- Audio-related functions accessible from elsewhere in the O_C codebase
namespace OC {
  namespace AudioDSP {

    enum ParamTargets {
      FILTER1_CUTOFF, FILTER1_RESONANCE,
      FILTER2_CUTOFF, FILTER2_RESONANCE,
      WAVEFOLD1_MOD, WAVEFOLD2_MOD,
      REVERB1_WET, REVERB1_SIZE, REVERB1_DAMP,
      REVERB2_WET, REVERB2_SIZE, REVERB2_DAMP,

      TARGET_COUNT
    };

    const int MAX_CV = 9216; // 6V

    extern uint8_t mods_enabled; // DAC outputs bitmask
    extern int bias[TARGET_COUNT]; // UI settings

    void Init();
    void Process(const int *values);

  } // AudioDSP namespace
} // OC namespace
