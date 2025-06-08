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
          param[0] = GAMEPAD::XBOX360::axis::LT;
          param[1] = GAMEPAD::XBOX360::axis::RT;
        }

        void Controller() {
            ForEachChannel(ch) {
                int cv = 0;
                switch (param[ch]) {
                    case GAMEPAD::XBOX360::axis::LT: {
                        cv = Proportion(frame.GpState.left_trigger_value, 255, HEMISPHERE_MAX_CV);
                        Out(ch, cv);
                        break;
                    }
                    case GAMEPAD::XBOX360::axis::RT: {
                        cv = Proportion(frame.GpState.right_trigger_value, 255, HEMISPHERE_MAX_CV);
                        Out(ch, cv);
                        break;
                    }
                    case GAMEPAD::XBOX360::axis::LX: {
                        if (frame.GpState.left_js_x_value < 0) {
                            cv = -Proportion(frame.GpState.left_js_x_value, 32767, HEMISPHERE_MIN_CV);
                        } else {
                            cv = Proportion(frame.GpState.left_js_x_value, 32767, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv);
                        break;
                    }
                    case GAMEPAD::XBOX360::axis::LY: {
                        if (frame.GpState.left_js_y_value < 0) {
                            cv = -Proportion(frame.GpState.left_js_y_value, 32767, HEMISPHERE_MIN_CV);
                        } else {
                            cv = Proportion(frame.GpState.left_js_y_value, 32767, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv);
                        break;
                    }
                    case GAMEPAD::XBOX360::axis::RX: {
                        if (frame.GpState.right_js_x_value < 0) {
                            cv = -Proportion(frame.GpState.right_js_x_value, 32767, HEMISPHERE_MIN_CV);
                        } else {
                            cv = Proportion(frame.GpState.right_js_x_value, 32767, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv);
                        break;
                    }
                    case GAMEPAD::XBOX360::axis::RY: {
                        if (frame.GpState.right_js_y_value < 0) {
                            cv = -Proportion(frame.GpState.right_js_y_value, 32767, HEMISPHERE_MIN_CV);
                        } else {
                            cv = Proportion(frame.GpState.right_js_y_value, 32767, HEMISPHERE_MAX_CV);
                        }
                        Out(ch, cv);
                        break;
                    }
                    default: break;
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
                { param[0], 0, GAMEPAD::XBOX360::MAX_AXIS }, // PARAM1
                { param[1], 0, GAMEPAD::XBOX360::MAX_AXIS }, // PARAM2
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
            param[0] = constrain(Unpack(data, PackLocation {0,8}), 0, GAMEPAD::XBOX360::MAX_AXIS);
            param[1] = constrain(Unpack(data, PackLocation {8,8}), 0, GAMEPAD::XBOX360::MAX_AXIS);
        }

    protected:
        void SetHelp() {
          // TODO
        }

    private:
        int cursor;

        uint8_t param[2];

        void DrawInterface() {

            int y = 14;
            gfxPrint(1, y, "p1: ");
            gfxPrint(GAMEPAD::XBOX360::axis_name[param[0]]);

            y += 14;
            gfxPrint(1, y, "p2: ");
            gfxPrint(GAMEPAD::XBOX360::axis_name[param[1]]);

            gfxCursor(12, 23 + cursor * 14, 37);
        }

    };
