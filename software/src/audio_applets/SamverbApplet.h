#pragma once

// F32-native: the Schroeder reverb's comb/allpass state was already float
// internally; now the wet path, post-filter, and dry/wet mix stay float32
// end to end inside the applet (see effect_reverb_schroeder_F32.h). The
// chain still sees int16 via the HemisphereAudioAppletF32 edge adapters.

#include "../extern/f32/AudioMixer_F32.h"
#include "../Audio/effect_reverb_schroeder_F32.h"
#include "../Audio/filter_variable2_F32.h"

class BungverbApplet : public HemisphereAudioAppletF32 {
    public:
        const char* applet_name() {
            return "Bungverb";
        }
        void Start() override {
            if (!reverb) {
              reverb = GetBungverb();
            }
            PatchCableF32(InputF32(), 0, dry_wet_mixer, 1);
            PatchCableF32(dry_wet_mixer, 0, OutputF32(), 0);
            if (!reverb) return;
            PatchCableF32(InputF32(), 0, *reverb, 0);
            PatchCableF32(*reverb, 0, filter, 0);
            PatchCableF32(filter, 0, dry_wet_mixer, 0);
        }
        void Unload() override {
          if (reverb) ReleaseBungverb(reverb);
          reverb = nullptr;
          AllowRestart();
        }

        void Controller() override {
            if (reverb) {
              reverb->setDecayTime(decay_time + decay_time_cv.InF());
              reverb->setDamping(1.0f - ((damp * 0.01f) + damp_cv.InF()));
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
            gfxPrint(1, 15, "T: ");
            gfxStartCursor();
            graphics.printf("%d.%1ds", SPLIT_INT_DEC(decay_time, 10));
            gfxEndCursor(cursor == DECAY_TIME);

            gfxStartCursor();
            gfxPrint(decay_time_cv);
            gfxEndCursor(cursor == DECAY_TIME_CV, false, decay_time_cv.InputName());

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
            data[0] = PackPackables(mix, damp);
            data[1] = PackPackables(decay_time_cv, damp_cv, mix_cv);
            data[2] = PackPackables(cutoff, cutoff_cv);
            data[3] = PackPackables(decay_time);
        }

        void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
            UnpackPackables(data[0], mix, damp);
            UnpackPackables(data[1], decay_time_cv, damp_cv, mix_cv);
            UnpackPackables(data[2], cutoff, cutoff_cv);
            UnpackPackables(data[3], decay_time);
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(MIX_CV, mix_cv),
                IndexedInput(DECAY_TIME_CV, decay_time_cv),
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
                case DECAY_TIME:
                    decay_time = constrain(decay_time + (direction * 0.1), 0, 20);
                    break;
                case DECAY_TIME_CV:
                    decay_time_cv.ChangeSource(direction);
                    break;
                case DAMP:
                    damp = constrain(damp + direction, 1, 99);
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

    protected:
        void SetHelp() override {}

    private:
        enum Cursor: int8_t {
            DECAY_TIME,
            DECAY_TIME_CV,
            DAMP,
            DAMP_CV,
            CUTOFF,
            CUTOFF_CV,
            MIX,
            MIX_CV
        };

        int8_t cursor = DECAY_TIME;

        AudioEffectReverbSchroederF32* reverb = nullptr;
        AudioFilterStateVariable2F32 filter;

        AudioMixer4_F32 dry_wet_mixer;

        int8_t mix = 50;
        float decay_time = 1.0f;
        int8_t damp = 50;
        int16_t cutoff = 15000;

        CVInputMap mix_cv;
        CVInputMap decay_time_cv;
        CVInputMap damp_cv;
        CVInputMap cutoff_cv;
    };
