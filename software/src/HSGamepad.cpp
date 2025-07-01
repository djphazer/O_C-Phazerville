#include "HSGamepad.h"
#include "HemisphereApplet.h"

#ifdef __IMXRT1062__
// #define GAMEPAD_DEBUG

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

GamePad PS3 { // WIP
    .type_name = "PS3",
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

GamePad PS4 {  // need to add D-Pad support
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

GamePad SNES {
    .type_name = "SNES",
    .button_name = (const char*[]){
        "X",  "A",  "B",  "Y",
        "L",  "R",  "-",  "-",
        "SEL",  "STRT"
    },
    .button_count = 10,
    .axis_name = (const char*[]){
        "D_X", "D_Y"
    },
    .axis_count = 2,
    .trig_count = 0,
    .trig_min = 0,
    .trig_max = 0,
    .axis_min = -128,
    .axis_max = 127
};

// GamePad N64 {

// };


// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x01, 0x01, 0x01, 0x3c, 0x2b , 0x1a}; // {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

static int axis_change_threshold;

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
            int16_t ltv; // left trigger value (axis[0])
            int16_t rtv; // right trigger value (axis[1])
            int16_t ljs_x; // left joy stick, x value (axis[2])
            int16_t ljs_y; // left joy stick, y value (axis[3])
            int16_t rjs_x; // right joy stick, x value (axis[4])
            int16_t rjs_y; // right joy stick, y value (axis[5])
            // int tilt_x; // future
            // int tily_y; // future

            switch (f.GamepadState.gamepad_type) {
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
                case JoystickController::PS4: {
                    axis_change_threshold = 256 / 8;
                    /* triggers */
                    ltv = device.getAxis(3);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = PS4.button_count + 0;
                    }
                    rtv = device.getAxis(4);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = PS4.button_count + 1;
                    }
                    /* axes */
                    ljs_x = device.getAxis(0) - 128;
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > 0)
                            f.GamepadState.last_changed = PS4.button_count + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = 255 - device.getAxis(1) - 128;
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > 32)
                            f.GamepadState.last_changed = PS4.button_count + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }
                    rjs_x = device.getAxis(2) - 128;
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > 0)
                            f.GamepadState.last_changed = PS4.button_count + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = 255 - device.getAxis(5) - 128;
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > 0)
                            f.GamepadState.last_changed = PS4.button_count + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }
                    /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    // printAngles();
                    break;
                }
                // case JoystickController::XBOX: {
                //     break;
                // }
                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                    axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;
                    /* triggers */
                    ltv = device.getAxis(4);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = XBOX360.button_count + 0;
                    }
                    rtv = device.getAxis(5);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = XBOX360.button_count + 1;
                    }
                    /* axes */
                    ljs_x = device.getAxis(6);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOX360.button_count + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = device.getAxis(8);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOX360.button_count + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }
                    rjs_x = device.getAxis(10);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOX360.button_count + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = device.getAxis(12);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOX360.button_count + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }
                    /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                case JoystickController::XBOXONE: {
                    axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;
                    /* triggers */
                    ltv = device.getAxis(3);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = XBOXONE.button_count + 0;
                    }
                    rtv = device.getAxis(4);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = XBOXONE.button_count + 1;
                    }
                    /* axes */
                    ljs_x = device.getAxis(0);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOXONE.button_count + 2;
                        f.GamepadState.axis[2] = ljs_x;
                    }
                    ljs_y = device.getAxis(1);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOXONE.button_count + 3;
                        f.GamepadState.axis[3] = ljs_y;
                    }
                    rjs_x = device.getAxis(2);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOXONE.button_count + 4;
                        f.GamepadState.axis[4] = rjs_x;
                    }
                    rjs_y = device.getAxis(5);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = XBOXONE.button_count + 5;
                        f.GamepadState.axis[5] = rjs_y;
                    }
                    /* feedback */
                    if (f.GamepadState.set_rumble) {
                        f.GamepadState.set_rumble = false;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                case JoystickController::SNES: {
                    axis_change_threshold = 256 / 8;
                    /* axes */
                    ljs_x = device.getAxis(0) - 128;
                    if (f.GamepadState.axis[0] != ljs_x) {
                        if (abs(f.GamepadState.axis[0] - ljs_x) > 0)
                            f.GamepadState.last_changed = SNES.button_count + 0;
                        f.GamepadState.axis[0] = ljs_x;
                    }
                    ljs_y = 255 - device.getAxis(1) - 128;
                    if (f.GamepadState.axis[1] != ljs_y) {
                        if (abs(f.GamepadState.axis[0] - ljs_y) > 0)
                            f.GamepadState.last_changed = SNES.button_count + 1;
                        f.GamepadState.axis[1] = ljs_y;
                    }
                }
                // case JoystickController::N64: {
                //     break:
                // }
                default: break;
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