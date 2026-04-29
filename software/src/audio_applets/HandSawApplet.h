#include "synth_waveform.h"

// Per-oscillator detune and phase scale factors.
// 12 oscillators split into 4 voices of 3 each.
// Detune: signed multiplier applied to detuneValue/detuneFactor.
// Phase:  multiplier applied to phaseValue/phaseFactor.
// Per-oscillator scale factors — file-scope constexpr, no linkage issues
static constexpr float HANDSAW_DETUNE[12] = {  0,  3, -2,   1,  4, -5,  -1,  2, -3,   6,  5, -4 };
static constexpr float HANDSAW_PHASE[12]  = {  1,  3,  2,   1,  4,  5,   1,  2,  3,   6,  5,  4 };

class HandSawApplet : public HemisphereAudioApplet {
    public:
        const char* applet_name() {
            return "HandSaw";
        }

        void Start() override {
            vca_level.Acquire();
            vca_level.Method(INTERPOLATION_LINEAR);

            // Wire each oscillator into its sub-mixer (3 per mixer).
            // Call order follows signal flow: sources before sinks,
            // so the audio scheduler can process them in one pass.
            for (int i = 0; i < 12; i++) {
                PatchCable(synths[i], 0, mixers[i / 3], i % 3);
            }

            // Sub-mixers into stack mixer
            for (int i = 0; i < 4; i++) {
                PatchCable(mixers[i], 0, stackMixer, i);
            }

            // Stack + passthru → output → VCA
            PatchCable(input_stream,  0, outputMixer, 0);
            PatchCable(stackMixer,    0, outputMixer, 1);
            PatchCable(outputMixer,   0, vca,         0);
            PatchCable(vca_level,     0, vca,         1);

            outputMixer.gain(0, 1.0f); // passthru
            outputMixer.gain(1, 1.0f);

            for (int i = 0; i < 12; i++) {
                synths[i].amplitude(1.0f);
            }

            // Each sub-mixer sums 3 oscillators; normalise to 1/3
            for (int m = 0; m < 4; m++) {
                for (int ch = 0; ch < 3; ch++) {
                    mixers[m].gain(ch, 0.33f);
                }
            }

            // Stack mixer sums 4 sub-mixers; normalise to 1/4
            for (int i = 0; i < 4; i++) {
                stackMixer.gain(i, 0.25f);
            }
        }

        void Unload() override {
            vca_level.Release();
            AllowRestart();
        }

        void Controller() override {
            float detuneValue = detune + (detune_cv.In() * 0.01f);
            float phaseValue  = phase  + (phase_cv.In()  * 0.01f);

            for (int v = 0; v < 4; v++) {
                float freq = PitchToRatio(pitch[v] + pitch_cv[v].In()) * C3;
                for (int o = 0; o < 3; o++) {
                    int idx = v * 3 + o;
                    synths[idx].frequency(freq + HANDSAW_DETUNE[idx] * detuneValue / detuneFactor);
                    synths[idx].phase(HANDSAW_PHASE[idx] * phaseValue / phaseFactor);
                }
            }

            float m = amp < LVL_MIN_DB ? 0.0f : dbToScalar(amp);
            m += amp_cv.InF();
            vca_level.Push(float_to_q15(m));
        }

        void View() override {
            for (int i = 0; i < 4; ++i) {
                gfxStartCursor(1 + 15*i, 15);
                gfxPrintTuningIndicator(pitch[i]);
                gfxEndCursor(cursor == PITCH1 + i);
                if (cursor == PITCH1 + i) {
                    gfxIcon(1 + 15*i, 25, UP_ICON, true);
                }
            }
            // CV mappings sit on top of the pitch row
            for (int i = 0; i < 4; ++i) {
                gfxStartCursor(10 + 15*i, 15);
                gfxPrint(pitch_cv[i]);
                gfxEndCursor(cursor == PITCH_CV1 + i, false, pitch_cv[i].InputName());
            }

            gfxPrint(1, 25, "Wave: ");
            gfxStartCursor();
            gfxPrint(WAVEFORM_NAMES[waveform]);
            gfxEndCursor(cursor == WAVEFORM);

            gfxPrint(1, 35, "DT: ");
            gfxStartCursor();
            graphics.printf("%d", detune);
            gfxEndCursor(cursor == DETUNE);

            gfxStartCursor();
            gfxPrint(detune_cv);
            gfxEndCursor(cursor == DETUNE_CV, false, detune_cv.InputName());

            gfxPrint(1, 45, "Ph: ");
            gfxStartCursor();
            graphics.printf("%d", phase);
            gfxEndCursor(cursor == PHASE);

            gfxStartCursor();
            gfxPrint(phase_cv);
            gfxEndCursor(cursor == PHASE_CV, false, phase_cv.InputName());

            gfxPrint(1, 55, "Amp:");
            gfxStartCursor();
            gfxPrintDb(amp);
            gfxEndCursor(cursor == AMP);

            gfxStartCursor();
            gfxPrint(amp_cv);
            gfxEndCursor(cursor == AMP_CV, false, amp_cv.InputName());

            gfxDisplayInputMapEditor();
        }

    #define SWARM_OSC_PARAMS \
        pitch[0], pitch[1], pitch[2], pitch[3]

        void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
            int8_t dummy = 0;
            data[0] = PackPackables(SWARM_OSC_PARAMS);
            data[1] = PackPackables(pitch_cv[0], pitch_cv[1], pitch_cv[2], pitch_cv[3]);
            data[2] = PackPackables(detune_cv, phase_cv, amp_cv);
            data[3] = PackPackables(waveform, detune, phase, dummy, amp);
        }

        void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
            int8_t dummy;
            UnpackPackables(data[0], SWARM_OSC_PARAMS);
            UnpackPackables(data[1], pitch_cv[0], pitch_cv[1], pitch_cv[2], pitch_cv[3]);
            UnpackPackables(data[2], detune_cv, phase_cv, amp_cv);
            UnpackPackables(data[3], waveform, detune, phase, dummy, amp);
            SetWaveform(waveform);
        }

        void AuxButton() override {
            switch (cursor) {
                case PITCH1:
                case PITCH2:
                case PITCH3:
                case PITCH4: {
                    // shortcut to snap to closest semitone
                    auto& p = pitch[(cursor - PITCH1) / 2];
                    p = (((p + 63) >> 7) << 7);
                    break;
                }
                case PITCH_CV1:
                case PITCH_CV2:
                case PITCH_CV3:
                case PITCH_CV4: {
                    // shortcut for auto-learn
                    auto& p = pitch_cv[(cursor - PITCH_CV1) / 2];
                    if (p.IsMidi()) p.AutoLearn();
                    break;
                }
            }
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(DETUNE_CV, detune_cv),
                IndexedInput(PHASE_CV,  phase_cv),
                IndexedInput(AMP_CV,    amp_cv)
            ))
                return;
            CursorToggle();
        }

        void SetWaveform(int wf) {
            waveform = constrain(wf, 0, 4);
            for (int i = 0; i < 12; i++) {
                synths[i].begin(WAVEFORMS[waveform]);
            }
        }

        void OnEncoderMove(int direction) override {
            if (!EditMode()) {
                MoveCursor(cursor, direction, AMP_CV);
                return;
            }
            if (EditSelectedInputMap(direction)) return;

            const int max_pitch =  7 * 12 * 128;
            const int min_pitch = -3 * 12 * 128;
            switch (cursor) {
                case PITCH1:
                    pitch[0] = constrain(pitch[0] + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV1:
                    pitch_cv[0].ChangeSource(direction);
                    break;
                case PITCH2:
                    pitch[1] = constrain(pitch[1] + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV2:
                    pitch_cv[1].ChangeSource(direction);
                    break;
                case PITCH3:
                    pitch[2] = constrain(pitch[2] + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV3:
                    pitch_cv[2].ChangeSource(direction);
                    break;
                case PITCH4:
                    pitch[3] = constrain(pitch[3] + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV4:
                    pitch_cv[3].ChangeSource(direction);
                    break;
                case WAVEFORM:
                    SetWaveform(waveform + direction);
                    break;
                case DETUNE:
                    detune = constrain(detune + direction, -2000, 2000);
                    break;
                case DETUNE_CV:
                    detune_cv.ChangeSource(direction);
                    break;
                case PHASE:
                    phase = constrain(phase + direction, 0, 360);
                    break;
                case PHASE_CV:
                    phase_cv.ChangeSource(direction);
                    break;
                case AMP:
                    amp = constrain(amp + direction, LVL_MIN_DB - 1, 0);
                    break;
                case AMP_CV:
                    amp_cv.ChangeSource(direction);
                    break;
                default:
                    break;
            }
        }

        AudioStream* InputStream()  override { return &input_stream; }
        AudioStream* OutputStream() override { return &vca; }

    protected:
        void SetHelp() override {}

    private:
        enum Cursor: int8_t {
            PITCH1,
            PITCH2,
            PITCH3,
            PITCH4,
            PITCH_CV1,
            PITCH_CV2,
            PITCH_CV3,
            PITCH_CV4,
            WAVEFORM,
            DETUNE,
            DETUNE_CV,
            PHASE,
            PHASE_CV,
            AMP,
            AMP_CV
        };

        static constexpr int8_t WAVEFORMS[5] = {
            WAVEFORM_SINE,
            WAVEFORM_TRIANGLE_VARIABLE,
            WAVEFORM_BANDLIMIT_SAWTOOTH,
            WAVEFORM_BANDLIMIT_PULSE,
            WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE
        };
        static constexpr char const* WAVEFORM_NAMES[5] = {"SIN", "TRI", "SAW", "PLS", "SAWR"};

        uint8_t  waveform = WAVEFORM_SINE;
        int8_t   cursor   = PITCH1;
        int16_t  pitch[4] = {
            -1 * 12 * 128, // C2
            -1 * 12 * 128,
            -1 * 12 * 128,
            -1 * 12 * 128,
        };

        int16_t detune = 0;
        int16_t phase  = 0;
        int8_t  amp    = 0;

        int8_t detuneFactor = 50;
        int8_t phaseFactor  = 1;

        CVInputMap pitch_cv[4];
        CVInputMap detune_cv;
        CVInputMap phase_cv;
        CVInputMap amp_cv;

        AudioPassthrough<MONO>  input_stream;
        AudioSynthWaveform      synths[12];
        AudioMixer<3>           mixers[4];
        AudioMixer<4>           stackMixer;
        AudioMixer<2>           outputMixer;
        AudioVCA                vca;
        InterpolatingStream<>   vca_level;
};
