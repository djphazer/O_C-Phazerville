#include "HSGamepad.h"
#include "HemisphereApplet.h"

#ifdef __IMXRT1062__
// #define GAMEPAD_DEBUG

// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x01, 0x01, 0x01, 0x3c, 0x2b , 0x1a}; // {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

static constexpr int axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;

void ProcessGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    f.GamepadState.gamepad_type = device.joystickType();

    // if (f.GamepadState.gamepad_type == JoystickController::PS3 && !f.GamepadState.ps3_paired)
    //     f.GamepadState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {
        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

        int axis_offset = 16;

        if (axis_changed_mask) {
            int16_t ltv; // left trigger value (axis[0])
            int16_t rtv; // right trigger value (axis[1])
            int16_t ljs_x; // left joy stick, x value (axis[2])
            int16_t ljs_y; // left joy stick, y value (axis[3])
            int16_t rjs_x; // right joy stick, x value (axis[4])
            int16_t rjs_y; // right joy stick, y value (axis[5])
            // int tilt_x; // future
            // int tily_y; // future

            switch (f.GamepadState.gamepad_type) {
                case JoystickController::PS4: {
                    //   printAngles();
                    ltv = device.getAxis(3);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = axis_offset + 0;
                    }
                    rtv = device.getAxis(4);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = axis_offset + 1;
                    }

                    ljs_x = device.getAxis(0) - 128;
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > 0)
                            f.GamepadState.last_changed = axis_offset + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = 255 - device.getAxis(1) - 128;
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > 0)
                            f.GamepadState.last_changed = axis_offset + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }

                    rjs_x = device.getAxis(2) - 128;
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > 0)
                            f.GamepadState.last_changed = axis_offset + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = 255 - device.getAxis(5) - 128;
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > 0)
                            f.GamepadState.last_changed = axis_offset + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }

                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                // case JoystickController::PS3: {
                //     ltv = device.getAxis(18);
                //     rtv = device.getAxis(19);
                //     if ((ltv != f.GamepadState.axis[0]) || (rtv != f.GamepadState.axis[1])) {
                //         f.GamepadState.axis[0] = ltv;
                //         f.GamepadState.axis[1] = rtv;
                //         device.setRumble(ltv, rtv, 50);
                //     }
                //     break;
                // }
                case JoystickController::XBOXONE: {
                    ltv = device.getAxis(3);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = axis_offset + 0;
                    }
                    rtv = device.getAxis(4);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = axis_offset + 1;
                    }

                    ljs_x = device.getAxis(0);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = device.getAxis(1);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }

                    rjs_x = device.getAxis(2);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = device.getAxis(5);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }

                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                    ltv = device.getAxis(4);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = axis_offset + 0;
                    }
                    rtv = device.getAxis(5);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = axis_offset + 1;
                    }

                    ljs_x = device.getAxis(6);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = device.getAxis(8);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }

                    rjs_x = device.getAxis(10);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = device.getAxis(12);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = axis_offset + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }

                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                default: break;
            }
        }

        if (f.GamepadState.button_mask != buttons) {
            uint32_t buttons_changed = (~f.GamepadState.button_mask) & buttons;
            for (uint8_t i = 0; buttons_changed != 0; i++, buttons_changed >>= 1) {
                if (buttons_changed & 1) f.GamepadState.last_changed = min(i, axis_offset-1);
            }
            f.GamepadState.button_mask = buttons;
        }

        device.joystickDataClear();
    }
}

GamePad UNKNOWN {
    .type_name = "UNKNOWN",
    .button_name = (const char*[]){
        "BTN1",  "BTN2",  "BTN3",  "BTN4",
        "BTN5",  "BTN6",  "BTN7",  "BTN8",
        "BTN9",  "BTN10", "BTN11", "BTN12",
        "BTN13", "BTN14", "BTN15", "BTN16"
    },
    .button_count = 16,
    .axis_name = (const char*[]){},
    .axis_count = 0,
    .trig_count = 0,
    .trig_min = 0,
    .trig_max = 0,
    .axis_min = 0,
    .axis_max = 0
};

GamePad PS4 {
    .type_name = "PS4",
    .button_name = (const char*[]){
        "Sqr",  "Crs",  "Cir",  "Tri",
        "L1",   "R1",   "L2",   "R2",
        "Shr",  "Opt",  "L3",   "R3",
        "PS",   "TPad"
    },
    .button_count = 14,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY"
    },
    .axis_count = 6,
    .trig_count = 2,
    .trig_min = 0,
    .trig_max = 255,
    .axis_min = -128,
    .axis_max = 127
};

GamePad XBOX {
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
    .axis_count = 12,
    .trig_count = 8,
    .trig_min = 0,
    .trig_max = 255,
    .axis_min = -32768,
    .axis_max = 32767
};

GamePad XBOX360 {
    .type_name = "XBOX360",
    .button_name = (const char*[]){
        "D_U", "D_D", "D_L", "D_R",
        ">",   "<",   "L3",  "R3",
        "LB",  "RB",  "(X)", "?",
        "A",   "B",   "X",   "Y"
    },
    .button_count = 16,
    .axis_name = (const char*[]){
        "LT", "RT",
        "LX", "LY",
        "RX", "RY"
    },
    .axis_count = 6,
    .trig_count = 2,
    .trig_min = 0,
    .trig_max = 255,
    .axis_min = -32768,
    .axis_max = 32767
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
    .axis_count = 6,
    .trig_count = 2,
    .trig_min = 0,
    .trig_max = 1023,
    .axis_min = -32768,
    .axis_max = 32767
};

#endif