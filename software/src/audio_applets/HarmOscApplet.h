#include "synth_waveform.h"



class HarmOscApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() override {
        return "HarmOsc";
    }

    static constexpr int max_partials = 16;
    static constexpr float detune_resolution = 256.0f;
    static constexpr float amplitude_resolution = 255.0f;

    void Start() override {
        vca_cv.Acquire();
        vca_cv.Method(INTERPOLATION_LINEAR);
        vca.rectify(true);
        PatchCable(input_stream, 0, mixer, 0);
        for (int i = 0; i < max_partials; ++i) {
            PatchCable(*partial[i], 0, harmosc, i);
        }
        PatchCable(vca_cv, 0, vca, 1);
        PatchCable(harmosc, 0, vca, 0);
        PatchCable(vca, 0, mixer, 1);
        InitWaveform(amplitudes, partial_ratios, max_partials);
    }

    void Unload() override {
        vca_cv.Release();
        AllowRestart();
    }

    void Controller() override {
        float freq = PitchToRatio(pitch + pitch_cv.In()) * C3;
        float total_amp = 0.0f;
        for (int i = 0; i < max_partials; ++i) {
            partial[i]->frequency(freq * ((float)partial_ratios[i] / detune_resolution));
            float amp = ((float)amplitudes[i]) / amplitude_resolution;
            partial[i]->amplitude(amp);
            total_amp += amp;
        }
        for (int i = 0; i < max_partials; ++i) {
            harmosc.gain(i, (total_amp > 0.0f) ? 1.0f / total_amp : 0.0f);  // normalize waveform, don't divide by 0
        }
        // stolen from original OscApplet
        float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);
        float gain = dbToScalar(level);
        if (level_cv.source) {
            vca.bias(0.0f);
            vca.level(gain);
            vca_cv.Push(float_to_q15(dbToScalar(-48 * (1.0f - level_cv.InF()))));
        } else {
            vca.bias(gain);
            vca.level(0.0f);
        }
        // There's a good chance of phase correlation if the incoming signal is
        // internal, so use equal amplitude
        mixer.gain(1, m);
        mixer.gain(0, 1.0f - m);
    }

    void View() override {
        gfxIcon(4 + 00, 26, NOTE_ICON);  // pitch
        gfxIcon(4 + 16, 26, PhzIcons::speaker);  // level
        gfxIcon(4 + 32, 26, BEAKER_ICON);  // mix
        if (partial_detune)
            gfxIcon(4 + 49, 26, PhzIcons::tuner);  // partial ratios
        else
            gfxIcon(4 + 47, 26, METER_ICON);  // amplitudes


        switch (cursor) {
            case OCTAVE:
            case PITCH:
            case PITCH_CV:
                gfxStartCursor(1, 15);
                gfxPrintTuningIndicator(pitch);
                gfxEndCursor(cursor == OCTAVE);
                gfxStartCursor(11, 15);
                gfxPrintPitchHz(pitch);
                gfxEndCursor(cursor == PITCH);

                gfxStartCursor();
                gfxPrint(pitch_cv);
                gfxEndCursor(cursor == PITCH_CV, false, pitch_cv.InputName());

                gfxInvert(3, 25, 10, 10);
                break;
            case LEVEL:
            case LEVEL_CV:
                gfxStartCursor(1, 15);
                gfxPrint(1, 15, "Lvl:");
                gfxStartCursor();
                graphics.printf("%3ddB", level);
                gfxEndCursor(cursor == LEVEL);

                gfxStartCursor();
                gfxPrint(level_cv);
                gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

                gfxInvert(3+16, 25, 10, 10);
                break;
            case MIX:
            case MIX_CV:
                gfxPrint(1, 15, "Mix: ");
                gfxStartCursor();
                graphics.printf("%3d%%", mix);
                gfxEndCursor(cursor == MIX);

                gfxStartCursor();
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

                gfxInvert(3+32, 25, 10, 10);
                break;
            default:
                int shifted_val;
                int int_part;
                int dec_part;
                gfxPos(1, 15);
                if (partial_detune) {
                    graphics.printf("R %X:",cursor-PARTIAL1);
                    shifted_val = ((float)partial_ratios[cursor-PARTIAL1] / detune_resolution) * 100;
                    int_part = shifted_val / 100;
                    dec_part = shifted_val % 100;
                } else {
                    graphics.printf("A %X:",cursor-PARTIAL1);
                    shifted_val = ((float)amplitudes[cursor-PARTIAL1] / amplitude_resolution) * 100;
                    int_part = shifted_val / 100;
                    dec_part = shifted_val % 100;
                }
                graphics.printf("%2d.%02d", int_part, dec_part);

                gfxInvert(3+48, 25, 10, 10);
                break;
        }

        // draw parial sliders
        for (int i = 0; i < max_partials; ++i) {
            const int y0 = 36;
            const int y1 = 64;
            int x = i * (64 / max_partials) + 2;
            int y = ((float)amplitudes[i] / amplitude_resolution) * (y1 - y0);
            gfxDottedLine(x, y0, x, y1, (uint8_t)2U);
            if (cursor == PARTIAL1 + i) gfxInvert(x, y0, 1, y1 - y0);
            gfxLine(x, y1-y, x, y1);
        }
        gfxDisplayInputMapEditor();
    }

#define HARMOSC_PARAMS \
    pitch, level, mix

    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(HARMOSC_PARAMS);
        data[1] = PackPackables(pitch_cv, level_cv);
        data[2] = PackPackables(mix_cv);
        // for (int i = 3; i < 3 + max_partials; ++i) {
        //     data[i] = PackPackables(amplitudes[i-3]);
        // }
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], HARMOSC_PARAMS);
        UnpackPackables(data[1], pitch_cv, level_cv);
        UnpackPackables(data[2], mix_cv);
        // for (int i = 3; i < 3 + max_partials; ++i) {
        //     UnpackPackables(data[i], amplitudes[i-3]);
        // }
    }

    void OnButtonPress() override {
        if (CheckEditInputMapPress(cursor,
            IndexedInput(PITCH_CV, pitch_cv),
            IndexedInput(MIX_CV, mix_cv),
            IndexedInput(LEVEL_CV, level_cv)
            ))
        return;
        CursorToggle();
    }

    void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, PARTIAL16);
            return;
        }
        if (EditSelectedInputMap(direction)) return;

        const int max_pitch = 7 * 12 * 128;
        const int min_pitch = -3 * 12 * 128;
        switch (cursor) {
            case OCTAVE:
                pitch = constrain(pitch + direction * 1 * 128, min_pitch, max_pitch);
                break;
            case PITCH:
                pitch = constrain(pitch + direction * 4, min_pitch, max_pitch);
                break;
            case PITCH_CV:
                pitch_cv.ChangeSource(direction);
                break;
            case LEVEL:
                level = constrain(level + direction, -90, 90);
                break;
            case LEVEL_CV:
                level_cv.ChangeSource(direction);
                break;
            case MIX:
                mix = constrain(mix + direction, 0, 100);
                break;
            case MIX_CV:
                mix_cv.ChangeSource(direction);
                break;
            default:  // partial faders (simplify this crap with int)
                if (partial_detune)
                    partial_ratios[cursor-PARTIAL1] = constrain(partial_ratios[cursor-PARTIAL1] + direction, 0, 32767);
                else
                    amplitudes[cursor-PARTIAL1] = constrain(amplitudes[cursor-PARTIAL1] + direction, 0, 255);
                break;
        }
    }

    void AuxButton() {
        if (cursor >= PARTIAL1) partial_detune = !partial_detune;
    }

    AudioStream* InputStream() override {
        return &input_stream;
    }
    AudioStream* OutputStream() override {
        return &mixer;
    }

    void InitWaveform(int* amp, int* rat, int n_partials) {
        for (int i = 0; i < n_partials; ++i) { // approximate saw wave
            amp[i] = 255 / (i + 1);
            rat[i] = 256 * (i + 1);
        }
    }

protected:
    void SetHelp() override {}

private:
    enum Cursor : int8_t {
        OCTAVE,
        PITCH,
        PITCH_CV,
        LEVEL,
        LEVEL_CV,
        MIX,
        MIX_CV,
        PARTIAL1, PARTIAL2, PARTIAL3, PARTIAL4, PARTIAL5, PARTIAL6, PARTIAL7, PARTIAL8,
        PARTIAL9, PARTIAL10, PARTIAL11, PARTIAL12, PARTIAL13, PARTIAL14, PARTIAL15, PARTIAL16,
    };

    int8_t cursor = OCTAVE;
    int16_t pitch = 1 * 12 * 128; // C4
    int8_t level = 0; // dB
    int8_t mix = 100;
    bool partial_detune = false;

    int amplitudes[max_partials];
    int partial_ratios[max_partials];

    CVInputMap pitch_cv;
    CVInputMap level_cv;
    CVInputMap mix_cv;

    AudioPassthrough<MONO> input_stream;
    AudioSynthWaveform partial1;
    AudioSynthWaveform partial2;
    AudioSynthWaveform partial3;
    AudioSynthWaveform partial4;
    AudioSynthWaveform partial5;
    AudioSynthWaveform partial6;
    AudioSynthWaveform partial7;
    AudioSynthWaveform partial8;
    AudioSynthWaveform partial9;
    AudioSynthWaveform partial10;
    AudioSynthWaveform partial11;
    AudioSynthWaveform partial12;
    AudioSynthWaveform partial13;
    AudioSynthWaveform partial14;
    AudioSynthWaveform partial15;
    AudioSynthWaveform partial16;
    AudioSynthWaveform* partial[max_partials] = {
        &partial1,
        &partial2,
        &partial3,
        &partial4,
        &partial5,
        &partial6,
        &partial7,
        &partial8,
        &partial9,
        &partial10,
        &partial11,
        &partial12,
        &partial13,
        &partial14,
        &partial15,
        &partial16
    };
    InterpolatingStream<> vca_cv;
    AudioVCA vca;
    AudioMixer<max_partials> harmosc;
    AudioMixer<2> mixer;
};
