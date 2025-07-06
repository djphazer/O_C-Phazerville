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
*/

#include "HSGamepad.h"

class JoyStyx : public HemisphereApplet {
    HS::GamepadFrame &gs = HS::frame.GamepadState;
    public:
        const char* applet_name() {
            return "JoyStyx";
        }
        const uint8_t* applet_icon() { return PhzIcons::gamepad; }

        enum JoyStyxCursor {
            PARAM1,
            PARAM2,

            CURSOR_LAST = PARAM2
        };

        void Start() {
            gamepad_type = gs.gamepad_type;
            ConnectGamepad();

            param[0] = 0;
            param[1] = 1;
        }

        void Controller() {
            if (gamepad_type != gs.gamepad_type) {
                gamepad_type = gs.gamepad_type;
                ConnectGamepad();

                ForEachChannel(ch) {
                    CONSTRAIN(param[ch], 0, gp->button_count-1 + gp->axis_count-1);
                };
            }

            if (learn > -1) {
                if (last_changed != gs.last_changed) {
                    last_changed = gs.last_changed;
                    param[learn] = last_changed;
                    learn = -1;
                }
            } else {
                ForEachChannel(ch) {
                    if (param[ch] > gp->button_count-1) {
                        cv[ch] = gs.axis[param[ch] - gp->button_count];
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
                { param[0], 0, gp->button_count + gp->axis_count - 1}, // PARAM1
                { param[1], 0, gp->button_count + gp->axis_count - 1}, // PARAM2
            };

            // adjust param
            params[cursor].p = constrain(params[cursor].p + direction, params[cursor].min, params[cursor].max);
        }

        uint64_t OnDataRequest() {
            uint64_t data = 0;
            Pack(data, PackLocation {0,8}, param[0]);
            Pack(data, PackLocation {8,8}, param[1]);
            return data;
        }

        void OnDataReceive(uint64_t data) {
            param[0] = constrain(Unpack(data, PackLocation {0,8}), 0, gp->button_count + gp->axis_count - 1);
            param[1] = constrain(Unpack(data, PackLocation {8,8}), 0, gp->button_count + gp->axis_count - 1);
        }

    protected:
        void SetHelp() {
          // TODO
        }

    private:
        int cursor;
        int cv[2] = {0, 0};
        uint8_t param[2];
        int learn = -1;
        uint32_t last_changed = 0;

        int gamepad_type;
        GamePad *gp;

        void DrawInterface() {
            int y = 14;
            ForEachChannel(ch) {
                char out_label[] = {(char)('A' + io_offset + ch), '\0' };
                gfxPrint(1, y, out_label); gfxPrint(": ");
                gfxPrint((learn == ch) ? "Learn" :
                    (param[ch] > gp->button_count-1) ?
                        gp->axis_name[param[ch] - gp->button_count] :
                        gp->button_name[param[ch]]
                );
                y += 14;
            }
            gfxPrint(1, y, cv[0]); gfxPrint(32, y, cv[1]);
            y += 14;
            gfxPrint(1, y, gp->type_name);
            gfxCursor(7*2, 23 + cursor * 14, 49);
        }

        void ConnectGamepad() {
            switch (gamepad_type) {
                case (JoystickController::joytype_t::PS4):
                    gp = &PS4;
                    break;
                case (JoystickController::joytype_t::XBOXONE):
                    gp = &XBOXONE;
                    break;
                case (JoystickController::joytype_t::XBOX360W):
                case (JoystickController::joytype_t::XBOX360USB):
                    gp = &XBOX360;
                    break;
                case (JoystickController::joytype_t::SNES):
                    gp = &SNES;
                    break;
                case (JoystickController::joytype_t::N64):
                    gp = &N64;
                    break;
                default:
                    gp = &UNKNOWN;
                    break;
            }
        }

};