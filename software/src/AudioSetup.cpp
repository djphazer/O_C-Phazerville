#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

#include "AudioSetup.h"

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
AudioEffectFreeverb      freeverb2;      //xy=1132.5553283691406,255.11116409301758
AudioEffectFreeverb      freeverb1;      //xy=1135.6664810180664,188.5555362701416
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
AudioConnection          patchCord20(mixer4, freeverb2);
AudioConnection          patchCord21(mixer3, 0, i2s2, 0);
AudioConnection          patchCord22(mixer3, freeverb1);
AudioConnection          patchCord23(freeverb2, 0, mixer4, 1);
AudioConnection          patchCord24(freeverb2, 0, mixer3, 2);
AudioConnection          patchCord25(freeverb1, 0, mixer3, 1);
AudioConnection          patchCord26(freeverb1, 0, mixer4, 2);
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

    uint8_t mods_enabled = 0xff; // DAC outputs bitmask
    int bias[TARGET_COUNT]; // UI settings

    float amplevel_left = 1.0;
    float amplevel_right = 1.0;
    float foldamt_left = 0.0;
    float foldamt_right = 0.0;

    bool enabled(int ch) { return (mods_enabled >> ch) & 0x1; }
    void toggle(int ch) { mods_enabled ^= (0x1 << ch); }

    // Left side ladder filter functions
    void EnableFilterL() {
      mixer1.gain(0, 1.0); // LPF
      mixer1.gain(3, 0.0); // Dry
    }
    void BypassFilterL() {
      mixer1.gain(0, 0.0); // LPF
      mixer1.gain(1, 0.0); // unused
      mixer1.gain(2, 0.0); // unused
      mixer1.gain(3, 1.0); // Dry
    }
    void ModFilterL(int cv) {
      // quartertones squared
      // 1 Volt is 576 Hz
      // 2V = 2304 Hz
      // 3V = 5184 Hz
      // 4V = 9216 Hz
      float freq = abs(cv) / 64;
      freq *= freq;
      ladder1.frequency(freq);
    }

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
    void BypassFilterR() {
      mixer2.gain(0, 0.0); // LPF
      mixer2.gain(1, 0.0); // BPF
      mixer2.gain(2, 0.0); // HPF
      mixer2.gain(3, 1.0); // Dry
    }
    void ModFilterR(int cv) {
      // quartertones squared
      // 1 Volt is 576 Hz
      // 2V = 2304 Hz
      // 3V = 5184 Hz
      // 4V = 9216 Hz
      float freq = abs(cv) / 64;
      freq *= freq;
      svfilter1.frequency(freq);
    }

    void WavefoldL(int cv) {
      foldamt_left = (float)cv / MAX_CV;
      dc1.amplitude(foldamt_left);
      mixer3.gain(0, amplevel_left * (1.0 - abs(foldamt_left)));
      mixer3.gain(3, foldamt_left);
    }
    void WavefoldR(int cv) {
      foldamt_right = (float)cv / MAX_CV;
      dc2.amplitude(foldamt_right);
      mixer4.gain(0, amplevel_right * (1.0 - abs(foldamt_right)));
      mixer4.gain(3, foldamt_right);
    }

    void AmpL(int cv) {
      amplevel_left = (float)cv / MAX_CV;
      //mixer3.gain(0, amplevel_left);
      mixer3.gain(0, amplevel_left * (1.0 - abs(foldamt_left)));
    }
    void AmpR(int cv) {
      amplevel_right = (float)cv / MAX_CV;
      //mixer4.gain(0, amplevel_right);
      mixer4.gain(0, amplevel_right * (1.0 - abs(foldamt_right)));
    }

    // Designated Integration Functions
    // ----- called from setup() in Main.cpp
    void Init() {
      AudioMemory(128);

      amp1.gain(0.85); // attenuate before filter
      amp2.gain(0.85); // attenuate before filter

      // --Filters
      //BypassFilterL();
      //BypassFilterR();
      EnableFilterL();
      SelectLPF();

      svfilter1.resonance(1.05);
      ladder1.resonance(0.6);

      // --Wavefolders
      dc1.amplitude(0.00);
      dc2.amplitude(0.00);
      mixer3.gain(3, 0.9);
      mixer4.gain(3, 0.9);

      // --Reverbs
      freeverb1.roomsize(0.7);
      freeverb1.damping(0.5);
      mixer3.gain(1, 0.08); // verb1
      mixer3.gain(2, 0.05); // verb2

      freeverb2.roomsize(0.8);
      freeverb2.damping(0.6);
      mixer4.gain(1, 0.08); // verb2
      mixer4.gain(2, 0.05); // verb1
      
    }

    // ----- called from Controller thread
    void Process(const int *values) {
      // Output A - VCA L
      if (enabled(0)) {
        AmpL(values[0]);
      }

      // Output B - VCF L
      if (enabled(1)) {
        ModFilterL(values[1]);
      }

      // Output C - VCA R
      if (enabled(2)) {
        AmpR(values[2]);
      }

      // Output D - VCF R
      if (enabled(3)) {
        ModFilterR(values[3]);
      }

      // Output E - Wavefolder L
      if (enabled(4)) {
        WavefoldL(values[4]);
      }

      // Output G - Wavefolder R
      if (enabled(6)) {
        WavefoldR(values[6]);
      }
    }
  } // AudioDSP namespace
} // OC namespace

#endif
