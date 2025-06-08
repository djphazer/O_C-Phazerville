#include "HSGamepad.h"
#include "HemisphereApplet.h"

// #define GAMEPAD_DEBUG

// connect PS3 controller to a PC and use Sixaxis Pair Tool to set or determine this address
// changing address will break association to your PS3
uint8_t ps3_address[6] = {0x1a, 0x2b, 0x3c, 0x01, 0x01, 0x01};

void ProcessGamepad(JoystickController &device) {
    HS::IOFrame &f = HS::frame;

    if (device.joystickType() == JoystickController::PS3 && !f.GpState.ps3_paired)
        f.GpState.ps3_paired = device.PS3Pair(ps3_address);

    if (device.available()) {
        uint64_t axis_changed_mask = device.axisChangedMask();
        uint32_t buttons = device.getButtons();

        if (axis_changed_mask) {
            // for (uint8_t i = 0; i < JoystickController::TOTAL_AXIS_COUNT; i++) {
            // // for (uint8_t i = 0; axis_changed_mask != 0; i++, axis_changed_mask >>= 1) {
            // //     if (axis_changed_mask & 1) {
            //         // f.GpState.axis[i] = device.getAxis(i);
            //         // Serial.print(device.getAxis(i));
            //         // Serial.print(" ");
            //     // }
            // }
            // Serial.print("\n");

            uint8_t ltv; // left trigger value
            uint8_t rtv; // right trigger value
            int16_t ljs_x; // left joy stick, x value
            int16_t ljs_y; // left joy stick, y value
            int16_t rjs_x; // right joy stick, x value
            int16_t rjs_y; // right joy stick, y value

            switch (device.joystickType()) {
                case JoystickController::PS4: {
                    //   printAngles();
                    ltv = device.getAxis(3);
                    rtv = device.getAxis(4);
                    if ((ltv != frame.GpState.left_trigger_value) || (rtv != frame.GpState.right_trigger_value)) {
                        frame.GpState.left_trigger_value = ltv;
                        frame.GpState.right_trigger_value = rtv;
                        device.setRumble(ltv, rtv);
                    }
                    break;
                }
                case JoystickController::PS3: {
                    ltv = device.getAxis(18);
                    rtv = device.getAxis(19);
                    if ((ltv != f.GpState.left_trigger_value) || (rtv != f.GpState.right_trigger_value)) {
                        f.GpState.left_trigger_value = ltv;
                        f.GpState.right_trigger_value = rtv;
                        device.setRumble(ltv, rtv, 50);
                    }
                    break;
                }
                case JoystickController::XBOXONE:
                case JoystickController::XBOX360W:
                case JoystickController::XBOX360USB: {
                    ltv = device.getAxis(4);
                    if (ltv != f.GpState.left_trigger_value) {
                        f.GpState.left_trigger_value = ltv;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ltv: %d \n", ltv);
                            f.GpState.set_rumble = true;
                        #endif
                    }
                    rtv = device.getAxis(5);
                    if (rtv != f.GpState.right_trigger_value) {
                        f.GpState.right_trigger_value = rtv;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rtv: %d \n", rtv);
                            f.GpState.set_rumble = true;
                        #endif
                    }

                    ljs_x = ((int16_t)device.getAxis(7) << 8) | device.getAxis(6);
                    if (f.GpState.left_js_x_value != ljs_x) {
                        f.GpState.left_js_x_value = ljs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_x: %d \n", ljs_x);
                        #endif
                    }
                    ljs_y = ((int16_t)device.getAxis(9) << 8) | device.getAxis(8);
                    if (f.GpState.left_js_y_value != ljs_y) {
                        f.GpState.left_js_y_value = ljs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("ljs_y: %d \n", ljs_y);
                        #endif
                    }

                    rjs_x = ((int16_t)device.getAxis(11) << 8) | device.getAxis(10);
                    if (f.GpState.right_js_x_value != rjs_x) {
                        f.GpState.right_js_x_value = rjs_x;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_x: %d \n", rjs_x);
                        #endif
                    }
                    rjs_y = ((int16_t)device.getAxis(13) << 8) | device.getAxis(12);
                    if (f.GpState.right_js_y_value != rjs_y) {
                        f.GpState.right_js_y_value = rjs_y;
                        #ifdef GAMEPAD_DEBUG
                            Serial.printf("rjs_y: %d \n", rjs_y);
                        #endif
                    }

                    if (f.GpState.set_rumble) {
                        device.setRumble(ltv, rtv);
                        f.GpState.set_rumble = false;
                    }
                    break;
                }
                default: break;
            }
        }

        if (f.GpState.button_mask != buttons) {
            f.GpState.button_mask = buttons;
            #ifdef GAMEPAD_DEBUG
                if (device.joystickType() == JoystickController::XBOX360USB) {
                    if (buttons & (1 << GAMEPAD::XBOX360::button::LB)) Serial.println("LB");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::RB)) Serial.println("RB");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::GUIDE)) Serial.println("GUIDE");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::A)) Serial.println("A");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::B)) Serial.println("B");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::X)) Serial.println("X");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::Y)) Serial.println("Y");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::D_UP)) Serial.println("D_UP");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::D_DOWN)) Serial.println("D_DOWN");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::D_LEFT)) Serial.println("D_LEFT");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::D_RIGHT)) Serial.println("D_RIGHT");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::START)) Serial.println("START");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::BACK)) Serial.println("BACK");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::L3)) Serial.println("L3");
                    if (buttons & (1 << GAMEPAD::XBOX360::button::R3)) Serial.println("R3");
                }
            #endif
        }

        device.joystickDataClear();
    }
}
