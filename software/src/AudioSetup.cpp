#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

#include "extern/dspinst.h"

#include "AudioSetup.h"
#include <TeensyVariablePlayback.h>
#include "OC_ADC.h"
#include "OC_DAC.h"
#include "HSUtils.h"
#include "HSicons.h"
#include "OC_strings.h"
#include "HSClockManager.h"

// Use the web GUI tool as a guide: https://www.pjrc.com/teensy/gui/

// GUItool: begin automatically generated code
AudioPlaySdResmp         wavplayer[2];     //xy=108,191
AudioSynthWaveform       waveform[2];      //xy=128,321
AudioInputI2S2           i2s1;           //xy=129,245
AudioSynthWaveformDc     dc2;            //xy=376.66668701171875,359.4444580078125
AudioMixer4              mixer1;         //xy=380.4444580078125,164.3333282470703
AudioMixer4              mixer2;         //xy=380.3333740234375,292.6666564941406
AudioSynthWaveformDc     dc1;            //xy=381.4444580078125,222.88888549804688
AudioEffectWaveFolder    wavefolder2;    //xy=571.4444580078125,334.888916015625
AudioEffectWaveFolder    wavefolder1;    //xy=575.2222290039062,217.44442749023438
AudioMixer4              mixer4;         //xy=743.000244140625,311.77783203125
AudioMixer4              mixer3;         //xy=750.4443359375,181.3333282470703
AudioAmplifier           amp1;           //xy=818,81
AudioAmplifier           amp2;           //xy=884,357
AudioFilterStateVariable svfilter1;        //xy=964,248
AudioFilterLadder        ladder1;        //xy=970,96.88888549804688
AudioMixer4              mixer5;         //xy=1113,156
AudioMixer4              mixer6;         //xy=1106,310
AudioMixer4              finalmixer[2];         //xy=1106,310
AudioEffectDynamics      complimiter[2];
AudioOutputI2S2          i2s2;           //xy=1270.2222900390625,227.88890075683594

AudioConnection          patchCordWav1L(wavplayer[0], 0, finalmixer[0], 1);
AudioConnection          patchCordWav1R(wavplayer[0], 1, finalmixer[1], 1);
AudioConnection          patchCordWav2L(wavplayer[1], 0, finalmixer[0], 2);
AudioConnection          patchCordWav2R(wavplayer[1], 1, finalmixer[1], 2);
AudioConnection          patchCord3(waveform[1], 0, mixer2, 1);
AudioConnection          patchCord4(waveform[0], 0, mixer1, 1);
AudioConnection          patchCord5(i2s1, 0, mixer1, 0);
AudioConnection          patchCord6(i2s1, 1, mixer2, 0);
AudioConnection          patchCord7(dc2, 0, wavefolder2, 1);
AudioConnection          patchCord8(mixer1, 0, wavefolder1, 0);
AudioConnection          patchCord9(mixer1, 0, mixer3, 0);
AudioConnection          patchCord10(mixer2, 0, wavefolder2, 0);
AudioConnection          patchCord11(mixer2, 0, mixer4, 0);
AudioConnection          patchCord12(dc1, 0, wavefolder1, 1);
AudioConnection          patchCord13(wavefolder2, 0, mixer4, 3);
AudioConnection          patchCord14(wavefolder1, 0, mixer3, 3);
AudioConnection          patchCord15(mixer4, 0, mixer6, 3);
AudioConnection          patchCord16(mixer4, amp2);
AudioConnection          patchCord17(mixer3, 0, mixer5, 3);
AudioConnection          patchCord18(mixer3, amp1);
AudioConnection          patchCord19(amp1, 0, ladder1, 0);
AudioConnection          patchCord20(amp2, 0, svfilter1, 0);
AudioConnection          patchCord21(svfilter1, 0, mixer6, 0);
AudioConnection          patchCord22(svfilter1, 0, mixer5, 1);
AudioConnection          patchCord23(ladder1, 0, mixer5, 0);
AudioConnection          patchCord24(ladder1, 0, mixer6, 1);

AudioConnection          patchCord25(mixer5, 0, finalmixer[0], 0);
AudioConnection          patchCord26(mixer6, 0, finalmixer[1], 0);
AudioConnection          patchCord27(finalmixer[0], 0, complimiter[0], 0);
AudioConnection          patchCord28(finalmixer[1], 0, complimiter[1], 0);
AudioConnection          patchCord29(complimiter[0], 0, i2s2, 0);
AudioConnection          patchCord30(complimiter[1], 0, i2s2, 1);

// GUItool: end automatically generated code

// Some time ago...
// These reverbs were fed from the final outputs and looped back into BOTH mixers...
// mixer3 and mixer4 controlled the balance:
// 0 - dry
// 1 - this reverb
// 2 - other reverb
// 3 - wavefold
//AudioEffectFreeverb      freeverb2;      //xy=1132.5553283691406,255.11116409301758
//AudioEffectFreeverb      freeverb1;      //xy=1135.6664810180664,188.5555362701416
//AudioConnection          patchCord20(mixer4, freeverb2);
//AudioConnection          patchCord22(mixer3, freeverb1);
//AudioConnection          patchCord23(freeverb2, 0, mixer4, 1);
//AudioConnection          patchCord24(freeverb2, 0, mixer3, 2);
//AudioConnection          patchCord25(freeverb1, 0, mixer3, 1);
//AudioConnection          patchCord26(freeverb1, 0, mixer4, 2);

// Notes:
//
// amp1 and amp2 are beginning of chain, for pre-filter attenuation
// dc1 and dc2 are control signals for modulating the wavefold amount.
//
// Every mixer input is a VCA.
// VCA modulation could control all of them.
//

namespace OC {
  namespace AudioDSP {

    const char * const mode_names[MODE_COUNT] = {
      "Off", "Osc", "VCA", "VCF", "FOLD", "File", "Loop", "FileVCA", "Speed"
    };

    /* Mod Targets:
      AMP_LEVEL,
      FILTER_CUTOFF,
      FILTER_RESONANCE,
      WAVEFOLD_MOD,
      WAV_LEVEL,
      REVERB_LEVEL,
      REVERB_SIZE,
      REVERB_DAMP,
     */
    int mod_map[2][TARGET_COUNT] = {
      { -1, -1, -1, -1, -1, -1, -1, -1, -1 },
      { -1, -1, -1, -1, -1, -1, -1, -1, -1 },
    };
    float bias[2][TARGET_COUNT];
    ChannelSetting audio_cursor[2] = { PASSTHRU, PASSTHRU };
    bool isEditing[2] = { false, false };

    float amplevel[2] = { 1.0, 1.0 };
    float foldamt[2] = { 0.0, 0.0 };

    bool filter_enabled[2];
    bool wavplayer_available = false;
    bool wavplayer_reload[2] = {false};
    uint8_t wavplayer_select[2] = { 1, 2 };
    float wavlevel[2] = { 1.0, 1.0 };
    uint8_t loop_length[2] = { 0, 0 };
    int8_t loop_count[2] = { 0, 0 };
    bool loop_on[2] = { false, false };

    void SetOscPitch(int ch, int cv) {
      float freq = 220.0 * powf(2.0, (cv - (9*128)) / (12.0 * 128));
      waveform[ch].frequency(freq);
    }

    void BypassFilter(int ch) {
      if (ch == 0) {
        mixer5.gain(0, 0.0); // VCF
        mixer6.gain(1, 0.0); // VCF

        mixer5.gain(3, 0.9); // Dry
      } else {
        mixer6.gain(0, 0.0); // VCF
        mixer5.gain(1, 0.0); // VCF

        mixer6.gain(3, 0.9); // Dry
      }
      filter_enabled[ch] = false;
    }

    void EnableFilter(int ch) {
      if (ch == 0) {
        mixer5.gain(0, 1.0); // VCF
        mixer6.gain(1, 1.0); // VCF

        mixer5.gain(3, 0.0); // Dry
      } else {
        mixer6.gain(0, 1.0); // VCF
        mixer5.gain(1, 1.0); // VCF

        mixer6.gain(3, 0.0); // Dry
      }
      filter_enabled[ch] = true;
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

    // SD file player functions
    void FileLoad(int ch) {
      char filename[] = "000.WAV";
      filename[1] += wavplayer_select[ch] / 10;
      filename[2] += wavplayer_select[ch] % 10;
      wavplayer[ch].playWav(filename);
    }
    void StartPlaying(int ch) {
      if (wavplayer[ch].available())
        wavplayer[ch].play();
      loop_count[ch] = -1;
    }
    bool FileIsPlaying(int ch = 0) {
      return wavplayer[ch].isPlaying();
    }
    void ToggleFilePlayer(int ch = 0) {
      if (wavplayer[ch].isPlaying()) {
        wavplayer[ch].stop();
      } else if (wavplayer_available) {
        StartPlaying(ch);
      }
    }
    void FileHotCue(int ch) {
      if (wavplayer[ch].available()) {
        wavplayer[ch].retrigger();
        loop_count[ch] = 0;
      }
    }
    void ToggleLoop(int ch) {
      if (loop_length[ch] && !loop_on[ch]) {
        const uint32_t start = wavplayer[ch].isPlaying() ?
                      wavplayer[ch].getPosition() : 0;
        wavplayer[ch].setLoopStart( start );
        wavplayer[ch].setPlayStart(play_start_loop);
        wavplayer[ch].retrigger();
        loop_on[ch] = true;
        loop_count[ch] = -1;
      } else {
        wavplayer[ch].setPlayStart(play_start_sample);
        loop_on[ch] = false;
      }
    }

    // simple hooks for beat-sync callbacks
    void FileToggle1() { ToggleFilePlayer(0); }
    void FileToggle2() { ToggleFilePlayer(1); }
    void FilePlay1() { StartPlaying(0); }
    void FilePlay2() { StartPlaying(1); }
    void FileLoopToggle1() { ToggleLoop(0); }
    void FileLoopToggle2() { ToggleLoop(1); }

    void ChangeToFile(int ch, int select) {
      wavplayer_select[ch] = (uint8_t)constrain(select, 0, 99);
      wavplayer_reload[ch] = true;
      if (wavplayer[ch].isPlaying()) {
        if (HS::clock_m.IsRunning()) {
          HS::clock_m.BeatSync( ch ? &FilePlay2 : &FilePlay1 );
        } else
          StartPlaying(ch);
      }
    }
    uint8_t GetFileNum(int ch) {
      return wavplayer_select[ch];
    }
    uint32_t GetFileTime(int ch) {
      return wavplayer[ch].positionMillis();
    }
    uint16_t GetFileBPM(int ch) {
      return (uint16_t)wavplayer[ch].getBPM();
    }
    void FileMatchTempo(int ch) {
      wavplayer[ch].matchTempo(HS::clock_m.GetTempoFloat());
    }
    void FileLevel(int ch, int cv) {
      wavlevel[ch] = (float)cv / MAX_CV + bias[ch][WAV_LEVEL];
      finalmixer[0].gain(1 + ch, 0.9 * wavlevel[ch]);
      finalmixer[1].gain(1 + ch, 0.9 * wavlevel[ch]);
    }
    void FileRate(int ch, int cv) {
      // bipolar CV has +/- 50% pitch bend
      wavplayer[ch].setPlaybackRate((float)cv / MAX_CV * 0.5 + 1.0);
    }

    // Designated Integration Functions
    // ----- called from setup() in Main.cpp
    void Init() {
      AudioMemory(128);

      // --Oscillators
      waveform[0].begin(0.0, 65.4, WAVEFORM_TRIANGLE_VARIABLE);
      waveform[1].begin(0.0, 110.0, WAVEFORM_SQUARE);

      // --Wavefolders
      dc1.amplitude(0.00);
      dc2.amplitude(0.00);
      mixer3.gain(3, 0.9);
      mixer4.gain(3, 0.9);

      // --Filters
      amp1.gain(0.85); // attenuate before ladder filter
      amp2.gain(0.85); // attenuate before svfilter1
      svfilter1.resonance(1.05);
      ladder1.resonance(0.65);
      BypassFilter(0);
      BypassFilter(1);

      // -- SD card WAV players
      finalmixer[0].gain(1, 0.9);
      finalmixer[1].gain(1, 0.9);
      finalmixer[0].gain(2, 0.9);
      finalmixer[1].gain(2, 0.9);
      wavplayer_available = SD.begin(BUILTIN_SDCARD);
      if (!wavplayer_available) {
        Serial.println("Unable to access the SD card");
      }
      else {
        wavplayer[0].enableInterpolation(true);
        wavplayer[1].enableInterpolation(true);
        wavplayer[0].setBufferInPSRAM(true);
        wavplayer[1].setBufferInPSRAM(true);
      }

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
      
      // -- Master Compressor / Limiter
      for (int ch = 0; ch < 2; ++ch) {
        complimiter[ch].compression(-10.0f);
        complimiter[ch].limit(-3.0f);
        complimiter[ch].makeupGain(0.0);
        // default gate() settings
      }
    }

    void mainloop() {
      if (wavplayer_available) {
        for (int ch = 0; ch < 2; ++ch) {
          if (wavplayer_reload[ch]) {
            FileLoad(ch);
            wavplayer_reload[ch] = false;
          }
        }
      }
    }

    // ----- called from Controller thread
    void Process(const int *values) {
      for (int i = 0; i < 2; ++i) {

        if (mod_map[i][OSC_PITCH] >= 0)
          SetOscPitch(i, values[mod_map[i][OSC_PITCH]]);

        if (mod_map[i][AMP_LEVEL] >= 0)
          AmpLevel(i, values[mod_map[i][AMP_LEVEL]]);

        if (mod_map[i][WAVEFOLD_MOD] >= 0)
          Wavefold(i, values[mod_map[i][WAVEFOLD_MOD]]);

        if (mod_map[i][FILTER_CUTOFF] >= 0)
          ModFilter(i, values[mod_map[i][FILTER_CUTOFF]]);

        if (mod_map[i][WAV_LEVEL] >= 0)
          FileLevel(i, values[mod_map[i][WAV_LEVEL]]);

        if (mod_map[i][WAV_RATE] >= 0)
          FileRate(i, values[mod_map[i][WAV_RATE]]);
        else
          FileMatchTempo(i);

        if (loop_length[i] && loop_on[i] && HS::clock_m.EndOfBeat()) {
          if (++loop_count[i] >= loop_length[i])
            FileHotCue(i);
        }
      }
    }

    // Encoder action
    void AudioMenuAdjust(int ch, int direction) {
      if (!isEditing[ch]) {
        audio_cursor[ch] = (ChannelSetting)constrain(audio_cursor[ch] + direction, 0, MODE_COUNT - 1);
        return;
      }

      int mod_target = AMP_LEVEL;
      switch (audio_cursor[ch]) {
        case PASSTHRU:
        {
          return;
          break;
        }
        case OSCILLATOR:
          mod_target = OSC_PITCH;
          if (direction > 0) waveform[ch].amplitude(0.8);
          break;
        case VCF_MODE:
          mod_target = FILTER_CUTOFF;
          break;
        case WAVEFOLDER:
          mod_target = WAVEFOLD_MOD;
          break;
        case WAV_PLAYER_VCA:
          mod_target = WAV_LEVEL;
          break;
        case WAV_PLAYER_RATE:
          mod_target = WAV_RATE;
          break;
        case WAV_PLAYER:
          ChangeToFile(ch, wavplayer_select[ch] + direction);

          return; // no other mapping to change
          break;
        case LOOP_LENGTH:
          loop_length[ch] = constrain(loop_length[ch] + direction, 0, 128);
          return; break;
        default: break;
      }

      // congrats you get to switch one of the input map mod sources
      int &targ = mod_map[ch][mod_target];
      targ = constrain(targ + direction + 1, 0, ADC_CHANNEL_LAST + DAC_CHANNEL_LAST) - 1;

      // turning off each mode restores a default value
      if (targ < 0) {
        switch (audio_cursor[ch]) {
          case OSCILLATOR:
            waveform[ch].amplitude(0.0);
            break;

          case VCF_MODE:
            ModFilter(ch, MAX_CV);
            break;

          case VCA_MODE:
            AmpLevel(ch, MAX_CV);
            break;

          case WAV_PLAYER_VCA:
            FileLevel(ch, MAX_CV);
            break;

          case WAV_PLAYER_RATE:
            FileRate(ch, 0);
            break;

          default: break;
        }
      }
    }

    void AudioSetupAuxButton(int ch) {
      switch (audio_cursor[ch]) {
        case PASSTHRU:
          Wavefold(ch, 0);
          AmpLevel(ch, MAX_CV);
          BypassFilter(ch);
          break;

        case WAV_PLAYER:
          if (HS::clock_m.IsRunning()) {
            HS::clock_m.BeatSync( ch ? &FileToggle2 : &FileToggle1 );
          } else
            ToggleFilePlayer(ch);
          break;

        case LOOP_LENGTH:
          if (HS::clock_m.IsRunning()) {
            HS::clock_m.BeatSync( ch ? &FileLoopToggle2 : &FileLoopToggle1 );
          } else
            ToggleLoop(ch);
          break;

        case VCF_MODE:
          Wavefold(ch, 0);
          AmpLevel(ch, MAX_CV);
          filter_enabled[ch] ? BypassFilter(ch) : EnableFilter(ch);
          break;

        default: break;
      }
      //isEditing[ch] = false;
    }

  } // AudioDSP namespace
} // OC namespace

#endif
