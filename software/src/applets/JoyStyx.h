// Copyright (c) 2025, Beau Sterling
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
    TODO:
    - update UI to handle mode selection
    - selectable cv output meters or revert to raw counts
    - selectable TR1 loopback for clock sync?
    - new mode: raw (default, as is now)
    - new mode: sample and hold (trig/clk clocked or button clocked?)
    - new mode: slewed momentary modulation (2 input buttons drive output hi or low with adj slew rate. optional rtz upon release)
    - show icons for axis/button input type
    - last cursor should display VID/PID
*/

#ifdef USB_GAMEPAD

#include "HSGamepad.h"

class JoyStyx : public HemisphereApplet {
    HS::GamepadFrame &gs = HS::frame.GamepadState;

    public:
        const char* applet_name() {
            return "JoyStyx";
        }
        const uint8_t* applet_icon() { return PhzIcons::gamepad; }

        enum JoyStyxCursor {
            OUTPUT1,
            OUTPUT2,
            MAP_INDEX,
            MAP_FUNCTION,
            GP_INPUT,
            GAMEPAD_INFO,

            CURSOR_LAST = GAMEPAD_INFO
        };

        void Start() {
            param[OUTPUT1] = 0;
            param[OUTPUT2] = 1;
            param[MAP_INDEX] = 0;
            param[MAP_FUNCTION] = gs.mapping->function;
            param[GP_INPUT] = gs.mapping->gamepad_input;
        }

        void Controller() {
            ForEachChannel(ch) {
                CONSTRAIN(param[ch], 0, gs.gamepad->button_count + gs.gamepad->axis_count-1); // in case a new controller is plugged in
            };

            if (learn > -1) {
                if (last_changed != gs.last_changed) {
                    last_changed = gs.last_changed;
                    param[learn] = last_changed;
                    learn = -1;
                }
            } else {
                ForEachChannel(ch) {
                    if (param[ch] > gs.gamepad->button_count-1) {
                        cv[ch] = gs.axis[param[ch] - gs.gamepad->button_count];
                        Out(ch, cv[ch]);
                    } else {
                        cv[ch] = (gs.button_mask & (1 << param[ch])) != 0;
                        GateOut(ch, cv[ch]);
                    }
                }
            }
        }

        void View() {
            DrawInterface();
        }

        void AuxButton() {
            if (learn == -1) {
                learn = cursor;
                last_changed = gs.last_changed;
            } else {
                learn = -1;
            }
        }

        void OnEncoderMove(int direction) {
            if (!EditMode()) {
                MoveCursor(cursor, direction, CURSOR_LAST);
                return;
            }

            // param LUT
            const struct { uint8_t &p; int min, max; } params[] = {
                { param[OUTPUT1], 0, gs.gamepad->button_count + gs.gamepad->axis_count - 1}, // pack local
                { param[OUTPUT2], 0, gs.gamepad->button_count + gs.gamepad->axis_count - 1}, // pack local
                { param[MAP_INDEX], 0, min(GAMEPAD_MAP_MAX - 1, gs.gamepad->button_count + gs.gamepad->axis_count - 1)},
                { param[MAP_FUNCTION], 0, GamepadFunctions::GP_FUNC_LAST}, // pack in ioframe
                { param[GP_INPUT], 0, gs.gamepad->button_count + gs.gamepad->axis_count - 1}, // pack in ioframe
                { param[GAMEPAD_INFO], 0, 0}
            };

            // adjust param
            params[cursor].p = constrain(params[cursor].p + direction, params[cursor].min, params[cursor].max);

            switch (cursor) {
                case MAP_FUNCTION:
                    gs.mapping[param[MAP_INDEX]].function = param[MAP_FUNCTION];
                    if ((param[MAP_FUNCTION] == GamepadFunctions::GP_LEARN) &&  // feedback to ioframe to exit learn
                        (gs.mapping[param[MAP_INDEX]].gamepad_input != param[GP_INPUT])) {
                            param[GP_INPUT] = gs.mapping[param[MAP_INDEX]].gamepad_input;
                            param[MAP_FUNCTION] = (param[GP_INPUT] < gs.gamepad->button_count) ?
                                GamepadFunctions::GP_GATE :
                                GamepadFunctions::GP_CV;
                            gs.mapping[param[MAP_INDEX]].function = param[MAP_FUNCTION];
                    }
                    break;
                case GP_INPUT:
                    gs.mapping[param[MAP_INDEX]].gamepad_input = param[GP_INPUT];
                    break;
                default:
                    break;
            }
        }

        uint64_t OnDataRequest() {
            uint64_t data = 0;
            Pack(data, PackLocation {0,8}, param[0]);
            Pack(data, PackLocation {8,8}, param[1]);
            return data;
        }

        void OnDataReceive(uint64_t data) {
            param[0] = constrain(Unpack(data, PackLocation {0,8}), 0, gs.gamepad->button_count + gs.gamepad->axis_count - 1);
            param[1] = constrain(Unpack(data, PackLocation {8,8}), 0, gs.gamepad->button_count + gs.gamepad->axis_count - 1);
        }

    protected:
        void SetHelp() {
          // TODO
        }

    private:
        int cursor;
        int cv[2] = {0, 0};
        uint8_t param[CURSOR_LAST+1];
        int learn = -1;
        uint32_t last_changed = 0;

        void DrawInterface() {
            if (cursor <= OUTPUT2) DrawOutputs();
            else if (cursor < GAMEPAD_INFO) DrawMappingConfig();
            else DrawGamepadInfo();
        }

        void DrawOutputs() {
            const int ROW_HEIGHT = 12;
            int y = 14;
            ForEachChannel(ch) {
                char out_label[] = {(char)('A' + io_offset + ch), '\0' };
                gfxPrint(1, y, out_label); gfxPrint(": ");
                gfxPrint((learn == ch) ? "Learn" :
                    (param[ch] < gs.gamepad->button_count) ?
                        gs.gamepad->button_name[param[ch]] :
                        gs.gamepad->axis_name[param[ch] - gs.gamepad->button_count]
                );
                y += ROW_HEIGHT;
            }
            gfxPrint(1, y, cv[0]); gfxPrint(32, y, cv[1]);
            y += ROW_HEIGHT;
            gfxPrint(1, y+6, gs.gamepad->type_name);
            gfxCursor(7*2, 23 + cursor * ROW_HEIGHT, 49);
        }

        void DrawMappingConfig() {
            const int ROW_HEIGHT = 14;
            int y = ROW_HEIGHT;
            gfxPrint(1, y, "Map: ");
            gfxPrint(OC::Strings::cv_input_names_none[3*32 + param[MAP_INDEX] + 1]);  // ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT + MIDIMAP_MAX + param[MAP_INDEX] + 1]);
            y += ROW_HEIGHT;
            gfxPrint(1, y, "Fn:  ");
            switch (gs.mapping[param[MAP_INDEX]].function) {
                case GamepadFunctions::GP_NOOP:
                    gfxPrint("None");
                    break;
                case GamepadFunctions::GP_CV:
                    gfxPrint("CV");
                    break;
                case GamepadFunctions::GP_GATE:
                    gfxPrint("Gate");
                    break;
                case GamepadFunctions::GP_TRIG:
                    gfxPrint("Trig");
                    break;
                case GamepadFunctions::GP_LEARN:
                    gfxPrint("Learn");
                    break;
                default:
                    break;
            }
            y += ROW_HEIGHT;
            gfxPrint(1, y, "In:  "); gfxPrint(parseMapInput(gs.mapping[param[MAP_INDEX]].gamepad_input));
            gfxCursor(4+7*3, 23 + (cursor - 2) * ROW_HEIGHT, 38);
        }

        void DrawGamepadInfo() {
            int y = 14;
            gfxPrint(1, y, gs.gamepad->type_name);
            y += 14;
            gfxPrint(1, y, "VID:"); graphics.printf("0x%04X", gs.vid);
            y += 14;
            gfxPrint(1, y, "PID:"); graphics.printf("0x%04X", gs.pid);
            y += 14;
            gfxPrint(1, y, "B:"); gfxPrint(gs.gamepad->button_count);
            gfxPrint(" X:"); gfxPrint(gs.gamepad->axis_count);
        }

        const char *const parseMapInput(int gp_in) {
            return (gp_in < gs.gamepad->button_count) ?
                gs.gamepad->button_name[gp_in] :
                gs.gamepad->axis_name[gp_in - gs.gamepad->button_count];
        }

};

#endif
