#pragma once

#include <Audio.h>
#include <Wire.h>

// --- Audio-related functions accessible from elsewhere in the O_C codebase
namespace OC {
  namespace AudioDSP {

    enum ParamTarget {
      AMP_LEVEL,
      FILTER_CUTOFF,
      FILTER_RESONANCE,
      WAVEFOLD_MOD,
      REVERB_LEVEL,
      REVERB_SIZE,
      REVERB_DAMP,

      TARGET_COUNT
    };

    enum ChannelMode {
      PASSTHRU,
      VCA_MODE,
      LPG_MODE,
      VCF_MODE,
      WAVEFOLDER,

      MODE_COUNT
    };
    extern const char * const mode_names[];

    const int MAX_CV = 9216; // 6V

    extern uint8_t mods_enabled; // DAC outputs bitmask
    extern ChannelMode mode[2]; // mode for each channel
    extern int mod_map[2][TARGET_COUNT]; // CV modulation sources (as channel indexes for [inputs..outputs])
    extern float bias[2][TARGET_COUNT]; // baseline settings
    extern uint8_t audio_cursor[2];
    static constexpr int CURSOR_MAX = 2;

    void Init();
    void Process(const int *values);
    void SwitchMode(int ch, ChannelMode newmode);
    void AudioMenuAdjust(int ch, int direction);
    void DrawAudioSetup();

    static inline void AudioSetupButtonAction(int ch) {
      ++audio_cursor[ch] %= CURSOR_MAX;
    }
  } // AudioDSP namespace
} // OC namespace
