// Copyright (c) 2023, Nicholas J. Michalek
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

/*
 * Based loosely on the Traffic module from Jasmine & Olive Trees
 * Also similar to Mutable Instruments Frames
 */

#ifdef ENABLE_APP_SCENES

#include "OC_apps.h"
#include "HSApplication.h"
#include "HSMIDI.h"
#include "util/util_settings.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "HemisphereApplet.h"

static const int NR_OF_SCENE_PRESETS = 4;
static const int NR_OF_SCENE_CHANNELS = 4;

#define SCENE_MAX_VAL HEMISPHERE_MAX_CV
#define SCENE_MIN_VAL HEMISPHERE_MIN_CV
#define SCENE_ACCEL_MIN 16
#define SCENE_ACCEL_MAX 256

struct Scene {
    int16_t values[4];

    void Init() {
        for (int i = 0; i < NR_OF_SCENE_CHANNELS; ++i) values[i] = 0;
    }
};

// Preset storage spec
enum ScenesSettings {
    SCENE_FLAGS, // 8 bits

    // 16 bits each
    SCENE1_VALUE_A,
    SCENE1_VALUE_B,
    SCENE1_VALUE_C,
    SCENE1_VALUE_D,

    SCENE2_VALUE_A,
    SCENE2_VALUE_B,
    SCENE2_VALUE_C,
    SCENE2_VALUE_D,

    SCENE3_VALUE_A,
    SCENE3_VALUE_B,
    SCENE3_VALUE_C,
    SCENE3_VALUE_D,

    SCENE4_VALUE_A,
    SCENE4_VALUE_B,
    SCENE4_VALUE_C,
    SCENE4_VALUE_D,

    SCENES_SETTING_LAST
};

class ScenesAppPreset : public settings::SettingsBase<ScenesAppPreset, SCENES_SETTING_LAST> {
public:

    bool is_valid() {
        return (values_[SCENE_FLAGS] & 0x01);
    }
    bool load_preset(Scene *s) {
        if (!is_valid()) return false; // don't try to load a blank

        int ix = 1; // skip validity flag
        for (int ch = 0; ch < NR_OF_SCENE_CHANNELS; ++ch) {
            // somestuff = values_[ix++];
            s[ch].values[0] = values_[ix++];
            s[ch].values[1] = values_[ix++];
            s[ch].values[2] = values_[ix++];
            s[ch].values[3] = values_[ix++];
        }

        return true;
    }
    void save_preset(Scene *s) {
        int ix = 0;

        values_[ix++] = 1; // validity flag

        for (int ch = 0; ch < NR_OF_SCENE_CHANNELS; ++ch) {
            values_[ix++] = s[ch].values[0];
            values_[ix++] = s[ch].values[1];
            values_[ix++] = s[ch].values[2];
            values_[ix++] = s[ch].values[3];
        }
    }

};

ScenesAppPreset scene_presets[NR_OF_SCENE_PRESETS];
static const char * scene_preset_id[] = { "A", "B", "C", "D" };

class ScenesApp : public HSApplication {
public:

	void Start() {
        // make sure to turn this off, just in case
        FreqMeasure.end();
        OC::DigitalInputs::reInit();
	}
	
    void ClearPreset() {
        for (int ch = 0; ch < NR_OF_SCENE_CHANNELS; ++ch) {
            scene[ch].Init();
        }
        //SavePreset();
    }
    void LoadPreset() {
        bool success = scene_presets[index].load_preset(scene);
        if (success) {
            for (int ch = 0; ch < NR_OF_SCENE_CHANNELS; ++ch) {
            }
            preset_modified = 0;
        }
        else
            ClearPreset();
    }
    void SavePreset() {
        scene_presets[index].save_preset(scene);

        // initiate actual EEPROM save - ONLY if necessary!
        if (preset_modified) {
            OC::CORE::app_isr_enabled = false;
            OC::draw_save_message(60);
            delay(1);
            OC::save_app_data();
            delay(1);
            OC::CORE::app_isr_enabled = true;
        }

        preset_modified = 0;
    }

    void Suspend() {
      if (preset_modified) SavePreset();
    }
    void Resume() { }

    void Controller() {
        const int OCTAVE = (12 << 7);
        // -- core processing --

        // explicit gate/trigger priority right here:
        if (Gate(0)) // TR1 takes precedence
            trig_chan = 0;
        else if (Gate(1)) // TR2
            trig_chan = 1;
        else if (Gate(2)) // TR3
            trig_chan = 2;
        else if (Gate(3)) // TR4 - TODO: aux trigger modes, random, etc.
            trig_chan = 3;
        // else, it's unchanged

        scene4seq = (In(3) > 2 * OCTAVE); // gate at CV4
        if (scene4seq) {
          if (!Sequence.active) Sequence.Generate();
          if (Clock(3)) Sequence.Advance();
        } else {
            Sequence.active = 0;
        }

        // CV2: bipolar offset added to all values
        cv_offset = DetentedIn(1);

        // CV3: Slew/Smoothing
        smoothing_mod = smoothing;
        if (DetentedIn(2) > 0) {
            Modulate(smoothing_mod, 2, 0, 127);
        }

        // -- update active scene values, with smoothing
        // CV1: smooth interpolation offset, starting from last triggered scene
        if (DetentedIn(0)) {
            int cv = In(0);
            int direction = (cv < 0) ? -1 : 1;
            int volt = cv / OCTAVE;
            int partial = abs(cv % OCTAVE);

            // for display cursor - scaled to pixels, 32px per volt
            smooth_offset = cv * 32 / OCTAVE;

            int first = (trig_chan + volt + NR_OF_SCENE_CHANNELS) % NR_OF_SCENE_CHANNELS;
            int second = (first + direction + NR_OF_SCENE_CHANNELS) % NR_OF_SCENE_CHANNELS;

            for (int i = 0; i < NR_OF_SCENE_CHANNELS; ++i) {
                int16_t v1 = scene[first].values[i];
                int16_t v2 = scene[second].values[i];

                // the sequence will determine which other value is blended in for Scene 4
                if (scene4seq) {
                  if (first == 3)
                    v1 = scene[Sequence.Get(i) / 4].values[Sequence.Get(i) % 4];
                  if (second == 3)
                    v2 = scene[Sequence.Get(i) / 4].values[Sequence.Get(i) % 4];
                }

                // a weighted average of the two chosen scene values
                int target = ( v1 * (OCTAVE - partial) + v2 * partial ) / OCTAVE;
                target = constrain(target + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);

                slew(active_scene.values[i], target);
            }
        } else if (scene4seq && trig_chan == 3) { // looped sequence for TR4
            for (int i = 0; i < NR_OF_SCENE_CHANNELS; ++i) {
                int target = scene[ Sequence.Get(i) / 4 ].values[ Sequence.Get(i) % 4 ];
                target = constrain(target + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);
                slew(active_scene.values[i], target);
            }
            smooth_offset = 0;
        } else { // a simple scene copy will suffice
            for (int i = 0; i < NR_OF_SCENE_CHANNELS; ++i) {
                int target = constrain(scene[trig_chan].values[i] + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);
                slew(active_scene.values[i], target);
            }
            smooth_offset = 0;
        }

        // set outputs
        for (int ch = 0; ch < NR_OF_SCENE_CHANNELS; ++ch) {
            if (trigsum_mode && ch == 3) { // TrigSum output overrides D
                if (Clock(0) || Clock(1) || Clock(2) || Clock(3))
                    ClockOut(3);
                continue;
            }
            Out(ch, active_scene.values[ch]);
        }

        // encoder deceleration
        if (left_accel > SCENE_ACCEL_MIN) --left_accel;
        else left_accel = SCENE_ACCEL_MIN; // just in case lol

        if (right_accel > SCENE_ACCEL_MIN) --right_accel;
        else right_accel = SCENE_ACCEL_MIN;
    }

    void View() {
        gfxHeader("Scenes");

        if (preset_select) {
            gfxPrint(70, 1, "- Presets");
            DrawPresetSelector();
        } else {
            gfxPos(110, 1);
            if (preset_modified) gfxPrint("*");
            if (scene_presets[index].is_valid()) gfxPrint(scene_preset_id[index]);

            DrawInterface();
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        // Toggle between A or B editing
        // also doubles as Load or Save for preset select
        edit_mode_left = !edit_mode_left;

        // prevent saving to the (clear) slot
        if (edit_mode_left && preset_select == 5) preset_select = 4;
    }

    void OnLeftButtonLongPress() {
        //if (preset_select) return;
        trigsum_mode = !trigsum_mode;
    }

    void OnRightButtonPress() {
        if (preset_select) {
            // special case to clear values
            if (!edit_mode_left && preset_select == NR_OF_SCENE_PRESETS + 1) {
                ClearPreset();
                preset_modified = 1;
            }
            else {
                index = preset_select - 1;
                if (edit_mode_left) SavePreset();
                else LoadPreset();
            }

            preset_select = 0;
            return;
        }

        // Toggle between C or D editing
        edit_mode_right = !edit_mode_right;
    }

    void SwitchChannel(bool up) {
        if (!preset_select) {
            sel_chan += (up? 1 : -1) + NR_OF_SCENE_CHANNELS;
            sel_chan %= NR_OF_SCENE_CHANNELS;
        } else {
            // always cancel preset select on single click
            preset_select = 0;
        }
    }

    void OnDownButtonLongPress() {
        // show preset screen, select last loaded
        preset_select = 1 + index;
    }

    // Left encoder: Edit A or B on current scene
    void OnLeftEncoderMove(int direction) {
        if (preset_select) {
            edit_mode_left = (direction>0);
            // prevent saving to the (clear) slot
            if (edit_mode_left && preset_select == 5) preset_select = 4;
            return;
        }

        preset_modified = 1;

        int idx = 0 + edit_mode_left;
        scene[sel_chan].values[idx] += direction * left_accel;
        scene[sel_chan].values[idx] = constrain( scene[sel_chan].values[idx], SCENE_MIN_VAL, SCENE_MAX_VAL );

        left_accel <<= 3;
        if (left_accel > SCENE_ACCEL_MAX) left_accel = SCENE_ACCEL_MAX;
    }

    // Right encoder: Edit C or D on current scene
    void OnRightEncoderMove(int direction) {
        if (preset_select) {
            preset_select = constrain(preset_select + direction, 1, NR_OF_SCENE_PRESETS + (1-edit_mode_left));
            return;
        }

        preset_modified = 1;

        int idx = 2 + edit_mode_right;
        scene[sel_chan].values[idx] += direction * right_accel;
        scene[sel_chan].values[idx] = constrain( scene[sel_chan].values[idx], SCENE_MIN_VAL, SCENE_MAX_VAL );

        right_accel <<= 3;
        if (right_accel > SCENE_ACCEL_MAX) right_accel = SCENE_ACCEL_MAX;
    }

private:
    static const int SEQ_LENGTH = 16;

    int index = 0;
    int cv_offset = 0;

	int sel_chan = 0;
    int trig_chan = 0;
    int preset_select = 0; // both a flag and an index
    bool preset_modified = 0;
    bool edit_mode_left = 0;
    bool edit_mode_right = 0;
    bool trigsum_mode = 0;
    bool scene4seq = 0;
    // oh jeez, why do we have so many bools?!

    struct {
        bool active = 0;
        uint64_t sequence[4]; // four 16-step sequences of 4-bit values
        uint8_t step;

        void Generate() {
            for (int i = 0; i < NR_OF_SCENE_PRESETS; ++i) {
                sequence[i] = random() | (uint64_t(random()) << 32);
            }
            step = 0;
            active = 1;
        }
        void Advance() {
            ++step %= SEQ_LENGTH;
        }
        const uint8_t Get(int i) {
            return (uint8_t)( (sequence[i] >> (step * 4)) & 0x0F ); // 4-bit value, 0 to 15
        }
    } Sequence;

    uint16_t left_accel = SCENE_ACCEL_MIN;
    uint16_t right_accel = SCENE_ACCEL_MIN;

    int smooth_offset = 0; // -128 to 128, for display

    Scene scene[NR_OF_SCENE_CHANNELS];
    Scene active_scene;

    int smoothing, smoothing_mod;

    template <typename T>
    void slew(T &old_val, const int new_val = 0) {
        const int s = 1 + smoothing_mod;
        // more smoothing causes more ticks to be skipped
        if (OC::CORE::ticks % s) return;

        old_val = (old_val * (s - 1) + new_val) / s;
    }

    void DrawPresetSelector() {
        // index is the currently loaded preset (0-3)
        // preset_select is current selection (1-4, 5=clear)
        int y = 5 + 10*preset_select;
        gfxPrint(25, y, edit_mode_left ? "Save" : "Load");
        gfxIcon(50, y, RIGHT_ICON);

        for (int i = 0; i < NR_OF_SCENE_PRESETS; ++i) {
            gfxPrint(60, 15 + i*10, scene_preset_id[i]);
            if (!scene_presets[i].is_valid())
                gfxPrint(" (empty)");
            else if (i == index)
                gfxPrint(" *");
        }
        if (!edit_mode_left)
            gfxPrint(60, 55, "[CLEAR]");
    }

    void DrawInterface() {
        for (int i = 0; i < NR_OF_SCENE_CHANNELS; ++i) {
            gfxPrint(i*32 + 13, 14, i+1);
        }
        // active scene indicator
        uint8_t x = (12 + trig_chan*32 + smooth_offset + 128) % 128;
        gfxInvert(x, 13, 9, 10);

        // edit pointer
        gfxIcon(sel_chan*32 + 13, 25, UP_ICON);

        gfxPrint(8, 35, "A:");
        gfxPrintVoltage(scene[sel_chan].values[0]);
        gfxPrint(8, 45, "B:");
        gfxPrintVoltage(scene[sel_chan].values[1]);

        gfxIcon(0, 35 + 10*edit_mode_left, RIGHT_ICON);

        gfxPrint(72, 35, "C:");
        gfxPrintVoltage(scene[sel_chan].values[2]);
        gfxPrint(72, 45, "D:");
        if (trigsum_mode)
            gfxPrint("(trig)");
        else
            gfxPrintVoltage(scene[sel_chan].values[3]);

        gfxIcon(64, 35 + 10*edit_mode_right, RIGHT_ICON);

        // ------------------- //
        gfxLine(0, 54, 127, 54);

        // -- Input indicators
        // bias (CV2)
        if (cv_offset) {
            gfxIcon(0, 56, UP_DOWN_ICON);
            gfxPos(10, 56);
            gfxPrintVoltage(cv_offset);
        }
        // slew amount (CV3)
        gfxIcon(64, 56, SLEW_ICON);
        gfxPrint(74, 56, smoothing_mod);

        // sequencer (CV4)
        if (scene4seq) gfxIcon( 108, 56, LOOP_ICON );
    }
};

// TOTAL EEPROM SIZE: 264 bytes
SETTINGS_DECLARE(ScenesAppPreset, SCENES_SETTING_LAST) {
    {0, 0, 255, "Flags", NULL, settings::STORAGE_TYPE_U8},

    {0, 0, 65535, "Scene1ValA", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene1ValB", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene1ValC", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene1ValD", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scene2ValA", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene2ValB", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene2ValC", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene2ValD", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scene3ValA", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene3ValB", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene3ValC", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene3ValD", NULL, settings::STORAGE_TYPE_U16},

    {0, 0, 65535, "Scene4ValA", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene4ValB", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene4ValC", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Scene4ValD", NULL, settings::STORAGE_TYPE_U16},
};


ScenesApp ScenesApp_instance;

// App stubs
void ScenesApp_init() { ScenesApp_instance.BaseStart(); }

static constexpr size_t ScenesApp_storageSize() {
    return ScenesAppPreset::storageSize() * NR_OF_SCENE_PRESETS;
}

static size_t ScenesApp_save(void *storage) {
    size_t used = 0;
    for (int i = 0; i < 4; ++i) {
        used += scene_presets[i].Save(static_cast<char*>(storage) + used);
    }
    return used;
}

static size_t ScenesApp_restore(const void *storage) {
    size_t used = 0;
    for (int i = 0; i < 4; ++i) {
        used += scene_presets[i].Restore(static_cast<const char*>(storage) + used);
    }
    ScenesApp_instance.LoadPreset();
    return used;
}

void ScenesApp_isr() { return ScenesApp_instance.BaseController(); }

void ScenesApp_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        //ScenesApp_instance.Resume();
        break;

    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
        ScenesApp_instance.Suspend();
        break;

    default: break;
    }
}

void ScenesApp_loop() {} // Deprecated

void ScenesApp_menu() { ScenesApp_instance.BaseView(); }

void ScenesApp_screensaver() {
    ScenesApp_instance.BaseScreensaver();
}

void ScenesApp_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    // For right encoder, only handle press (long press is reserved)
    // For up button, handle only press (long press is reserved)
    // For down button, handle press and long press
    switch (event.type) {
    case UI::EVENT_BUTTON_DOWN:
        break;
    case UI::EVENT_BUTTON_PRESS: {
        switch (event.control) {
        case OC::CONTROL_BUTTON_L:
            ScenesApp_instance.OnLeftButtonPress();
            break;
        case OC::CONTROL_BUTTON_R:
            ScenesApp_instance.OnRightButtonPress();
            break;
        case OC::CONTROL_BUTTON_DOWN:
        case OC::CONTROL_BUTTON_UP:
            ScenesApp_instance.SwitchChannel(event.control == OC::CONTROL_BUTTON_DOWN);
            break;
        default: break;
        }
    } break;
    case UI::EVENT_BUTTON_LONG_PRESS:
        if (event.control == OC::CONTROL_BUTTON_L) {
            ScenesApp_instance.OnLeftButtonLongPress();
        }
        if (event.control == OC::CONTROL_BUTTON_DOWN) {
            ScenesApp_instance.OnDownButtonLongPress();
        }
        break;

    default: break;
    }
}

void ScenesApp_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) ScenesApp_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) ScenesApp_instance.OnRightEncoderMove(event.value);
}

#endif // ENABLE_APP_SCENES
