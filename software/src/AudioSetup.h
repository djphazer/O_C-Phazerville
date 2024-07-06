#pragma once

#include <Audio.h>
#include <Wire.h>

// Use the web GUI tool as a guide: https://www.pjrc.com/teensy/gui/

// GUItool: begin automatically generated code
AudioInputI2S2           i2s1;           //xy=58,274
AudioAmplifier           amp1;           //xy=243,158
AudioAmplifier           amp2;           //xy=240.20001220703125,421.20001220703125
AudioFilterStateVariable svfilter1;        //xy=415,356
AudioFilterLadder        ladder1;        //xy=435,78
AudioMixer4              mixer1;         //xy=608,135
AudioMixer4              mixer2;         //xy=618,401
AudioSynthWaveformDc     dc1;            //xy=603.2000122070312,215.20001220703125
AudioSynthWaveformDc     dc2;            //xy=622,513
AudioEffectWaveFolder    wavefolder1;    //xy=815,138
AudioEffectWaveFolder    wavefolder2;    //xy=825.2000122070312,362.20001220703125
AudioEffectFreeverb      freeverb1;      //xy=1003,171
AudioEffectFreeverb      freeverb2;      //xy=988,452
AudioMixer4              mixer3;         //xy=1160,103
AudioMixer4              mixer4;         //xy=1159,387
AudioOutputI2S2          i2s2;           //xy=1329,266
AudioConnection          patchCord1(i2s1, 0, amp1, 0);
AudioConnection          patchCord2(i2s1, 1, amp2, 0);
AudioConnection          patchCord5(amp1, 0, ladder1, 0);
AudioConnection          patchCord6(amp1, 0, mixer1, 3);
AudioConnection          patchCord3(amp2, 0, svfilter1, 0);
AudioConnection          patchCord4(amp2, 0, mixer2, 3);
AudioConnection          patchCord7(svfilter1, 0, mixer2, 0);
AudioConnection          patchCord8(svfilter1, 1, mixer2, 1);
AudioConnection          patchCord9(svfilter1, 2, mixer2, 2);
AudioConnection          patchCord10(ladder1, 0, mixer1, 0);
AudioConnection          patchCord11(dc1, 0, wavefolder1, 1);
AudioConnection          patchCord14(dc2, 0, wavefolder2, 1);
AudioConnection          patchCord12(mixer1, 0, wavefolder1, 0);
AudioConnection          patchCord13(mixer2, 0, wavefolder2, 0);
AudioConnection          patchCord15(wavefolder1, 0, mixer3, 0);
AudioConnection          patchCord16(wavefolder1, freeverb1);
AudioConnection          patchCord17(wavefolder2, 0, mixer4, 0);
AudioConnection          patchCord18(wavefolder2, freeverb2);
AudioConnection          patchCord20(freeverb1, 0, mixer3, 1);
AudioConnection          patchCord19(freeverb2, 0, mixer4, 1);
AudioConnection          patchCord21(mixer3, 0, i2s2, 0);
AudioConnection          patchCord22(mixer4, 0, i2s2, 1);
// GUItool: end automatically generated code

// Notes:
//
// amp1 and amp2 are beginning of chain, for pre-filter attenuation
// Every mixer input is a VCA.
//
// dc1 and dc2 are control signals for modulating whatever here, it's the wavefold amount.
//

// this could be used for the Tuner or other pitch-tracking tricks
//AudioAnalyzeNoteFrequency notefreq1;
//AudioConnection          patchCord8(i2s1, 1, notefreq1, 0);


// --- Audio-related functions accessible from elsewhere in the O_C codebase
namespace OC {
  namespace AudioDSP {

    const int MAX_CV = 9216; // 6V

    uint8_t mods_enabled = 0xff; // bitmask for DAC outputs

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
      dc1.amplitude((float)cv / MAX_CV);
    }
    void WavefoldR(int cv) {
      dc2.amplitude((float)cv / MAX_CV);
    }

    // ----- called from setup() in Main.cpp
    static void Init() {
      AudioMemory(96);

      // Reverb
      freeverb1.roomsize(0.8);
      freeverb1.damping(0.5);
      mixer3.gain(1, 0.20); // wet

      freeverb2.roomsize(0.8);
      freeverb2.damping(0.5);
      mixer4.gain(1, 0.20); // wet
      
      amp1.gain(0.85); // attenuate before filter
      amp2.gain(0.85); // attenuate before filter

      dc1.amplitude(0.5);
      dc2.amplitude(0.5);

      //BypassFilterL();
      //BypassFilterR();
      EnableFilterL();
      SelectLPF();

      svfilter1.resonance(1.05);
      ladder1.resonance(0.6);
    }

    void Process(const int *values) {
      // Output A - VCA+VCF L
      if (enabled(0)) {
        mixer3.gain(0, (float)values[0] / MAX_CV);
        ModFilterL(values[0]);
      }

      // Output B - Wavefold L
      if (enabled(1)) {
        WavefoldL(values[1]);
      }

      // Output C - VCA R
      if (enabled(2)) {
        mixer4.gain(0, (float)values[2] / MAX_CV);
      }

      // Output D - VCF R
      if (enabled(3)) {
        ModFilterR(values[3]);
      }
    }

  } // AudioDSP namespace
} // OC namespace
