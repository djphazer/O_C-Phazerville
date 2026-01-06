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
    - clean out old namespace stuff / move comments where they need to be elsewhere
    - reorder gamepad struct
    - add some Report Descriptor member variables to gamepad struct for proper order mapping and cv scaling when looping over axes
*/

#pragma once

#ifdef USB_GAMEPAD

#include <USBHost_t36.h>

/*
    defines may not be the best way to do this but it saves me from (un)commenting
    a bunch of stuff when i need to reduce size temporarily
*/
// #define ENABLE_PS3
// #define ENABLE_PS3_MOTION
// #define ENABLE_PS4
// #define ENABLE_XBOX
#define ENABLE_XBOX360
#define ENABLE_XBOXONE
// #define ENABLE_SpaceNav
// #define ENABLE_SWITCH
#define ENABLE_SNES
// #define ENABLE_N64

/*  this is how i had it kinda working previously but it sucked so its all the way different now,
    still useful as a reference, will purge it later

    namespace GAMEPAD {
        struct XBOXONE { // gamepad_type = 3
            enum Button { // bytes 4 & 5
                RESERVED = 0,   // 0x01 0x00
                KEEP_ALIVE,     // 0x02 0x00
                MENU,           // 0x04 0x00
                VIEW,           // 0x08 0x00

                A,              // 0x10 0x00
                B,              // 0x20 0x00
                X,              // 0x40 0x00
                Y,              // 0x80 0x00

                D_UP,           // 0x00 0x01
                D_DOWN,         // 0x00 0x02
                D_LEFT,         // 0x00 0x04
                D_RIGHT,        // 0x00 0x08

                LB,             // 0x00 0x10
                RB,             // 0x00 0x20
                L3,             // 0x00 0x40
                R3,             // 0x00 0x80

                BUTTON_LAST = R3
            };
            static const char* button_name[BUTTON_LAST+1];

            // static const uint8_t xbox_axis_order_mapping[] = {3, 4, 0, 1, 2, 5}; // fix this in joystick.cpp:1205, or find out why it exists
            // "LT"<-LY, "RT"<-RX, "LX"<-LT, "LY"<-RT, "RX"<-LX, "RY" should be fine lol

            enum Axis { // bytes 6-17
                LT = 0, // left trigger (bytes 6 & 7, 16-bit signed little-endian, 0-1023)
                RT,     // right trigger (bytes 8 & 9, 16-bit signed little-endian, 0-1023)
                LX,     // left joystick x-axis (bytes 10 & 11, 16-bit signed little-endian, -32768-32767)
                LY,     // left joystick y-axis (bytes 12 & 13, 16-bit signed little-endian, -32768-32767)
                RX,     // right joystick x-axis (bytes 14 & 15, 16-bit signed little-endian, -32768-32767)
                RY,     // right joystick y-axis (bytes 16 & 17, 16-bit signed little-endian, -32768-32767)

                AXIS_LAST = RY
            };
            static const char* axis_name[AXIS_LAST+1];

            static const int TRIG_MIN = 0;
            static const int TRIG_MAX = 1023;
            static const int AXIS_MIN = -32768;
            static const int AXIS_MAX = 32767;
        }; // XBOXONE

        struct XBOX360USB { // gamepad_type = 5 (and 4?)
            enum Button { // bytes 2 & 3
                D_UP = 0,   // 0x01 0x00
                D_DOWN,     // 0x02 0x00
                D_LEFT,     // 0x04 0x00
                D_RIGHT,    // 0x08 0x00

                START,      // 0x10 0x00
                BACK,       // 0x20 0x00
                L3,         // 0x40 0x00
                R3,         // 0x80 0x00

                LB,         // 0x00 0x01
                RB,         // 0x00 0x02
                GUIDE,      // 0x00 0x04
                unused_8,   // 0x00 0x08

                A,          // 0x00 0x10
                B,          // 0x00 0x20
                X,          // 0x00 0x40
                Y,          // 0x00 0x80

                BUTTON_LAST = Y
            };
            static const char* button_name[BUTTON_LAST+1];

            enum Axis { // bytes 4-13
                LT = 0, // left trigger (byte 4, 8-bit unsigned, 0-255)
                RT,     // right trigger (byte 5, 8-bit unsigned, 0-255)
                LX,     // left joystick x-axis (bytes 6 & 7, 16-bit signed little-endian)
                LY,     // left joystick y-axis (bytes 8 & 9, 16-bit signed little-endian)
                RX,     // right joystick x-axis (bytes 10 & 11, 16-bit signed little-endian)
                RY,     // right joystick y-axis (bytes 12 & 13, 16-bit signed little-endian)

                AXIS_LAST = RY
            };
            static const char* axis_name[AXIS_LAST+1];

            static const int TRIG_MIN = 0;
            static const int TRIG_MAX = 255;
            static const int AXIS_MIN = -32768;
            static const int AXIS_MAX = 32767;
        }; // XBOX360USB
    } // namespace GAMEPAD
*/

    extern JoystickController joystick;

    enum GamepadFunctions {
        GP_NOOP = 0,
        GP_CV,
        GP_GATE,
        GP_TRIG,
        GP_LEARN,
        GP_FUNC_LAST = GP_LEARN
    };

    struct GamePad {
        const char* type_name;

        const char* const* button_name;
        const int button_count;

        const char* const* axis_name;
        const int axis_count;

        const int* axis_byte_map;
        const int* axis_scaling; // bit depth, e.g. 10-bit -> 0-1024
        const int* axis_symmetry; // 0 = unipolar, 1 = bipolar
        const int* axis_inversion; // 0 = normal, 1 = inverted
        const int axis_center;
        const int dpad_byte;
        const int* dpad_shift_map;
    };

    extern GamePad UNKNOWN;
#ifdef ENABLE_PS3
    extern GamePad PS3;
#endif
#ifdef ENABLE_PS3_MOTION
    extern GamePad PS3_MOTION;
#endif
#ifdef ENABLE_PS4
    extern GamePad PS4;
#endif
#ifdef ENABLE_XBOX
    extern GamePad XBOX;
#endif
#ifdef ENABLE_XBOX360
    extern GamePad XBOX360;
#endif
#ifdef ENABLE_XBOXONE
    extern GamePad XBOXONE;
#endif
#ifdef ENABLE_SpaceNav
    extern GamePad SpaceNav;
#endif
#ifdef ENABLE_SWITCH
    extern GamePad SWITCH;
#endif
#ifdef ENABLE_SNES
    extern GamePad SNES;
#endif
#ifdef ENABLE_N64
    extern GamePad N64;
#endif

    void ProcessGamepad(JoystickController &device);
#endif