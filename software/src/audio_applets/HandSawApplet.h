#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "dsputils_arm.h"
#include "synth_waveform.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

class HandSawApplet : public HemisphereAudioApplet {
    public: 
        const char* applet_name() override {
            return "HandSaw";
        }
        void Start() override {
            
        }
        void Unload() override {
            AllowRestart();
        }
        
        void Controller() override {
            float detuneValue = detune + (detune_cv.In() * 0.01f);
            float phaseValue = phase + (phase_cv.In() * 0.01f);

            float freq1 = PitchToRatio(pitch1 + pitch_cv1.In()) * C3;
            // set the first 3 oscillators to freq1
            synth1.frequency(freq1);
            synth2.frequency(freq1 + (3 * detuneValue / detuneFactor));
            synth3.frequency(freq1 - (2 * detuneValue / detuneFactor));
            synth1.phase(phaseValue);
            synth2.phase((3 * phaseValue / phaseFactor));
            synth3.phase((2 * phaseValue / phaseFactor));
            synth1.amplitude(1.0f);
            synth2.amplitude(1.0f);
            synth3.amplitude(1.0f);

            float freq2 = PitchToRatio(pitch2 + pitch_cv2.In()) * C3;
            // set the next 3 oscillators to freq2
            synth4.frequency(freq2 + (detuneValue / detuneFactor));
            synth5.frequency(freq2 + (4 * detuneValue / detuneFactor));
            synth6.frequency(freq2 - (5 * detuneValue / detuneFactor));
            synth4.phase(phaseValue);
            synth5.phase((4 * phaseValue / phaseFactor));
            synth6.phase((5 * phaseValue / phaseFactor));
            synth4.amplitude(1.0f);
            synth5.amplitude(1.0f);
            synth6.amplitude(1.0f);

            float freq3 = PitchToRatio(pitch3 + pitch_cv3.In()) * C3;
            // set the last 3 oscillators to freq3
            synth7.frequency(freq3 - (detuneValue / detuneFactor));
            synth8.frequency(freq3 + (2 * detuneValue / detuneFactor));
            synth9.frequency(freq3 - (3 * detuneValue / detuneFactor));
            synth7.phase(phaseValue);
            synth8.phase((2 * phaseValue / phaseFactor));
            synth9.phase((3 * phaseValue / phaseFactor));
            synth7.amplitude(1.0f);
            synth8.amplitude(1.0f);
            synth9.amplitude(1.0f);

            float freq4 = PitchToRatio(pitch4 + pitch_cv4.In()) * C3;
            // set the last 3 oscillators to freq4
            synth10.frequency(freq4 + (6 * detuneValue / detuneFactor));
            synth11.frequency(freq4 + (5 * detuneValue / detuneFactor));
            synth12.frequency(freq4 - (4 * detuneValue / detuneFactor));
            synth10.phase((6 * phaseValue / phaseFactor));
            synth11.phase((5 * phaseValue / phaseFactor));
            synth12.phase((4 * phaseValue / phaseFactor));
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

            mixer1.gain(3, 0.0f);
            
            stackMixer.gain(0, 0.25f);
            stackMixer.gain(1, 0.25f);
            stackMixer.gain(2, 0.25f);
            stackMixer.gain(3, 0.25f);

            float m
                = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);

            outputMixer.gain(1, m);
            outputMixer.gain(0, 1.0f - m);
        }

        void View() override {
            
            gfxStartCursor(1, 15);
            gfxPrintTuningIndicator(pitch1);
            gfxEndCursor(cursor == PITCH1);
            gfxStartCursor();
            gfxPrint(pitch_cv1);
            gfxEndCursor(cursor == PITCH_CV1, false, pitch_cv1.InputName());

            gfxStartCursor(16, 15);
            gfxPrintTuningIndicator(pitch2);
            gfxEndCursor(cursor == PITCH2);
            gfxStartCursor();
            gfxPrint(pitch_cv2);
            gfxEndCursor(cursor == PITCH_CV2, false, pitch_cv2.InputName());

            gfxStartCursor(31, 15);
            gfxPrintTuningIndicator(pitch3);
            gfxEndCursor(cursor == PITCH3);
            gfxStartCursor();
            gfxPrint(pitch_cv3);
            gfxEndCursor(cursor == PITCH_CV3, false, pitch_cv3.InputName());

            gfxStartCursor(46, 15);
            gfxPrintTuningIndicator(pitch4);
            gfxEndCursor(cursor == PITCH4);
            gfxStartCursor();
            gfxPrint(pitch_cv4);
            gfxEndCursor(cursor == PITCH_CV4, false, pitch_cv4.InputName());

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

            gfxPrint(1, 55, "Mix: ");
            gfxStartCursor();
            graphics.printf("%3d%%", mix);
            gfxEndCursor(cursor == MIX);
            
            gfxStartCursor();
            gfxPrint(mix_cv);
            gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());  

            gfxDisplayInputMapEditor();
        }

    #define SWARM_OSC_PARAMS \
        pitch1, pitch2, pitch3, pitch4

        void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
            data[0] = PackPackables(SWARM_OSC_PARAMS);
            data[1] = PackPackables(pitch_cv1, pitch_cv2, pitch_cv3, pitch_cv4);
            data[2] = PackPackables(detune_cv, phase_cv, mix_cv);
            data[3] = PackPackables(waveform, detune, phase, mix);
        }

        void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
            UnpackPackables(data[0], SWARM_OSC_PARAMS);
            UnpackPackables(data[1], pitch_cv1, pitch_cv2, pitch_cv3, pitch_cv4);
            UnpackPackables(data[2], detune_cv, phase_cv, mix_cv);
            UnpackPackables(data[3], waveform, detune, phase, mix);

            SetWaveform(waveform);
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(DETUNE_CV, detune_cv),
                IndexedInput(PHASE_CV, phase_cv),
                IndexedInput(MIX_CV, mix_cv)
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
                MoveCursor(cursor, direction, MIX_CV);
                return;
            }
            if (EditSelectedInputMap(direction)) return;

            const int max_pitch = 7 * 12 * 128;
            const int min_pitch = -3 * 12 * 128;
            switch (cursor) {
                case PITCH1:
                    pitch1 = constrain(pitch1 + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV1:
                    pitch_cv1.ChangeSource(direction);
                    break;
                case PITCH2:
                    pitch2 = constrain(pitch2 + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV2:
                    pitch_cv2.ChangeSource(direction);
                    break;
                case PITCH3:
                    pitch3 = constrain(pitch3 + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV3:
                    pitch_cv3.ChangeSource(direction);
                    break;
                case PITCH4:
                    pitch4 = constrain(pitch4 + direction * 4, min_pitch, max_pitch);
                    break;
                case PITCH_CV4:
                    pitch_cv4.ChangeSource(direction);
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
                case MIX:
                    mix = constrain(mix + direction, 0, 100);
                    break;
                case MIX_CV:
                    mix_cv.ChangeSource(direction);
                    break;
                default:
                    break;
            }
        }

        AudioStream* InputStream() override {
            return &input_stream;
        }
        AudioStream* OutputStream() override {
            return &outputMixer;
        }
    protected:
        void SetHelp() override {}
    
    private:
        enum Cursor: int8_t {
            PITCH1,
            PITCH_CV1,
            PITCH2,
            PITCH_CV2,
            PITCH3,
            PITCH_CV3,
            PITCH4,
            PITCH_CV4,
            WAVEFORM,
            DETUNE,
            DETUNE_CV,
            PHASE,
            PHASE_CV,
            MIX,
            MIX_CV
        };

        static constexpr int8_t WAVEFORMS[5]
    = {WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE, WAVEFORM_BANDLIMIT_SAWTOOTH, WAVEFORM_BANDLIMIT_PULSE, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE};
        static constexpr char const* WAVEFORM_NAMES[5] = {"SIN", "TRI", "SAW", "PLS", "SAWR"};
        uint8_t waveform = WAVEFORM_SINE;
        int8_t cursor = PITCH1;
        int16_t pitch1 = -1 * 12 * 128; // C2
        int16_t pitch2 = -1 * 12 * 128; // C2
        int16_t pitch3 = -1 * 12 * 128; // C2
        int16_t pitch4 = -1 * 12 * 128; // C2

        int16_t detune = 0;
        int16_t phase = 0;
        int8_t mix = 100;

        /// sensitivity of detune
        int8_t detuneFactor = 50;
        int8_t phaseFactor = 1;

        CVInputMap pitch_cv1;
        CVInputMap pitch_cv2;
        CVInputMap pitch_cv3;
        CVInputMap pitch_cv4;

        CVInputMap detune_cv;
        CVInputMap phase_cv;
        CVInputMap mix_cv;

        AudioPassthrough<MONO> input_stream;
        AudioMixer<4> mixer1;
        AudioMixer<4> mixer2;
        AudioMixer<4> mixer3;
        AudioMixer<4> mixer4;
        AudioMixer<4> stackMixer;
        AudioMixer<2> outputMixer;
        
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

        AudioConnection synth1_to_mixer1{synth1, 0, mixer1, 0};
        AudioConnection synth2_to_mixer1{synth2, 0, mixer1, 1};
        AudioConnection synth3_to_mixer1{synth3, 0, mixer1, 2};
        AudioConnection in_to_mixer1{input_stream, 0, mixer1, 3};
        AudioConnection synth4_to_mixer2{synth4, 0, mixer2, 0};
        AudioConnection synth5_to_mixer2{synth5, 0, mixer2, 1};
        AudioConnection synth6_to_mixe2{synth6, 0, mixer2, 2};
        AudioConnection synth7_to_mixer3{synth7, 0, mixer3, 0};
        AudioConnection synth8_to_mixer3{synth8, 0, mixer3, 1};
        AudioConnection synth9_to_mixer3{synth9, 0, mixer3, 2};
        AudioConnection synth10_to_mixer4{synth10, 0, mixer4, 0};
        AudioConnection synth11_to_mixer4{synth11, 0, mixer4, 1};
        AudioConnection synth12_to_mixer4{synth12, 0, mixer4, 2};

        AudioConnection mixer1_to_stackMixer{mixer1, 0, stackMixer, 0};
        AudioConnection mixer2_to_stackMixer{mixer2, 0, stackMixer, 1};
        AudioConnection mixer3_to_stackMixer{mixer3, 0, stackMixer, 2};
        AudioConnection mixer4_to_stackMixer{mixer4, 0, stackMixer, 3};
        
        AudioConnection input_to_outputMixer{input_stream, 0, outputMixer, 0};
        AudioConnection stackMixer_to_outputMixer{stackMixer, 0, outputMixer, 1};
};