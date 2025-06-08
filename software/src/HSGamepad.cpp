#include "HSGamepad.h"
#include "HemisphereApplet.h"

// #define GAMEPAD_DEBUG

// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

void ProcessGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    f.GamepadState.gamepad_type = device.joystickType();

    if (f.GamepadState.gamepad_type == JoystickController::PS3 && !f.GamepadState.ps3_paired)
        f.GamepadState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {
        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

        if (axis_changed_mask) {
            // for (uint8_t i = 0; i < JoystickController::TOTAL_AXIS_COUNT; i++) {
            // // for (uint8_t i = 0; axis_changed_mask != 0; i++, axis_changed_mask >>= 1) {
            // //     if (axis_changed_mask & 1) {
            //         // f.GamepadState.axis[i] = device.getAxis(i);
            //         // Serial.print(device.getAxis(i));
            //         // Serial.print(" ");
            //     // }
            // }
            // Serial.print("\n");

            uint8_t ltv; // left trigger value (axis[0])
            uint8_t rtv; // right trigger value (axis[1])
            int16_t ljs_x; // left joy stick, x value (axis[2])
            int16_t ljs_y; // left joy stick, y value (axis[3])
            int16_t rjs_x; // right joy stick, x value (axis[4])
            int16_t rjs_y; // right joy stick, y value (axis[5])
            // int16_t tilt_x; // future
            // int16_t tily_y; // future

            switch (f.GamepadState.gamepad_type) {
                case JoystickController::PS4: {
                    //   printAngles();
                    ltv = device.getAxis(3);
                    rtv = device.getAxis(4);
                    if ((ltv != frame.GamepadState.axis[0]) || (rtv != frame.GamepadState.axis[1])) {
                        frame.GamepadState.axis[0] = ltv;
                        frame.GamepadState.axis[1] = rtv;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                case JoystickController::PS3: {
                    ltv = device.getAxis(18);
                    rtv = device.getAxis(19);
                    if ((ltv != f.GamepadState.axis[0]) || (rtv != f.GamepadState.axis[1])) {
                        f.GamepadState.axis[0] = ltv;
                        f.GamepadState.axis[1] = rtv;
                        device.setRumble(ltv, rtv, 50);
                    }
                    break;
                }
                case JoystickController::XBOXONE:
                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                    using namespace GAMEPAD::XBOX360USB;
                    ltv = device.getAxis(4);
                    if (ltv != f.GamepadState.axis[LT]) {
                        f.GamepadState.axis[LT] = ltv;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ltv: %d \n", ltv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }
                    rtv = device.getAxis(5);
                    if (rtv != f.GamepadState.axis[RT]) {
                        f.GamepadState.axis[RT] = rtv;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rtv: %d \n", rtv);
                            f.GamepadState.set_rumble = true;
                        #endif
                    }

                    ljs_x = ((int16_t)device.getAxis(7) << 8) | device.getAxis(6);
                    if (f.GamepadState.axis[LX] != ljs_x) {
                        f.GamepadState.axis[LX] = ljs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_x: %d \n", ljs_x);
                        #endif
                    }
                    ljs_y = ((int16_t)device.getAxis(9) << 8) | device.getAxis(8);
                    if (f.GamepadState.axis[LY] != ljs_y) {
                        f.GamepadState.axis[LY] = ljs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_y: %d \n", ljs_y);
                        #endif
                    }

                    rjs_x = ((int16_t)device.getAxis(11) << 8) | device.getAxis(10);
                    if (f.GamepadState.axis[RX] != rjs_x) {
                        f.GamepadState.axis[RX] = rjs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_x: %d \n", rjs_x);
                        #endif
                    }
                    rjs_y = ((int16_t)device.getAxis(13) << 8) | device.getAxis(12);
                    if (f.GamepadState.axis[RY] != rjs_y) {
                        f.GamepadState.axis[RY] = rjs_y;
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
            f.GamepadState.button_mask = buttons;
            #ifdef GAMEPAD_DEBUG
                if (f.GamepadState.gamepad_type == JoystickController::XBOX360USB) {
                    using namespace GAMEPAD::XBOX360USB;
                    if (buttons & (1 << LB)) Serial.println(button_name[LB]);
                    if (buttons & (1 << RB)) Serial.println(button_name[RB]);
                    if (buttons & (1 << GUIDE)) Serial.println(button_name[GUIDE]);
                    if (buttons & (1 << A)) Serial.println(button_name[A]);
                    if (buttons & (1 << B)) Serial.println(button_name[B]);
                    if (buttons & (1 << X)) Serial.println(button_name[X]);
                    if (buttons & (1 << Y)) Serial.println(button_name[Y]);
                    if (buttons & (1 << D_UP)) Serial.println(button_name[D_UP]);
                    if (buttons & (1 << D_DOWN)) Serial.println(button_name[D_DOWN]);
                    if (buttons & (1 << D_LEFT)) Serial.println(button_name[D_LEFT]);
                    if (buttons & (1 << D_RIGHT)) Serial.println(button_name[D_RIGHT]);
                    if (buttons & (1 << START)) Serial.println(button_name[START]);
                    if (buttons & (1 << BACK)) Serial.println(button_name[BACK]);
                    if (buttons & (1 << L3)) Serial.println(button_name[L3]);
                    if (buttons & (1 << R3)) Serial.println(button_name[R3]);
                }
            #endif
        }

        device.joystickDataClear();
    }
}
