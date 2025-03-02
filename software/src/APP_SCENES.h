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
#ifdef __IMXRT1062__
static const char * const SCENERY_SAVEFILE = "SCENERY.DAT";
static const int NR_OF_SCENES = 8;
#else
static const int NR_OF_SCENES = 4;
#endif
static const int NR_OF_OUTPUTS = DAC_CHANNEL_LAST;

#define SCENE_MAX_VAL HEMISPHERE_MAX_CV
#define SCENE_MIN_VAL HEMISPHERE_MIN_CV

static constexpr int SCENE_ACCEL_MIN = 16;
static constexpr int SCENE_ACCEL_MAX = 256;

struct Scene {
    int16_t values[NR_OF_OUTPUTS];

    void Init() {
        for (int i = 0; i < NR_OF_OUTPUTS; ++i) values[i] = 0;
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

#ifdef __IMXRT1062__
struct ScenesAppPreset {
  bool valid = false;
  size_t index = 0;

  const bool is_valid() { return valid; }
  bool load_preset(Scene *s) {
    uint64_t data;
    size_t word = 0, blob = index << 8;

    for (int sn = 0; sn < NR_OF_SCENES; ++sn) {
      for (int ch = 0; ch < NR_OF_OUTPUTS; ++ch) {
        if (word == 0) {
          if (!PhzConfig::getValue(blob++, data))
            return false;
        }
        s[sn].values[ch] = Unpack(data, PackLocation{word*16, 16});
        ++word %= 4;
      }
    }
    valid = true;
    return true;
  }
  void save_preset(Scene *s) {
    uint64_t data = 0;
    size_t word = 0, blob = index << 8;
    for (int sn = 0; sn < NR_OF_SCENES; ++sn) {
      for (int ch = 0; ch < NR_OF_OUTPUTS; ++ch) {
        Pack(data, PackLocation{word*16, 16}, (uint16_t)s[sn].values[ch]);
        if (++word == 4) {
          PhzConfig::setValue(blob++, data);
          data = 0;
          word = 0;
        }
      }
    }
    valid = true;
    PhzConfig::save_config(SCENERY_SAVEFILE);
  }
};
#else
class ScenesAppPreset : public settings::SettingsBase<ScenesAppPreset, SCENES_SETTING_LAST> {
public:

    bool is_valid() {
        return (values_[SCENE_FLAGS] & 0x01);
    }
    bool load_preset(Scene *s) {
        if (!is_valid()) return false; // don't try to load a blank

        int ix = 1; // skip validity flag
        for (int i = 0; i < NR_OF_SCENES; ++i) {
            for (int d = 0; d < NR_OF_OUTPUTS; ++d) {
              s[i].values[d] = values_[ix++];
            }
        }

        return true;
    }
    void save_preset(Scene *s) {
        int ix = 0;

        values_[ix++] = 1; // validity flag

        for (int i = 0; i < NR_OF_SCENES; ++i) {
            for (int d = 0; d < NR_OF_OUTPUTS; ++d) {
              values_[ix++] = s[i].values[d];
            }
        }
    }

};
#endif

ScenesAppPreset scene_presets[NR_OF_SCENE_PRESETS];

class ScenesApp : public HSApplication {
public:

	void Start() {
        // make sure to turn this off, just in case
        FreqMeasure.end();
        OC::DigitalInputs::reInit();
	}
	
    void ClearPreset() {
        for (int ch = 0; ch < NR_OF_SCENES; ++ch) {
            scene[ch].Init();
        }
        //SavePreset();
    }
    void LoadPreset() {
        bool success = scene_presets[index].load_preset(scene);
        if (success) {
            preset_modified = 0;
        }
        else
            ClearPreset();
    }
    void SavePreset() {
        scene_presets[index].save_preset(scene);

#ifndef __IMXRT1062__
        // initiate actual EEPROM save - ONLY if necessary!
        if (preset_modified) {
            OC::CORE::app_isr_enabled = false;
            OC::draw_save_message(60);
            delay(1);
            OC::save_app_data();
            delay(1);
            OC::CORE::app_isr_enabled = true;
        }
#endif

        preset_modified = 0;
    }

    void Suspend() {
      if (preset_modified) SavePreset();
    }
    void Resume() {
#ifdef __IMXRT1062__
      PhzConfig::load_config(SCENERY_SAVEFILE);
      uint64_t data;
      for (int id = 0; id < NR_OF_SCENE_PRESETS; ++id) {
        scene_presets[id].index = id;
        scene_presets[id].valid = PhzConfig::getValue(id << 8, data);
      }
#endif
    }

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
          bool altseq = (In(3) < -2 * OCTAVE); // negative gate
          if (altseq && Clock(3)) {
            NextScene();
          }
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

            // for display cursor - scaled to pixels
            smooth_offset = cv * SWIDTH / OCTAVE;

            int first = (trig_chan + volt + NR_OF_SCENES) % NR_OF_SCENES;
            int second = (first + direction + NR_OF_SCENES) % NR_OF_SCENES;

            for (int i = 0; i < NR_OF_OUTPUTS; ++i) {
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
            for (int i = 0; i < NR_OF_OUTPUTS; ++i) {
                int target = scene[ Sequence.Get(i) / 4 ].values[ Sequence.Get(i) % 4 ];
                target = constrain(target + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);
                slew(active_scene.values[i], target);
            }
            smooth_offset = 0;
        } else { // a simple scene copy will suffice
            for (int i = 0; i < NR_OF_OUTPUTS; ++i) {
                int target = constrain(scene[trig_chan].values[i] + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);
                slew(active_scene.values[i], target);
            }
            smooth_offset = 0;
        }

        // set outputs
        for (int ch = 0; ch < NR_OF_OUTPUTS; ++ch) {
            if (trigsum_mode && ch == 3) { // TrigSum output overrides D
                if (Clock(0) || Clock(1) || Clock(2) || Clock(3))
                    ClockOut(3);
                continue;
            }
            Out(ch, active_scene.values[ch]);
        }

        // encoder deceleration
        if (right_accel > SCENE_ACCEL_MIN) --right_accel;
        else right_accel = SCENE_ACCEL_MIN;

        if (edit_timer) --edit_timer;
    }

    void View() {
        gfxHeader("Scenery", PhzIcons::mixerBal);

        if (preset_select) {
            gfxPrint(70, 1, "- Presets");
            DrawPresetSelector();
        } else {
            gfxPos(116, 1);
            if (preset_modified) gfxPrint("*");
            if (scene_presets[index].is_valid()) gfxPrint(OC::Strings::capital_letters[index]);

            DrawInterface();
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void PreviousScene() {
      if (--trig_chan < 0) trig_chan = NR_OF_SCENES - 1;
    }
    void NextScene() {
      ++trig_chan %= NR_OF_SCENES;
    }
    void ZapButton() {
      trig_chan = random(NR_OF_SCENES);
    }

    void OnLeftButtonPress() {
        isEditing = !isEditing;
    }

    void OnLeftButtonLongPress() {
        //if (preset_select) return;
        trigsum_mode = !trigsum_mode;
    }

    void OnRightButtonPress() {
        if (preset_select) {
            // special case to clear values
            if (!save_select && preset_select == NR_OF_SCENE_PRESETS + 1) {
                ClearPreset();
                preset_modified = 1;
            }
            else {
                index = preset_select - 1;
                if (save_select) SavePreset();
                else LoadPreset();
            }

            preset_select = 0;
            return;
        }

        isEditing = !isEditing;
    }

    void SwitchEditChannel(bool up) {
        if (!preset_select) {
            sel_chan += (up? 1 : -1) + NR_OF_SCENES;
            sel_chan %= NR_OF_SCENES;
        } else {
            // always cancel preset select on single click
            preset_select = 0;
        }
    }

    void OnDownButtonLongPress() {
        // show preset screen, select last loaded
        preset_select = 1 + index;
    }

    // Left encoder: move cursor or coarse adjust
    void OnLeftEncoderMove(int direction) {
        if (preset_select) {
            save_select = (direction>0);
            // prevent saving to the (clear) slot
            if (save_select && preset_select == 5) preset_select = 4;
            return;
        }
        if (!isEditing) {
          cursor = constrain(cursor + direction, 0, NR_OF_OUTPUTS - 1);
          ResetCursor();
          return;
        }

        preset_modified = 1;
        edit_timer = EDIT_TIMEOUT;
        scene[sel_chan].values[cursor] += direction * (12 << 7); // coarse 1volt steps
        CONSTRAIN( scene[sel_chan].values[cursor], SCENE_MIN_VAL, SCENE_MAX_VAL );
    }

    // Right encoder: move cursor or fine-tune adjust
    void OnRightEncoderMove(int direction) {
        if (preset_select) {
            preset_select = constrain(preset_select + direction, 1, NR_OF_SCENE_PRESETS + (1-save_select));
            return;
        }
        if (!isEditing) {
          cursor = constrain(cursor + direction, 0, NR_OF_OUTPUTS - 1);
          ResetCursor();
          return;
        }

        preset_modified = 1;
        edit_timer = EDIT_TIMEOUT;
        scene[sel_chan].values[cursor] += direction * right_accel;
        CONSTRAIN( scene[sel_chan].values[cursor], SCENE_MIN_VAL, SCENE_MAX_VAL );

        right_accel <<= 3;
        if (right_accel > SCENE_ACCEL_MAX) right_accel = SCENE_ACCEL_MAX;
    }

private:
    static const int SEQ_LENGTH = 16;
    static const int EDIT_TIMEOUT = HEMISPHERE_CURSOR_TICKS * 2;
    static const int CHWIDTH = 128/NR_OF_OUTPUTS;
    static const int SWIDTH = 128/NR_OF_SCENES;

    int cursor = 0;
    int edit_timer = 0;
    int index = 0;
    int cv_offset = 0;

    int sel_chan = 0;
    int trig_chan = 0;
    int preset_select = 0; // both a flag and an index
    bool preset_modified = 0;
    bool save_select = 0;
    bool trigsum_mode = 0;
    bool scene4seq = 0;
    // oh jeez, why do we have so many bools?!

    struct {
        bool active = 0;
        uint64_t sequence[NR_OF_OUTPUTS]; // 16-step sequences of 4-bit values
        uint8_t step;

        void Generate() {
            for (int i = 0; i < NR_OF_OUTPUTS; ++i) {
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

    uint16_t right_accel = SCENE_ACCEL_MIN;

    int smooth_offset = 0; // -128 to 128, for display

    Scene scene[NR_OF_SCENES];
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
        gfxPrint(25, y, save_select ? "Save" : "Load");
        gfxIcon(50, y, RIGHT_ICON);

        for (int i = 0; i < NR_OF_SCENE_PRESETS; ++i) {
            gfxPrint(60, 15 + i*10, OC::Strings::capital_letters[i]);
            if (!scene_presets[i].is_valid())
                gfxPrint(" (empty)");
            else if (i == index)
                gfxPrint(" *");
        }
        if (!save_select)
            gfxPrint(60, 55, "[CLEAR]");
    }

    void DrawInterface() {
        for (int i = 0; i < NR_OF_SCENES; ++i) {
            gfxPrint(i*SWIDTH + SWIDTH/2 - 3, 12, i+1);
        }

        // edit indicator
        gfxFrame(sel_chan*SWIDTH + SWIDTH/2 - 4, 11, 9, 10, true);
        // active scene indicator
        {
          const uint8_t scene_x = (SWIDTH/2 - 4 + trig_chan*SWIDTH + smooth_offset + 128) % 128;
          gfxInvert(scene_x, 11, 9, 10);
        }

        for (int ch = 0; ch < NR_OF_OUTPUTS; ++ch) {
          const int x = CHWIDTH/2 + ch*CHWIDTH;
          const int y = 20;
          const int target = constrain(scene[sel_chan].values[ch] + cv_offset, SCENE_MIN_VAL, SCENE_MAX_VAL);
          const int v = 48 - ProportionCV(target, 28);

          if (cursor == ch && (isEditing || CursorBlink())) {
            gfxLine(x, y, x, y + 42);
            if (isEditing)
              gfxLine(x+1, y, x+1, y + 42);
            gfxRect(x - 3, v - 1, 7+isEditing, 3);
          } else {
            gfxDottedLine(x, y, x, y + 42);
            gfxFrame(x - 2, v - 1, 5, 3);
          }

        }

        if (trigsum_mode)
          gfxIcon(1, 1, PhzIcons::pigeons, true);

        // ------------------- //
        gfxDottedLine(0, 48, 127, 48, 4);

        // -- Input indicators
        // bias (CV2)
        if (cv_offset) {
            gfxIcon(0, 56, UP_DOWN_ICON);
        }
        // slew amount (CV3)
        gfxIcon(64, 1, SLEW_ICON);
        gfxPrint(73, 1, smoothing_mod);

        // sequencer (CV4)
        if (scene4seq) gfxIcon( 108, 1, LOOP_ICON );

        if (edit_timer) {
          // popup value display
          const int w = 40;
          graphics.clearRect(64 - w/2, 32 - 5, w+1, 13);
          gfxFrame(64 - w/2, 32 - 5, w+1, 13);
          gfxPos(66 - w/2, 33 - 3);
          gfxPrintVoltage(scene[sel_chan].values[cursor]);
        }
    }
};

#ifndef __IMXRT1062__
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
#endif


ScenesApp ScenesApp_instance;

// App stubs
void ScenesApp_init() { ScenesApp_instance.BaseStart(); }

static constexpr size_t ScenesApp_storageSize() {
#ifdef __IMXRT1062__
  return 0;
#else
    return ScenesAppPreset::storageSize() * NR_OF_SCENE_PRESETS;
#endif
}

static size_t ScenesApp_save(void *storage) {
    size_t used = 0;
#ifndef __IMXRT1062__
    for (int i = 0; i < 4; ++i) {
        used += scene_presets[i].Save(static_cast<char*>(storage) + used);
    }
#endif
    return used;
}

static size_t ScenesApp_restore(const void *storage) {
    size_t used = 0;
#ifndef __IMXRT1062__
    for (int i = 0; i < 4; ++i) {
        used += scene_presets[i].Restore(static_cast<const char*>(storage) + used);
    }
#endif
    ScenesApp_instance.LoadPreset();
    return used;
}

void ScenesApp_isr() { return ScenesApp_instance.BaseController(); }

void ScenesApp_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        ScenesApp_instance.Resume();
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
        if (event.control == OC::CONTROL_BUTTON_Z)
          ScenesApp_instance.ZapButton();
        if (event.control == OC::CONTROL_BUTTON_X)
          ScenesApp_instance.PreviousScene();
        if (event.control == OC::CONTROL_BUTTON_Y)
          ScenesApp_instance.NextScene();
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
            ScenesApp_instance.SwitchEditChannel(event.control == OC::CONTROL_BUTTON_DOWN);
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
