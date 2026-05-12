#include "synth_waveform.h"

class HarmOscApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() {
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
            PatchCable(partial[i], 0, harmosc, i);
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
        float amp = 0.0f;
        float total_amp = 0.0f;
        for (int i = 0; i < max_partials; ++i) {
            partial[i].frequency(freq * ((float)partial_ratios[i] / detune_resolution));
            amp = constrain(((float)amplitudes[i] + (float)amp_cv[i].InRescaled(255)) / amplitude_resolution, 0.0f, 1.0f);
            partial[i].amplitude(amp);  // probably a better way to do this
            total_amp += amp;
        }
        for (int i = 0; i < max_partials; ++i) {
            harmosc.gain(i, (total_amp > 0.0f) ? 1.0f / total_amp : 0.0f);  // normalize waveform, don't divide by 0
        }

        // stolen from OscApplet
        float gain = dbToScalar(level);
        if (level_cv.source) {
            vca.bias(0.0f);
            vca.level(gain);
            vca_cv.Push(float_to_q15(dbToScalar(-48 * (1.0f - level_cv.InF()))));
        } else {
            vca.bias(gain);
            vca.level(0.0f);
        }
        // There's a good chance of phase correlation if the incoming signal is internal, so use equal amplitude
        float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);
        mixer.gain(1, m);
        mixer.gain(0, 1.0f - m);
    }

    void View() override {
        gfxIcon(4 + 00, 26, NOTE_ICON);  // pitch
        gfxIcon(4 + 16, 26, PhzIcons::speaker);  // level
        gfxIcon(4 + 32, 26, BEAKER_ICON);  // mix
        if (partial_param == 2) gfxIcon(4 + 48, 27, CV_ICON);  // cv mapping
        else if (partial_param == 1) gfxIcon(4 + 49, 26, PhzIcons::tuner);  // partial ratios
        else if (partial_param == 0) gfxIcon(4 + 47, 26, METER_ICON);  // amplitudes

        switch (cursor) {
            case OCTAVE:
            case PITCH:
            case PITCH_CV: {
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
            }
            case LEVEL:
            case LEVEL_CV: {
                // gfxStartCursor(1, 15);
                gfxPrint(1, 15, "Lvl:");
                gfxStartCursor();
                graphics.printf("%3ddB", level);
                gfxEndCursor(cursor == LEVEL);

                gfxStartCursor();
                gfxPrint(level_cv);
                gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

                gfxInvert(3 + 16, 25, 10, 10);
                break;
            }
            case MIX:
            case MIX_CV: {
                gfxPrint(1, 15, "Mix: ");
                gfxStartCursor();
                graphics.printf("%3d%%", mix);
                gfxEndCursor(cursor == MIX);

                gfxStartCursor();
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());

                gfxInvert(3 + 32, 25, 10, 10);
                break;
            }
            default: {
                int shifted_val;
                int int_part;
                int dec_part;
                gfxPos(1, 15);

                if (partial_param == 2) {
                    graphics.printf("C %X:", cursor - PARTIAL1);
                    gfxStartCursor(48, 14);
                    gfxPrint(amp_cv[cursor - PARTIAL1]);
                    gfxEndCursor(cursor >= PARTIAL1, false, amp_cv[cursor - PARTIAL1].InputName());

                } else if (partial_param == 1) {
                    graphics.printf("R %X:", cursor - PARTIAL1);
                    shifted_val = ((float)partial_ratios[cursor - PARTIAL1] / detune_resolution) * 100;
                    int_part = shifted_val / 100;
                    dec_part = shifted_val % 100;

                } else if (partial_param == 0) {
                    graphics.printf("A %X:", cursor - PARTIAL1);
                    shifted_val = ((float)amplitudes[cursor - PARTIAL1] / amplitude_resolution) * 100;
                    int_part = shifted_val / 100;
                    dec_part = shifted_val % 100;
                }

                if (partial_param < 2) graphics.printf("%2d.%02d", int_part, dec_part);
                gfxInvert(3 + 48, 25, 10, 10);
                break;
            }
        }

        // draw parial sliders
        for (int i = 0; i < max_partials; ++i) {
            const int y0 = 36;
            const int y1 = 64;
            int x = i * (64 / max_partials) + 2;
            float amp = constrain(((float)amplitudes[i] + (float)amp_cv[i].InRescaled(255)) / amplitude_resolution, 0.0f, 1.0f);
            int y = amp * (y1 - y0);
            gfxDottedLine(x, y0, x, y1, (uint8_t)2U);
            if (cursor == PARTIAL1 + i) gfxInvert(x, y0, 1, y1 - y0);
            gfxLine(x, y1-y, x, y1);
        }

        gfxDisplayInputMapEditor();
    }

    void OnButtonPress() override {
        if (cursor > PARTIAL1 && partial_param == 2) {
            if (CheckEditInputMapPress( cursor,
                IndexedInput(cursor, amp_cv[cursor - PARTIAL1])
            )) return;
        } else {
            if (CheckEditInputMapPress( cursor,
                IndexedInput(PITCH_CV, pitch_cv),
                IndexedInput(MIX_CV, mix_cv),
                IndexedInput(LEVEL_CV, level_cv)
            )) return;
        }
        CursorToggle();
    }

    void AuxButton() {
        if (cursor >= PARTIAL1)
            (partial_param + 1) > 2 ? partial_param = 0 : ++partial_param;
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
            default:
                if (partial_param == 2)
                    amp_cv[cursor - PARTIAL1].ChangeSource(direction);
                else if (partial_param == 1)
                    partial_ratios[cursor - PARTIAL1] = constrain(partial_ratios[cursor - PARTIAL1] + direction, 0, 32767);
                else if (partial_param == 0)
                    amplitudes[cursor - PARTIAL1] = constrain(amplitudes[cursor - PARTIAL1] + direction, 0, 255);
                break;
        }
    }

    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(pitch, level, mix);
        data[1] = PackPackables(pitch_cv, level_cv, mix_cv);
        for (size_t i = 0; i < max_partials; ++i) {
          Pack(data[2 + i / 8], PackLocation{(i % 8) * 8, 8}, amplitudes[i]);
        }
        // no room for amp CV
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], pitch, level, mix);
        UnpackPackables(data[1], pitch_cv, level_cv, mix_cv);
        for (size_t i = 0; i < max_partials; ++i) {
          amplitudes[i] = Unpack(data[2 + i / 8], PackLocation{(i % 8) * 8, 8});
        }
    }

    AudioStream* InputStream() override {
        return &input_stream;
    }
    AudioStream* OutputStream() override {
        return &mixer;
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
        PARTIAL1,
        PARTIAL16 = PARTIAL1 + 15,
    };

    int8_t cursor = OCTAVE;
    int16_t pitch = 1 * 12 * 128; // C4
    int8_t level = 0; // dB
    int8_t mix = 100;
    uint8_t partial_param = 0;  // 0 = amplitude, 1 = detune, 3 = cvmap
    bool partial_map = false;

    uint8_t amplitudes[max_partials];
    uint16_t partial_ratios[max_partials];

    CVInputMap pitch_cv;
    CVInputMap level_cv;
    CVInputMap mix_cv;
    CVInputMap amp_cv[max_partials];

    AudioPassthrough<MONO> input_stream;
    AudioSynthWaveform partial[max_partials];
    InterpolatingStream<> vca_cv;
    AudioVCA vca;
    AudioMixer<max_partials> harmosc;
    AudioMixer<2> mixer;

    void InitWaveform(uint8_t* amp, uint16_t* rat, int n_partials) {
        for (int i = 0; i < n_partials; ++i) {  // approximate saw wave
            amp[i] = 255 / (i + 1);
            rat[i] = 256 * (i + 1);
        }
    }
};
