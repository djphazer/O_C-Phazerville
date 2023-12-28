// Copyright (c) 2018, Jason Justian
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

#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_visualfx.h"
#include "OC_patterns.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
namespace menu = OC::menu;

#include "HemisphereApplet.h"
#include "HSApplication.h"
#include "HSicons.h"
#include "HSMIDI.h"
#include "HSClockManager.h"

ClockManager *ClockManager::instance = 0;

#include "hemisphere_config.h"
namespace HS {
Applet available_applets[] = HEMISPHERE_APPLETS;
Applet clock_setup_applet = DECLARE_APPLET(9999, 0x01, ClockSetup);
}

// The settings specify the selected applets, and 64 bits of data for each applet,
// plus 64 bits of data for the ClockSetup applet (which includes some misc config).
// This is the structure of a HemispherePreset in eeprom.
enum HEMISPHERE_SETTINGS {
    HEMISPHERE_SELECTED_LEFT_ID,
    HEMISPHERE_SELECTED_RIGHT_ID,
    HEMISPHERE_LEFT_DATA_B1,
    HEMISPHERE_RIGHT_DATA_B1,
    HEMISPHERE_LEFT_DATA_B2,
    HEMISPHERE_RIGHT_DATA_B2,
    HEMISPHERE_LEFT_DATA_B3,
    HEMISPHERE_RIGHT_DATA_B3,
    HEMISPHERE_LEFT_DATA_B4,
    HEMISPHERE_RIGHT_DATA_B4,
    HEMISPHERE_CLOCK_DATA1,
    HEMISPHERE_CLOCK_DATA2,
    HEMISPHERE_CLOCK_DATA3,
    HEMISPHERE_CLOCK_DATA4,
    HEMISPHERE_SETTING_LAST
};

static constexpr int HEMISPHERE_AVAILABLE_APPLETS = ARRAY_SIZE(HS::available_applets);
static const int HEM_NR_OF_PRESETS = 8;

static const char * hem_preset_name[HEM_NR_OF_PRESETS] = { "A", "B", "C", "D", "E", "F", "G", "H" };

/* Hemisphere Preset
 * - conveniently store/recall multiple configurations
 */
class HemispherePreset : public SystemExclusiveHandler,
    public settings::SettingsBase<HemispherePreset, HEMISPHERE_SETTING_LAST> {
public:
    int GetAppletId(int h) {
        return (h == LEFT_HEMISPHERE) ? values_[HEMISPHERE_SELECTED_LEFT_ID]
                                      : values_[HEMISPHERE_SELECTED_RIGHT_ID];
    }
    void SetAppletId(int h, int id) {
        apply_value(h, id);
    }
    bool is_valid() {
        return values_[HEMISPHERE_SELECTED_LEFT_ID] != 0;
    }

    uint64_t GetClockData() {
        return ( (uint64_t(values_[HEMISPHERE_CLOCK_DATA4]) << 48) |
                 (uint64_t(values_[HEMISPHERE_CLOCK_DATA3]) << 32) |
                 (uint64_t(values_[HEMISPHERE_CLOCK_DATA2]) << 16) |
                  uint64_t(values_[HEMISPHERE_CLOCK_DATA1]) );
    }
    void SetClockData(const uint64_t data) {
        apply_value(HEMISPHERE_CLOCK_DATA1, data & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA2, (data >> 16) & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA3, (data >> 32) & 0xffff);
        apply_value(HEMISPHERE_CLOCK_DATA4, (data >> 48) & 0xffff);
    }


    // Manually get data for one side
    uint64_t GetData(int h) {
        return (uint64_t(values_[8 + h]) << 48) |
               (uint64_t(values_[6 + h]) << 32) |
               (uint64_t(values_[4 + h]) << 16) |
               (uint64_t(values_[2 + h]));
    }

    /* Manually store state data for one side */
    void SetData(int h, uint64_t data) {
        apply_value(2 + h, data & 0xffff);
        apply_value(4 + h, (data >> 16) & 0xffff);
        apply_value(6 + h, (data >> 32) & 0xffff);
        apply_value(8 + h, (data >> 48) & 0xffff);
    }

    // TODO: I haven't updated the SysEx data structure here because I don't use it.
    // Clock data would probably be useful if it's not too big. -NJM
    void OnSendSysEx() {
        // Describe the data structure for the audience
        uint8_t V[18];
        V[0] = (uint8_t)values_[HEMISPHERE_SELECTED_LEFT_ID];
        V[1] = (uint8_t)values_[HEMISPHERE_SELECTED_RIGHT_ID];
        V[2] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B1] & 0xff);
        V[3] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B1] >> 8) & 0xff);
        V[4] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B1] & 0xff);
        V[5] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B1] >> 8) & 0xff);
        V[6] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B2] & 0xff);
        V[7] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B2] >> 8) & 0xff);
        V[8] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B2] & 0xff);
        V[9] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B2] >> 8) & 0xff);
        V[10] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B3] & 0xff);
        V[11] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B3] >> 8) & 0xff);
        V[12] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B3] & 0xff);
        V[13] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B3] >> 8) & 0xff);
        V[14] = (uint8_t)(values_[HEMISPHERE_LEFT_DATA_B4] & 0xff);
        V[15] = (uint8_t)((values_[HEMISPHERE_LEFT_DATA_B4] >> 8) & 0xff);
        V[16] = (uint8_t)(values_[HEMISPHERE_RIGHT_DATA_B4] & 0xff);
        V[17] = (uint8_t)((values_[HEMISPHERE_RIGHT_DATA_B4] >> 8) & 0xff);

        // Pack it up, ship it out
        UnpackedData unpacked;
        unpacked.set_data(18, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'H');
    }

    void OnReceiveSysEx() {
        uint8_t V[18];
        if (ExtractSysExData(V, 'H')) {
            values_[HEMISPHERE_SELECTED_LEFT_ID] = V[0];
            values_[HEMISPHERE_SELECTED_RIGHT_ID] = V[1];
            values_[HEMISPHERE_LEFT_DATA_B1] = ((uint16_t)V[3] << 8) + V[2];
            values_[HEMISPHERE_RIGHT_DATA_B1] = ((uint16_t)V[5] << 8) + V[4];
            values_[HEMISPHERE_LEFT_DATA_B2] = ((uint16_t)V[7] << 8) + V[6];
            values_[HEMISPHERE_RIGHT_DATA_B2] = ((uint16_t)V[9] << 8) + V[8];
            values_[HEMISPHERE_LEFT_DATA_B3] = ((uint16_t)V[11] << 8) + V[10];
            values_[HEMISPHERE_RIGHT_DATA_B3] = ((uint16_t)V[13] << 8) + V[12];
            values_[HEMISPHERE_LEFT_DATA_B4] = ((uint16_t)V[15] << 8) + V[14];
            values_[HEMISPHERE_RIGHT_DATA_B4] = ((uint16_t)V[17] << 8) + V[16];
        }
    }

};

// HemispherePreset hem_config; // special place for Clock data and Config data, 64 bits each

HemispherePreset hem_presets[HEM_NR_OF_PRESETS];
HemispherePreset *hem_active_preset = 0;

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Manager
////////////////////////////////////////////////////////////////////////////////

using namespace HS;

class HemisphereManager : public HSApplication {
public:
    enum PopupType {
      CLOCK_POPUP, PRESET_POPUP,
    };
    void Start() {
        //select_mode = -1; // Not selecting

        help_hemisphere = -1;
        clock_setup = 0;

        for (int i = 0; i < 4; ++i) {
            quant_scale[i] = OC::Scales::SCALE_SEMI;
            quantizer[i].Init();
            quantizer[i].Configure(OC::Scales::GetScale(quant_scale[i]), 0xffff);
        }

        SetApplet(0, get_applet_index_by_id(18)); // DualTM
        SetApplet(1, get_applet_index_by_id(15)); // EuclidX
    }

    void Resume() {
        if (!hem_active_preset)
            LoadFromPreset(0);
        // TODO: restore quantizer settings...
    }
    void Suspend() {
        if (hem_active_preset) {
            if (HS::auto_save_enabled || 0 == preset_id) StoreToPreset(preset_id, !HS::auto_save_enabled);
            hem_active_preset->OnSendSysEx();
        }
    }

    void StoreToPreset(HemispherePreset* preset, bool skip_eeprom = false) {
        bool doSave = (preset != hem_active_preset);

        hem_active_preset = preset;
        for (int h = 0; h < 2; h++)
        {
            int index = my_applet[h];
            if (hem_active_preset->GetAppletId(h) != HS::available_applets[index].id)
                doSave = 1;
            hem_active_preset->SetAppletId(h, HS::available_applets[index].id);

            uint64_t data = HS::available_applets[index].instance[h].OnDataRequest();
            if (data != applet_data[h]) doSave = 1;
            applet_data[h] = data;
            hem_active_preset->SetData(h, data);
        }
        uint64_t data = HS::clock_setup_applet.instance[0].OnDataRequest();
        if (data != clock_data) doSave = 1;
        clock_data = data;
        hem_active_preset->SetClockData(data);

        // initiate actual EEPROM save - ONLY if necessary!
        if (doSave && !skip_eeprom) {
            OC::CORE::app_isr_enabled = false;
            OC::draw_save_message(60);
            delay(1);
            OC::save_app_data();
            delay(1);
            OC::CORE::app_isr_enabled = true;
        }

    }
    void StoreToPreset(int id, bool skip_eeprom = false) {
        StoreToPreset( (HemispherePreset*)(hem_presets + id), skip_eeprom );
        preset_id = id;
    }
    void LoadFromPreset(int id) {
        hem_active_preset = (HemispherePreset*)(hem_presets + id);
        if (hem_active_preset->is_valid()) {
            clock_data = hem_active_preset->GetClockData();
            HS::clock_setup_applet.instance[0].OnDataReceive(clock_data);

            for (int h = 0; h < 2; h++)
            {
                int index = get_applet_index_by_id( hem_active_preset->GetAppletId(h) );
                applet_data[h] = hem_active_preset->GetData(h);
                SetApplet(h, index);
                HS::available_applets[index].instance[h].OnDataReceive(applet_data[h]);
            }
        }
        preset_id = id;
        PokePopup(PRESET_POPUP);
    }

    // does not modify the preset, only the manager
    void SetApplet(int hemisphere, int index) {
        my_applet[hemisphere] = index;
        HS::available_applets[index].instance[hemisphere].BaseStart(hemisphere);
    }

    void ChangeApplet(int h, int dir) {
        int index = get_next_applet_index(my_applet[h], dir);
        SetApplet(select_mode, index);
    }

    bool SelectModeEnabled() {
        return select_mode > -1;
    }

    void ProcessMIDI() {
        HS::IOFrame &f = HS::frame;

        while (usbMIDI.read()) {
            const int message = usbMIDI.getType();
            const int data1 = usbMIDI.getData1();
            const int data2 = usbMIDI.getData2();

            if (message == usbMIDI.SystemExclusive) {
                ReceiveManagerSysEx();
                continue;
            }

            if (message == usbMIDI.ProgramChange) {
                int slot = usbMIDI.getData1();
                if (slot < HEM_NR_OF_PRESETS) LoadFromPreset(slot);
                continue;
            }

            f.MIDIState.ProcessMIDIMsg(usbMIDI.getChannel(), message, data1, data2);
        }
    }

    void CheckPresetTriggers() {
        for (int i = 0; i < 4; ++i) {
            if (HS::trigger_mapping[i] > 4 && HS::frame.clocked[i]) {
                LoadFromPreset(HS::trigger_mapping[i] - 5);
                break;
            }
        }
    }

    void Controller() {
        // top-level MIDI-to-CV handling - alters frame outputs
        ProcessMIDI();
        CheckPresetTriggers();

        // Clock Setup applet handles internal clock duties
        HS::clock_setup_applet.instance[0].Controller();

        // execute Applets
        for (int h = 0; h < 2; h++)
        {
            int index = my_applet[h];
            if (HS::available_applets[index].id != 150) // not MIDI In
            {
                ForEachChannel(ch) {
                    int chan = h*2 + ch;
                    // mix CV inputs with applicable MIDI signals
                    switch (HS::frame.MIDIState.function[chan]) {
                    case HEM_MIDI_CC_OUT:
                    case HEM_MIDI_NOTE_OUT:
                    case HEM_MIDI_VEL_OUT:
                    case HEM_MIDI_AT_OUT:
                    case HEM_MIDI_PB_OUT:
                        HS::frame.inputs[chan] += HS::frame.MIDIState.outputs[chan];
                        break;
                    case HEM_MIDI_GATE_OUT:
                        HS::frame.gate_high[chan] |= (HS::frame.MIDIState.outputs[chan] > (12 << 7));
                        break;
                    case HEM_MIDI_TRIG_OUT:
                    case HEM_MIDI_CLOCK_OUT:
                    case HEM_MIDI_START_OUT:
                        HS::frame.clocked[chan] |= HS::frame.MIDIState.trigout_q[chan];
                        HS::frame.MIDIState.trigout_q[chan] = 0;
                        break;
                    }
                }
            }
            HS::available_applets[index].instance[h].BaseController();
        }
    }

    inline void PokePopup(PopupType pop) {
      popup_type = pop;
      popup_tick = OC::CORE::ticks;
    }

    void DrawPopup() {
        graphics.clearRect(24, 23, 80, 18);
        graphics.drawFrame(25, 24, 78, 16);

        graphics.setPrintPos(29, 28);

        switch (popup_type) {
          case CLOCK_POPUP:
            graphics.print("Clock ");
            if (clock_m->IsRunning())
              graphics.print("Start");
            else
              graphics.print(clock_m->IsPaused() ? "Armed" : "Stop");
          break;

          case PRESET_POPUP:
            graphics.print("> Preset ");
            graphics.print(hem_preset_name[preset_id]);
          break;
        }
    }

    void View() {
        if (config_menu) {
            DrawConfigMenu();
        }
        else if (clock_setup) {
            HS::clock_setup_applet.instance[0].View();
        }
        else if (help_hemisphere > -1) {
            int index = my_applet[help_hemisphere];
            HS::available_applets[index].instance[help_hemisphere].BaseView();
        } else {
            for (int h = 0; h < 2; h++)
            {
                int index = my_applet[h];
                HS::available_applets[index].instance[h].BaseView();
            }

            if (clock_m->IsRunning()) {
                // Metronome icon
                gfxIcon(56, 1, clock_m->Cycle() ? METRO_L_ICON : METRO_R_ICON);
            } else if (clock_m->IsPaused()) {
                gfxIcon(56, 1, PAUSE_ICON);
            }

            if (select_mode == LEFT_HEMISPHERE) graphics.drawFrame(0, 0, 64, 64);
            if (select_mode == RIGHT_HEMISPHERE) graphics.drawFrame(64, 0, 64, 64);
        }

        // Overlay popup window last
        if (OC::CORE::ticks - popup_tick < HEMISPHERE_CURSOR_TICKS) {
            DrawPopup();
        }
    }

    void DelegateEncoderPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (config_menu) {
            // button release for config screen
            if (!down) ConfigButtonPush(h);
            return;
        }

        // button down
        if (down) {
            // Clock Setup is more immediate for manual triggers
            if (clock_setup) HS::clock_setup_applet.instance[0].OnButtonPress();
            // TODO: consider a new OnButtonDown handler for applets
            return;
        }

        // button release
        if (select_mode == h) {
            select_mode = -1; // Pushing a button for the selected side turns off select mode
        } else if (!clock_setup) {
            // regular applets get button release
            int index = my_applet[h];
            HS::available_applets[index].instance[h].OnButtonPress();
        }
    }

    void DelegateSelectButtonPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        int hemisphere = (event.control == OC::CONTROL_BUTTON_UP) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;

        if (config_menu) {
            // cancel preset select, or config screen on select button release
            if (!down) {
                if (preset_cursor) {
                    preset_cursor = 0;
                }
                else config_menu = 0;
            }
            return;
        }

        if (clock_setup && !down) {
            clock_setup = 0; // Turn off clock setup with any single-click button release
            return;
        }

        // -- button down
        if (down) {
            // dual press for Clock Setup... check first_click, so we only process the 2nd button event
            if (event.mask == (OC::CONTROL_BUTTON_UP | OC::CONTROL_BUTTON_DOWN) && hemisphere != first_click) {
                clock_setup = 1;
                SetHelpScreen(-1);
                select_mode = -1;
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME) {
                // This is a double-click on one button. Activate corresponding help screen and deactivate select mode.
                if (hemisphere == first_click)
                    SetHelpScreen(hemisphere);

                // reset double-click timer either way
                click_tick = 0;
                return;
            }

            // -- Single click
            // If a help screen is already selected, and the button is for
            // the opposite one, go to the other help screen
            if (help_hemisphere > -1) {
                if (help_hemisphere != hemisphere) SetHelpScreen(hemisphere);
                else SetHelpScreen(-1); // Exit help screen if same button is clicked
                OC::ui.SetButtonIgnoreMask(); // ignore release
            }

            // mark this single click
            click_tick = OC::CORE::ticks;
            first_click = hemisphere;
            return;
        }

        // -- button release
        if (!clock_setup) {
            // Select Mode
            if (hemisphere == select_mode) select_mode = -1; // Exit Select Mode if same button is pressed
            else if (help_hemisphere < 0) // Otherwise, set Select Mode - UNLESS there's a help screen
                select_mode = hemisphere;
        }
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        if (config_menu) {
            ConfigEncoderAction(h, event.value);
            return;
        }

        if (clock_setup) {
            HS::clock_setup_applet.instance[0].OnEncoderMove(event.value);
        } else if (select_mode == h) {
            ChangeApplet(h, event.value);
        } else {
            int index = my_applet[h];
            HS::available_applets[index].instance[h].OnEncoderMove(event.value);
        }
    }

    void ToggleClockRun() {
        if (clock_m->IsRunning()) {
            clock_m->Stop();
        } else {
            bool p = clock_m->IsPaused();
            clock_m->Start( !p );
        }
        PokePopup(CLOCK_POPUP);
    }

    void ToggleClockSetup() {
        clock_setup = 1 - clock_setup;
    }

    void ToggleConfigMenu() {
        config_menu = !config_menu;
        if (config_menu) SetHelpScreen(-1);
    }

    void SetHelpScreen(int hemisphere) {
        if (help_hemisphere > -1) { // Turn off the previous help screen
            int index = my_applet[help_hemisphere];
            HS::available_applets[index].instance[help_hemisphere].ToggleHelpScreen();
        }

        if (hemisphere > -1) { // Turn on the next hemisphere's screen
            int index = my_applet[hemisphere];
            HS::available_applets[index].instance[hemisphere].ToggleHelpScreen();
        }

        help_hemisphere = hemisphere;
    }

private:
    int preset_id = 0;
    int preset_cursor = 0;
    int my_applet[2]; // Indexes to available_applets
    uint64_t clock_data, applet_data[2]; // cache of applet data
    bool clock_setup;
    bool config_menu;
    bool isEditing = false;
    int config_cursor = 0;

    int help_hemisphere; // Which of the hemispheres (if any) is in help mode, or -1 if none
    uint32_t click_tick; // Measure time between clicks for double-click
    uint32_t popup_tick; // for button feedback
    PopupType popup_type = PRESET_POPUP;
    int first_click; // The first button pushed of a double-click set, to see if the same one is pressed
    ClockManager *clock_m = clock_m->get();

    enum HEMConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,

        MAX_CURSOR = CURSOR_MODE
    };

    void ConfigEncoderAction(int h, int dir) {
        if (!isEditing && !preset_cursor) {
            config_cursor += dir;
            config_cursor = constrain(config_cursor, 0, MAX_CURSOR);
            ResetCursor();
            return;
        }

        switch (config_cursor) {
        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        //case SCREENSAVER_MODE:
            // TODO?
            //break;
        case SAVE_PRESET:
        case LOAD_PRESET:
            preset_cursor = constrain(preset_cursor + dir, 1, HEM_NR_OF_PRESETS);
            break;
        }
    }
    void ConfigButtonPush(int h) {
        if (preset_cursor) {
            // Save or Load on button push
            if (config_cursor == SAVE_PRESET)
                StoreToPreset(preset_cursor-1);
            else
                LoadFromPreset(preset_cursor-1);

            preset_cursor = 0; // deactivate preset selection
            config_menu = 0;
            isEditing = false;
            return;
        }

        switch (config_cursor) {
        case SAVE_PRESET:
        case LOAD_PRESET:
            preset_cursor = preset_id + 1;
            break;

        case AUTO_SAVE:
            HS::auto_save_enabled = !HS::auto_save_enabled;
            break;

        case TRIG_LENGTH:
            isEditing = !isEditing;
            break;

        case SCREENSAVER_MODE:
            ++HS::screensaver_mode %= 4;
            break;

        case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
            break;
        }
    }

    void DrawConfigMenu() {
        // --- Preset Selector
        if (preset_cursor) {
            DrawPresetSelector();
            return;
        }

        // --- Config Selection
        gfxHeader("Hemisphere Config");
        gfxPrint(1, 15, "Preset: ");
        gfxPrint(48, 15, "Load");
        gfxIcon(100, 15, HS::auto_save_enabled ? CHECK_ON_ICON : CHECK_OFF_ICON );
        gfxPrint(48, 25, "Save   (auto)");

        gfxPrint(1, 35, "Trig Length: ");
        gfxPrint(HS::trig_length);
        gfxPrint("ms");

        const char * ssmodes[4] = { "[blank]", "Meters", "Zaps",
        #if defined(__IMXRT1062__)
        "Stars"
        #else
        "Zips"
        #endif
        };
        gfxPrint(1, 45, "Screensaver:  ");
        gfxPrint( ssmodes[HS::screensaver_mode] );

        const char * cursor_mode_name[3] = { "modal", "modal+wrap" };
        gfxPrint(1, 55, "Cursor:  ");
        gfxPrint(cursor_mode_name[HS::cursor_wrap]);
        
        switch (config_cursor) {
        case LOAD_PRESET:
        case SAVE_PRESET:
            gfxIcon(73, 15 + (config_cursor - LOAD_PRESET)*10, LEFT_ICON);
            break;

        case AUTO_SAVE:
            gfxIcon(90, 15, RIGHT_ICON);
            break;

        case TRIG_LENGTH:
            if (isEditing) gfxInvert(79, 34, 25, 9);
            else gfxCursor(80, 43, 24);
            break;
        case SCREENSAVER_MODE:
            gfxIcon(73, 45, RIGHT_ICON);
            break;
        case CURSOR_MODE:
            gfxIcon(43, 55, RIGHT_ICON);
            break;
        }
    }

    void DrawPresetSelector() {
        gfxHeader("Hemisphere Presets");
        int y = 5 + constrain(preset_cursor,1,5)*10;
        gfxPrint(1, y, (config_cursor == SAVE_PRESET) ? "Save" : "Load");
        gfxIcon(26, y, RIGHT_ICON);
        const int top = constrain(preset_cursor - 4, 1, HEM_NR_OF_PRESETS) - 1;
        y = 15;
        for (int i = top; i < HEM_NR_OF_PRESETS && i < top + 5; ++i)
        {
            gfxPrint(35, y, hem_preset_name[i]);

            if (!hem_presets[i].is_valid())
                gfxPrint(" (empty)");
            else if (i == preset_id)
                gfxIcon(45, y, ZAP_ICON);

            y += 10;
        }
    }

    int get_applet_index_by_id(int id) {
        int index = 0;
        for (int i = 0; i < HEMISPHERE_AVAILABLE_APPLETS; i++)
        {
            if (HS::available_applets[i].id == id) index = i;
        }
        return index;
    }

    int get_next_applet_index(int index, int dir) {
        index += dir;
        if (index >= HEMISPHERE_AVAILABLE_APPLETS) index = 0;
        if (index < 0) index = HEMISPHERE_AVAILABLE_APPLETS - 1;

        return index;
    }
};

// TOTAL EEPROM SIZE: 4 * 26 bytes
SETTINGS_DECLARE(HemispherePreset, HEMISPHERE_SETTING_LAST) {
    {0, 0, 255, "Applet ID L", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Applet ID R", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 65535, "Data L block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 4", NULL, settings::STORAGE_TYPE_U16}
};

HemisphereManager manager;

void ReceiveManagerSysEx() {
    if (hem_active_preset)
        hem_active_preset->OnReceiveSysEx();
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void HEMISPHERE_init() {
    manager.BaseStart();
}

constexpr size_t HEMISPHERE_storageSize() {
    return HemispherePreset::storageSize() * HEM_NR_OF_PRESETS;
}

size_t HEMISPHERE_save(void *storage) {
    size_t used = 0;
    for (int i = 0; i < HEM_NR_OF_PRESETS; ++i) {
        used += hem_presets[i].Save(static_cast<char*>(storage) + used);
    }
    return used;
}

size_t HEMISPHERE_restore(const void *storage) {
    size_t used = 0;
    for (int i = 0; i < HEM_NR_OF_PRESETS; ++i) {
        used += hem_presets[i].Restore(static_cast<const char*>(storage) + used);
    }
    return used;
}

void FASTRUN HEMISPHERE_isr() {
    manager.BaseController();
}

void HEMISPHERE_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        manager.Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        manager.Suspend();
        break;

    default: break;
    }
}

void HEMISPHERE_loop() {} // Essentially deprecated in favor of ISR

void HEMISPHERE_menu() {
    manager.View();
}

typedef struct {
    int x = 0;
    int y = 0;
    int x_v = 6;
    int y_v = 3;

    void Move(bool stars) {
        if (stars) Move(6100, 2900);
        else Move();
    }
    void Move(int target_x = -1, int target_y = -1) {
        x += x_v;
        y += y_v;
        if (x > 12700 || x < 0 || y > 6300 || y < 0) {
            if (target_x < 0 || target_y < 0) {
                x = random(12700);
                y = random(6300);
            } else {
                x = target_x + random(400);
                y = target_y + random(400);
                CONSTRAIN(x, 0, 12700);
                CONSTRAIN(y, 0, 6300);
            }

            x_v = random(30) - 15;
            y_v = random(30) - 15;
            if (x_v == 0) ++x_v;
            if (y_v == 0) ++y_v;
        }
    }
} Zap;
static constexpr int HOW_MANY_ZAPS = 30;
static Zap zaps[HOW_MANY_ZAPS];
static void ZapScreensaver(const bool stars = false) {
  static int frame_delay = 0;
  for (int i = 0; i < (stars ? HOW_MANY_ZAPS : 5); i++) {
    if (frame_delay & 0x1) {
        #if defined(__IMXRT1062__)
        zaps[i].Move(stars); // centered starfield
        #else
        // Zips respawn from their previous sibling
        if (0 == i) zaps[0].Move();
        else zaps[i].Move(zaps[i-1].x, zaps[i-1].y);
        #endif
    }

    if (stars && frame_delay == 0) {
      // accel
      zaps[i].x_v *= 2;
      zaps[i].y_v *= 2;
    }

    if (stars)
      gfxPixel(zaps[i].x/100, zaps[i].y/100);
    else
      gfxIcon(zaps[i].x/100, zaps[i].y/100, ZAP_ICON);
  }
  if (--frame_delay < 0) frame_delay = 100;
}

void HEMISPHERE_screensaver() {
    switch (HS::screensaver_mode) {
    case 0x3: // Zips or Stars
        ZapScreensaver(true);
        break;
    case 0x2: // Zaps
        ZapScreensaver();
        break;
    case 0x1: // Meters
        manager.BaseScreensaver(true); // show note names
        break;
    default: break; // blank screen
    }
}

void HEMISPHERE_handleButtonEvent(const UI::Event &event) {
    switch (event.type) {
    case UI::EVENT_BUTTON_DOWN:
        #ifdef VOR
        if (event.control == OC::CONTROL_BUTTON_M) {
            manager.ToggleClockRun();
            OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
            break;
        }
        #endif
    case UI::EVENT_BUTTON_PRESS:
        if (event.control == OC::CONTROL_BUTTON_UP || event.control == OC::CONTROL_BUTTON_DOWN) {
            manager.DelegateSelectButtonPush(event);
        } else if (event.control == OC::CONTROL_BUTTON_L || event.control == OC::CONTROL_BUTTON_R) {
            manager.DelegateEncoderPush(event);
        }
        break;

    case UI::EVENT_BUTTON_LONG_PRESS:
        if (event.control == OC::CONTROL_BUTTON_DOWN) manager.ToggleConfigMenu();
        if (event.control == OC::CONTROL_BUTTON_L) manager.ToggleClockRun();
        break;

    default: break;
    }
}

void HEMISPHERE_handleEncoderEvent(const UI::Event &event) {
    manager.DelegateEncoderMovement(event);
}
