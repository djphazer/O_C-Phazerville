#pragma once

#include <USBHost_t36.h>

enum xbox_button {
    LB = 0x0001,
    RB = 0x0002,
    GUIDE = 0x0004,
    unknown_8 = 0x0008,

    A = 0x0010,
    B = 0x0020,
    X = 0x0040,
    Y = 0x0080,

    D_UP = 0x0100,
    D_DOWN = 0x0200,
    D_LEFT = 0x0400,
    D_RIGHT = 0x0800,

    START = 0x1000,
    BACK = 0x2000,
    JOY_LEFT = 0x4000,
    JOY_RIGHT = 0x8000,
};

extern USBHub hub1;
extern JoystickController joystick;
extern USBHIDParser hid1;

// button/axis map enums here?