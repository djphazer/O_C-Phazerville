// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

/*
  ghostils:
  Envelopes are now independent for control and mod source/destination allowing two individual ADSR's with Release MOD CV input per hemisphere.
  * CV mod is now limited to release for each channel
  * Output Level indicators have been shrunk to make room for additional on screen indicators for which envelope you are editing.
  * Switching between envelopes is currently handled by simply pressing the encoder button until you pass the release stage on each envelope which will toggle the active envelope you are editing
  * Envelope is indicated by A or B just above the ADSR segments.
  *
  * TODO: UI Design:
  * Update to allow menu to select CV destinations for CV Input Sources on CH1/CH2
  *   This could be assignable to a different destination based on probability potentially as well
  * Update to allow internal GATE/Trig count to apply a modulation value to any or each of the envelope segments

*/

class ADSREG : public HemisphereApplet {
public:

    static constexpr int SUSTAIN_CONST = 35;
    static constexpr int DISPLAY_HEIGHT = 30;

    // Release time multiplier (MAX_TICKS_R / MAX_TICKS_AD ≈ 4)
    static constexpr int RELEASE_TIME_MULTIPLIER = 4;

    static constexpr int STAGE_MAX_VALUE = 255;
    static constexpr int NUM_CHANNELS = 2;

    // Single inline precomputed LUT for monotonic stage-time mapping: O(1) lookup, no math in hot path.
    // Attack/Decay (AD): kStageTicks[1..255] → 1..33333 ticks (0..~4 seconds).
    // Release (R): kStageTicks[1..255] * RELEASE_TIME_MULTIPLIER → ~4..133332 ticks (0..~8 seconds).
    // Formula: ticks = p + (n²·extra_max)/(STAGE_MAX_VALUE-1)² where n=p-1, strictly monotonic (Δ≥1 per step).
    // Index 0 is unused: ScaleStageToTicksAD clamps to 1..255, ScaleStageToTicksR returns early for p==0.
    static constexpr uint16_t kStageTicks[STAGE_MAX_VALUE + 1] = {
      0, 1, 2, 5, 8, 13, 18, 25, 33, 41, 51, 62, 74, 86, 100, 115,
      131, 148, 166, 185, 205, 226, 248, 271, 295, 320, 346, 373, 401, 430, 461, 492,
      524, 558, 592, 627, 664, 701, 739, 779, 819, 861, 903, 947, 992, 1037, 1084, 1131,
      1180, 1230, 1281, 1332, 1385, 1439, 1494, 1550, 1606, 1664, 1723, 1783, 1844, 1906, 1969, 2033,
      2098, 2165, 2232, 2300, 2369, 2439, 2511, 2583, 2656, 2730, 2806, 2882, 2959, 3038, 3117, 3198,
      3279, 3362, 3445, 3530, 3616, 3702, 3790, 3879, 3968, 4059, 4151, 4243, 4337, 4432, 4528, 4625,
      4723, 4822, 4922, 5023, 5125, 5228, 5332, 5437, 5543, 5650, 5758, 5867, 5978, 6089, 6201, 6314,
      6429, 6544, 6660, 6778, 6896, 7016, 7136, 7257, 7380, 7504, 7628, 7754, 7880, 8008, 8137, 8266,
      8397, 8529, 8662, 8795, 8930, 9066, 9203, 9341, 9480, 9620, 9761, 9903, 10046, 10190, 10335, 10481,
      10628, 10776, 10925, 11075, 11227, 11379, 11532, 11686, 11842, 11998, 12156, 12314, 12473, 12634, 12795, 12958,
      13121, 13286, 13451, 13618, 13786, 13954, 14124, 14295, 14466, 14639, 14813, 14988, 15164, 15341, 15518, 15697,
      15877, 16058, 16240, 16423, 16607, 16792, 16978, 17166, 17354, 17543, 17733, 17924, 18116, 18310, 18504, 18699,
      18896, 19093, 19291, 19491, 19691, 19893, 20095, 20299, 20503, 20709, 20915, 21123, 21332, 21541, 21752, 21964,
      22177, 22390, 22605, 22821, 23038, 23256, 23475, 23695, 23916, 24137, 24361, 24585, 24810, 25036, 25263, 25491,
      25720, 25950, 26181, 26414, 26647, 26881, 27117, 27353, 27590, 27829, 28068, 28308, 28550, 28792, 29036, 29280,
      29526, 29773, 30020, 30269, 30519, 30769, 31021, 31274, 31527, 31782, 32038, 32295, 32553, 32812, 33072, 33333,
    };

    static int ScaleStageToTicksAD(int setting) {
      const int p = constrain(setting, 1, STAGE_MAX_VALUE);
      return int(kStageTicks[p]);
    }

    static int ScaleStageToTicksR(int setting) {
      const int p = constrain(setting, 0, STAGE_MAX_VALUE);
      if (p == 0) return 0;
      return int(kStageTicks[p]) * RELEASE_TIME_MULTIPLIER;
    }

    enum ADSRStage {
      ATTACK_STAGE = 0,
      DECAY_STAGE = 1,
      SUSTAIN_STAGE = 2,
      RELEASE_STAGE = 3,

      NUM_STAGES,

      NO_STAGE = -1,
    };
    static constexpr int CURSOR_MAX = NUM_STAGES * NUM_CHANNELS - 1;

    // ++ This is where the magic happens ++
    struct MiniADSR {
      // Attack rate from 1-255 where 1 is fast
      // Decay rate from 1-255 where 1 is fast
      // Sustain level from 1-255 where 1 is low
      // Release rate from 1-255 where 1 is fast
      uint8_t setting[NUM_STAGES];
      uint8_t shape[NUM_STAGES]; // 0 = exp, 128 = linear, 255 = log

      // Stage management
      ADSRStage stage; // The current ASDR stage of the current envelope
      int stage_ticks; // Current number of ticks into the current stage
      bool gated; // Gate was on in last tick
      simfloat amplitude; // Amplitude of the envelope at the current position

      int cv_mod; // CV modulated values

      void Init(int ch = 0) {
        stage_ticks = 0;
        gated = 0;
        stage = NO_STAGE;

        setting[ATTACK_STAGE] = 10 + ch * 10;
        setting[DECAY_STAGE] = 30;
        setting[SUSTAIN_STAGE] = 120;
        setting[RELEASE_STAGE] = 25 + ch * 10;
        cv_mod = 0;
      }

      // TODO: shaping math (exp -> lin -> log)
      void AttackAmplitude() {
          int effective_attack = constrain(setting[ATTACK_STAGE], 1, STAGE_MAX_VALUE);
          int total_stage_ticks = ScaleStageToTicksAD(effective_attack);
          int ticks_remaining = total_stage_ticks - stage_ticks;
          if (effective_attack == 1) ticks_remaining = 0;
          if (ticks_remaining <= 0) { // End of attack; move to decay
              stage = DECAY_STAGE;
              stage_ticks = 0;
              amplitude = int2simfloat(HEMISPHERE_MAX_CV);
          } else {
              simfloat amplitude_remaining = int2simfloat(HEMISPHERE_MAX_CV) - amplitude;
              simfloat increase = amplitude_remaining / ticks_remaining;
              amplitude += increase;
          }
      }

      void DecayAmplitude() {
          int total_stage_ticks = ScaleStageToTicksAD(setting[DECAY_STAGE]);
          int ticks_remaining = total_stage_ticks - stage_ticks;
          simfloat amplitude_remaining = amplitude
            - int2simfloat(Proportion(setting[SUSTAIN_STAGE], STAGE_MAX_VALUE, HEMISPHERE_MAX_CV));
          if (setting[SUSTAIN_STAGE] == 255) ticks_remaining = 0; // skip decay if sustain is maxed
          if (ticks_remaining <= 0) { // End of decay; move to sustain
              stage = SUSTAIN_STAGE;
              stage_ticks = 0;
              amplitude = int2simfloat(Proportion(setting[SUSTAIN_STAGE], STAGE_MAX_VALUE, HEMISPHERE_MAX_CV));
          } else {
              simfloat decrease = amplitude_remaining / ticks_remaining;
              amplitude -= decrease;
          }
      }

      void SustainAmplitude() {
          amplitude = int2simfloat(Proportion(setting[SUSTAIN_STAGE] - 1, STAGE_MAX_VALUE, HEMISPHERE_MAX_CV));
      }

      void ReleaseAmplitude() {
          int effective_release = constrain(setting[RELEASE_STAGE] + cv_mod, 1, STAGE_MAX_VALUE) - 1;
          int total_stage_ticks = ScaleStageToTicksR(effective_release);
          int ticks_remaining = total_stage_ticks - stage_ticks;
          if (effective_release == 0) ticks_remaining = 0;
          if (ticks_remaining <= 0 || amplitude <= 0) { // End of release; turn off envelope
              stage = NO_STAGE;
              stage_ticks = 0;
              amplitude = 0;
          } else {
              simfloat decrease = amplitude / ticks_remaining;
              amplitude -= decrease;
          }
      }

      void Process(bool gatehigh = true) {
          if (gatehigh) {
              if (!gated) { // The gate wasn't on last time, so this is a newly-gated EG
                  stage_ticks = 0;
                  if (stage != RELEASE_STAGE) amplitude = 0;
                  stage = ATTACK_STAGE;
                  AttackAmplitude();
              } else { // The gate is STILL on, so process the appopriate stage
                  ++stage_ticks;
                  if (stage == ATTACK_STAGE) AttackAmplitude();
                  if (stage == DECAY_STAGE) DecayAmplitude();
                  if (stage == SUSTAIN_STAGE) SustainAmplitude();
              }
              gated = 1;
          } else {
              if (gated) { // The gate was on last time, so this is a newly-released EG
                  stage = RELEASE_STAGE;
                  stage_ticks = 0;
              }

              if (stage == RELEASE_STAGE) { // Process the release stage, if necessary
                  ++stage_ticks;
                  ReleaseAmplitude();
              }
              gated = 0;
          }
      }

      int GetLength() const {
          return setting[ATTACK_STAGE]
               + setting[DECAY_STAGE]
               + setting[RELEASE_STAGE]
               + SUSTAIN_CONST; // Sustain is constant because it's a level
      }
      int GetAmplitude() {
          return simfloat2int(amplitude);
      }
    };

    // --- Applet Code Below ---

    const char* applet_name() { // Maximum 10 characters
        return "ADSR EG";
    }
    const uint8_t* applet_icon() { return PhzIcons::ADSR_EG; }

    void Start() {
        cursor = 0;
        ForEachChannel(ch)
        {
          adsr_env[ch].Init(ch);
        }
    }

    void Controller() {
        ForEachChannel(ch)
        {
            auto &adsr = adsr_env[ch];
            adsr.cv_mod = get_modification_with_input(ch);

            adsr.Process(Gate(ch));
            Out(ch, adsr.GetAmplitude());
        }
    }

    void View() final;

    /*
    void AuxButton() {
        shape_edit ^= 1;
    }
    */
    //void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
          cursor = constrain(cursor + direction, 0, CURSOR_MAX);
          return;
        }

        const int curEG = (cursor / NUM_STAGES);
        const int stage = cursor % NUM_STAGES;
        auto &adsr = adsr_env[curEG];
        /*
        if (shape_edit)
          adsr.shape[stage] = constrain(adsr.shape[stage] + direction, 0, 0xff);
        else
        */
          adsr.setting[stage] = constrain(adsr.setting[stage] + direction, 1, STAGE_MAX_VALUE);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for(size_t ch = 0; ch < 2; ++ch) {
          Pack(data, PackLocation {ch*32 +  0,8}, adsr_env[ch].setting[0]);
          Pack(data, PackLocation {ch*32 +  8,8}, adsr_env[ch].setting[1]);
          Pack(data, PackLocation {ch*32 + 16,8}, adsr_env[ch].setting[2]);
          Pack(data, PackLocation {ch*32 + 24,8}, adsr_env[ch].setting[3]);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
      if (!data) {
        Start(); // If empty data, initialize
        return;
      }
      for(size_t ch = 0; ch < 2; ++ch) {
        adsr_env[ch].setting[0] = constrain(Unpack(data, PackLocation {ch*32 +  0,8}), 1, STAGE_MAX_VALUE);
        adsr_env[ch].setting[1] = constrain(Unpack(data, PackLocation {ch*32 +  8,8}), 1, STAGE_MAX_VALUE);
        adsr_env[ch].setting[2] = constrain(Unpack(data, PackLocation {ch*32 + 16,8}), 1, STAGE_MAX_VALUE);
        adsr_env[ch].setting[3] = constrain(Unpack(data, PackLocation {ch*32 + 24,8}), 1, STAGE_MAX_VALUE);
      }
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "GateCh1";
    help[HELP_DIGITAL2] = "GateCh2";
    help[HELP_CV1]      = "Releas1";
    help[HELP_CV2]      = "Releas2";
    help[HELP_OUT1]     = "AmpCh1";
    help[HELP_OUT2]     = "AmpCh2";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor;
    MiniADSR adsr_env[2];
    //bool shape_edit = false;

    // TODO: implement destination mapping; currently hardcoded to Release stage

    const char* const labels[NUM_STAGES] = { "A=", "D=", "S=", "R=" };

    void DrawIndicator() {
        ForEachChannel(ch)
        {
            int w = Proportion(adsr_env[ch].GetAmplitude(), HEMISPHERE_MAX_CV, 62);
            //-ghostils:Update to make smaller to allow for additional information on the screen:
            //gfxRect(0, 15 + (ch * 10), w, 6);
            gfxRect(0, 15 + (ch * 3), w, 2);
        }

        gfxPrint(0,22, OutputLabel(cursor / NUM_STAGES) );
        gfxInvert(0,21,7,9);

        if (EditMode()) {
          DrawActiveParam();
        }
    }

    void DrawActiveParam() {
        const int curEG = (cursor / NUM_STAGES);
        const int stage = cursor % NUM_STAGES;
        auto &adsr = adsr_env[curEG];

        gfxPrint(9, 22, labels[stage]);
        if (SUSTAIN_STAGE == stage) {
          int level = Proportion(adsr.setting[stage], STAGE_MAX_VALUE, 1000);
          gfxPrint(level / 10);
          gfxPrint(".");
          gfxPrint(level % 10);
          gfxPrint("%");
        } else {
          int total_stage_ticks;
          if (stage == RELEASE_STAGE) {
            // setting[stage] is already clamped to 1..255 by OnEncoderMove/OnDataReceive
            // const int effective_release = constrain(adsr.setting[stage], 1, STAGE_MAX_VALUE) - 1;
            const int effective_release = adsr.setting[stage] - 1;
            total_stage_ticks = ScaleStageToTicksR(effective_release);
          } else {
            total_stage_ticks = ScaleStageToTicksAD(adsr.setting[stage]);
          }
          int ms_value = total_stage_ticks / 17;
          gfxPrint(ms_value);
          gfxPrint("ms");
        }
    }

    void DrawADSR() {
        const int curEG = (cursor / NUM_STAGES);
        auto &adsr = adsr_env[curEG];
        int length = adsr.GetLength();
        int x = 0;
        x = DrawAttack(x, length);
        x = DrawDecay(x, length);
        x = DrawSustain(x, length);
        DrawRelease(x, length);
    }

    int DrawAttack(int x, int length) {
        const int curEG = (cursor / NUM_STAGES);
        auto &adsr = adsr_env[curEG];
        int xA = x + Proportion(adsr.setting[ATTACK_STAGE], length, 62);
        gfxLine(x, BottomAlign(0), xA, BottomAlign(DISPLAY_HEIGHT), (cursor%NUM_STAGES) != ATTACK_STAGE);
        return xA;
    }

    int DrawDecay(int x, int length) {
        const int curEG = (cursor / NUM_STAGES);
        auto &adsr = adsr_env[curEG];
        int xD = x + Proportion(adsr.setting[DECAY_STAGE], length, 62);
        if (xD < 0) xD = 0;
        int yS = Proportion(adsr.setting[SUSTAIN_STAGE], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
        gfxLine(x, BottomAlign(DISPLAY_HEIGHT), xD, BottomAlign(yS), (cursor%NUM_STAGES) != DECAY_STAGE);
        return xD;
    }

    int DrawSustain(int x, int length) {
        const int curEG = (cursor / NUM_STAGES);
        auto &adsr = adsr_env[curEG];
        int xS = x + Proportion(SUSTAIN_CONST, length, 62);
        int yS = Proportion(adsr.setting[SUSTAIN_STAGE], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
        if (yS < 0) yS = 0;
        if (xS < 0) xS = 0;
        gfxLine(x, BottomAlign(yS), xS, BottomAlign(yS), (cursor%NUM_STAGES) != SUSTAIN_STAGE);
        return xS;
    }

    int DrawRelease(int x, int length) {
        const int curEG = (cursor / NUM_STAGES);
        auto &adsr = adsr_env[curEG];
        int xR = x + Proportion(adsr.setting[RELEASE_STAGE], length, 62);
        int yS = Proportion(adsr.setting[SUSTAIN_STAGE], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
        gfxLine(x, BottomAlign(yS), xR, BottomAlign(0), (cursor%NUM_STAGES) != RELEASE_STAGE);
        return xR;
    }

    int get_modification_with_input(int in) {
        int mod = 0;
        mod = Proportion(DetentedIn(in), HEMISPHERE_MAX_INPUT_CV, STAGE_MAX_VALUE / 2);
        return mod;
    }
};

FLASHMEM void ADSREG::View() {
  DrawIndicator();
  DrawADSR();
}
