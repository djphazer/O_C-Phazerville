// Phazer applet and DSP effect
// Authored by Ivan Cohen
// modified by djphazer

#include "../src/Audio/effect_phaser.h"

// MONO only... for now
class PhazerApplet : public HemisphereAudioApplet {
    public:
        const char* applet_name() override {
            return "Phazer";
        }
        void Start() override {
            if (!phaser && OC::CORE::FreeRam() > (int)sizeof(AudioEffectPhazer)) {
                phaser = new AudioEffectPhazer();
            }
            if (!phaser) return;

            PatchCable(input, 0, dry_wet_mixer, 1);
            PatchCable(input, 0, *phaser, 0);
            PatchCable(*phaser, 0, dry_wet_mixer, 0);

            dry_wet_mixer.gain(1, 1.0f);
        }

        void Controller() override {
            if (!phaser) {
                return;
            }

            phaser->setDepth(depth + depth_cv.InF());
            phaser->setFeedback(feedback + feedback_cv.InF());

            float freq = powf(10.f, 3.f * constrain(rate + rate_cv.InF(), 0.0f, 1.f) - 2.f);
            phaser->setRate(freq);

            float m = constrain(static_cast<float>(mix) * 0.01f + mix_cv.InF(), 0.0f, 1.0f);

            dry_wet_mixer.gain(0, m);
            dry_wet_mixer.gain(1, 1.0f - m);
        }

        void View() override {
            if (!phaser) {
                gfxPrint(1, 15, "Out Of RAM !!!");
                return;
            };
            gfxPrint(1, 15, "Depth: ");
            gfxStartCursor();
            graphics.printf("%3d%%", depth); // KEEP THIS WAY?
            gfxEndCursor(cursor == DEPTH);

            gfxStartCursor();
            gfxPrint(depth_cv);
            gfxEndCursor(cursor == DEPTH_CV, false, depth_cv.InputName());

            gfxPrint(1, 25, "Feed:");
            gfxStartCursor();
            graphics.printf("%3d%%", feedback); // KEEP THIS WAY?
            gfxEndCursor(cursor == FEED);

            gfxStartCursor();
            gfxPrint(feedback_cv);
            gfxEndCursor(cursor == FEED_CV, false, feedback_cv.InputName());

            gfxPrint(1, 35, "Rate:"); 
            gfxStartCursor();
            graphics.printf("%3d%%", rate); // KEEP THIS WAY?
            gfxEndCursor(cursor == RATE);

            gfxStartCursor();
            gfxPrint(rate_cv);
            gfxEndCursor(cursor == RATE_CV, false, rate_cv.InputName());

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
            // TO DO
            data[0] = PackPackables(mix, damp);
            data[1] = PackPackables(decay_time_cv, damp_cv, mix_cv);
            data[2] = PackPackables(cutoff, cutoff_cv);
            data[3] = PackPackables(decay_time);
        }

        void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
            // TO DO
            UnpackPackables(data[0], mix, damp);
            UnpackPackables(data[1], decay_time_cv, damp_cv, mix_cv);
            UnpackPackables(data[2], cutoff, cutoff_cv);
            UnpackPackables(data[3], decay_time);
        }

        void OnButtonPress() override {
            if (CheckEditInputMapPress(cursor,
                IndexedInput(MIX_CV, mix_cv),
                IndexedInput(DEPTH_CV, depth_cv),
                IndexedInput(FEED_CV, feedback_cv),
                IndexedInput(RATE_CV, rate_cv)
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
                case DEPTH:
                    depth = constrain(decay_time + (direction * 0.1), 0, 20); // TO DO
                    break;
                case DEPTH_CV:
                    depth_cv.ChangeSource(direction);
                    break;
                case FEED:
                    feedback = constrain(damp + direction, 1, 99); // TO DO
                    break;
                case FEED_CV:
                    feedback_cv.ChangeSource(direction);
                    break;
                case RATE:
                    rate = constrain(cutoff + direction * 50, 0, 17500); // TO DO
                    break;
                case RATE_CV:
                    rate_cv.ChangeSource(direction);
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
            DEPTH,
            DEPTH_CV,
            FEED,
            FEED_CV,
            RATE,
            RATE_CV,
            MIX,
            MIX_CV
        };

        int8_t cursor = DEPTH;
        AudioPassthrough<MONO> input;

        AudioEffectPhazer* phaser;
        AudioMixer<2> dry_wet_mixer;

        int8_t mix = 50;
        float depth = 0.5f; // KEEP THIS WAY?
        float feedback = 0.5f; // KEEP THIS WAY?
        float rate = 0.5f; // KEEP THIS WAY?

        CVInputMap mix_cv;
        CVInputMap depth_cv;
        CVInputMap feedback_cv;
        CVInputMap rate_cv;
    };
