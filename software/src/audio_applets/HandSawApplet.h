#include "synth_waveform.h"

class HandSawApplet : public HemisphereAudioApplet {
    public: 
        const char* applet_name() override {
            return "HandSaw";
        }
        void Start() override {
          vca_level.Acquire();
          vca_level.Method(INTERPOLATION_LINEAR);

          PatchCable(synth1, 0, mixer1, 0);
          PatchCable(synth2, 0, mixer1, 1);
          PatchCable(synth3, 0, mixer1, 2);
          PatchCable(synth4, 0, mixer2, 0);
          PatchCable(synth5, 0, mixer2, 1);
          PatchCable(synth6, 0, mixer2, 2);
          PatchCable(synth7, 0, mixer3, 0);
          PatchCable(synth8, 0, mixer3, 1);
          PatchCable(synth9, 0, mixer3, 2);
          PatchCable(synth10, 0, mixer4, 0);
          PatchCable(synth11, 0, mixer4, 1);
          PatchCable(synth12, 0, mixer4, 2);

          PatchCable(mixer1, 0, stackMixer, 0);
          PatchCable(mixer2, 0, stackMixer, 1);
          PatchCable(mixer3, 0, stackMixer, 2);
          PatchCable(mixer4, 0, stackMixer, 3);

          PatchCable(input_stream, 0, outputMixer, 0);
          PatchCable(stackMixer, 0, outputMixer, 1);
          PatchCable(outputMixer, 0, vca, 0);
          PatchCable(vca_level, 0, vca, 1);

          outputMixer.gain(0, 1.0f); // passthru
          outputMixer.gain(1, 1.0f);

          //vca.bias(0.0f);
          //vca.level(0.0f);

          synth1.amplitude(1.0f);
          synth2.amplitude(1.0f);
          synth3.amplitude(1.0f);
          synth4.amplitude(1.0f);
          synth5.amplitude(1.0f);
          synth6.amplitude(1.0f);
          synth7.amplitude(1.0f);
          synth8.amplitude(1.0f);
          synth9.amplitude(1.0f);
          synth10.amplitude(1.0f);
          synth11.amplitude(1.0f);
          synth12.amplitude(1.0f);

          mixer1.gain(0, 0.33f);
          mixer2.gain(0, 0.33f);
          mixer3.gain(0, 0.33f);
          mixer4.gain(0, 0.33f);

          mixer1.gain(1, 0.33f);
          mixer2.gain(1, 0.33f);
          mixer3.gain(1, 0.33f);
          mixer4.gain(1, 0.33f);

          mixer1.gain(2, 0.33f);
          mixer2.gain(2, 0.33f);
          mixer3.gain(2, 0.33f);
          mixer4.gain(2, 0.33f);

          stackMixer.gain(0, 0.25f);
          stackMixer.gain(1, 0.25f);
          stackMixer.gain(2, 0.25f);
          stackMixer.gain(3, 0.25f);
        }
        void Unload() override {
          vca_level.Release();
          AllowRestart();
        }

        void Controller() override {
            float detuneValue = detune + (detune_cv.In() * 0.01f);
            float phaseValue = phase + (phase_cv.In() * 0.01f);

            float freq1 = PitchToRatio(pitch[0] + pitch_cv[0].In()) * C3;
            // set the first 3 oscillators to freq1
            synth1.frequency(freq1);
            synth2.frequency(freq1 + (3 * detuneValue / detuneFactor));
            synth3.frequency(freq1 - (2 * detuneValue / detuneFactor));
            synth1.phase(phaseValue);
            synth2.phase((3 * phaseValue / phaseFactor));
            synth3.phase((2 * phaseValue / phaseFactor));

            float freq2 = PitchToRatio(pitch[1] + pitch_cv[1].In()) * C3;
            // set the next 3 oscillators to freq2
            synth4.frequency(freq2 + (detuneValue / detuneFactor));
            synth5.frequency(freq2 + (4 * detuneValue / detuneFactor));
            synth6.frequency(freq2 - (5 * detuneValue / detuneFactor));
            synth4.phase(phaseValue);
            synth5.phase((4 * phaseValue / phaseFactor));
            synth6.phase((5 * phaseValue / phaseFactor));

            float freq3 = PitchToRatio(pitch[2] + pitch_cv[2].In()) * C3;
            // set the last 3 oscillators to freq3
            synth7.frequency(freq3 - (detuneValue / detuneFactor));
            synth8.frequency(freq3 + (2 * detuneValue / detuneFactor));
            synth9.frequency(freq3 - (3 * detuneValue / detuneFactor));
            synth7.phase(phaseValue);
            synth8.phase((2 * phaseValue / phaseFactor));
            synth9.phase((3 * phaseValue / phaseFactor));

            float freq4 = PitchToRatio(pitch[3] + pitch_cv[3].In()) * C3;
            // set the last 3 oscillators to freq4
            synth10.frequency(freq4 + (6 * detuneValue / detuneFactor));
            synth11.frequency(freq4 + (5 * detuneValue / detuneFactor));
            synth12.frequency(freq4 - (4 * detuneValue / detuneFactor));
            synth10.phase((6 * phaseValue / phaseFactor));
            synth11.phase((5 * phaseValue / phaseFactor));
            synth12.phase((4 * phaseValue / phaseFactor));

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
            // the mappings need to sit on top of the pitches
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
            int8_t dummy = 0; // former mix/amp value in %
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
            case PITCH_CV4:
              // shortcut for auto-learn
              auto& p = pitch_cv[(cursor - PITCH_CV1) / 2];
              if (p.IsMidi()) p.AutoLearn();
              break;
          }
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(DETUNE_CV, detune_cv),
                IndexedInput(PHASE_CV, phase_cv),
                IndexedInput(AMP_CV, amp_cv)
            ))
            return;
          CursorToggle();
        }

        void SetWaveform(int wf) {
            waveform = constrain(wf, 0, 4);
            synth1.begin(WAVEFORMS[waveform]);
            synth2.begin(WAVEFORMS[waveform]);
            synth3.begin(WAVEFORMS[waveform]);
            synth4.begin(WAVEFORMS[waveform]);
            synth5.begin(WAVEFORMS[waveform]);
            synth6.begin(WAVEFORMS[waveform]);
            synth7.begin(WAVEFORMS[waveform]);
            synth8.begin(WAVEFORMS[waveform]);
            synth9.begin(WAVEFORMS[waveform]);
            synth10.begin(WAVEFORMS[waveform]);
            synth11.begin(WAVEFORMS[waveform]);
            synth12.begin(WAVEFORMS[waveform]);
        }

        void OnEncoderMove(int direction) override {
             if (!EditMode()) {
                MoveCursor(cursor, direction, AMP_CV);
                return;
            }
            if (EditSelectedInputMap(direction)) return;

            const int max_pitch = 7 * 12 * 128;
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

        AudioStream* InputStream() override {
            return &input_stream;
        }
        AudioStream* OutputStream() override {
            return &vca;
        }
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

        static constexpr int8_t WAVEFORMS[5]
    = {WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE, WAVEFORM_BANDLIMIT_SAWTOOTH, WAVEFORM_BANDLIMIT_PULSE, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE};
        static constexpr char const* WAVEFORM_NAMES[5] = {"SIN", "TRI", "SAW", "PLS", "SAWR"};
        uint8_t waveform = WAVEFORM_SINE;
        int8_t cursor = PITCH1;
        int16_t pitch[4] = {
          -1 * 12 * 128, // C2
          -1 * 12 * 128, // C2
          -1 * 12 * 128, // C2
          -1 * 12 * 128, // C2
        };

        int16_t detune = 0;
        int16_t phase = 0;
        int8_t amp = 0;

        /// sensitivity of detune
        int8_t detuneFactor = 50;
        int8_t phaseFactor = 1;

        CVInputMap pitch_cv[4];

        CVInputMap detune_cv;
        CVInputMap phase_cv;
        CVInputMap amp_cv;

        AudioPassthrough<MONO> input_stream;
        AudioMixer<3> mixer1;
        AudioMixer<3> mixer2;
        AudioMixer<3> mixer3;
        AudioMixer<3> mixer4;
        AudioMixer<4> stackMixer;
        AudioMixer<2> outputMixer;
        AudioVCA vca;
        InterpolatingStream<> vca_level;

        AudioSynthWaveform synth1;
        AudioSynthWaveform synth2;
        AudioSynthWaveform synth3;
        AudioSynthWaveform synth4;
        AudioSynthWaveform synth5;
        AudioSynthWaveform synth6;
        AudioSynthWaveform synth7;
        AudioSynthWaveform synth8;
        AudioSynthWaveform synth9;
        AudioSynthWaveform synth10;
        AudioSynthWaveform synth11;
        AudioSynthWaveform synth12;
};
