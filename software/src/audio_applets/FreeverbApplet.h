#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include "dsputils_arm.h"
#include "synth_waveform.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/InterpolatingStream.h"
#include <Audio.h>

class ReverbApplet : public HemisphereAudioApplet {
    public:
        const char* applet_name() override {
            return "Reverb";
        }
        void Start() override {
            if (!reverb && OC::CORE::FreeRam() > (int)sizeof(AudioEffectFreeverb)) {
              reverb = new AudioEffectFreeverb();
            }
            PatchCable(input, 0, dry_wet_mixer, 1);
            if (!reverb) return;
            PatchCable(input, 0, *reverb, 0);
            PatchCable(*reverb, 0, filter, 0);
            PatchCable(filter, 0, dry_wet_mixer, 0);
            filter.frequency(15000);
        }

        void Controller() override {
            if (reverb) {
              reverb->roomsize((size * 0.01f) + size_cv.InF());
              reverb->damping((damp * 0.01f) + damp_cv.InF());
            } else {
              dry_wet_mixer.gain(1, 1.0f);
              return;
            }

            float freq = constrain(static_cast<float>(cutoff)
              + (cutoff_cv.InF() * abs(cutoff_cv.InF()) * 18000.0), 10.0f, 20000.0);
            filter.frequency(freq);

            float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);

            dry_wet_mixer.gain(0, m);
            dry_wet_mixer.gain(1, 1.0f - m);
        }

        void View() override {
            if (!reverb) {
              gfxPrint(1, 15, "Out Of RAM !!!");
              return;
            };
            gfxPrint(1, 15, "Size:");
            gfxStartCursor();
            graphics.printf("%3d%%", size);
            gfxEndCursor(cursor == SIZE);

            gfxStartCursor();
            gfxPrint(size_cv);
            gfxEndCursor(cursor == SIZE_CV, false, size_cv.InputName());

            gfxPrint(1, 25, "Damp:");
            gfxStartCursor();
            graphics.printf("%3d%%", damp);
            gfxEndCursor(cursor == DAMP);

            gfxStartCursor();
            gfxPrint(damp_cv);
            gfxEndCursor(cursor == DAMP_CV, false, damp_cv.InputName());

            gfxPrint(1, 35, "C:");
            gfxStartCursor();
            graphics.printf("%5dHz", cutoff);
            gfxEndCursor(cursor == CUTOFF);

            gfxStartCursor();
            gfxPrint(cutoff_cv);
            gfxEndCursor(cursor == CUTOFF_CV, false, cutoff_cv.InputName());

            gfxPrint(1, 45, "Mix:");
            gfxStartCursor();
            graphics.printf("%3d%%", mix);
            gfxEndCursor(cursor == MIX);
            
            gfxStartCursor();
            gfxPrint(mix_cv);
            gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());  

            gfxDisplayInputMapEditor();
        }

        void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
            data[0] = PackPackables(mix, size, damp);
            data[1] = PackPackables(size_cv, damp_cv, mix_cv);
            data[2] = PackPackables(cutoff, cutoff_cv);
        }

        void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
            UnpackPackables(data[0], mix, size, damp);
            UnpackPackables(data[1], size_cv, damp_cv, mix_cv);
            UnpackPackables(data[2], cutoff, cutoff_cv);
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(MIX_CV, mix_cv),
                IndexedInput(SIZE_CV, size_cv),
                IndexedInput(DAMP_CV, damp_cv),
                IndexedInput(CUTOFF_CV, cutoff_cv)
            ))
            return;
          CursorToggle();
        }
        
        void OnEncoderMove(int direction) override {
            if (!EditMode()) {
                MoveCursor(cursor, direction, MIX_CV);
                return;
            }

            if (EditSelectedInputMap(direction)) return;

            switch (cursor) {
                case SIZE:
                    size = constrain(size + direction, 0, 100);
                    break;
                case SIZE_CV:
                    size_cv.ChangeSource(direction);
                    break;
                case DAMP:
                    damp = constrain(damp + direction, 1, 100);
                    break;
                case DAMP_CV:
                    damp_cv.ChangeSource(direction);
                    break;
                case CUTOFF:
                    cutoff = constrain(cutoff + direction * 50, 0, 17500);
                    break;
                case CUTOFF_CV:
                    cutoff_cv.ChangeSource(direction);
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
            return &input;
        }
        AudioStream* OutputStream() override {
            return &dry_wet_mixer;
        }
    protected:
        void SetHelp() override {}
    
    private:
        enum Cursor: int8_t {
            SIZE,
            SIZE_CV,
            DAMP,
            DAMP_CV,
            CUTOFF,
            CUTOFF_CV,
            MIX,
            MIX_CV
        };

        int8_t cursor = SIZE;
        AudioPassthrough<MONO> input;

        AudioEffectFreeverb* reverb;
        AudioFilterStateVariable filter;

        AudioMixer<2> dry_wet_mixer;

        int8_t mix = 50;
        int8_t size = 50;
        int8_t damp = 50;
        int16_t cutoff = 15000;

        CVInputMap mix_cv;
        CVInputMap size_cv;
        CVInputMap damp_cv;
        CVInputMap cutoff_cv;
    };
