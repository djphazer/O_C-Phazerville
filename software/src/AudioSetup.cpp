#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

#include "AudioSetup.h"
#include "OC_ADC.h"
#include "OC_DAC.h"
#include "HSUtils.h"
#include "HSicons.h"
#include "OC_strings.h"

// Use the web GUI tool as a guide: https://www.pjrc.com/teensy/gui/

// GUItool: begin automatically generated code
AudioInputI2S2           i2s1;           //xy=67,259
AudioAmplifier           amp2;           //xy=249,406
AudioAmplifier           amp1;           //xy=252,143
AudioFilterStateVariable svfilter1;      //xy=424,341
AudioFilterLadder        ladder1;        //xy=444,63
AudioSynthWaveformDc     dc1;            //xy=612,200
AudioMixer4              mixer2;         //xy=615,370
AudioMixer4              mixer1;         //xy=617,120
AudioSynthWaveformDc     dc2;            //xy=631,498
AudioEffectWaveFolder    wavefolder2;    //xy=859.8889083862305,425.22222232818604
AudioEffectWaveFolder    wavefolder1;    //xy=874.7777633666992,155.22220993041992
AudioMixer4              mixer4;         //xy=1130.2223625183105,375.33338928222656
AudioMixer4              mixer3;         //xy=1132.3332710266113,80.22221755981445
//AudioEffectFreeverb      freeverb2;      //xy=1132.5553283691406,255.11116409301758
//AudioEffectFreeverb      freeverb1;      //xy=1135.6664810180664,188.5555362701416
AudioOutputI2S2          i2s2;           //xy=1434.77783203125,232.5555591583252

AudioConnection          patchCord1(i2s1, 0, amp1, 0);
AudioConnection          patchCord2(i2s1, 1, amp2, 0);
AudioConnection          patchCord3(amp2, 0, svfilter1, 0);
AudioConnection          patchCord4(amp2, 0, mixer2, 3);
AudioConnection          patchCord5(amp1, 0, ladder1, 0);
AudioConnection          patchCord6(amp1, 0, mixer1, 3);
AudioConnection          patchCord7(svfilter1, 0, mixer2, 0);
AudioConnection          patchCord8(svfilter1, 1, mixer2, 1);
AudioConnection          patchCord9(svfilter1, 2, mixer2, 2);
AudioConnection          patchCord10(ladder1, 0, mixer1, 0);
AudioConnection          patchCord11(dc1, 0, wavefolder1, 1);
AudioConnection          patchCord12(mixer2, 0, wavefolder2, 0);
AudioConnection          patchCord13(mixer2, 0, mixer4, 0);
AudioConnection          patchCord14(mixer1, 0, wavefolder1, 0);
AudioConnection          patchCord15(mixer1, 0, mixer3, 0);
AudioConnection          patchCord16(dc2, 0, wavefolder2, 1);
AudioConnection          patchCord17(wavefolder2, 0, mixer4, 3);
AudioConnection          patchCord18(wavefolder1, 0, mixer3, 3);
AudioConnection          patchCord19(mixer4, 0, i2s2, 1);
AudioConnection          patchCord21(mixer3, 0, i2s2, 0);
//AudioConnection          patchCord20(mixer4, freeverb2);
//AudioConnection          patchCord22(mixer3, freeverb1);
//AudioConnection          patchCord23(freeverb2, 0, mixer4, 1);
//AudioConnection          patchCord24(freeverb2, 0, mixer3, 2);
//AudioConnection          patchCord25(freeverb1, 0, mixer3, 1);
//AudioConnection          patchCord26(freeverb1, 0, mixer4, 2);
// GUItool: end automatically generated code

// Notes:
//
// amp1 and amp2 are beginning of chain, for pre-filter attenuation
// dc1 and dc2 are control signals for modulating the wavefold amount.
//
// The reverbs are fed from the final outputs and looped back into BOTH mixers...
// mixer3 and mixer4 control the balance:
// 0 - dry
// 1 - this reverb
// 2 - other reverb
// 3 - wavefold
//
// Every mixer input is a VCA.
// VCA modulation could control all of them.
//

// this could be used for the Tuner or other pitch-tracking tricks
//AudioAnalyzeNoteFrequency notefreq1;
//AudioConnection          patchCord8(i2s1, 1, notefreq1, 0);

namespace OC {
  namespace AudioDSP {

    const char * const mode_names[] = {
      "Off", "VCA", "LPG", "VCF", "FOLD",
    };

    /* Mod Targets:
      AMP_LEVEL
      FILTER_CUTOFF,
      FILTER_RESONANCE,
      WAVEFOLD_MOD,
      REVERB_LEVEL,
      REVERB_SIZE,
      REVERB_DAMP,
     */
    ChannelMode mode[2] = { PASSTHRU, PASSTHRU };
    int mod_map[2][TARGET_COUNT] = {
      { 8, 8, -1, 8, -1, -1, -1 },
      { 10, 10, -1, 10, -1, -1, -1 },
    };
    float bias[2][TARGET_COUNT];
    uint8_t audio_cursor[2] = { 0, 0 };

    float amplevel[2] = { 1.0, 1.0 };
    float foldamt[2] = { 0.0, 0.0 };


    // Right side state variable filter functions
    void SelectHPF() {
      mixer2.gain(0, 0.0); // LPF
      mixer2.gain(1, 0.0); // BPF
      mixer2.gain(2, 1.0); // HPF
      mixer2.gain(3, 0.0); // Dry
    }
    void SelectBPF() {
      mixer2.gain(0, 0.0); // LPF
      mixer2.gain(1, 1.0); // BPF
      mixer2.gain(2, 0.0); // HPF
      mixer2.gain(3, 0.0); // Dry
    }
    void SelectLPF() {
      mixer2.gain(0, 1.0); // LPF
      mixer2.gain(1, 0.0); // BPF
      mixer2.gain(2, 0.0); // HPF
      mixer2.gain(3, 0.0); // Dry
    }

    void BypassFilter(int ch) {
      if (ch == 0) {
        mixer1.gain(0, 0.0); // LPF
        mixer1.gain(1, 0.0); // unused
        mixer1.gain(2, 0.0); // unused
        mixer1.gain(3, 1.0); // Dry
      } else {
        mixer2.gain(0, 0.0); // LPF
        mixer2.gain(1, 0.0); // BPF
        mixer2.gain(2, 0.0); // HPF
        mixer2.gain(3, 1.0); // Dry
      }
    }

    void EnableFilter(int ch) {
      if (ch == 0) {
        mixer1.gain(0, 1.0); // LPF
        mixer1.gain(1, 0.0); // unused
        mixer1.gain(2, 0.0); // unused
        mixer1.gain(3, 0.0); // Dry
      } else {
        SelectLPF();
      }
    }

    void ModFilter(int ch, int cv) {
      // quartertones squared
      // 1 Volt is 576 Hz
      // 2V = 2304 Hz
      // 3V = 5184 Hz
      // 4V = 9216 Hz
      float freq = abs(cv) / 64 + bias[ch][FILTER_CUTOFF];
      freq *= freq;

      if (ch == 0)
        ladder1.frequency(freq);
      else
        svfilter1.frequency(freq);
    }


    void Wavefold(int ch, int cv) {
      foldamt[ch] = (float)cv / MAX_CV + bias[ch][WAVEFOLD_MOD];
      if (ch == 0) {
        dc1.amplitude(foldamt[ch]);
        mixer3.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
        mixer3.gain(3, foldamt[ch] * 0.9);
      } else {
        dc2.amplitude(foldamt[ch]);
        mixer4.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
        mixer4.gain(3, foldamt[ch] * 0.9);
      }
    }

    void AmpLevel(int ch, int cv) {
      amplevel[ch] = (float)cv / MAX_CV + bias[ch][AMP_LEVEL];
      if (ch == 0)
        mixer3.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
      else
        mixer4.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
    }

    // Designated Integration Functions
    // ----- called from setup() in Main.cpp
    void Init() {
      AudioMemory(128);

      amp1.gain(0.85); // attenuate before filter
      amp2.gain(0.85); // attenuate before filter

      // --Filters
      BypassFilter(0);
      BypassFilter(1);

      svfilter1.resonance(1.05);
      ladder1.resonance(0.65);

      // --Wavefolders
      dc1.amplitude(0.00);
      dc2.amplitude(0.00);
      mixer3.gain(3, 0.9);
      mixer4.gain(3, 0.9);

      // --Reverbs
      /*
      freeverb1.roomsize(0.7);
      freeverb1.damping(0.5);
      mixer3.gain(1, 0.08); // verb1
      mixer3.gain(2, 0.05); // verb2

      freeverb2.roomsize(0.8);
      freeverb2.damping(0.6);
      mixer4.gain(1, 0.08); // verb2
      mixer4.gain(2, 0.05); // verb1
      */
      
    }

    // ----- called from Controller thread
    void Process(const int *values) {
      for (int i = 0; i < 2; ++i) {

        // some things depend on mode
        switch(mode[i]) {
          default:
          case PASSTHRU:
            break;

          case LPG_MODE:
            if (mod_map[i][AMP_LEVEL] < 0) continue;
            ModFilter(i, values[mod_map[i][AMP_LEVEL]]);
          case VCA_MODE:
            if (mod_map[i][AMP_LEVEL] < 0) continue;
            AmpLevel(i, values[mod_map[i][AMP_LEVEL]]);
            break;

          case WAVEFOLDER:
            if (mod_map[i][WAVEFOLD_MOD] < 0) continue;
            Wavefold(i, values[mod_map[i][WAVEFOLD_MOD]]);
            //break;
            // wavefolder + filter
          case VCF_MODE:
            if (mod_map[i][FILTER_CUTOFF] < 0) continue;
            ModFilter(i, values[mod_map[i][FILTER_CUTOFF]]);
            break;

        }

        // other modulation happens regardless of mode

      }
    }

    void SwitchMode(int ch, ChannelMode newmode) {
      mode[ch] = newmode;
      switch(newmode) {
          case PASSTHRU:
          case VCA_MODE:
            Wavefold(ch, 0);
            AmpLevel(ch, MAX_CV);
            BypassFilter(ch);
            break;

          case VCF_MODE:
          case LPG_MODE:
            Wavefold(ch, 0);
            AmpLevel(ch, MAX_CV);
            EnableFilter(ch);
            break;

          case WAVEFOLDER:
            AmpLevel(ch, MAX_CV);
            BypassFilter(ch);
            break;
          default: break;
      }
    }

    void AudioMenuAdjust(int ch, int direction) {
      if (audio_cursor[ch]) {
        int mod_target = AMP_LEVEL;
        switch (mode[ch]) {
          case VCF_MODE:
            mod_target = FILTER_CUTOFF;
            break;
          case WAVEFOLDER:
            mod_target = WAVEFOLD_MOD;
            break;
          default: break;
        }

        int &targ = mod_map[ch][mod_target];
        targ = constrain(targ + direction + 1, 0, ADC_CHANNEL_LAST + DAC_CHANNEL_LAST) - 1;
      } else {
        int newmode = mode[ch] + direction;
        CONSTRAIN(newmode, 0, MODE_COUNT - 1);
        SwitchMode(ch, ChannelMode(newmode));
      }
    }

  } // AudioDSP namespace
} // OC namespace

#endif
