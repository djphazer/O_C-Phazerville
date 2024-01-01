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



#define HEM_EG_ATTACK 0
#define HEM_EG_DECAY 1
#define HEM_EG_SUSTAIN 2
#define HEM_EG_RELEASE 3
#define HEM_EG_NO_STAGE -1
#define HEM_EG_MAX_VALUE 255

#define HEM_SUSTAIN_CONST 35
#define HEM_EG_DISPLAY_HEIGHT 30

//-ghostils: DEFINE Main menu inactivity timeout ~5secs this will return the user to the main menu:
#define HEM_EG_UI_INACT_TICKS 41666

//-ghostils: amount of time to handle Double Encoder Press ~250ms.
#define HEM_EG_UI_DBLPRESS_TICKS 4096

// About four seconds
#define HEM_EG_MAX_TICKS_AD 33333

// About eight seconds
#define HEM_EG_MAX_TICKS_R 133333

class ADSREG : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "ADSR EG";
    }

    void Start() {
        edit_stage = 0;
        //attack = 20;
        //decay = 30;
        //sustain = 120;
        //release = 25;
        ForEachChannel(ch)
        {
            stage_ticks[ch] = 0;
            gated[ch] = 0;
            stage[ch] = HEM_EG_NO_STAGE;

            //-ghostils:Initialize ADSR channels independently
            attack[ch] = 20;
            decay[ch] = 30;
            sustain[ch] = 120;
            release[ch] = 25;
            release_mod[ch] = 0;
        }

        //-ghostils:Multiple ADSR Envelope Tracking:
        curEG = 0;

    }

    void Controller() {
        // Look for CV modification
        //attack_mod = get_modification_with_input(0);
        //release_mod[0] = get_modification_with_input(1);

        //-ghostils: Update CV1/CV2 to support release only but on each ADSR independently:
        release_mod[0] = get_modification_with_input(0);
        release_mod[1] = get_modification_with_input(1);

        ForEachChannel(ch)
        {
            if (Gate(ch)) {
                if (!gated[ch]) { // The gate wasn't on last time, so this is a newly-gated EG
                    stage_ticks[ch] = 0;
                    if (stage[ch] != HEM_EG_RELEASE) amplitude[ch] = 0;
                    stage[ch] = HEM_EG_ATTACK;
                    AttackAmplitude(ch);
                } else { // The gate is STILL on, so process the appopriate stage
                    stage_ticks[ch]++;
                    if (stage[ch] == HEM_EG_ATTACK) AttackAmplitude(ch);
                    if (stage[ch] == HEM_EG_DECAY) DecayAmplitude(ch);
                    if (stage[ch] == HEM_EG_SUSTAIN) SustainAmplitude(ch);
                }
                gated[ch] = 1;
            } else {
                if (gated[ch]) { // The gate was on last time, so this is a newly-released EG
                    stage[ch] = HEM_EG_RELEASE;
                    stage_ticks[ch] = 0;
                }

                if (stage[ch] == HEM_EG_RELEASE) { // Process the release stage, if necessary
                    stage_ticks[ch]++;
                    ReleaseAmplitude(ch);
                }
                gated[ch] = 0;
            }


            Out(ch, GetAmplitudeOf(ch));
        }

    }

    void View() {
        DrawIndicator();
        DrawADSR();
    }

    void OnButtonPress() {
      //if (++edit_stage > HEM_EG_RELEASE) {edit_stage = HEM_EG_ATTACK;}
      //-ghostils: flip editing focus between A/B ADSR when we hit the end of the Release stage:
      if (++edit_stage > HEM_EG_RELEASE) {
        edit_stage = HEM_EG_ATTACK;
        curEG ^= 1;
        }
    }

    void OnEncoderMove(int direction) {
        //-ghostils:Reference curEG as the indexer to current ADSR when editing stages:
        int adsr[4] = {attack[curEG], decay[curEG], sustain[curEG], release[curEG]};
        adsr[edit_stage] = constrain(adsr[edit_stage] += direction, 1, HEM_EG_MAX_VALUE);
        attack[curEG] = adsr[HEM_EG_ATTACK];
        decay[curEG] = adsr[HEM_EG_DECAY];
        sustain[curEG] = adsr[HEM_EG_SUSTAIN];
        release[curEG] = adsr[HEM_EG_RELEASE];
    }

    uint64_t OnDataRequest() {
        //-ghostils:Update to use an array and snapshot the values using curEG as the index
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, attack[curEG]);
        Pack(data, PackLocation {8,8}, decay[curEG]);
        Pack(data, PackLocation {16,8}, sustain[curEG]);
        Pack(data, PackLocation {24,8}, release[curEG]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        //-ghostils:Update to use an array and snapshot the values using curEG as the index
        attack[curEG] = Unpack(data, PackLocation {0,8});
        decay[curEG] = Unpack(data, PackLocation {8,8});
        sustain[curEG] = Unpack(data, PackLocation {16,8});
        release[curEG] = Unpack(data, PackLocation {24,8});

        if (attack[curEG] == 0) Start(); // If empty data, initialize
    }

protected:
    /* Set help text. Each help section can have up to 18 characters. Be concise! */
    void SetHelp() {
        /*
        help[HEMISPHERE_HELP_DIGITALS] = "Gate 1=Ch1 2=Ch2";
        help[HEMISPHERE_HELP_CVS] = "Mod 1=Att 2=Rel";
        help[HEMISPHERE_HELP_OUTS] = "Amp A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER] = "A/D/S/R";
        */

        //-ghostils:Update onboard help:
        help[HEMISPHERE_HELP_DIGITALS] = "Gate 1=Ch1 2=Ch2";
        help[HEMISPHERE_HELP_CVS] = "Mod 1=Rel 2=Rel";
        help[HEMISPHERE_HELP_OUTS] = "Amp A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER] = "A/D/S/R";
    }

private:
    int edit_stage;
    int attack[2]; // Attack rate from 1-255 where 1 is fast
    int decay[2]; // Decay rate from 1-255 where 1 is fast
    int sustain[2]; // Sustain level from 1-255 where 1 is low
    int release[2]; // Release rate from 1-255 where 1 is fast

    //-ghostils:TODO Modify to adjust independently for each envelope, we won't be able to do both Attack and Release simultaneously so we either have to build a menu or just do Release.
    //-Parameterize 

    int attack_mod; // Modification to attack from CV1

    int release_mod[2]; // Modification to release from CV2

    //-ghostils:Additions for tracking multiple ADSR's in each Hemisphere:
    int curEG;

    // Stage management
    int stage[2]; // The current ASDR stage of the current envelope
    int stage_ticks[2]; // Current number of ticks into the current stage
    bool gated[2]; // Gate was on in last tick
    simfloat amplitude[2]; // Amplitude of the envelope at the current position

    int GetAmplitudeOf(int ch) {
        return simfloat2int(amplitude[ch]);
    }

    void DrawIndicator() {
        ForEachChannel(ch)
        {
            int w = Proportion(GetAmplitudeOf(ch), HEMISPHERE_MAX_CV, 62);
            //-ghostils:Update to make smaller to allow for additional information on the screen:
            //gfxRect(0, 15 + (ch * 10), w, 6);
            gfxRect(0, 15 + (ch * 3), w, 2);
        }

        //-ghostils:Indicate which ADSR envelope we are selected on:
        if(curEG == 0) {
          gfxPrint(0,22,"A");
          gfxInvert(0,21,7,9);
        }else{
          gfxPrint(0,22,"B");
          gfxInvert(0,21,7,9);
        }
    }

    void DrawADSR() {
        int length = attack[curEG] + decay[curEG] + release[curEG] + HEM_SUSTAIN_CONST; // Sustain is constant because it's a level
        int x = 0;
        x = DrawAttack(x, length);
        x = DrawDecay(x, length);
        x = DrawSustain(x, length);
        DrawRelease(x, length);
    }

    int DrawAttack(int x, int length) {
        //-ghostils:Update to reference curEG:
        int xA = x + Proportion(attack[curEG], length, 62);
        gfxLine(x, BottomAlign(0), xA, BottomAlign(HEM_EG_DISPLAY_HEIGHT), edit_stage != HEM_EG_ATTACK);
        return xA;
    }

    int DrawDecay(int x, int length) {
        //-ghostils:Update to reference curEG:
        int xD = x + Proportion(decay[curEG], length, 62);
        if (xD < 0) xD = 0;
        int yS = Proportion(sustain[curEG], HEM_EG_MAX_VALUE, HEM_EG_DISPLAY_HEIGHT);
        gfxLine(x, BottomAlign(HEM_EG_DISPLAY_HEIGHT), xD, BottomAlign(yS), edit_stage != HEM_EG_DECAY);
        return xD;
    }

    int DrawSustain(int x, int length) {
        int xS = x + Proportion(HEM_SUSTAIN_CONST, length, 62);
        //-ghostils:Update to reference curEG:
        int yS = Proportion(sustain[curEG], HEM_EG_MAX_VALUE, HEM_EG_DISPLAY_HEIGHT);
        if (yS < 0) yS = 0;
        if (xS < 0) xS = 0;
        gfxLine(x, BottomAlign(yS), xS, BottomAlign(yS), edit_stage != HEM_EG_SUSTAIN);
        return xS;
    }

    int DrawRelease(int x, int length) {
        //-ghostils:Update to reference curEG:
        int xR = x + Proportion(release[curEG], length, 62);
        int yS = Proportion(sustain[curEG], HEM_EG_MAX_VALUE, HEM_EG_DISPLAY_HEIGHT);
        gfxLine(x, BottomAlign(yS), xR, BottomAlign(0), edit_stage != HEM_EG_RELEASE);
        return xR;
    }

    void AttackAmplitude(int ch) {
        //-ghostils:Update to reference current channel:
        //-Remove attack_mod CV:
        //int effective_attack = constrain(attack[ch] + attack_mod, 1, HEM_EG_MAX_VALUE);
        int effective_attack = constrain(attack[ch], 1, HEM_EG_MAX_VALUE);
        int total_stage_ticks = Proportion(effective_attack, HEM_EG_MAX_VALUE, HEM_EG_MAX_TICKS_AD);
        int ticks_remaining = total_stage_ticks - stage_ticks[ch];
        if (effective_attack == 1) ticks_remaining = 0;
        if (ticks_remaining <= 0) { // End of attack; move to decay
            stage[ch] = HEM_EG_DECAY;
            stage_ticks[ch] = 0;
            amplitude[ch] = int2simfloat(HEMISPHERE_MAX_CV);
        } else {
            simfloat amplitude_remaining = int2simfloat(HEMISPHERE_MAX_CV) - amplitude[ch];
            simfloat increase = amplitude_remaining / ticks_remaining;
            amplitude[ch] += increase;
        }
    }

    void DecayAmplitude(int ch) {
        //-ghostils:Update to reference current channel:
        int total_stage_ticks = Proportion(decay[ch], HEM_EG_MAX_VALUE, HEM_EG_MAX_TICKS_AD);
        int ticks_remaining = total_stage_ticks - stage_ticks[ch];
        simfloat amplitude_remaining = amplitude[ch] - int2simfloat(Proportion(sustain[ch], HEM_EG_MAX_VALUE, HEMISPHERE_MAX_CV));
        if (sustain[ch] == 1) ticks_remaining = 0;
        if (ticks_remaining <= 0) { // End of decay; move to sustain
            stage[ch] = HEM_EG_SUSTAIN;
            stage_ticks[ch] = 0;
            amplitude[ch] = int2simfloat(Proportion(sustain[ch], HEM_EG_MAX_VALUE, HEMISPHERE_MAX_CV));
        } else {
            simfloat decrease = amplitude_remaining / ticks_remaining;
            amplitude[ch] -= decrease;
        }
    }

    void SustainAmplitude(int ch) {
        //-ghostils:Update to reference current channel:
        amplitude[ch] = int2simfloat(Proportion(sustain[ch] - 1, HEM_EG_MAX_VALUE, HEMISPHERE_MAX_CV));
    }

    void ReleaseAmplitude(int ch) {
        //-ghostils:Update to reference current channel:
        //-CV1 = ADSR A release MOD, CV2 = ADSR A release MOD
        int effective_release = constrain(release[ch] + release_mod[ch], 1, HEM_EG_MAX_VALUE) - 1;
        int total_stage_ticks = Proportion(effective_release, HEM_EG_MAX_VALUE, HEM_EG_MAX_TICKS_R);
        int ticks_remaining = total_stage_ticks - stage_ticks[ch];
        if (effective_release == 0) ticks_remaining = 0;
        if (ticks_remaining <= 0 || amplitude[ch] <= 0) { // End of release; turn off envelope
            stage[ch] = HEM_EG_NO_STAGE;
            stage_ticks[ch] = 0;
            amplitude[ch] = 0;
        } else {
            simfloat decrease = amplitude[ch] / ticks_remaining;
            amplitude[ch] -= decrease;
        }
    }

    int get_modification_with_input(int in) {
        int mod = 0;
        mod = Proportion(DetentedIn(in), HEMISPHERE_MAX_INPUT_CV, HEM_EG_MAX_VALUE / 2);
        return mod;
    }
};
