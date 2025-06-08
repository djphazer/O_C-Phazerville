// Copyright (c) 2024, _________
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
            param[0] = GAMEPAD::XBOX360USB::axis::LT;
            param[1] = GAMEPAD::XBOX360USB::axis::RT;
        }

        void Controller() {
            ForEachChannel(ch) {
                if (param[ch] > GAMEPAD::XBOX360USB::MAX_BUTTON) {
                    int p = param[ch] - GAMEPAD::XBOX360USB::MAX_BUTTON - 1;
                    if (p > GAMEPAD::XBOX360USB::RT) {
                        if (frame.GamepadState.axis[p] < 0) {
                            cv[ch] = -Proportion(frame.GamepadState.axis[p], 32767, HEMISPHERE_MIN_CV);
                        } else {
                            cv[ch] = Proportion(frame.GamepadState.axis[p], 32767, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv[ch]);
                    } else {
                        cv[ch] = Proportion(frame.GamepadState.axis[p], 255, HEMISPHERE_MAX_CV);
                        Out(ch, cv[ch]);
                    }
                } else {
                    cv[ch] = (frame.GamepadState.button_mask & (1 << param[ch])) != 0;
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
                { param[0], 0, GAMEPAD::XBOX360USB::MAX_BUTTON + GAMEPAD::XBOX360USB::MAX_AXIS + 1}, // PARAM1
                { param[1], 0, GAMEPAD::XBOX360USB::MAX_BUTTON + GAMEPAD::XBOX360USB::MAX_AXIS + 1}, // PARAM2
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
            param[0] = constrain(Unpack(data, PackLocation {0,8}), 0, GAMEPAD::XBOX360USB::MAX_BUTTON + GAMEPAD::XBOX360USB::MAX_AXIS + 1);
            param[1] = constrain(Unpack(data, PackLocation {8,8}), 0, GAMEPAD::XBOX360USB::MAX_BUTTON + GAMEPAD::XBOX360USB::MAX_AXIS + 1);
        }

    protected:
        void SetHelp() {
          // TODO
        }

    private:
        int cursor;
        int cv[2] = {0, 0};
        uint8_t param[2];

        void DrawInterface() {
            int y = 14;
            ForEachChannel(ch) {
                char out_label[] = {(char)('A' + io_offset + ch), '\0' };
                gfxPrint(1, y, out_label); gfxPrint(": ");
                gfxPrint((param[ch] > GAMEPAD::XBOX360USB::MAX_BUTTON) ?
                    GAMEPAD::XBOX360USB::axis_name[param[ch] - GAMEPAD::XBOX360USB::MAX_BUTTON - 1] :
                    GAMEPAD::XBOX360USB::button_name[param[ch]]
                );
                y += 14;
            }
            gfxPrint(1, y, cv[0]); gfxPrint(32, y, cv[1]);
            y += 14;
            gfxPrint(1, y, GAMEPAD::controller_type[frame.GamepadState.gamepad_type]);
            gfxCursor(7*2, 23 + cursor * 14, 49);
        }

    };
