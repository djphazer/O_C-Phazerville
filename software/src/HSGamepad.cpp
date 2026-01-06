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
    - parseHatSwitch() function to reduce code duplication
*/

#ifdef USB_GAMEPAD

#include "HSGamepad.h"
#include "HemisphereApplet.h"

// #define GAMEPAD_DEBUG

extern USBHost thisUSB;

void printBits(uint32_t value) {
  for (int i = 31; i >= 0; i--) {
    // Shift right by i and mask out the lowest bit
    uint8_t bit = (value >> i) & 1;
    Serial.print(bit);

    // add a space every 8 bits for readability
    if (i % 8 == 0 && i != 0) {
      Serial.print(" ");
    }
  }
  Serial.println();
}


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

#ifdef ENABLE_PS3
GamePad PS3 { // WIP
    .type_name = "PS3",
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
#endif

#ifdef ENABLE_PS3_MOTION
GamePad PS3_MOTION { // WIP
    .type_name = "PS Move",
    .button_name = (const char*[]){
        "SEL", "STRT", "PS",  "TRI",
        "CIR", "CRS",  "SQR", "MOV",
        "TRIG"
    },
    .button_count = 9,
    .axis_name = (const char*[]){
        "X1",  "X2",  "X3",  "X4",
        "X5",  "X6",  "X7",  "X8",
        "X9",  "X10", "X11", "X12",
        "X13", "X14", "X15", "X16"
    },
    .axis_count = 16
};
#endif

#ifdef ENABLE_PS4
GamePad PS4 {
    .type_name = "PS4",
    .button_name = (const char*[]){
        "SQR", "CRS",  "CIR", "TRI",
        "L1",  "R1",   "L2",  "R2",
        "SHR", "OPT",  "L3",  "R3",
        "PS",  "TPAD", "D_U", "D_R",
        "D_D", "D_L"
    },
    .button_count = 18,  // dpad buttons
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY",
        "AcclX", "AcclY", "AcclZ",
        "GyroX", "GyroY", "GyroZ",
        // "T1_X", "T1_Y", "T2_X", "T2_Y"
    },
    .axis_count = 12,
    .axis_byte_map = (const int[]){  // 2 triggers, 4 joystick axes
        3,4,
        0,1,
        2,5
    },
    .axis_scaling = (const int[]){  // 8-bit unsigned triggers, 8-bit "signed" joysticks
        8,8,
        7,7,
        7,7
    },
    .axis_symmetry = (const int[]){
        0,0,
        1,1,
        1,1
    },
    .axis_inversion = (const int[]){
        0,0,
        0,1,
        0,1
    },
    .axis_center = 128,
    .dpad_byte = 9,
    .dpad_shift_map = (const int[]){ 14, 15, 16, 17 }  // up, right, down, left
};
#endif

#ifdef ENABLE_XBOX
GamePad XBOX {  // WIP
    .type_name = "XBOX",
    .button_name = (const char*[]){
        "D_U", "D_D", "D_L", "D_R",
        ">",   "<",   "L3",  "R3"
    },
    .button_count = 8,
    .axis_name = (const char*[]){
        "A",   "B",   "X",  "Y"
        "BLK", "WHT", "LT", "RT",
        "LX",  "LY",  "RX", "RY"
    },
    .axis_count = 12
};
#endif

#ifdef ENABLE_XBOX360
GamePad XBOX360 {
    .type_name = "XBOX360",
    .button_name = (const char*[]){
        "D_U",  "D_D",  "D_L", "D_R",
        "STRT", "BACK", "L3",  "R3",
        "LB",   "RB",   "(X)", "?",
        "A",    "B",    "X",   "Y"
    },
    .button_count = 16,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY"
    },
    .axis_count = 6,
    .axis_byte_map = (const int[]){ // 2 triggers, 4 joystick axes
        4,5,
        6,8,
        10,12
    },
    .axis_scaling = (const int[]){  // 8-bit unsigned triggers, 16-bit signed joysticks
        8,8,
        15,15,
        15,15
    },
    .axis_symmetry = (const int[]){
        0,0,
        1,1,
        1,1
    },
    .axis_inversion = (const int[]){
        0,0,
        0,0,
        0,0
    },
    .axis_center = 0
};
#endif

#ifdef ENABLE_XBOXONE
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
    .axis_count = 6,
    .axis_byte_map = (const int[]){  // 2 triggers, 4 joystick axes
        3,4,
        0,1,
        2,5
    },
    .axis_scaling = (const int[]){  // 10-bit unsigned triggers, 16-bit signed joysticks
        10,10,
        15,15,
        15,15
    },
    .axis_symmetry = (const int[]){
        0,0,
        1,1,
        1,1
    },
    .axis_inversion = (const int[]){
        0,0,
        0,0,
        0,0
    },
    .axis_center = 0
};
#endif

// #ifdef ENABLE_SpaceNav
// #endif

// #ifdef ENABLE_SWITCH
// #endif

#ifdef ENABLE_SNES
GamePad SNES {
    .type_name = "SNES",
    .button_name = (const char*[]){
        "X",   "A",   "B",   "Y",
        "L",   "R",   "...", "...",
        "SEL", "STRT"
    },
    .button_count = 10,
    .axis_name = (const char*[]){
        "D_X", "D_Y"
    },
    .axis_count = 2,
    .axis_byte_map = (const int[]){ 0, 1 },  // D-Pad up/down are y/x axes
    .axis_scaling = (const int[]){ 7, 7 },  // 8-bit "signed"
    .axis_symmetry = (const int[]){ 1, 1 },  // -128 -> 127
    .axis_inversion = (const int[]){ 0, 1},  // y inverted
    .axis_center = 128
};
#endif

#ifdef ENABLE_N64
GamePad N64 {
    .type_name = "N64",
    .button_name = (const char*[]){
        "C_L",  "B",   "A",   "C_D",
        "L",    "R",   "Z_L", "Z_R",
        "C_U",  "C_R", "...", "...",
        "STRT", "D_U", "D_R", "D_D",
        "D_L"
    },
    .button_count = 17,
    .axis_name = (const char*[]){
        "J_X", "J_Y"
    },
    .axis_count = 2,
    .axis_byte_map = (const int[]){ 0, 1 },  // D-Pad up/down are y/x axes
    .axis_scaling = (const int[]){ 7, 7 },  // 8-bit "signed"
    .axis_symmetry = (const int[]){ 1, 1 },  // -128 -> 127
    .axis_inversion = (const int[]){ 0, 1},  // y inverted
    .axis_center = 128,
    .dpad_byte = 9,
    .dpad_shift_map = (const int[]){ 13, 14, 15, 16 }  // up, right, down, left
};
#endif

// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x01, 0x01, 0x01, 0x3c, 0x2b, 0x1a}; // {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

static int data; // delete
static int scaled_axis[16]; // delete

static const int axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;

// static bool connected = false;


void ConnectGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    switch (f.GamepadState.gamepad_type) {
#ifdef ENABLE_PS3_MOTION
        case (JoystickController::joytype_t::PS3_MOTION):
            f.GamepadState.gamepad = &PS3_MOTION;
            break;
#endif
#ifdef ENABLE_PS4
        case (JoystickController::joytype_t::PS4):
            f.GamepadState.gamepad = &PS4;
            break;
#endif
#ifdef ENABLE_XBOXONE
        case (JoystickController::joytype_t::XBOXONE):
            f.GamepadState.gamepad = &XBOXONE;
            break;
#endif
#ifdef ENABLE_XBOX360
        case (JoystickController::joytype_t::XBOX360W):
        case (JoystickController::joytype_t::XBOX360USB):
            f.GamepadState.gamepad = &XBOX360;
            break;
#endif
#ifdef ENABLE_SNES
        case (JoystickController::joytype_t::SNES):
            f.GamepadState.gamepad = &SNES;
            break;
#endif
#ifdef ENABLE_N64
        case (JoystickController::joytype_t::N64):
            f.GamepadState.gamepad = &N64;
            break;
#endif
        default:
            f.GamepadState.gamepad = &UNKNOWN;
            break;
    }

    f.GamepadState.Init();
}

void ConvertAxisData(int axis, int value) {
    HS::IOFrame &f = HS::frame;
    for(int ch = 0; ch < HS::GAMEPAD_MAP_MAX; ++ch) {
        HS::GamepadMapping &map = f.GamepadState.mapping[ch];
        if (map.function == GP_LEARN) {
            map.gamepad_input = f.GamepadState.gamepad->button_count + axis;
            map.function = GP_CV;
        }
        if (map.gamepad_input != f.GamepadState.gamepad->button_count + axis) continue;
        switch (map.function) {
            case GP_CV:
                map.output = value;
                break;
            default:
                continue;
                break;
        }
    }
}

void UpdateAxis(JoystickController &device, GamePad &gp_type, const int axis_index) {
    HS::IOFrame &f = HS::frame;

    int data = ((2 * gp_type.axis_center - 1) * gp_type.axis_inversion[axis_index])
                - gp_type.axis_center * gp_type.axis_symmetry[axis_index]
                + device.getAxis(gp_type.axis_byte_map[axis_index]) * (1 - 2 * gp_type.axis_inversion[axis_index]);

    int scaled_axis = Proportion(data,  (data < 0)
                                            ? (-(1 << gp_type.axis_scaling[axis_index]) * gp_type.axis_symmetry[axis_index])
                                            : (1 << gp_type.axis_scaling[axis_index]) - 1
                                    , (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);

    if (f.GamepadState.axis[axis_index] != scaled_axis) {
        if (abs(f.GamepadState.axis[axis_index] - scaled_axis) > axis_change_threshold) {
            f.GamepadState.last_changed = gp_type.button_count + axis_index;
        }
        ConvertAxisData(axis_index, scaled_axis);
        f.GamepadState.axis[axis_index] = scaled_axis;
    }
}

void UpdateDpad(JoystickController &device, GamePad &gp_type, uint32_t &buttons) {
    enum HatSwitch {
        UP = 0,
        UP_RIGHT,
        RIGHT,
        RIGHT_DOWN,
        DOWN,
        DOWN_LEFT,
        LEFT,
        LEFT_UP,
    };

    int data = device.getAxis(gp_type.dpad_byte);
    int dpad_state = 0;
    for (int d = 0; d < 4; ++d) {
        switch (d) {
            case 0:
                dpad_state = (data == HatSwitch::LEFT_UP)
                        || (data == HatSwitch::UP)
                        || (data == HatSwitch::UP_RIGHT);
                break;
            case 1:
                dpad_state = (data == HatSwitch::UP_RIGHT)
                        || (data == HatSwitch::RIGHT)
                        || (data == HatSwitch::RIGHT_DOWN);
                break;
            case 2:
                dpad_state = (data == HatSwitch::RIGHT_DOWN)
                        || (data == HatSwitch::DOWN)
                        || (data == HatSwitch::DOWN_LEFT);
                break;
            case 3:
                dpad_state = (data == HatSwitch::DOWN_LEFT)
                        || (data == HatSwitch::LEFT)
                        || (data == HatSwitch::LEFT_UP);
                break;
        }
        buttons = (buttons & ~(1 << gp_type.dpad_shift_map[d])) | ((dpad_state & 1) << gp_type.dpad_shift_map[d]);
    }
}

void ConvertButtonData(int button, int mask) {
    HS::IOFrame &f = HS::frame;
    for(int ch = 0; ch < HS::GAMEPAD_MAP_MAX; ++ch) {
        HS::GamepadMapping &map = f.GamepadState.mapping[ch];
        if (map.function == GP_LEARN) {
            map.gamepad_input = button;
        }
        if (map.gamepad_input != button) continue;
        switch (map.function) {
            case GP_GATE:
                map.GateOut((mask & (1 << button)) != 0);  // add GATE_INV case?
                break;
            case GP_TRIG:
                if ((mask & (1 << button)) != 0) map.ClockOut();  // add TRIG_ALWAYS case?
            default:
                continue;
                break;
        }
    }
}

void UpdateButtons(JoystickController &device, uint32_t &buttons) {
    HS::IOFrame &f = HS::frame;

    uint32_t buttons_changed = f.GamepadState.button_mask ^ buttons;
    for (uint8_t i = 0; buttons_changed != 0; i++, buttons_changed >>= 1) {
        if (buttons_changed & 1) {
            f.GamepadState.last_changed = i;
            ConvertButtonData(i, buttons);
        }
    }
    f.GamepadState.button_mask = buttons;
}

void ProcessGamepad(JoystickController &device) {
    thisUSB.Task();

    HS::IOFrame &f = HS::frame;

    f.GamepadState.gamepad_type = device.joystickType();
    f.GamepadState.vid = device.idVendor();
    f.GamepadState.pid = device.idProduct();

    if (f.GamepadState.prev_gamepad_type != f.GamepadState.gamepad_type) {
        ConnectGamepad(device);
        f.GamepadState.prev_gamepad_type = f.GamepadState.gamepad_type;
    }

    // if (f.GamepadState.gamepad_type == JoystickController::PS3 && !f.GamepadState.ps3_paired)
    //     f.GamepadState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {

        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

        if (axis_changed_mask) {
#ifdef GAMEPAD_DEBUG
            // Serial.printf("axis mask: %x\n", axis_changed_mask);
#endif
            switch (f.GamepadState.gamepad_type) {
#ifdef ENABLE_PS3
                case JoystickController::PS3: {
                    scaled_axis[0] = device.getAxis(18);
                    scaled_axis[1] = device.getAxis(19);
                    if ((scaled_axis[0] != f.GamepadState.axis[0]) || (scaled_axis[1] != f.GamepadState.axis[1])) {
                        f.GamepadState.axis[0] = scaled_axis[0];
                        f.GamepadState.axis[1] = scaled_axis[1];
                        device.setRumble(scaled_axis[0], scaled_axis[1], 50);
                    }
                    break;
                }
#endif
#ifdef ENABLE_PS3_MOTION
                case JoystickController::PS3_MOTION: {
                /* triggers */
                    // only trigger
                    if (axis_changed_mask & (1 << 3)) {
                        data = device.getAxis(3);  // 8-bit data range
                        scaled_axis[0] = Proportion(data,  (data < 0) ? 0 : 255,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (scaled_axis[0] != f.GamepadState.axis[0]) {
                            f.GamepadState.axis[0] = scaled_axis[0];
                            f.GamepadState.last_changed = PS4.button_count + 0;
                        }
                    }

                /* axes */
                    // sphere x-position
                    if (axis_changed_mask & (1 << 0)) {
                        data = device.getAxis(0) - 128;  // 8-bit data range, center is 128
                        scaled_axis[2] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[2] != scaled_axis[2]) {
                            if (abs(f.GamepadState.axis[2] - scaled_axis[2]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 2;
                            f.GamepadState.axis[2] = scaled_axis[2];
                        }
                    }
                    // sphere y-position
                    if (axis_changed_mask & (1 << 1)) {
                        data = device.getAxis(1) - 128;
                        scaled_axis[3] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[3] != scaled_axis[3]) {
                            if (abs(f.GamepadState.axis[3] - scaled_axis[3]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 3;
                            f.GamepadState.axis[3] = scaled_axis[3];
                        }
                    }
                    // sphere z-position
                    if (axis_changed_mask & (1 << 2)) {
                        data = device.getAxis(2) - 128;
                        scaled_axis[4] = Proportion(data,  (data < 0) ? -128 : 127,  (data < 0) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                        if (f.GamepadState.axis[4] != scaled_axis[4]) {
                            if (abs(f.GamepadState.axis[4] - scaled_axis[4]) > axis_change_threshold)
                                f.GamepadState.last_changed = PS4.button_count + 4;
                            f.GamepadState.axis[4] = scaled_axis[4];
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

                /* touchpad */
                    // ForEachChannel(fng) { // fingers 1 and 2
                    //     if (axis_changed_mask & (0xFF << (35+(4*fng)))) {
                    //         uint8_t touch = device.getAxis(35+(4*fng));
                    //         uint8_t x_lo = device.getAxis(36+(4*fng));
                    //         uint8_t xhi_ylo = device.getAxis(37+(4*fng));
                    //         uint8_t y_hi = device.getAxis(38+(4*fng));

                    //         bool finger_active = !(touch & 0x80);
                    //         uint16_t x = ((xhi_ylo & 0x0F) << 8) | x_lo;
                    //         uint16_t y = (y_hi << 4) | ((xhi_ylo & 0xF0) >> 4);

                    //         if (finger_active) {
                    //             int x_res = 1920;
                    //             int y_res = 942;
                    //             scaled_axis[12+2*fng] = Proportion(x, x_res, (x < x_res/2) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                    //             scaled_axis[12+1+2*fng] = Proportion(y, y_res, (y < y_res/2) ? HEMISPHERE_MIN_CV : HEMISPHERE_MAX_CV);
                    //         } else {
                    //             scaled_axis[12+2*fng] = 0;
                    //             scaled_axis[12+1+2*fng] = 0;
                    //         }

                    //         ForEachChannel(crd) { // x and y coords
                    //             if (f.GamepadState.axis[12+2*fng+crd] != scaled_axis[12+2*fng+crd]) {
                    //                 if (abs(f.GamepadState.axis[12+2*fng+crd] - scaled_axis[12+2*fng+crd]) > axis_change_threshold)
                    //                     f.GamepadState.last_changed = PS4.button_count + 12+2*fng+crd;
                    //                 f.GamepadState.axis[12+2*fng+crd] = scaled_axis[12+2*fng+crd];
                    //             }
                    //         }
                    //     }
                    // }

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
#endif
#ifdef ENABLE_PS4
                case JoystickController::PS4: {
                /* triggers and axes */
                    // for (int i = 0; i < PS4.axis_count; ++i) {
                    for (int i = 0; i < 6; ++i) {  // special case for PS4, motion controls are "axes"
                        if (axis_changed_mask & (1 << PS4.axis_byte_map[i])) {
                            UpdateAxis(device, PS4, i);
                        }
                    }
                /* d-pad */
                    if (axis_changed_mask & (1 << PS4.dpad_byte)) {
                        UpdateDpad(device, PS4, buttons);
                    }
                /* motion */  // still experimental
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
#endif
#ifdef ENABLE_XBOX
                case JoystickController::XBOX: {
                    break;
                }
#endif
#ifdef ENABLE_XBOX360
                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                /* triggers and axes */
                    for (int i = 0; i < XBOX360.axis_count; ++i) {
                        if (axis_changed_mask & (1 << XBOX360.axis_byte_map[i])) {
                            UpdateAxis(device, XBOX360, i);
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
#endif
#ifdef ENABLE_XBOXONE
                case JoystickController::XBOXONE: {
                /* triggers and axes */
                    for (int i = 0; i < XBOXONE.axis_count; ++i) {
                        if (axis_changed_mask & (1 << XBOXONE.axis_byte_map[i])) {
                            UpdateAxis(device, XBOXONE, i);
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
#endif
#ifdef ENABLE_SpaceNav
#endif
#ifdef ENABLE_SWITCH
#endif
#ifdef ENABLE_SNES
                case JoystickController::SNES: {
                /* "axes" */
                    for (int i = 0; i < SNES.axis_count; ++i) {
                        if (axis_changed_mask & (1 << SNES.axis_byte_map[i])) {
                            UpdateAxis(device, SNES, i);
                        }
                    }
                    break;
                }
#endif
#ifdef ENABLE_N64
                case JoystickController::N64: {
                /* axes */
                    for (int i = 0; i < N64.axis_count; ++i) {
                        if (axis_changed_mask & (1 << N64.axis_byte_map[i])) {
                            UpdateAxis(device, N64, i);
                        }
                    }
                /* d-pad */
                    if (axis_changed_mask & (1 << N64.dpad_byte)) {
                        UpdateDpad(device, N64, buttons);
                    }
                    break;
                }
#endif
                case JoystickController::UNKNOWN:
                default: {
                    for (int i = 0; i < 16; ++i)
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
            // Serial.printf("buttons: %x\n", buttons);
#endif
            UpdateButtons(device, buttons);
        }

        device.joystickDataClear();
    }
}

#endif
