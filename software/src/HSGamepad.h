#pragma once

#include <USBHost_t36.h>

#if defined(ARDUINO_TEENSY41)
    namespace GAMEPAD {
        const char* const controller_type[9] {
            "UNKNOWN",
            "PS3",
            "PS4",
            "XBOXONE",
            "XBOX360W",
            "XBOX360USB",
            "PS3_MOTION",
            "SpaceNav",
            "SWITCH"
        };

        namespace UNKNOWN { // gamepad_type = 0
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace UNKNOWN

        namespace PS3 { // gamepad_type = 1
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace PS3

        namespace PS4 { // gamepad_type = 2
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace PS4

        namespace XBOXONE { // gamepad_type = 3
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace XBOXONE

        namespace XBOX360USB { // gamepad_type = 5 (and 4?)
            enum button { // bytes 2 & 3
                LB = 0,     // 0x00 0x01
                RB,         // 0x00 0x02
                GUIDE,      // 0x00 0x04
                unused_8,   // 0x00 0x08

                A,          // 0x00 0x10
                B,          // 0x00 0x20
                X,          // 0x00 0x40
                Y,          // 0x00 0x80

                D_UP,       // 0x01 0x00
                D_DOWN,     // 0x02 0x00
                D_LEFT,     // 0x04 0x00
                D_RIGHT,    // 0x08 0x00

                START,      // 0x10 0x00
                BACK,       // 0x20 0x00
                L3,         // 0x40 0x00
                R3,         // 0x80 0x00

                MAX_BUTTON = R3
            };
            const char* const button_name[MAX_BUTTON+1] = {
                "LB", "RB", "(X)", "?",
                "A", "B", "X", "Y",
                "D_U", "D_D", "D_L", "D_R",
                ">", "<", "L3", "R3"
            };

            enum axis { // bytes 4-13
                LT = 0, // left trigger (byte 4)
                RT,     // right trigger (byte 5)
                LX,     // left joystick x-axis (bytes 6 & 7, 16-bit signed little-endian)
                LY,     // left joystick y-axis (bytes 8 & 9, 16-bit signed little-endian)
                RX,     // right joystick x-axis (bytes 10 & 11, 16-bit signed little-endian)
                RY,     // right joystick y-axis (bytes 12 & 13, 16-bit signed little-endian)

                MAX_AXIS = RY
            };
            const char* const axis_name[MAX_AXIS+1] = {
                "LT", "RT",
                "LX", "LY",
                "RX", "RY"
            };
        }; // namespace XBOX360USB

        namespace PS3_MOTION { // gamepad_type = 6
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace PS3_MOTION

        namespace SpaceNav { // gamepad_type = 7
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace SpaceNav

        namespace SWITCH { // gamepad_type = 8
            enum button {MAX_BUTTON};
            const char* const button_name[MAX_BUTTON+1] = {""};
            enum axis {MAX_AXIS};
            const char* const axis_name[MAX_AXIS+1] = {""};
        }; // namespace SWITCH

    } // namespace GAMEPAD

    extern USBHub hub1;
    extern JoystickController gamepad;
    extern USBHIDParser hid1;

    void ProcessGamepad(JoystickController &device);
#endif