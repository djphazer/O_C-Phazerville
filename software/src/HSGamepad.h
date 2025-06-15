#pragma once

#include <USBHost_t36.h>

#if defined(ARDUINO_TEENSY41)
    // namespace GAMEPAD {
    //     struct XBOXONE { // gamepad_type = 3
    //         enum Button { // bytes 4 & 5
    //             RESERVED = 0,   // 0x01 0x00
    //             KEEP_ALIVE,     // 0x02 0x00
    //             MENU,           // 0x04 0x00
    //             VIEW,           // 0x08 0x00
    //
    //             A,              // 0x10 0x00
    //             B,              // 0x20 0x00
    //             X,              // 0x40 0x00
    //             Y,              // 0x80 0x00
    //
    //             D_UP,           // 0x00 0x01
    //             D_DOWN,         // 0x00 0x02
    //             D_LEFT,         // 0x00 0x04
    //             D_RIGHT,        // 0x00 0x08
    //
    //             LB,             // 0x00 0x10
    //             RB,             // 0x00 0x20
    //             L3,             // 0x00 0x40
    //             R3,             // 0x00 0x80
    //
    //             BUTTON_LAST = R3
    //         };
    //         static const char* button_name[BUTTON_LAST+1];
    //
    //         // static const uint8_t xbox_axis_order_mapping[] = {3, 4, 0, 1, 2, 5}; // fix this in joystick.cpp:1205, or find out why it exists
    //         // "LT"<-LY, "RT"<-RX, "LX"<-LT, "LY"<-RT, "RX"<-LX, "RY" should be fine lol
    //
    //         enum Axis { // bytes 6-17
    //             LT = 0, // left trigger (bytes 6 & 7, 16-bit signed little-endian, 0-1023)
    //             RT,     // right trigger (bytes 8 & 9, 16-bit signed little-endian, 0-1023)
    //             LX,     // left joystick x-axis (bytes 10 & 11, 16-bit signed little-endian, -32768-32767)
    //             LY,     // left joystick y-axis (bytes 12 & 13, 16-bit signed little-endian, -32768-32767)
    //             RX,     // right joystick x-axis (bytes 14 & 15, 16-bit signed little-endian, -32768-32767)
    //             RY,     // right joystick y-axis (bytes 16 & 17, 16-bit signed little-endian, -32768-32767)
    //
    //             AXIS_LAST = RY
    //         };
    //         static const char* axis_name[AXIS_LAST+1];
    //
    //         static const int TRIG_MIN = 0;
    //         static const int TRIG_MAX = 1023;
    //         static const int AXIS_MIN = -32768;
    //         static const int AXIS_MAX = 32767;
    //     }; // XBOXONE
    //
    //     struct XBOX360USB { // gamepad_type = 5 (and 4?)
    //         enum Button { // bytes 2 & 3
    //             D_UP = 0,   // 0x01 0x00
    //             D_DOWN,     // 0x02 0x00
    //             D_LEFT,     // 0x04 0x00
    //             D_RIGHT,    // 0x08 0x00
    //
    //             START,      // 0x10 0x00
    //             BACK,       // 0x20 0x00
    //             L3,         // 0x40 0x00
    //             R3,         // 0x80 0x00
    //
    //             LB,         // 0x00 0x01
    //             RB,         // 0x00 0x02
    //             GUIDE,      // 0x00 0x04
    //             unused_8,   // 0x00 0x08
    //
    //             A,          // 0x00 0x10
    //             B,          // 0x00 0x20
    //             X,          // 0x00 0x40
    //             Y,          // 0x00 0x80
    //
    //             BUTTON_LAST = Y
    //         };
    //         static const char* button_name[BUTTON_LAST+1];
    //
    //         enum Axis { // bytes 4-13
    //             LT = 0, // left trigger (byte 4, 8-bit unsigned, 0-255)
    //             RT,     // right trigger (byte 5, 8-bit unsigned, 0-255)
    //             LX,     // left joystick x-axis (bytes 6 & 7, 16-bit signed little-endian)
    //             LY,     // left joystick y-axis (bytes 8 & 9, 16-bit signed little-endian)
    //             RX,     // right joystick x-axis (bytes 10 & 11, 16-bit signed little-endian)
    //             RY,     // right joystick y-axis (bytes 12 & 13, 16-bit signed little-endian)
    //
    //             AXIS_LAST = RY
    //         };
    //         static const char* axis_name[AXIS_LAST+1];
    //
    //         static const int TRIG_MIN = 0;
    //         static const int TRIG_MAX = 255;
    //         static const int AXIS_MIN = -32768;
    //         static const int AXIS_MAX = 32767;
    //     }; // XBOX360USB

    // } // namespace GAMEPAD

    struct GamePad {
        const char* type_name;

        const char* const* button_name;
        const int button_count;

        const char* const* axis_name;
        const int axis_count;
        const int trig_count;

        const int trig_min;
        const int trig_max;
        const int axis_min;
        const int axis_max;
    };

    extern GamePad UNKNOWN; // gamepad_type = 0
    extern GamePad PS3; // gamepad_type = 1
    extern GamePad PS4; // gamepad_type = 2
    extern GamePad XBOXONE; // gamepad_type = 3
    extern GamePad XBOX360; // gamepad_type = 4 & 5
    extern GamePad PS3_MOTION; // gamepad_type = 6
    extern GamePad SpaceNav; // gamepad_type = 7
    extern GamePad SWITCH; // gamepad_type = 8

    extern USBHub hub1;
    extern JoystickController gamepad;
    extern USBHIDParser hid1;

    void ProcessGamepad(JoystickController &device);
#endif