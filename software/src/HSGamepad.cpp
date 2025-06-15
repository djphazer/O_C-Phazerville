#include "HSGamepad.h"
#include "HemisphereApplet.h"

// #define GAMEPAD_DEBUG

// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

static constexpr int axis_change_threshold = (-HEMISPHERE_MIN_CV) / 8;

void ProcessGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    f.GamepadState.gamepad_type = device.joystickType();

    if (f.GamepadState.gamepad_type == JoystickController::PS3 && !f.GamepadState.ps3_paired)
        f.GamepadState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {
        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

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
                // case JoystickController::PS4: {
                //     //   printAngles();
                //     ltv = device.getAxis(3);
                //     rtv = device.getAxis(4);
                //     if ((ltv != frame.GamepadState.axis[0]) || (rtv != frame.GamepadState.axis[1])) {
                //         frame.GamepadState.axis[0] = ltv;
                //         frame.GamepadState.axis[1] = rtv;
                //         device.setRumble(ltv, rtv);
                //     }
                //     break;
                // }
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
                // static const uint8_t xbox_axis_order_mapping[] = {3, 4, 0, 1, 2, 5}; // fix this in joystick.cpp:1205, or find out why it exists
                // "LT"<-LY, "RT"<-RX, "LX"<-LT, "LY"<-RT, "RX"<-LX, "RY" should be fine lol
                    ltv = device.getAxis(6);
                    if (ltv != f.GamepadState.axis[0]) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.last_changed = 16+0;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ltv: %d \n", ltv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }
                    rtv = device.getAxis(8);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = 16+1;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rtv: %d \n", rtv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }

                    // ljs_x = ((int16_t)device.getAxis(7) << 8) | device.getAxis(6);
                    ljs_x = device.getAxis(10);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+2;
                        f.GamepadState.axis[2] = ljs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_x: %d \n", ljs_x);
                        #endif
                    }
                    // ljs_y = ((int16_t)device.getAxis(9) << 8) | device.getAxis(8);
                    ljs_y = device.getAxis(12);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+3;
                        f.GamepadState.axis[3] = ljs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_y: %d \n", ljs_y);
                        #endif
                    }

                    // rjs_x = ((int16_t)device.getAxis(11) << 8) | device.getAxis(10);
                    rjs_x = device.getAxis(14);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+4;
                        f.GamepadState.axis[4] = rjs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_x: %d \n", rjs_x);
                        #endif
                    }
                    // rjs_y = ((int16_t)device.getAxis(13) << 8) | device.getAxis(12);
                    rjs_y = device.getAxis(16);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+5;
                        f.GamepadState.axis[5] = rjs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_y: %d \n", rjs_y);
                        #endif
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
                        f.GamepadState.last_changed = 16+0;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ltv: %d \n", ltv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }
                    rtv = device.getAxis(5);
                    if (rtv != f.GamepadState.axis[1]) {
                        f.GamepadState.axis[1] = rtv;
                        f.GamepadState.last_changed = 16+1;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rtv: %d \n", rtv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }

                    // ljs_x = ((int16_t)device.getAxis(7) << 8) | device.getAxis(6);
                    ljs_x = device.getAxis(6);
                    if (f.GamepadState.axis[2] != ljs_x) {
                        if (abs(f.GamepadState.axis[2] - ljs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+2;
                        f.GamepadState.axis[2] = ljs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_x: %d \n", ljs_x);
                        #endif
                    }
                    // ljs_y = ((int16_t)device.getAxis(9) << 8) | device.getAxis(8);
                    ljs_y = device.getAxis(8);
                    if (f.GamepadState.axis[3] != ljs_y) {
                        if (abs(f.GamepadState.axis[3] - ljs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+3;
                        f.GamepadState.axis[3] = ljs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_y: %d \n", ljs_y);
                        #endif
                    }

                    // rjs_x = ((int16_t)device.getAxis(11) << 8) | device.getAxis(10);
                    rjs_x = device.getAxis(10);
                    if (f.GamepadState.axis[4] != rjs_x) {
                        if (abs(f.GamepadState.axis[4] - rjs_x) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+4;
                        f.GamepadState.axis[4] = rjs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_x: %d \n", rjs_x);
                        #endif
                    }
                    // rjs_y = ((int16_t)device.getAxis(13) << 8) | device.getAxis(12);
                    rjs_y = device.getAxis(12);
                    if (f.GamepadState.axis[5] != rjs_y) {
                        if (abs(f.GamepadState.axis[5] - rjs_y) > axis_change_threshold)
                            f.GamepadState.last_changed = 16+5;
                        f.GamepadState.axis[5] = rjs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_y: %d \n", rjs_y);
                        #endif
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
                if (buttons_changed & 1) f.GamepadState.last_changed = min(i,15);
            }
            f.GamepadState.button_mask = buttons;
            #ifdef GAMEPAD_DEBUG
                if (f.GamepadState.gamepad_type == JoystickController::XBOX360USB) {
                    using GP = GAMEPAD::XBOX360USB;
                    if (buttons & (1 << GP::LB))        Serial.println(GP::button_name[GP::LB]);
                    if (buttons & (1 << GP::RB))        Serial.println(GP::button_name[GP::RB]);
                    if (buttons & (1 << GP::GUIDE))     Serial.println(GP::button_name[GP::GUIDE]);
                    if (buttons & (1 << GP::A))         Serial.println(GP::button_name[GP::A]);
                    if (buttons & (1 << GP::B))         Serial.println(GP::button_name[GP::B]);
                    if (buttons & (1 << GP::X))         Serial.println(GP::button_name[GP::X]);
                    if (buttons & (1 << GP::Y))         Serial.println(GP::button_name[GP::Y]);
                    if (buttons & (1 << GP::D_UP))      Serial.println(GP::button_name[GP::D_UP]);
                    if (buttons & (1 << GP::D_DOWN))    Serial.println(GP::button_name[GP::D_DOWN]);
                    if (buttons & (1 << GP::D_LEFT))    Serial.println(GP::button_name[GP::D_LEFT]);
                    if (buttons & (1 << GP::D_RIGHT))   Serial.println(GP::button_name[GP::D_RIGHT]);
                    if (buttons & (1 << GP::START))     Serial.println(GP::button_name[GP::START]);
                    if (buttons & (1 << GP::BACK))      Serial.println(GP::button_name[GP::BACK]);
                    if (buttons & (1 << GP::L3))        Serial.println(GP::button_name[GP::L3]);
                    if (buttons & (1 << GP::R3))        Serial.println(GP::button_name[GP::R3]);
                }
            #endif
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


// const char* GAMEPAD::UNKNOWN::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::UNKNOWN::axis_name[AXIS_LAST+1] = {};

// const char* GAMEPAD::PS3::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::PS3::axis_name[AXIS_LAST+1] = {};

// const char* GAMEPAD::PS4::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::PS4::axis_name[AXIS_LAST+1] = {};

// const char* GAMEPAD::XBOXONE::button_name[BUTTON_LAST+1] = {
//     "RSVD", "?", "MENU", "VIEW",
//     "A", "B", "X", "Y",
//     "D_U", "D_D", "D_L", "D_R",
//     "LB", "RB", "L3", "R3"
// };
// const char* GAMEPAD::XBOXONE::axis_name[AXIS_LAST+1] = {
//     "LT", "RT", "LX", "LY", "RX", "RY"
// };

// const char* GAMEPAD::XBOX360USB::button_name[BUTTON_LAST+1] = {
//     "D_U", "D_D", "D_L", "D_R",
//     ">", "<", "L3", "R3",
//     "LB", "RB", "(X)", "?",
//     "A", "B", "X", "Y"
// };
// const char* GAMEPAD::XBOX360USB::axis_name[AXIS_LAST+1] = {
//     "LT", "RT", "LX", "LY", "RX", "RY"
// };

// const char* GAMEPAD::PS3_MOTION::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::PS3_MOTION::axis_name[AXIS_LAST+1] = {};

// const char* GAMEPAD::SpaceNav::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::SpaceNav::axis_name[AXIS_LAST+1] = {};

// const char* GAMEPAD::SWITCH::button_name[BUTTON_LAST+1] = {};
// const char* GAMEPAD::SWITCH::axis_name[AXIS_LAST+1] = {};
