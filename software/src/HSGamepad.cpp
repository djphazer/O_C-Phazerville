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
    - handle gaps in button map, to avoid "..."
    - dynamic map of HID controllers from Report Descriptor
    - use loops over the button and axis ranges, but still need to handle scaling
    - dynamic gamepad.end() to free up resources for usbHostMIDI in Main. user selectable mode?
    - parseHatSwitch() function to reduce code duplication
*/

#include "HSGamepad.h"
#include "HemisphereApplet.h"

#ifdef __IMXRT1062__
#define GAMEPAD_DEBUG

GamePad UNKNOWN {
    .type_name = "UNKNOWN",
    .button_name = (const char*[]){
        "B1",  "B2",  "B3",  "B4",
        "B5",  "B6",  "B7",  "B8",
        "B9",  "B10", "B11", "B12",
        "B13", "B14", "B15", "B16",
        "B17", "B18", "B19", "B20",
        "B21", "B22", "B23", "B24",
        "B25", "B26", "B27", "B28",
        "B29", "B30", "B31", "B32"
    },
    .button_count = 32,
    .axis_name = (const char*[]){
        "X1",  "X2",  "X3",  "X4",
        "X5",  "X6",  "X7",  "X8",
        "X9",  "X10", "X11", "X12",
        "X13", "X14", "X15", "X16"
    },
    .axis_count = 16
};

GamePad PS3 { // WIP
    .type_name = "PS3",
    .button_name = (const char*[]){
        "B1",  "B2",  "B3",  "B4",
        "B5",  "B6",  "B7",  "B8",
        "B9",  "B10", "B11", "B12",
        "B13", "B14", "B15", "B16"
    },
    .button_count = 16,
    .axis_name = (const char*[]){},
    .axis_count = 0
};

GamePad PS4 {
    .type_name = "PS4",
    .button_name = (const char*[]){
        "SQR",  "CRS",  "CIR",  "TRI",
        "L1",   "R1",   "L2",   "R2",
        "SHR",  "OPT",  "L3",   "R3",
        "PS",   "TPAD", "D_U",  "D_R",
        "D_D",  "D_L"
    },
    .button_count = 18,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY",
        "AcclX", "AcclY", "AcclZ",
        "GyroX", "GyroY", "GyroZ",
        // "PadX", "PadY"
    },
    .axis_count = 12
};

GamePad XBOX {  // WIP
    .type_name = "XBOX",
    .button_name = (const char*[]){
        "D_U", "D_D", "D_L", "D_R",
        ">",   "<",   "L3",  "R3"
    },
    .button_count = 8,
    .axis_name = (const char*[]){
        "A",   "B",   "X",   "Y"
        "BLK",  "WHT",  "LT", "RT",
        "LX", "LY", "RX", "RY"
    },
    .axis_count = 12
};

GamePad XBOX360 {
    .type_name = "XBOX360",
    .button_name = (const char*[]){
        "D_U", "D_D", "D_L", "D_R",
        "STRT",   "BACK",   "L3",  "R3",
        "LB",  "RB",  "(X)", "?",
        "A",   "B",   "X",   "Y"
    },
    .button_count = 16,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY"
    },
    .axis_count = 6
};

GamePad XBOXONE {
    .type_name = "XBOXONE",
    .button_name = (const char*[]){
        "RSVD", "?",   "MENU", "VIEW",
        "A",    "B",   "X",    "Y",
        "D_U",  "D_D", "D_L",  "D_R",
        "LB",   "RB",  "L3",   "R3"
    },
    .button_count = 16,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY"
    },
    .axis_count = 6
};

GamePad SNES {
    .type_name = "SNES",
    .button_name = (const char*[]){
        "X",  "A",  "B",  "Y",
        "L",  "R",  "...",  "...",
        "SEL",  "STRT"
    },
    .button_count = 10,
    .axis_name = (const char*[]){
        "D_X", "D_Y"
    },
    .axis_count = 2
};

GamePad N64 {
    .type_name = "N64",
    .button_name = (const char*[]){
        "C_L",  "B",  "A",  "C_D",
        "L",  "R",  "Z_L",  "Z_R",
        "C_U",  "C_R", "...", "...",
        "STRT", "D_U",  "D_R", "D_D",
        "D_L"
    },
    .button_count = 17,
    .axis_name = (const char*[]){
        "J_X", "J_Y"
    },
    .axis_count = 2
};


// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x01, 0x01, 0x01, 0x3c, 0x2b , 0x1a}; // {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

static int data;
static int scaled_axis[GAMEPAD_AXIS_MAX];

static const int axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;

static bool connected = false;


void ProcessGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    f.GamepadState.gamepad_type = device.joystickType();

    // if (f.GamepadState.gamepad_type == JoystickController::PS3 && !f.GamepadState.ps3_paired)
    //     f.GamepadState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {
        if (!connected) {
            connected = true;
#ifdef GAMEPAD_DEBUG
            Serial.printf("VID: 0x%x\n", gamepad.idVendor());
            Serial.printf("PID: 0x%x\n", gamepad.idProduct());
#endif
        }

        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

        if (axis_changed_mask) {
#ifdef GAMEPAD_DEBUG
            Serial.printf("axis mask: %x\n", axis_changed_mask);
#endif
            switch (f.GamepadState.gamepad_type) {
                // case JoystickController::PS3: {
                //     scaled_axis[0] = device.getAxis(18);
                //     scaled_axis[1] = device.getAxis(19);
                //     if ((scaled_axis[0] != f.GamepadState.axis[0]) || (scaled_axis[1] != f.GamepadState.axis[1])) {
                //         f.GamepadState.axis[0] = scaled_axis[0];
                //         f.GamepadState.axis[1] = scaled_axis[1];
                //         device.setRumble(scaled_axis[0], scaled_axis[1], 50);
                //     }
                //     break;
                // }

                case JoystickController::PS4: {
                /* triggers */
                    // left trigger
                    if (axis_changed_mask & (1 << 3)) {
                        data = device.getAxis(3);  // 8-bit data range
                        scaled_axis[0] = Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[0] != f.GamepadState.axis[0]) {
                            f.GamepadState.axis[0] = scaled_axis[0];
                            f.GamepadState.last_changed = PS4.button_count + 0;
                        }
                    }
                    // right trigger
                    if (axis_changed_mask & (1 << 4)) {
                        data = device.getAxis(4);
                        scaled_axis[1] = Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[1] != f.GamepadState.axis[1]) {
                            f.GamepadState.axis[1] = scaled_axis[1];
                            f.GamepadState.last_changed = PS4.button_count + 1;
                        }
                    }

                /* axes */
                    // left joystick x-axis
                    if (axis_changed_mask & (1 << 0)) {
                        data = device.getAxis(0) - 128;  // 8-bit data range, center is 128
                        scaled_axis[2] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[2] != scaled_axis[2]) {
                            if (abs(f.GamepadState.axis[2] - scaled_axis[2]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 2;
                            f.GamepadState.axis[2] = scaled_axis[2];
                        }
                    }
                    // left joystick y-axis
                    if (axis_changed_mask & (1 << 1)) {
                        data = 255 - device.getAxis(1) - 128;  // y-axis is inverted
                        scaled_axis[3] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[3] != scaled_axis[3]) {
                            if (abs(f.GamepadState.axis[3] - scaled_axis[3]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 3;
                            f.GamepadState.axis[3] = scaled_axis[3];
                        }
                    }
                    // right joystick x-axis
                    if (axis_changed_mask & (1 << 2)) {
                        data = device.getAxis(2) - 128;
                        scaled_axis[4] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[4] != scaled_axis[4]) {
                            if (abs(f.GamepadState.axis[4] - scaled_axis[4]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 4;
                            f.GamepadState.axis[4] = scaled_axis[4];
                        }
                    }
                    // right joystick y-axis
                    if (axis_changed_mask & (1 << 5)) {
                        data = 255 - device.getAxis(5) - 128;  // y-axis is inverted
                        scaled_axis[5] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[5] != scaled_axis[5]) {
                            if (abs(f.GamepadState.axis[5] - scaled_axis[5]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 5;
                            f.GamepadState.axis[5] = scaled_axis[5];
                        }
                    }

                /* d-pad */
                    enum DPadShift {
                        UP_SHIFT = 14,
                        RIGHT_SHIFT = 15,
                        DOWN_SHIFT = 16,
                        LEFT_SHIFT = 17
                    };
                    enum HatSwitch {
                        UP = 0,
                        UP_RIGHT,
                        RIGHT,
                        RIGHT_DOWN,
                        DOWN,
                        DOWN_LEFT,
                        LEFT,
                        LEFT_UP,
                        OFF = 8
                    };
                    data = device.getAxis(9);
                    int dpad_state = 0;
                    for (int d = UP_SHIFT; d <= LEFT_SHIFT; ++d) {
                        switch (d) {
                            case UP_SHIFT:
                                    dpad_state = (data == HatSwitch::LEFT_UP)
                                            || (data == HatSwitch::UP)
                                            || (data == HatSwitch::UP_RIGHT);
                                    break;
                            case RIGHT_SHIFT:
                                    dpad_state = (data == HatSwitch::UP_RIGHT)
                                            || (data == HatSwitch::RIGHT)
                                            || (data == HatSwitch::RIGHT_DOWN);
                                    break;
                            case DOWN_SHIFT:
                                    dpad_state = (data == HatSwitch::RIGHT_DOWN)
                                            || (data == HatSwitch::DOWN)
                                            || (data == HatSwitch::DOWN_LEFT);
                                    break;
                            case LEFT_SHIFT:
                                    dpad_state = (data == HatSwitch::DOWN_LEFT)
                                            || (data == HatSwitch::LEFT)
                                            || (data == HatSwitch::LEFT_UP);
                                    break;
                        }
                        buttons = (buttons & ~(1 << d)) | ((dpad_state & 1) << d);
                    }

                /* motion */
                    for (int i = 6, j = 0; j < 12; ++i, j+=2) {
                        if (axis_changed_mask & (3 << (13+j))) { // 16-bit, check if either byte has changed
                            int motion_sensor_scale = (i > 8) ? 32767 : 8192;  // axes 6,7,8 are accel; axes 9,10,11 are gyro
                            data = (int16_t)(device.getAxis(13+j+1) << 8) | device.getAxis(13+j);
                            scaled_axis[i] = constrain(
                                Proportion(data,  (data < 0) ? -(motion_sensor_scale+1) : motion_sensor_scale,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV),
                                HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
                            if (f.GamepadState.axis[i] != scaled_axis[i]) {
                                if (abs(f.GamepadState.axis[i] - scaled_axis[i]) > axis_change_threshold)
                                    f.GamepadState.last_changed = PS4.button_count + i;
                                f.GamepadState.axis[i] = scaled_axis[i];
                            }
                        }
                    }

                /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        // int l, r;
                        // device.setRumble(l, r);
                    }
                    if (f.GamepadState.set_leds) {
                        f.GamepadState.set_leds = false;
                        // int r, g, b;
                        // device.setLEDs(r, g, b);
                    }

                /* extra stuff */
                    // printAngles();
                    break;
                }

                // case JoystickController::XBOX: {
                //     break;
                // }

                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                /* triggers */
                    // left trigger
                    if (axis_changed_mask & (1 << 4)) {
                        data = device.getAxis(4);  // 8-bit data range
                        scaled_axis[0] = Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[0] != f.GamepadState.axis[0]) {
                            f.GamepadState.axis[0] = scaled_axis[0];
                            f.GamepadState.last_changed = XBOX360.button_count + 0;
                        }
                    }
                    // right trigger
                    if (axis_changed_mask & (1 << 5)) {
                        data = device.getAxis(5);
                        scaled_axis[1] = Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[1] != f.GamepadState.axis[1]) {
                            f.GamepadState.axis[1] = scaled_axis[1];
                            f.GamepadState.last_changed = XBOX360.button_count + 1;
                        }
                    }

                /* axes */
                    // left joystick x-axis
                    if (axis_changed_mask & (1 << 6)) {
                        data = device.getAxis(6); // signed 16-bit, little endian
                        scaled_axis[2] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[2] != scaled_axis[2]) {
                            if (abs(f.GamepadState.axis[2] - scaled_axis[2]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOX360.button_count + 2;
                            f.GamepadState.axis[2] = scaled_axis[2];
                        }
                    }
                    // left joystick y-axis
                    if (axis_changed_mask & (1 << 8)) {
                        data = device.getAxis(8);
                        scaled_axis[3] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[3] != scaled_axis[3]) {
                            if (abs(f.GamepadState.axis[3] - scaled_axis[3]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOX360.button_count + 3;
                            f.GamepadState.axis[3] = scaled_axis[3];
                        }
                    }
                    // right joystick x-axis
                    if (axis_changed_mask & (1 << 10)) {
                        data = device.getAxis(10);
                        scaled_axis[4] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[4] != scaled_axis[4]) {
                            if (abs(f.GamepadState.axis[4] - scaled_axis[4]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOX360.button_count + 4;
                            f.GamepadState.axis[4] = scaled_axis[4];
                        }
                    }
                    // right joystick y-axis
                    if (axis_changed_mask & (1 << 12)) {
                        data = device.getAxis(12);
                        scaled_axis[5] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[5] != scaled_axis[5]) {
                            if (abs(f.GamepadState.axis[5] - scaled_axis[5]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOX360.button_count + 5;
                            f.GamepadState.axis[5] = scaled_axis[5];
                        }
                    }

                /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        // int l, r;
                        // device.setRumble(l, r);
                    }
                    if (f.GamepadState.set_leds) {
                        f.GamepadState.set_leds = false;
                        // int r, g, b;
                        // device.setLEDs(r, g, b);
                    }
                    break;
                }

                case JoystickController::XBOXONE: {
                /* triggers */
                    // left trigger
                    if (axis_changed_mask & (1 << 3)) {
                        data = device.getAxis(3); // 10-bit data range
                        scaled_axis[0] = Proportion(data,  (data < 0) ? 0 : 1023,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[0] != f.GamepadState.axis[0]) {
                            f.GamepadState.axis[0] = scaled_axis[0];
                            f.GamepadState.last_changed = XBOXONE.button_count + 0;
                        }
                    }
                    // right trigger
                    if (axis_changed_mask & (1 << 4)) {
                        data = device.getAxis(4);
                        scaled_axis[1] = Proportion(data,  (data < 0) ? 0 : 1023,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[1] != f.GamepadState.axis[1]) {
                            f.GamepadState.axis[1] = scaled_axis[1];
                            f.GamepadState.last_changed = XBOXONE.button_count + 1;
                        }
                    }

                /* axes */
                    // left joystick x-axis
                    if (axis_changed_mask & (1 << 0)) {
                        data = device.getAxis(0);  // signed 16-bit, little endian
                        scaled_axis[2] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[2] != scaled_axis[2]) {
                            if (abs(f.GamepadState.axis[2] - scaled_axis[2]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOXONE.button_count + 2;
                            f.GamepadState.axis[2] = scaled_axis[2];
                        }
                    }
                    // left joystick y-axis
                    if (axis_changed_mask & (1 << 1)) {
                        data = device.getAxis(1);
                        scaled_axis[3] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[3] != scaled_axis[3]) {
                            if (abs(f.GamepadState.axis[3] - scaled_axis[3]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOXONE.button_count + 3;
                            f.GamepadState.axis[3] = scaled_axis[3];
                        }
                    }
                    // right joystick x-axis
                    if (axis_changed_mask & (1 << 2)) {
                        data = device.getAxis(2);
                        scaled_axis[4] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[4] != scaled_axis[4]) {
                            if (abs(f.GamepadState.axis[4] - scaled_axis[4]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOXONE.button_count + 4;
                            f.GamepadState.axis[4] = scaled_axis[4];
                        }
                    }
                    // right joystick y-axis
                    if (axis_changed_mask & (1 << 5)) {
                        data = device.getAxis(5);
                        scaled_axis[5] = Proportion(data,  (data < 0) ? -32768 : 32767,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[5] != scaled_axis[5]) {
                            if (abs(f.GamepadState.axis[5] - scaled_axis[5]) > axis_change_threshold)
                                f.GamepadState.last_changed = XBOXONE.button_count + 5;
                            f.GamepadState.axis[5] = scaled_axis[5];
                        }
                    }

                /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        // int l, r;
                        // device.setRumble(l, r);
                    }
                    if (f.GamepadState.set_leds) {
                        f.GamepadState.set_leds = false;
                        // int r, g, b;
                        // device.setLEDs(r, g, b);
                    }
                    break;
                }

                case JoystickController::SNES: {
                /* axes */
                    // d-pad x-axis
                    if (axis_changed_mask & (1 << 0)) {  // scaled_axis[2]?
                        data = device.getAxis(0) - 128; // 8-bit data range, center is 128
                        scaled_axis[0] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[0] != scaled_axis[0]) {
                            if (abs(f.GamepadState.axis[0] - scaled_axis[0]) > axis_change_threshold)
                                f.GamepadState.last_changed = SNES.button_count + 0;
                            f.GamepadState.axis[0] = scaled_axis[0];
                        }
                    }
                    // d-pad y-axis
                    if (axis_changed_mask & (1 << 1)) {  // scaled_axis[3]?
                        data = 255 - device.getAxis(1) - 128;  // y-axis is inverted
                        scaled_axis[1] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[1] != scaled_axis[1]) {
                            if (abs(f.GamepadState.axis[1] - scaled_axis[1]) > axis_change_threshold)
                                f.GamepadState.last_changed = SNES.button_count + 1;
                            f.GamepadState.axis[1] = scaled_axis[1];
                        }
                    }
                    break;
                }

                case JoystickController::N64: {
                /* axes */
                    // joystick x-axis
                    if (axis_changed_mask & (1 << 0)) {
                        data = device.getAxis(0) - 128;  // 8-bit data range, center is 128
                        scaled_axis[0] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[0] != scaled_axis[0]) {
                            if (abs(f.GamepadState.axis[0] - scaled_axis[0]) > 0)
                                f.GamepadState.last_changed = N64.button_count + 0;
                            f.GamepadState.axis[0] = scaled_axis[0];
                        }
                    }
                    // joystick y-axis
                    if (axis_changed_mask & (1 << 1)) {
                        data = 255 - device.getAxis(1) - 128; // y-axis is inverted
                        scaled_axis[1] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[1] != scaled_axis[1]) {
                            if (abs(f.GamepadState.axis[1] - scaled_axis[1]) > 0)
                                f.GamepadState.last_changed = N64.button_count + 1;
                            f.GamepadState.axis[1] = scaled_axis[1];
                        }
                    }

                /* d-pad */
                    enum DPadShift {
                        UP_SHIFT = 13,
                        RIGHT_SHIFT = 14,
                        DOWN_SHIFT = 15,
                        LEFT_SHIFT = 16
                    };
                    enum HatSwitch {
                        UP = 0,
                        UP_RIGHT,
                        RIGHT,
                        RIGHT_DOWN,
                        DOWN,
                        DOWN_LEFT,
                        LEFT,
                        LEFT_UP,
                        OFF = 15
                    };
                    data = device.getAxis(9);
                    int dpad_state = 0;
                    for (int d = UP_SHIFT; d <= LEFT_SHIFT; ++d) {
                        switch (d) {
                            case UP_SHIFT:
                                    dpad_state = (data == HatSwitch::LEFT_UP)
                                            || (data == HatSwitch::UP)
                                            || (data == HatSwitch::UP_RIGHT);
                                    break;
                            case RIGHT_SHIFT:
                                    dpad_state = (data == HatSwitch::UP_RIGHT)
                                            || (data == HatSwitch::RIGHT)
                                            || (data == HatSwitch::RIGHT_DOWN);
                                    break;
                            case DOWN_SHIFT:
                                    dpad_state = (data == HatSwitch::RIGHT_DOWN)
                                            || (data == HatSwitch::DOWN)
                                            || (data == HatSwitch::DOWN_LEFT);
                                    break;
                            case LEFT_SHIFT:
                                    dpad_state = (data == HatSwitch::DOWN_LEFT)
                                            || (data == HatSwitch::LEFT)
                                            || (data == HatSwitch::LEFT_UP);
                                    break;
                        }
                        buttons = (buttons & ~(1 << d)) | ((dpad_state & 1) << d);
                    }
                    break;
                }

                case JoystickController::UNKNOWN:
                default: {
                    for (int i = 0; i < GAMEPAD_AXIS_MAX; ++i)
                    {
                        if (axis_changed_mask & (1 << i)) {
                            data = device.getAxis(i);
                            scaled_axis[i] = data;  // Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                            if (scaled_axis[i] != f.GamepadState.axis[i]) {
                                f.GamepadState.axis[i] = scaled_axis[i];
                                f.GamepadState.last_changed = UNKNOWN.button_count + i;
                            }
                        }
                    }
                    break;
                }
            }
        }

        if (f.GamepadState.button_mask != buttons) {
#ifdef GAMEPAD_DEBUG
            Serial.printf("buttons: %x\n", buttons);
#endif
            uint32_t buttons_changed = (~f.GamepadState.button_mask) & buttons;
            for (uint8_t i = 0; buttons_changed != 0; i++, buttons_changed >>= 1) {
                if (buttons_changed & 1) f.GamepadState.last_changed = i;
            }
            f.GamepadState.button_mask = buttons;
        }

        device.joystickDataClear();
    }
}

#endif
