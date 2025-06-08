#pragma once

#include <USBHost_t36.h>

#if defined(ARDUINO_TEENSY41)
    namespace GAMEPAD {
        namespace XBOX360 {
            enum button {
                LB = 0,     // 0x0001,
                RB,         // 0x0002,
                GUIDE,      // 0x0004,
                unused_8,   // 0x0008,

                A,          // 0x0010,
                B,          // 0x0020,
                X,          // 0x0040,
                Y,          // 0x0080,

                D_UP,       // 0x0100,
                D_DOWN,     // 0x0200,
                D_LEFT,     // 0x0400,
                D_RIGHT,    // 0x0800,

                START,      // 0x1000,
                BACK,       // 0x2000,
                L3,         // 0x4000,
                R3,         // 0x8000,

                MAX_BUTTON = R3
            };
            const char* const button_name[16] = {
                "LB", "RB", "(X)", "?",
                "A", "B", "X", "Y",
                "D_UP", "D_DN", "D_L", "D_R",
                "START", "BACK", "L3", "R3"
            };

            enum axis {
                LT = 0,
                RT,
                LX,
                LY,
                RX,
                RY,

                MAX_AXIS = RY
            };
            const char* const axis_name[6] = {
                "LT", "RT",
                "LX", "LY",
                "RX", "RY"
            };
        };

        struct PS3 {
            enum button {};
            const char* const button_name[16] = {};
        };
    }

    extern USBHub hub1;
    extern JoystickController gamepad;
    extern USBHIDParser hid1;

    void ProcessGamepad(JoystickController &device);
#endif