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

#include "HSGamepad.h"

class JoyStyx : public HemisphereApplet {
    HS::GamepadFrame &gs = HS::frame.GamepadState;
    public:

        const char* applet_name() {
            return "JoyStyx";
        }
        const uint8_t* applet_icon() { return PhzIcons::gamepad; }

        enum MyAppletCursor {
            PARAM1,
            PARAM2,

            CURSOR_LAST = PARAM2
        };

        void Start() {
            gamepad_type = gs.gamepad_type;
            // switch (gamepad_type) {
            //     case (JoystickController::joytype_t::UNKNOWN): {
            //         BUTTON_LAST = GAMEPAD::UNKNOWN::BUTTON_LAST;
            //         button_name = *GAMEPAD::UNKNOWN::button_name;
            //         AXIS_LAST = GAMEPAD::UNKNOWN::AXIS_LAST;
            //         axis_name = *GAMEPAD::UNKNOWN::axis_name;
            //         TRIG_MIN = GAMEPAD::UNKNOWN::TRIG_MIN;
            //         TRIG_MAX = GAMEPAD::UNKNOWN::TRIG_MAX;
            //         AXIS_MIN = GAMEPAD::UNKNOWN::AXIS_MIN;
            //         AXIS_LAST = GAMEPAD::UNKNOWN::AXIS_LAST;
            //         break;
            //     }
            //     case (JoystickController::joytype_t::XBOX360USB): {
            //         BUTTON_LAST = GAMEPAD::XBOX360USB::BUTTON_LAST;
            //         button_name = *GAMEPAD::XBOX360USB::button_name;
            //         AXIS_LAST = GAMEPAD::XBOX360USB::AXIS_LAST;
            //         axis_name = *GAMEPAD::XBOX360USB::axis_name;
            //         TRIG_MIN = GAMEPAD::XBOX360USB::TRIG_MIN;
            //         TRIG_MAX = GAMEPAD::XBOX360USB::TRIG_MAX;
            //         AXIS_MIN = GAMEPAD::XBOX360USB::AXIS_MIN;
            //         AXIS_LAST = GAMEPAD::XBOX360USB::AXIS_LAST;
            //         break;
            //     }
            //     case (JoystickController::joytype_t::XBOX360USB): {
            //         BUTTON_LAST = GAMEPAD::XBOX360USB::BUTTON_LAST;
            //         button_name = *GAMEPAD::XBOX360USB::button_name;
            //         AXIS_LAST = GAMEPAD::XBOX360USB::AXIS_LAST;
            //         axis_name = *GAMEPAD::XBOX360USB::axis_name;
            //         TRIG_MIN = GAMEPAD::XBOX360USB::TRIG_MIN;
            //         TRIG_MAX = GAMEPAD::XBOX360USB::TRIG_MAX;
            //         AXIS_MIN = GAMEPAD::XBOX360USB::AXIS_MIN;
            //         AXIS_LAST = GAMEPAD::XBOX360USB::AXIS_LAST;
            //         break;
            //     }
            //     default: break;
            // }

            param[0] = GAMEPAD::XBOX360USB::LT;
            param[1] = GAMEPAD::XBOX360USB::RT;
        }

        void Controller() {
            if (gamepad_type != gs.gamepad_type) {
                gamepad_type = gs.gamepad_type;
            }

            ForEachChannel(ch) {
                if (param[ch] > GAMEPAD::XBOX360USB::BUTTON_LAST) {
                    int p = param[ch] - GAMEPAD::XBOX360USB::BUTTON_LAST - 1;
                    if (p > GAMEPAD::XBOX360USB::RT) {
                        if (gs.axis[p] < 0) {
                            cv[ch] = Proportion(gs.axis[p], GAMEPAD::XBOX360USB::AXIS_MIN, HEMISPHERE_MIN_CV);
                        } else {
                            cv[ch] = Proportion(gs.axis[p], GAMEPAD::XBOX360USB::AXIS_MAX, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv[ch]);
                    } else {
                        cv[ch] = Proportion(gs.axis[p], GAMEPAD::XBOX360USB::TRIG_MAX, HEMISPHERE_MAX_CV);
                        Out(ch, cv[ch]);
                    }
                } else {
                    cv[ch] = (gs.button_mask & (1 << param[ch])) != 0;
                    GateOut(ch, cv[ch]);
                }
            }
        }

        void View() {
            DrawInterface();
        }

        /* The default encoder press action is to toggle editing.
         * You can override this for more complex behavior. */
        // void OnButtonPress() { }

        /* Pressing the select button after highlighting a parameter for editing
         * can invoke a secondary action here. By default, it just cancels editing. */
        // void AuxButton() { }

        void OnEncoderMove(int direction) {
            if (!EditMode()) {
                MoveCursor(cursor, direction, CURSOR_LAST);
                return;
            }

            // param LUT
            const struct { uint8_t &p; int min, max; } params[] = {
                { param[0], 0, GAMEPAD::XBOX360USB::BUTTON_LAST + GAMEPAD::XBOX360USB::AXIS_LAST + 1}, // PARAM1
                { param[1], 0, GAMEPAD::XBOX360USB::BUTTON_LAST + GAMEPAD::XBOX360USB::AXIS_LAST + 1}, // PARAM2
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
            param[0] = constrain(Unpack(data, PackLocation {0,8}), 0, GAMEPAD::XBOX360USB::BUTTON_LAST + GAMEPAD::XBOX360USB::AXIS_LAST + 1);
            param[1] = constrain(Unpack(data, PackLocation {8,8}), 0, GAMEPAD::XBOX360USB::BUTTON_LAST + GAMEPAD::XBOX360USB::AXIS_LAST + 1);
        }

    protected:
        void SetHelp() {
          // TODO
        }

    private:
        int cursor;
        int cv[2] = {0, 0};
        uint8_t param[2];

        // int BUTTON_LAST;
        // const char* button_name;
        // int AXIS_LAST;
        // const char* axis_name;
        // int TRIG_MIN;
        // int TRIG_MAX;
        // int AXIS_MIN;
        // int AXIS_MAX;

        int gamepad_type;
        int button;
        int axis;


        void DrawInterface() {
            int y = 14;
            ForEachChannel(ch) {
                char out_label[] = {(char)('A' + io_offset + ch), '\0' };
                gfxPrint(1, y, out_label); gfxPrint(": ");
                gfxPrint((param[ch] > GAMEPAD::XBOX360USB::BUTTON_LAST) ?
                    GAMEPAD::XBOX360USB::axis_name[param[ch] - GAMEPAD::XBOX360USB::BUTTON_LAST - 1] :
                    GAMEPAD::XBOX360USB::button_name[param[ch]]
                );
                y += 14;
            }
            gfxPrint(1, y, cv[0]); gfxPrint(32, y, cv[1]);
            y += 14;
            gfxPrint(1, y, GAMEPAD::type_name[gamepad_type]);
            gfxCursor(7*2, 23 + cursor * 14, 49);
        }

    };
