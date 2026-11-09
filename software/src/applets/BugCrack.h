// Copyright (c) 2022, Korbinian Schreiber
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

#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"
#include "../tiny_dsp.h"

#define BNC_MAX_PARAM 63
#define CH_KICK 0
#define CH_SNARE 1
#define CH_PUNCH_DECAY 2

// same order as the settings
#define CV_MODE_TONE  0
#define CV_MODE_DECAY 1
#define CV_MODE_FM    2 // punch / snap
#define CV_MODE_DROP_OR_BLEND  3
#define CV_MODE_LEVEL 4

#define BNC_MAX_LEVEL 15

class BugCrack : public HemisphereApplet {
public:

    const char* applet_name() {
        return "BugCrack";
    }
    const uint8_t* applet_icon() { return PhzIcons::bugCrack; }

    void Start() {
        tone_kick = 22;
        decay_kick = 60;
        punch = 50;
        decay_punch = 32;

        tone_snare = 14;
        decay_snare = 54; // Snare decay
        snap = 58;
        blend_snare = 36;

        level_kick = BNC_MAX_LEVEL;
        level_snare = BNC_MAX_LEVEL;

        cv_mode_kick = CV_MODE_LEVEL;
        cv_mode_snare = CV_MODE_LEVEL;

        noise = random(0, (1<<12));

        kick = WaveformManager::VectorOscillatorFromWaveform(HS::Sine);
        kick.SetFrequency(Proportion(tone_kick, BNC_MAX_PARAM, 3000) + 3000);
        kick.SetScale((12 << 7) * 3);

        ForEachChannel(ch) levels[ch] = 0;

        env_kick = WaveformManager::VectorOscillatorFromWaveform(HS::Exponential);
        env_kick.SetScale(HEMISPHERE_MAX_CV);
        env_kick.Offset(HEMISPHERE_MAX_CV);
        env_kick.Cycle(0);
        SetEnvDecayKick(decay_kick);

        env_punch = WaveformManager::VectorOscillatorFromWaveform(HS::Exponential);
        env_punch.SetScale(HEMISPHERE_3V_CV);
        env_punch.Offset(HEMISPHERE_3V_CV);
        env_punch.Cycle(0);
        SetEnvDecayPunch(decay_punch);

        snare = WaveformManager::VectorOscillatorFromWaveform(HS::Sine);
        snare.SetFrequency(Proportion(tone_snare, BNC_MAX_PARAM, 60000) + 10000);
        snare.SetScale((12 << 7) * 3);

        env_snare = WaveformManager::VectorOscillatorFromWaveform(HS::Exponential);
        env_snare.SetScale(HEMISPHERE_MAX_CV);
        env_snare.Offset(HEMISPHERE_MAX_CV);
        env_snare.Cycle(0);
        env_noise = WaveformManager::VectorOscillatorFromWaveform(HS::Exponential);
        env_noise.SetScale(HEMISPHERE_MAX_CV);
        env_noise.Offset(HEMISPHERE_MAX_CV);
        env_noise.Cycle(0);
        SetEnvDecaySnare(decay_snare);

        env_snap = WaveformManager::VectorOscillatorFromWaveform(HS::Exponential);
        env_snap.SetScale(HEMISPHERE_3V_CV);
        env_snap.Offset(HEMISPHERE_3V_CV);
        env_snap.Cycle(0);
        SetEnvDecaySnap(decay_snare);
    }

    void Controller() {
        int32_t bd_signal = 0;
        int32_t sd_signal = 0;
        int32_t ns_signal = 0;
        cv_kick = Proportion(DetentedIn(CH_KICK), HEMISPHERE_MAX_INPUT_CV, BNC_MAX_PARAM);
        cv_snare = Proportion(DetentedIn(CH_SNARE), HEMISPHERE_MAX_INPUT_CV, BNC_MAX_PARAM);

        // Kick drum
        if (cv_mode_kick == CV_MODE_TONE) {
            _tone_kick = constrain(tone_kick + cv_kick, 0, BNC_MAX_PARAM);
        } else {
            _tone_kick = tone_kick;
        }
        if (cv_mode_kick == CV_MODE_DECAY) {
            _decay_kick = constrain(decay_kick + cv_kick, 0, BNC_MAX_PARAM);
        } else {
            _decay_kick = decay_kick;
        }
        if (cv_mode_kick == CV_MODE_FM) {
            _punch = constrain(punch + cv_kick, 0, BNC_MAX_PARAM);
        } else {
            _punch = punch;
        }
        if (cv_mode_kick == CV_MODE_DROP_OR_BLEND) {
            _decay_punch = constrain(decay_punch + cv_kick, 0, BNC_MAX_PARAM);
        } else {
            _decay_punch = decay_punch;
        }
        _level_kick = Proportion(level_kick, BNC_MAX_LEVEL, BNC_MAX_PARAM); // 0 = mute
        if (cv_mode_kick == CV_MODE_LEVEL) {
            _level_kick = constrain(_level_kick - cv_kick, 0, BNC_MAX_PARAM);
        }
        if (Clock(CH_KICK, 0)) {
            SetEnvDecayKick(_decay_kick);
            SetEnvDecayPunch(_decay_punch);
            env_kick.Start();
            env_punch.Start();
            kick.Start();
        }
        if (!env_kick.GetEOC()) {
            // base frequency
            int freq_kick = Proportion(_tone_kick, BNC_MAX_PARAM, 3000) + 3000;
            // punchy FM drop
            if (!env_punch.GetEOC()) {
                int df = Proportion(env_punch.Next(), HEMISPHERE_3V_CV, freq_kick);
                df = Proportion(_punch, BNC_MAX_PARAM/4, df);
                freq_kick += df;
            }
            kick.SetFrequency(freq_kick);
            levels[0] = env_kick.Next();
            levels[0] = Proportion(_level_kick, BNC_MAX_PARAM, levels[0]);
            bd_signal = Proportion(levels[0], HEMISPHERE_MAX_CV, kick.Next());
            // Because of overtones induced by the linear interpolation of the
            // sine wave vector oscilator, we have to low-pass filter the signal
            bd_signal = filter_lp.filter(bd_signal, freq_kick);
        }

        // Snare drum
        noise = random(0, HEMISPHERE_MAX_CV); // simple random noise works best I've found
        if (cv_mode_snare == CV_MODE_TONE) {
            _tone_snare = constrain(tone_snare + cv_snare, 0, BNC_MAX_PARAM);
        } else {
            _tone_snare = tone_snare;
        }
        if (cv_mode_snare == CV_MODE_DECAY) {
            _decay_snare = constrain(decay_snare + cv_snare, 0, BNC_MAX_PARAM);
        } else {
            _decay_snare = decay_snare;
        }
        if (cv_mode_snare == CV_MODE_FM) {
            _snap = constrain(snap + cv_snare, 0, BNC_MAX_PARAM);
        } else {
            _snap = snap;
        }
        if (cv_mode_snare == CV_MODE_DROP_OR_BLEND) {
            _blend_snare = constrain(blend_snare + cv_snare, 0, BNC_MAX_PARAM);
        } else {
            _blend_snare = blend_snare;
        }
        _level_snare = Proportion(level_snare, BNC_MAX_LEVEL, BNC_MAX_PARAM); // 0 = mute
        if (cv_mode_snare == CV_MODE_LEVEL) {
            _level_snare = constrain(_level_snare - cv_snare, 0, BNC_MAX_PARAM);
        }
        if (Clock(CH_SNARE, 0)) {
            SetEnvDecaySnare(_decay_snare);
            SetEnvDecaySnap(_decay_snare);
            env_snare.Start();
            env_noise.Start();
            env_snap.Start();
            snare.Start();
        }
        if (!env_noise.GetEOC()) {
            int64_t freq_snare = Proportion(_tone_snare, BNC_MAX_PARAM, 600) + 100;
            freq_snare *= 100;
            if (!env_snap.GetEOC()) {
                int64_t df = Proportion(env_snap.Next(), HEMISPHERE_3V_CV, freq_snare/1024);
                df = Proportion(_snap, BNC_MAX_PARAM/4, df);
                df *= 1024;
                freq_snare += df;
            }
            snare.SetFrequency(freq_snare);

            // noise levels
            levels[1] = env_noise.Next();
            levels[1] = Proportion(_level_snare, BNC_MAX_PARAM, levels[1]);
            // same level as level*noise/3V on 5V hardware; on 10V hardware the
            // old formula overflowed the 16-bit filter and the snare crackled
            ns_signal = Proportion(levels[1], HEMISPHERE_MAX_CV, noise) * 5 / 3;
            ns_signal = constrain(ns_signal, -32767, 32767);
            filter_sv.feed(ns_signal, (Proportion(_tone_snare, BNC_MAX_PARAM, 60000) + 100000), 500);
            ns_signal = filter_sv.get_bp();

            // osc levels
            levels[2] = env_snare.Next();
            levels[2] = Proportion(_level_snare, BNC_MAX_PARAM, levels[2]);
            sd_signal = Proportion(levels[2], HEMISPHERE_3V_CV, snare.Next());
            sd_signal = filter_lp2.filter(sd_signal, freq_snare);

            // blend osc and noise
            sd_signal = Proportion(BNC_MAX_PARAM - _blend_snare, BNC_MAX_PARAM, sd_signal);
            sd_signal += Proportion(_blend_snare, BNC_MAX_PARAM, ns_signal);
        }

        // Kick Drum Output
        Out(0, bd_signal);

        // Snare Drum Output
        Out(1, sd_signal + (mix_outs ? bd_signal : 0));
    }

    void View() final;

    void OnButtonPress() {
      if (cursor == 12) mix_outs = !mix_outs;
      else
        CursorToggle();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 12);
            return;
        }

        switch (cursor) {
        // Kick drum
        case 0:
            tone_kick = constrain(tone_kick + direction, 0, BNC_MAX_PARAM);
            break;
        case 1:
            decay_kick = constrain(decay_kick + direction, 0, BNC_MAX_PARAM);
            break;
        case 2:
            punch = constrain(punch + direction, 0, BNC_MAX_PARAM);
            break;
        case 3:
            decay_punch = constrain(decay_punch + direction, 0, BNC_MAX_PARAM);
            break;
        case 4:
            level_kick = constrain(level_kick + direction, 0, BNC_MAX_LEVEL);
            break;
        case 5:
            cv_mode_kick = constrain(cv_mode_kick + direction, 0, 4);
            break;

        // Snare drum
        case 6:
            tone_snare = constrain(tone_snare + direction, 0, BNC_MAX_PARAM);
            break;
        case 7:
            decay_snare = constrain(decay_snare + direction, 0, BNC_MAX_PARAM);
            break;
        case 8:
            snap = constrain(snap + direction, 0, BNC_MAX_PARAM);
            break;
        case 9:
            blend_snare = constrain(blend_snare + direction, 0, BNC_MAX_PARAM);
            break;
        case 10:
            level_snare = constrain(level_snare + direction, 0, BNC_MAX_LEVEL);
            break;
        case 11:
            cv_mode_snare = constrain(cv_mode_snare + direction, 0, 4);
            break;
        }

    }

    uint64_t OnDataRequest() {
        // parameters are 6 bits each
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, tone_kick);
        Pack(data, PackLocation {6,6}, decay_kick);
        Pack(data, PackLocation {12,6}, punch);
        Pack(data, PackLocation {18,6}, decay_punch);

        Pack(data, PackLocation {24,6}, tone_snare);
        Pack(data, PackLocation {30,6}, decay_snare);
        Pack(data, PackLocation {36,6}, snap);
        Pack(data, PackLocation {42,6}, blend_snare);

        Pack(data, PackLocation {48,3}, cv_mode_kick);
        Pack(data, PackLocation {51,1}, 1); // CV modes in settings order
        Pack(data, PackLocation {52,3}, cv_mode_snare);

        Pack(data, PackLocation {56, 1}, mix_outs);

        // Levels were added later and squeezed into the free bits. They are
        // stored as attenuation so older presets (all zeros there) load at full level.
        const int atten_kick = BNC_MAX_LEVEL - level_kick;
        const int atten_snare = BNC_MAX_LEVEL - level_snare;
        Pack(data, PackLocation {57,4}, atten_kick);
        Pack(data, PackLocation {61,3}, atten_snare & 0x7);
        Pack(data, PackLocation {55,1}, atten_snare >> 3);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        tone_kick = Unpack(data, PackLocation {0,6});
        decay_kick = Unpack(data, PackLocation {6,6});
        punch = Unpack(data, PackLocation {12,6});
        decay_punch = Unpack(data, PackLocation {18,6});

        tone_snare = Unpack(data, PackLocation {24,6});
        decay_snare = Unpack(data, PackLocation {30,6});
        snap = Unpack(data, PackLocation {36,6});
        blend_snare = Unpack(data, PackLocation {42,6});

        cv_mode_kick = constrain(Unpack(data, PackLocation {48,3}), 0, 4);
        cv_mode_snare = constrain(Unpack(data, PackLocation {52,3}), 0, 4);
        if (!Unpack(data, PackLocation {51,1})) {
            // older presets had level first: atn, ton, dec, FM, dro/bln
            cv_mode_kick = (cv_mode_kick + 4) % 5;
            cv_mode_snare = (cv_mode_snare + 4) % 5;
        }

        mix_outs = Unpack(data, PackLocation {56,1});

        level_kick = BNC_MAX_LEVEL - Unpack(data, PackLocation {57,4});
        level_snare = BNC_MAX_LEVEL - (Unpack(data, PackLocation {61,3})
                                       | (Unpack(data, PackLocation {55,1}) << 3));
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Kick";
    help[HELP_DIGITAL2] = "Snare";
    help[HELP_CV1]      = CV_MODE_NAMES_BD[cv_mode_kick];
    help[HELP_CV2]      = CV_MODE_NAMES_SN[cv_mode_snare];
    help[HELP_OUT1]     = "Kick";
    help[HELP_OUT2]     = mix_outs ? "Kik+Snr" : "Snare";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
    int cursor = 0;
    VectorOscillator kick;
    VectorOscillator env_kick;
    VectorOscillator env_punch;

    VectorOscillator snare;
    VectorOscillator env_snare;
    VectorOscillator env_noise;
    VectorOscillator env_snap;

    uint32_t noise;

    TDSP::FilterLP filter_lp;
    TDSP::FilterLP filter_lp2;
    TDSP::FilterStateVariable filter_sv;

    int cv_kick;
    int cv_snare;
    int levels[3]; // For display

    // Settings
    int tone_kick;
    int _tone_kick;
    int decay_kick;
    int _decay_kick;
    int punch;
    int _punch;
    int decay_punch;
    int _decay_punch;

    int tone_snare;
    int _tone_snare;
    int decay_snare;
    int _decay_snare;
    int snap;
    int _snap;
    int blend_snare;
    int _blend_snare;

    int level_kick;
    int _level_kick;
    int level_snare;
    int _level_snare;

    bool mix_outs = true;

    // short names, in the same order as the settings
    const char *CV_MODE_NAMES_BD[5] = {"ton", "dec", "FM", "dro", "atn"};
    const char *CV_MODE_NAMES_SN[5] = {"ton", "dec", "FM", "bln", "atn"};

    const char *PARAM_LABELS[13] = {
        "Kik Tone", "Kik Decay", "Kik Punch", "Kik Drop", "Kik Level", "Kik CV",
        "Snr Tone", "Snr Decay", "Snr Snap", "Snr Blend", "Snr Level", "Snr CV",
        "MixOut"};

    uint8_t cv_mode_kick;
    uint8_t cv_mode_snare;

    void DrawInterface() {
        DrawDrumBody(1, _tone_kick, _decay_kick, _punch, _decay_punch, 0);
        DrawDrumBody(32, _tone_snare, _decay_snare, _snap, _blend_snare, 1);

        // name shown in the header while editing
        SetLabel(PARAM_LABELS[cursor]);
        SetAux(false);

        switch (cursor) {
            // Kick drum
            case 0:
                gfxPrint(1, 55, Proportion(_tone_kick, BNC_MAX_PARAM, 30) + 30);
                gfxIcon(22, 54, HERTZ_ICON);
                break;
            case 1:
                gfxPrint(1, 55, "decay"); break;
            case 2:
                gfxPrint(1, 55, "punch"); break;
            case 3:
                gfxPrint(1, 55, "drop"); break;
            case 4:
                if (level_kick) {
                    gfxPrint(1, 55, "lv");
                    gfxPrint(level_kick);
                } else {
                    gfxPrint(1, 55, "mute");
                }
                break;
            case 5: // CV modes
                gfxIcon(1, 57, CV_ICON);
                gfxPrint(10, 55, CV_MODE_NAMES_BD[cv_mode_kick]);
                break;

            // Snare drum
            case 6:
                gfxPrint(35, 55, Proportion(_tone_snare, BNC_MAX_PARAM, 600) + 100);
                gfxIcon(54, 54, HERTZ_ICON);
                break;
            case 7:
                gfxPrint(32, 55, "decay"); break;
            case 8:
                gfxPrint(32, 55, "snap"); break;
            case 9:
                gfxPrint(32, 55, "blend"); break;
            case 10:
                if (level_snare) {
                    gfxPrint(32, 55, "lv");
                    gfxPrint(level_snare);
                } else {
                    gfxPrint(32, 55, "mute");
                }
                break;
            case 11: // CV modes
                gfxIcon(32, 57, CV_ICON);
                gfxPrint(41, 55, CV_MODE_NAMES_SN[cv_mode_snare]);
                break;

            case 12: // mix outs @ B/D
                gfxPrint(1, 55, "MixOut:");
                gfxIcon(50, 55, mix_outs ? CHECK_ON_ICON : CHECK_OFF_ICON);
                break;
        }
        if (EditMode()) gfxInvert(1 + (cursor<6?0:31), 54, 31, 9);

        // Level indicators
        ForEachChannel(ch)
            gfxInvert(1 + (31*ch), 53 - ProportionCV(levels[ch], 34),
                      30, ProportionCV(levels[ch], 34));
    }

    void DrawDrumBody(byte x, byte tone, byte decay, byte punch, byte pdecay, bool is_snare);

    void SetEnvDecayKick(int decay) {
        // 100 ms - 1000 ms -> 10 Hz - 1 Hz
        env_kick.SetFrequency(1000 - Proportion(decay, BNC_MAX_PARAM, 900));
    }
    void SetEnvDecayPunch(int decay) {
        // 25 ms - 200 ms -> 4000 cHz - 500 cHz
        env_punch.SetFrequency(
            4000 - Proportion(decay, BNC_MAX_PARAM, 3500));
    }

    void SetEnvDecaySnare(int decay) {
        // 12.5 ms - 1000 ms -> 80 Hz - 1 Hz
        env_snare.SetFrequency(8000 - Proportion(decay, BNC_MAX_PARAM, 7900));
        env_noise.SetFrequency(4000 - Proportion(decay, BNC_MAX_PARAM, 3950));
    }
    void SetEnvDecaySnap(int decay) {
        // 12.5 ms - 200 ms -> 80 Hz - 5 Hz
        env_snap.SetFrequency(
            8000 - Proportion(decay, BNC_MAX_PARAM, 4000));
    }
};

FLASHMEM void BugCrack::View() {
  DrawInterface();
}

FLASHMEM __attribute__((noinline))
void BugCrack::DrawDrumBody(byte x, byte tone, byte decay, byte punch, byte pdecay, bool is_snare) {
    const int8_t wmin = 10;
    const int8_t wmax = 30;
    const int8_t hmin = 6;
    const int8_t hmax = 16;

    int8_t w = Proportion(decay, BNC_MAX_PARAM, wmax - wmin) + wmin;
    int8_t h = Proportion(punch, BNC_MAX_PARAM, hmax - hmin) + hmin;
    int8_t body_h = (2*h/hmin - 1) + hmin - 2;
    int8_t r = Proportion(pdecay, BNC_MAX_PARAM, body_h);
    int8_t y = 40 - Proportion(tone, BNC_MAX_PARAM, 18);

    int8_t cx = x + wmax/2;
    int8_t cy = y;

    // Body
    int dx = w/5;
    gfxLine(
        cx - dx + 1, cy - body_h/2,
        cx + dx - 1, cy - body_h/2);
    gfxLine(
        cx - dx + 2, cy - body_h/2-1,
        cx + dx - 2, cy - body_h/2-1);
    gfxLine(
        cx - dx + 1, cy + body_h/2,
        cx + dx - 1, cy + body_h/2);
    gfxRect(
        cx - dx + 1, cy - body_h/2 + 1,
        2*dx - 1, r - 1);

    // Legs
    for(int p=-1; p<=1; p+=2) { // parity for both sides
        int _dx = p*dx;
        // Front legs
        gfxLine(
            cx + _dx, cy - body_h/2,
            cx + p*w/3, cy - h/2);
        // Mid legs
        gfxLine(
            cx + _dx, cy - 1,
            cx + _dx + 2*p, cy-1);
        gfxLine(
            cx + _dx + 2*p, cy - 1,
            cx + p*w/2, cy - 2);
        // Rear legs
        gfxLine(
            cx + _dx, cy + 1,
            cx + p*w/3, cy + hmax/h);
        gfxLine(
            cx + p*w/3, cy + hmax/h,
            cx + p*w/2, cy + h/2);
        // Body flank
        gfxLine(
            cx+_dx, cy-body_h/2+1,
            cx+_dx, cy+body_h/2-1);
        if(is_snare) {
            // Some feelers for snare bug
            gfxLine(
                cx + p, cy - body_h/2 - 2,
                cx + 2*p, cy - body_h/2 - 2 - hmax/h);
        } else {
            // Some eyes on the kick bug
            gfxInvert(
                cx + _dx - p, cy - body_h/2 + 1,
                1, 1);
        }
    }
}
