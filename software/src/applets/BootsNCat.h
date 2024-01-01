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

#include "vector_osc/HSVectorOscillator.h"
#include "vector_osc/WaveformManager.h"

class BootsNCat : public HemisphereApplet {
public:

    static constexpr uint8_t BNC_MAX_PARAM = 63;

    const char* applet_name() {
        return "BootsNCat";
    }

    void Start() {
        tone[0] = 32; // Bass drum freq
        decay[0] = 32; // Bass drum decay
        tone[1] = 55; // Snare low limit
        decay[1] = 16; // Snare decay
        noise_tone_countdown = 1;
        blend = 0;

        bass = WaveformManager::VectorOscillatorFromWaveform(HS::Triangle);
        SetBDFreq();
        bass.SetScale((12 << 7) * 3); // Audio signal is -3V to +3V due to DAC asymmetry

        ForEachChannel(ch)
        {
            levels[ch] = 0;
            eg[ch] = WaveformManager::VectorOscillatorFromWaveform(HS::Sawtooth);
            eg[ch].SetFrequency(decay[ch]);
            eg[ch].SetScale(ch ? HEMISPHERE_3V_CV : HEMISPHERE_MAX_CV);
            eg[ch].Offset(ch ? HEMISPHERE_3V_CV : HEMISPHERE_MAX_CV);
            eg[ch].Cycle(0);
            SetEGFreq(ch);
        }
    }

    void Controller() {
        // Bass and snare signals are calculated independently
        int32_t signal = 0;
        int32_t bd_signal = 0;
        int32_t sd_signal = 0;

        ForEachChannel(ch)
        {
            if (Changed(ch)) eg[ch].SetScale((ch ? HEMISPHERE_3V_CV : HEMISPHERE_MAX_CV) - In(ch));
            if (Clock(ch, 1)) eg[ch].Start(); // Use physical-only clocking
        }

        // Calculate bass drum signal
        if (!eg[0].GetEOC()) {
            levels[0] = eg[0].Next();
            bd_signal = Proportion(levels[0], HEMISPHERE_MAX_CV, bass.Next());
        }

        // Calculate snare drum signal
        if (--noise_tone_countdown == 0) {
            noise = random(0, (12 << 7) * 6) - ((12 << 7) * 3);
            noise_tone_countdown = BNC_MAX_PARAM - tone[1] + 1;
        }

        if (!eg[1].GetEOC()) {
            levels[1] = eg[1].Next();
            sd_signal = Proportion(levels[1], HEMISPHERE_MAX_CV, noise);
        }

        // Bass Drum Output
        signal = Proportion((BNC_MAX_PARAM - blend) + BNC_MAX_PARAM, BNC_MAX_PARAM * 2, bd_signal);
        signal += Proportion(blend, BNC_MAX_PARAM * 2, sd_signal); // Blend in snare drum
        Out(0, signal);

        // Snare Drum Output
        signal = Proportion((BNC_MAX_PARAM - blend) + BNC_MAX_PARAM, BNC_MAX_PARAM * 2, sd_signal);
        signal += Proportion(blend, BNC_MAX_PARAM * 2, bd_signal); // Blend in bass drum
        Out(1, signal);
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, 4);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 4);
            return;
        }

        if (cursor == 4) { // Blend
            blend = constrain(blend + direction, 0, BNC_MAX_PARAM);
        } else {
            byte ch = cursor > 1 ? 1 : 0;
            byte c = cursor;
            if (ch) c -= 2;

            if (c == 0) { // Tone
                tone[ch] = constrain(tone[ch] + direction, 0, BNC_MAX_PARAM);
                if (ch == 0) SetBDFreq();
            }

            if (c == 1) { // Decay
                decay[ch] = constrain(decay[ch] + direction, 0, BNC_MAX_PARAM);
                SetEGFreq(ch);
            }
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, tone[0]);
        Pack(data, PackLocation {6,6}, decay[0]);
        Pack(data, PackLocation {12,6}, tone[1]);
        Pack(data, PackLocation {18,6}, decay[1]);
        Pack(data, PackLocation {24,6}, blend);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        tone[0] = Unpack(data, PackLocation {0,6});
        decay[0] = Unpack(data, PackLocation {6,6});
        tone[1] = Unpack(data, PackLocation {12,6});
        decay[1] = Unpack(data, PackLocation {18,6});
        blend = Unpack(data, PackLocation {24,6});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1,2 Play";
        help[HEMISPHERE_HELP_CVS]      = "Atten. 1=BD 2=SD";
        help[HEMISPHERE_HELP_OUTS]     = "A=Left B=Right";
        help[HEMISPHERE_HELP_ENCODER]  = "Preset/Pan";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor = 0;
    VectorOscillator bass;
    VectorOscillator eg[2];
    int noise_tone_countdown = 0;
    uint32_t noise;
    int levels[2]; // For display
    
    // Settings
    int tone[2];
    int decay[2];
    int8_t blend;

    void DrawInterface() {
        const uint8_t w = 16;
        const uint8_t x = 45;

        gfxPrint(1, 15, "BD Tone");
        DrawSlider(x, 15, w, tone[0], BNC_MAX_PARAM, cursor == 0);

        gfxPrint(1, 25, "  Decay");
        DrawSlider(x, 25, w, decay[0], BNC_MAX_PARAM, cursor == 1);

        gfxPrint(1, 35, "SD Tone");
        DrawSlider(x, 35, w, tone[1], BNC_MAX_PARAM, cursor == 2);

        gfxPrint(1, 45, "  Decay");
        DrawSlider(x, 45, w, decay[1], BNC_MAX_PARAM, cursor == 3);

        gfxPrint(1, 55, "Blend");
        DrawSlider(x, 55, w, blend, BNC_MAX_PARAM, cursor == 4);

        // Level indicators
        ForEachChannel(ch) gfxInvert(1, 14 + (20 * ch), ProportionCV(levels[ch], 42), 9);
    }

    void SetBDFreq() {
        bass.SetFrequency(Proportion(tone[0], BNC_MAX_PARAM, 3000) + 3000);
    }

    void SetEGFreq(byte ch) {
        eg[ch].SetFrequency(1000 - Proportion(decay[ch], BNC_MAX_PARAM, 900));
    }
};
