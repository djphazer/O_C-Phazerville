/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_

//#define DEBUG_JOYSTICK
#ifdef  DEBUG_JOYSTICK
#define DBGPrintf USBHDBGSerial.printf
#else
#define DBGPrintf(...)
#endif


template<class T>
const T& clamp(const T& x, const T& lower, const T& upper) {
    return min(upper, max(x, lower));
}

bool ack_rvd = false;

bool JoystickController::queue_Data_Transfer_Debug(Pipe_t *pipe, void *buffer,
    uint32_t len, USBDriver *driver, uint32_t line)
{
    if ((pipe == nullptr) || (driver == nullptr) || ((len > 0) && (buffer == nullptr))) {
        // something wrong:
        DBGPrintf("\n !!!!!!!!!!! JoystickController::queue_Data_Transfer called with bad data line: %u\n", line);
        DBGPrintf("\t pipe:%p buffer:%p len:%u driver:%p\n", pipe, buffer, len, driver);
        return false;
    }
    return queue_Data_Transfer(pipe, buffer, len, driver);
}


// PID/VID to joystick mapping - Only the XBOXOne is used to claim the USB interface directly,
// The others are used after claim-hid code to know which one we have and to use it for
// doing other features.
JoystickController::product_vendor_mapping_t JoystickController::pid_vid_mapping[] = {
    { 0x045e, 0x02dd, XBOXONE, false },  // Xbox One Controller
    { 0x045e, 0x02ea, XBOXONE, false },  // Xbox One S Controller
    { 0x045e, 0x0b12, XBOXONE, false },  // Xbox Core Controller (Series S/X)
    { 0x045e, 0x0719, XBOX360, false},
    { 0x045e, 0x028E, SWITCH, false},  // Switch?
    { 0x057E, 0x2009, SWITCH, true},   // Switch Pro controller.  // Let the swtich grab it, but...
    { 0x0079, 0x201C, SWITCH, false},
    { 0x054C, 0x0268, PS3, true},
    { 0x054C, 0x042F, PS3, true},   // PS3 Navigation controller
    { 0x054C, 0x03D5, PS3_MOTION, true},    // PS3 Motion controller
    { 0x054C, 0x05C4, PS4, true},   {0x054C, 0x09CC, PS4, true },
    { 0x0A5C, 0x21E8, PS4, true},
    { 0x046D, 0xC626, SpaceNav, true},  // 3d Connextion Space Navigator, 0x10008
    { 0x046D, 0xC628, SpaceNav, true}  // 3d Connextion Space Navigator, 0x10008
};


//-----------------------------------------------------------------------------
// Switch controller Bluetooth config structure.
//-----------------------------------------------------------------------------
static uint8_t switch_packet_num = 0;
struct SWProBTSendConfigData {
        uint8_t hid_hdr;
        uint8_t id;
        uint8_t gpnum; //GlobalPacketNumber
        uint8_t rumbleDataL[4];
        uint8_t rumbleDataR[4];
        uint8_t subCommand;
        uint8_t subCommandData[38];
} __attribute__((packed));

struct SWProBTSendConfigData1 {
        uint8_t hid_hdr;
        uint8_t id;
        uint8_t gpnum; //GlobalPacketNumber
        uint8_t subCommand;
        uint8_t subCommandData[38];
} __attribute__((packed));

struct SWProIMUCalibration {
	int16_t acc_offset[3];
	int16_t acc_sensitivity[3] = {16384, 16384, 16384};
	int16_t gyro_offset[3];
	int16_t gyro_sensitivity[3] = {15335, 15335, 15335};
}  __attribute__((packed));

struct SWProIMUCalibration SWIMUCal;

struct SWProStickCalibration {
	int16_t rstick_center_x;
	int16_t rstick_center_y;
	int16_t rstick_x_min;
	int16_t rstick_x_max;
	int16_t rstick_y_min;
	int16_t rstick_y_max;

	int16_t lstick_center_x;
	int16_t lstick_center_y;
	int16_t lstick_x_min;
	int16_t lstick_x_max;
	int16_t lstick_y_min;
	int16_t lstick_y_max;

	int16_t deadzone_left;
	int16_t deadzone_right;
}  __attribute__((packed));

struct SWProStickCalibration SWStickCal;

//-----------------------------------------------------------------------------
void JoystickController::init()
{
    contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
    contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
    contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs) / sizeof(strbuf_t));
    driver_ready_for_device(this);
    USBHIDParser::driver_ready_for_hid_collection(this);
    BluetoothController::driver_ready_for_bluetooth(this);
}

//-----------------------------------------------------------------------------
JoystickController::joytype_t JoystickController::mapVIDPIDtoJoystickType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices)
{
    for (uint8_t i = 0; i < (sizeof(pid_vid_mapping) / sizeof(pid_vid_mapping[0])); i++) {
        if ((idVendor == pid_vid_mapping[i].idVendor) && (idProduct == pid_vid_mapping[i].idProduct)) {
            println("Match PID/VID: ", i, DEC);
            if (exclude_hid_devices && pid_vid_mapping[i].hidDevice) return UNKNOWN;
            return pid_vid_mapping[i].joyType;
        }
    }
    return UNKNOWN;     // Not in our list
}

//*****************************************************************************
// Some simple query functions depend on which interface we are using...
//*****************************************************************************

uint16_t JoystickController::idVendor()
{
    if (device != nullptr) return device->idVendor;
    if (mydevice != nullptr) return mydevice->idVendor;
    return 0;
}

uint16_t JoystickController::idProduct()
{
    if (device != nullptr) return device->idProduct;
    if (mydevice != nullptr) return mydevice->idProduct;
    return 0;
}

const uint8_t *JoystickController::manufacturer()
{
    if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
    //if ((btdevice != nullptr) && (btdevice->strbuf != nullptr)) return &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
    if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
    return nullptr;
}

const uint8_t *JoystickController::product()
{
    if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
    if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
    if (btconnect != nullptr) return btconnect->remote_name_;
    return nullptr;
}

const uint8_t *JoystickController::serialNumber()
{
    if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
    if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
    return nullptr;
}


static uint8_t rumble_counter = 0;




bool JoystickController::setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout)
{
    // Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
    // joystick types.
    rumble_lValue_ = lValue;
    rumble_rValue_ = rValue;
    rumble_timeout_ = timeout;

    switch (joystickType_) {
    default:
        break;
    case PS3:
        return transmitPS3UserFeedbackMsg();
    case PS3_MOTION:
        return transmitPS3MotionUserFeedbackMsg();
    case PS4:
        return transmitPS4UserFeedbackMsg();
    case XBOXONE:
        // Lets try sending a request to the XBox 1.
		if (btdriver_) {
			DBGPrintf("\nXBOXONE BT Joystick update Rumble %d %d %d\n", lValue, rValue, timeout);
			txbuf_[0] = 0xA2;                  // HID BT DATA (0xA0) | Report Type (Output 0x02)
			txbuf_[1] = 0x03; // ID 0x03
			txbuf_[2] = 0x0F; // Rumble mask (what motors are activated) (0000 lT rT L R)
			txbuf_[3] = map(lValue, 0, 1023, 0, 100); // lT force
			txbuf_[4] = map(rValue, 0, 1023, 0, 100); // rT force
			txbuf_[5] = map(lValue, 0, 1023, 0, 100); // L force
			txbuf_[6] = map(rValue, 0, 1023, 0, 100); // R force
			txbuf_[7] = 0xff; // Length of pulse
			txbuf_[8] = 0x00; // Period between pulses
			txbuf_[9] = 0x00; // Repeat
			btdriver_->sendL2CapCommand(txbuf_, 10, BluetoothController::INTERRUPT_SCID);
			return true;
		}

        txbuf_[0] = 0x9;
        txbuf_[1] = 0x0;
        txbuf_[2] = 0x0;
        txbuf_[3] = 0x09; // Substructure (what substructure rest of this packet has)
        txbuf_[4] = 0x00; // Mode
        txbuf_[5] = 0x0f; // Rumble mask (what motors are activated) (0000 lT rT L R)
        txbuf_[6] = 0x0; // lT force
        txbuf_[7] = 0x0; // rT force
        txbuf_[8] = lValue; // L force
        txbuf_[9] = rValue; // R force
        txbuf_[10] = 0xff; // Length of pulse
        txbuf_[11] = 0x00; // Period between pulses
        txbuf_[12] = 0x00; // Repeat
        if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 13, this, __LINE__)) {
            println("XBoxOne rumble transfer fail");
        }
        return true;    //
    case XBOX360:
        txbuf_[0] = 0x00;
        txbuf_[1] = 0x01;
        txbuf_[2] = 0x0F;
        txbuf_[3] = 0xC0;
        txbuf_[4] = 0x00;
        txbuf_[5] = lValue;
        txbuf_[6] = rValue;
        txbuf_[7] = 0x00;
        txbuf_[8] = 0x00;
        txbuf_[9] = 0x00;
        txbuf_[10] = 0x00;
        txbuf_[11] = 0x00;
        if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 12, this, __LINE__)) {
            println("XBox360 rumble transfer fail");
        }
        return true;
    case SWITCH:
        if (btdriver_) {
            struct SWProBTSendConfigData *packet =  (struct SWProBTSendConfigData *)txbuf_ ;
            memset((void*)packet, 0, sizeof(struct SWProBTSendConfigData));
            packet->hid_hdr = 0xA2; // HID BT Get_report (0xA0) | Report Type (Output)
            packet->id = 0x10;
			if(switch_packet_num > 0x10) switch_packet_num = 0;
            packet->gpnum = switch_packet_num;
            switch_packet_num = (switch_packet_num + 1) & 0x0f;
            // 2-9 rumble data;
            Serial.printf("Set Rumble data: %d, %d\n", lValue, rValue);
            // 2-9 rumble data;
			uint8_t rumble_on[8] = {0x28, 0x88, 0x60, 0x61, 0x28, 0x88, 0x60, 0x61};
			uint8_t rumble_off[8] =  {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};

			//if ((lValue == 0x00) && (rValue == 0x00)) {
			//	for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_off[i];
			//	for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_off[i];
			//}
			if ((lValue != 0x0) || (rValue != 0x0)) {
                if (lValue != 0 && rValue == 0) {
					for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_off[i];
					for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_on[i];
                } else if (rValue != 0 && lValue == 0) {
					for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_on[i];
					for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_off[i];
                } else if (rValue != 0 && lValue != 0) {
					for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_on[i];
					for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_on[i];
                }
            }


            packet->subCommand = 0x0;
            packet->subCommandData[0] = 0;
            btdriver_->sendL2CapCommand((uint8_t *)packet, sizeof(struct SWProBTSendConfigData), BluetoothController::INTERRUPT_SCID /*0x40*/);
            return true;
        }

        Serial.printf("Set Rumble data (USB): %d, %d\n", lValue, rValue);

        memset(txbuf_, 0, 18);  // make sure it is cleared out
        //txbuf_[0] = 0x80;
        //txbuf_[1] = 0x92;
        //txbuf_[3] = 0x31;
        txbuf_[0] = 0x10;   // Command

        // Now add in subcommand data:
        // Probably do this better soon
		if(switch_packet_num > 0x10) switch_packet_num = 0;
        txbuf_[1 + 0] = switch_packet_num;
        switch_packet_num = (switch_packet_num + 1) & 0x0f; //

		uint8_t rumble_on[8] = {0x28, 0x88, 0x60, 0x61, 0x28, 0x88, 0x60, 0x61};
		uint8_t rumble_off[8] =  {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};

		//if ((lValue == 0x00) && (rValue == 0x00)) {
		//	for(uint8_t i = 0; i < 4; i++) packet->rumbleDataR[i] = rumble_off[i];
		//	for(uint8_t i = 4; i < 8; i++) packet->rumbleDataL[i-4] = rumble_off[i];
		//}
		if ((lValue != 0x0) || (rValue != 0x0)) {
			if (lValue != 0 && rValue == 0x00) {
				for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_off[i];
				for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_on[i];
			} else if (rValue != 0 && lValue == 0x00) {
				for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_on[i];
				for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_off[i];
			} else if (rValue != 0 && lValue != 0) {
				for(uint8_t i = 0; i < 4; i++) txbuf_[i + 2] = rumble_on[i];
				for(uint8_t i = 4; i < 8; i++) txbuf_[i - 4 + 6] = rumble_on[i];
			}
		}
		txbuf_[11] = 0x00;
		txbuf_[12] = 0x00;

		if(driver_) {
			driver_->sendPacket(txbuf_, 18);
		} else if (txpipe_) {
			if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 12, this, __LINE__)) {
				println("switch transfer fail");
				Serial.printf("Switch transfer fail\n");
			}
		}

        return true;
    }
    return false;
}


bool JoystickController::setLEDs(uint8_t lr, uint8_t lg, uint8_t lb)
{
    // Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
    // joystick types.
    DBGPrintf("::setLEDS(%x %x %x)\n", lr, lg, lb);
    if ((leds_[0] != lr) || (leds_[1] != lg) || (leds_[2] != lb)) {
        leds_[0] = lr;
        leds_[1] = lg;
        leds_[2] = lb;

        switch (joystickType_) {
        case PS3:
            return transmitPS3UserFeedbackMsg();
        case PS3_MOTION:
            return transmitPS3MotionUserFeedbackMsg();
        case PS4:
            return transmitPS4UserFeedbackMsg();
        case XBOX360:
            // 0: off, 1: all blink then return to before
            // 2-5(TL, TR, BL, BR) - blink on then stay on
            // 6-9() - On
            // ...
            txbuf_[1] = 0x00;
            txbuf_[2] = 0x08;
            txbuf_[3] = 0x40 + lr;
            txbuf_[4] = 0x00;
            txbuf_[5] = 0x00;
            txbuf_[6] = 0x00;
            txbuf_[7] = 0x00;
            txbuf_[8] = 0x00;
            txbuf_[9] = 0x00;
            txbuf_[10] = 0x00;
            txbuf_[11] = 0x00;
            if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 12, this, __LINE__)) {
                println("XBox360 set leds fail");
            }
            return true;
        case SWITCH:
            if (btdriver_) {
                DBGPrintf("Init LEDs\n");
                struct SWProBTSendConfigData *packet =  (struct SWProBTSendConfigData *)txbuf_ ;
                memset((void*)packet, 0, sizeof(struct SWProBTSendConfigData));
                packet->hid_hdr = 0xA2; // HID BT Get_report (0xA0) | Report Type (Output)
                packet->id = 1;
                packet->gpnum = switch_packet_num;
                switch_packet_num = (switch_packet_num + 1) & 0x0f;
                // 2-9 rumble data;
                /*packet->rumbleDataL[0] = 0x00;
                packet->rumbleDataL[1] = 0x01;
                packet->rumbleDataL[2] = 0x40;
                packet->rumbleDataL[3] = 0x00;
                packet->rumbleDataR[0] = 0x00;
                packet->rumbleDataR[1] = 0x01;
                packet->rumbleDataR[2] = 0x40;
                packet->rumbleDataR[3] = 0x00; */

                packet->subCommand = 0x30; // Report ID
                packet->subCommandData[0] = lr; // try full 0x30?; // Report ID
                btdriver_->sendL2CapCommand((uint8_t *)packet, sizeof(struct SWProBTSendConfigData), BluetoothController::INTERRUPT_SCID /*0x40*/);
                return true;
            }

            memset(txbuf_, 0, 20);  // make sure it is cleared out

            txbuf_[0] = 0x01;   // Command

            // Now add in subcommand data:
            // Probably do this better soon
            txbuf_[1 + 0] = rumble_counter++; //
            txbuf_[1 + 1] = 0x00;
            txbuf_[1 + 2] = 0x01;
            txbuf_[1 + 3] = 0x40;
            txbuf_[1 + 4] = 0x40;
            txbuf_[1 + 5] = 0x00;
            txbuf_[1 + 6] = 0x01;
            txbuf_[1 + 7] = 0x40;
            txbuf_[1 + 8] = 0x40;

            txbuf_[1 + 9] = 0x30; // LED Command
            txbuf_[1 + 10] = lr;
            println("Switch set leds: driver? ", (uint32_t)driver_, HEX);
            print_hexbytes((uint8_t*)txbuf_, 20);

			if(driver_) {
				driver_->sendPacket(txbuf_, 20);
			} else if (txpipe_) {
				if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 20, this, __LINE__)) {
					println("switch transfer fail");
					Serial.printf("Switch transfer fail\n");
				}
			}

        case XBOXONE:
        default:
            return false;
        }
    }
    return false;
}


bool JoystickController::transmitPS4UserFeedbackMsg() {
    if (driver_)  {
        uint8_t packet[32];
        memset(packet, 0, sizeof(packet));

        packet[0] = 0x05; // Report ID
        packet[1] = 0xFF;

        packet[4] = rumble_lValue_; // Small Rumble
        packet[5] = rumble_rValue_; // Big rumble
        packet[6] = leds_[0]; // RGB value
        packet[7] = leds_[1];
        packet[8] = leds_[2];
        // 9, 10 flash ON, OFF times in 100ths of second?  2.5 seconds = 255
        DBGPrintf("Joystick update Rumble/LEDs\n");
        return driver_->sendPacket(packet, 32);
    } else if (btdriver_) {
        uint8_t packet[79];
        memset(packet, 0, sizeof(packet));
//0xa2, 0x11, 0xc0, 0x20, 0xf0, 0x04, 0x00
        packet[0] = 0x52;
        packet[1] = 0x11;      // Report ID
        packet[2] = 0x80;
        //packet[3] = 0x20;
        packet[4] = 0xFF;

        packet[7] = rumble_lValue_; // Small Rumble
        packet[8] = rumble_rValue_; // Big rumble
        packet[9] = leds_[0]; // RGB value
        packet[10] = leds_[1];
        packet[11] = leds_[2];

        // 12, 13 flash ON, OFF times in 100ths of sedond?  2.5 seconds = 255
        DBGPrintf("Joystick update Rumble/LEDs\n");
        btdriver_->sendL2CapCommand(packet, sizeof(packet), 0x40);

        return true;
    }
    return false;
}

static const uint8_t PS3_USER_FEEDBACK_INIT[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0xff, 0x27, 0x10, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

bool JoystickController::transmitPS3UserFeedbackMsg() {
    if (driver_) {
        memcpy(txbuf_, PS3_USER_FEEDBACK_INIT, 48);

        txbuf_[1] = rumble_lValue_ ? rumble_timeout_ : 0;
        txbuf_[2] = rumble_lValue_; // Small Rumble
        txbuf_[3] = rumble_rValue_ ? rumble_timeout_ : 0;
        txbuf_[4] = rumble_rValue_; // Big rumble
        txbuf_[9] = leds_[2] << 1; // RGB value     // using third led now...
        //DBGPrintf("\nJoystick update Rumble/LEDs %d %d %d %d %d\n",  txbuf_[1], txbuf_[2],  txbuf_[3],  txbuf_[4],  txbuf_[9]);
        return driver_->sendControlPacket(0x21, 9, 0x201, 0, 48, txbuf_);
    } else if (btdriver_) {
        txbuf_[0] = 0x52;
        txbuf_[1] = 0x1;
        memcpy(&txbuf_[2], PS3_USER_FEEDBACK_INIT, 48);

        txbuf_[3] = rumble_lValue_ ? rumble_timeout_ : 0;
        txbuf_[4] = rumble_lValue_; // Small Rumble
        txbuf_[5] = rumble_rValue_ ? rumble_timeout_ : 0;
        txbuf_[6] = rumble_rValue_; // Big rumble
        txbuf_[11] = leds_[2] << 1; // RGB value
        DBGPrintf("\nJoystick update Rumble/LEDs %d %d %d %d %d\n",  txbuf_[3], txbuf_[4],  txbuf_[5],  txbuf_[6],  txbuf_[11]);
        btdriver_->sendL2CapCommand(txbuf_, 50, BluetoothController::CONTROL_SCID);
        return true;
    }
    return false;
}

#define MOVE_REPORT_BUFFER_SIZE 7
#define MOVE_HID_BUFFERSIZE 50 // Size of the buffer for the Playstation Motion Controller

bool JoystickController::transmitPS3MotionUserFeedbackMsg() {
    if (driver_) {
        txbuf_[0] = 0x02; // Set report ID, this is needed for Move commands to work
        txbuf_[2] = leds_[0];
        txbuf_[3] = leds_[1];
        txbuf_[4] = leds_[2];
        txbuf_[6] = rumble_lValue_; // Set the rumble value into the write buffer

        //return driver_->sendControlPacket(0x21, 9, 0x201, 0, MOVE_REPORT_BUFFER_SIZE, txbuf_);
        return driver_->sendPacket(txbuf_, MOVE_REPORT_BUFFER_SIZE);

    } else if (btdriver_) {
        txbuf_[0] = 0xA2; // HID BT DATA_request (0xA0) | Report Type (Output 0x02)
        txbuf_[1] = 0x02; // Report ID
        txbuf_[3] = leds_[0];
        txbuf_[4] = leds_[1];
        txbuf_[5] = leds_[2];
        txbuf_[7] = rumble_lValue_;
        btdriver_->sendL2CapCommand(txbuf_, MOVE_HID_BUFFERSIZE, BluetoothController::INTERRUPT_SCID);
        return true;
    }
    return false;
}

//*****************************************************************************
// Support for Joysticks that Use HID data.
//*****************************************************************************

hidclaim_t JoystickController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
    DBGPrintf("JoystickController::claim_collection(%p) Driver:%p(%u %u) Dev:%p Top:%x\n", this, driver,
      driver->interfaceSubClass(), driver->interfaceProtocol(), dev, topusage);
    // only claim Desktop/Joystick and Desktop/Gamepad
    if (topusage != 0x10004 && topusage != 0x10005 && topusage != 0x10008) return CLAIM_NO;
    // only claim from one physical device
    if (mydevice != NULL && dev != mydevice) return CLAIM_NO;

    // Also don't allow us to claim if it is used as a standard usb object (XBox...)
    if (device != nullptr) return CLAIM_NO;

    mydevice = dev;
    collections_claimed++;
    anychange = true; // always report values on first read
    driver_ = driver;   // remember the driver.
    driver_->setTXBuffers(txbuf_, nullptr, sizeof(txbuf_));
    connected_ = true;      // remember that hardware is actually connected...

    // Lets see if we know what type of joystick this is. That is, is it a PS3 or PS4 or ...
    joystickType_ = mapVIDPIDtoJoystickType(mydevice->idVendor, mydevice->idProduct, false);
    DBGPrintf("JoystickController::claim_collection joystickType_=%d\n", joystickType_);
    switch (joystickType_) {
    case PS3:
    case PS3_MOTION: // not sure yet
        additional_axis_usage_page_ = 0x1;
        additional_axis_usage_start_ = 0x100;
        additional_axis_usage_count_ = 39;
        axis_change_notify_mask_ = (uint64_t) - 1;  // Start off assume all bits
        break;
    case PS4:
        additional_axis_usage_page_ = 0xFF00;
        additional_axis_usage_start_ = 0x21;
        additional_axis_usage_count_ = 54;
        axis_change_notify_mask_ = (uint64_t)0xfffffffffffff3ffl;   // Start off assume all bits - 10 and 11
        break;
     case SWITCH:
        // bugbug set the hand shake...
        {
            DBGPrintf("Send Handshake\n");
            sw_sendCmdUSB(0x02, SW_CMD_TIMEOUT);
            initialPass_ = true;
            connectedComplete_pending_ = 0;
        }
        // fall through

    default:
        additional_axis_usage_page_ = 0x09;
        additional_axis_usage_start_ = 0x21;
        additional_axis_usage_count_ = 5;
        axis_change_notify_mask_ = 0x3ff;   // Start off assume only the 10 bits...
    }


    //DBGPrintf("Claim Additional axis: %x %x %d\n", additional_axis_usage_page_, additional_axis_usage_start_, additional_axis_usage_count_);
    USBHDBGSerial.printf("\tJoystickController claim collection\n");
    return CLAIM_REPORT;
}

void JoystickController::disconnect_collection(Device_t *dev)
{
    if (--collections_claimed == 0) {
        mydevice = NULL;
        driver_ = nullptr;
        axis_mask_ = 0;
        axis_changed_mask_ = 0;
    }
}

void JoystickController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
    // TODO: set up translation from logical min/max to consistent 16 bit scale

}

void JoystickController::hid_input_data(uint32_t usage, int32_t value)
{
    DBGPrintf("joystickType_=%d\n", joystickType_);
    DBGPrintf("Joystick: usage=%X, value=%d\n", usage, value);
    uint32_t usage_page = usage >> 16;
    usage &= 0xFFFF;
    if (usage_page == 9 && usage >= 1 && usage <= 32) {
        uint32_t bit = 1 << (usage - 1);
        if (value == 0) {
            if (buttons & bit) {
                buttons &= ~bit;
                anychange = true;
            }
        } else {
            if (!(buttons & bit)) {
                buttons |= bit;
                anychange = true;
            }
        }
    } else if (usage_page == 1 && usage >= 0x30 && usage <= 0x39) {
        // TODO: need scaling of value to consistent API, 16 bit signed?
        // TODO: many joysticks repeat slider usage.  Detect & map to axis?
        uint32_t i = usage - 0x30;
        axis_mask_ |= (1 << i);     // Keep record of which axis we have data on.
        if (axis[i] != value) {
            axis[i] = value;
            axis_changed_mask_ |= (1 << i);
            if (axis_changed_mask_ & axis_change_notify_mask_)
                anychange = true;
        }
    } else if (usage_page == additional_axis_usage_page_) {
        // see if the usage is witin range.
        //DBGPrintf("UP: usage_page=%x usage=%x User: %x %d\n", usage_page, usage, user_buttons_usage_start, user_buttons_count_);
        if ((usage >= additional_axis_usage_start_) && (usage < (additional_axis_usage_start_ + additional_axis_usage_count_))) {
            // We are in the user range.
            uint16_t usage_index = usage - additional_axis_usage_start_ + STANDARD_AXIS_COUNT;
            if (usage_index < (sizeof(axis) / sizeof(axis[0]))) {
                if (axis[usage_index] != value) {
                    axis[usage_index] = value;
                    if (usage_index > 63) usage_index = 63; // don't overflow our mask
                    axis_changed_mask_ |= ((uint64_t)1 << usage_index);     // Keep track of which ones changed.
                    if (axis_changed_mask_ & axis_change_notify_mask_)
                        anychange = true;   // We have changes...
                }
                axis_mask_ |= ((uint64_t)1 << usage_index);     // Keep record of which axis we have data on.
            }
            //DBGPrintf("UB: index=%x value=%x\n", usage_index, value);
        }

    } else {
        DBGPrintf("UP: usage_page=%x usage=%x add: %x %x %d\n", usage_page, usage, additional_axis_usage_page_, additional_axis_usage_start_, additional_axis_usage_count_);

    }
    // TODO: hat switch?

}

void JoystickController::hid_input_end()
{
    if (anychange) {
        joystickEvent = true;
    }
}

bool JoystickController::hid_process_out_data(const Transfer_t *transfer)
{
    DBGPrintf("JoystickController::hid_process_out_data\n");
    return true;
}

//---------------------------------------------------
// Try to handle some of the startup code messages for Switch controll
// b
bool JoystickController::sw_handle_usb_init_of_joystick(uint8_t *buffer, uint16_t cb, bool timer_event)
{
    if (buffer) {
        if ((buffer[0] != 0x81) && (buffer[0] != 0x21))
            return false; // was not an event message
        driver_->stopTimer();
        uint8_t ack_rpt = buffer[0];
        if (ack_rpt == 0x81) {
            uint8_t ack_81_subrpt = buffer[1];
            DBGPrintf("\t(%u)CMD last sent: %x ack cmd: %x ", (uint32_t)em_sw_, sw_last_cmd_sent_, ack_81_subrpt);
            switch(ack_81_subrpt) {
                case 0x02: DBGPrintf("Handshake Complete......\n"); break;
                case 0x03: DBGPrintf("Baud Rate Change Complete......\n"); break;
                case 0x04: DBGPrintf("Disabled USB Timeout Complete......\n"); break;
                default:  DBGPrintf("???");
            }

            if (!initialPass_) return true; // don't need to process

            if (sw_last_cmd_sent_ == ack_rpt) {
                sw_last_cmd_repeat_count = 0;
                connectedComplete_pending_++;
            } else {
                DBGPrintf("\tcmd != ack rpt count:%u ", sw_last_cmd_repeat_count);
                if (sw_last_cmd_repeat_count) {
                    DBGPrintf("Skip to next\n");
                    sw_last_cmd_repeat_count = 0;
                    connectedComplete_pending_++;
                } else {
                    DBGPrintf("Retry\n");
                    sw_last_cmd_repeat_count++;
                }
            }
        } else {
            // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
            //21 0a 71 00 80 00 01 e8 7f 01 e8 7f 0c 80 40 00 00 00 00 00 00 ...
            uint8_t ack_21_subrpt = buffer[14];
			sw_parseAckMsg(buffer);
            DBGPrintf("\t(%u)CMD Submd ack cmd: %x \n", (uint32_t)em_sw_, ack_21_subrpt);
            switch (ack_21_subrpt) {
                case 0x40: DBGPrintf("IMU Enabled......\n"); break;
                case 0x48: DBGPrintf("Rumbled Enabled......\n"); break;
                case 0x10: DBGPrintf("IMU Cal......\n"); break;
                case 0x30: DBGPrintf("Std Rpt Enabled......\n"); break;
                default: DBGPrintf("Other"); break;
            }

            if (!initialPass_) return true; // don't need to process
            sw_last_cmd_repeat_count = 0;
            connectedComplete_pending_++;

        }

    } else if (timer_event) {
        if (!initialPass_) return true; // don't need to process
        DBGPrintf("\t(%u)Timer event - advance\n", (uint32_t)em_sw_);
        sw_last_cmd_repeat_count = 0;
        connectedComplete_pending_++;
    }

    // Now lets Send out the next state
    // Note: we don't increment it here, but let the ack or
    // timeout increment it.
    uint8_t packet_[8];
    switch(connectedComplete_pending_) {
        case 0:
            //Change Baud
            DBGPrintf("Change Baud\n");
            sw_sendCmdUSB(0x03, SW_CMD_TIMEOUT);
            break;
        case 1:
            DBGPrintf("Handshake2\n");
            sw_sendCmdUSB(0x02, SW_CMD_TIMEOUT);
            break;
        case 2:
            DBGPrintf("Try to get IMU Cal\n");
            packet_[0] = 0x20;
            packet_[1] = 0x60;
            packet_[2] = 0x00;
            packet_[3] = 0x00;
            packet_[4] = (0x6037 - 0x6020 + 1);
            sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);   // doesnt work wired
            break;
		case 3:
			DBGPrintf("\nTry to Get IMU Horizontal Offset Data\n");
			packet_[0] = 0x80;
			packet_[1] = 0x60;
			packet_[2] = 0x00;
			packet_[3] = 0x00;
			packet_[4] = (0x6085 - 0x6080 + 1);
			sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);
			break;
		case 4:
			DBGPrintf("\n Read: Factory Analog stick calibration and Controller Colours\n");
			packet_[0] = 0x3D;
			packet_[1] = 0x60;
			packet_[2] = 0x00;
			packet_[3] = 0x00;
			packet_[4] = (0x6055 - 0x603D + 1);
			sw_sendSubCmdUSB(0x10, packet_, 5, SW_CMD_TIMEOUT);
            break;
        case 5:
            connectedComplete_pending_++;
            DBGPrintf("Enable IMU\n");
            packet_[0] = 0x01;
            sw_sendSubCmdUSB(0x40, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 6:
            DBGPrintf("JC_USB_CMD_NO_TIMEOUT\n");
            sw_sendCmdUSB(0x04, SW_CMD_TIMEOUT);
            break;
        case 7:
            DBGPrintf("Enable Rumble\n");
            packet_[0] = 0x01;
            sw_sendSubCmdUSB(0x48, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 8:
            DBGPrintf("Enable Std Rpt\n");
            packet_[0] = 0x30;
            sw_sendSubCmdUSB(0x03, packet_, 1, SW_CMD_TIMEOUT);
            break;
        case 9:
            DBGPrintf("JC_USB_CMD_NO_TIMEOUT\n");
            packet_[0] = 0x04;
            sw_sendCmdUSB(0x04, SW_CMD_TIMEOUT);
            break;
        case 10:
            connectedComplete_pending_ = 99;
            initialPass_ = false;
    }
    return true;
}


bool JoystickController::hid_process_in_data(const Transfer_t *transfer)
{
    uint8_t *buffer = (uint8_t *)transfer->buffer;
    if (*buffer) report_id_ = *buffer;
    uint8_t cnt = transfer->length;
    if (!buffer || *buffer == 1) return false; // don't do report 1

    DBGPrintf("hid_process_in_data %x %u %u %p %x %x:", transfer->buffer, transfer->length, joystickType_, txpipe_, initialPass_, connectedComplete_pending_);
    for (uint8_t i = 0; i < cnt; i++) DBGPrintf(" %02x", buffer[i]);
    DBGPrintf("\n");

	if (joystickType_ == SWITCH) {
        if (sw_handle_usb_init_of_joystick(buffer, cnt, false))
            return true;
		// the main HID parse code should handle it.
		sw_process_HID_data(buffer, cnt);
        return true; // don't let main hid code process this.
	}


	return false;
}

void JoystickController::hid_timer_event(USBDriverTimer *whichTimer)
{
    DBGPrintf("JoystickController: Timer\n");
    if (!driver_) return;
    driver_->stopTimer();
    sw_handle_usb_init_of_joystick(nullptr, 0, true);
}

void JoystickController::bt_hid_timer_event(USBDriverTimer *whichTimer)
{
    DBGPrintf("Bluetooth JoystickController: Timer\n");
    if (!btconnect) return;
    btconnect->stopTimer();
    sw_handle_bt_init_of_joystick(nullptr, 0, true);
}

bool JoystickController::hid_process_control(const Transfer_t *transfer) {
    Serial.printf("USBHIDParser::control msg: %x %x : %x %u :", transfer->setup.word1, transfer->setup.word2, transfer->buffer, transfer->length);
    if (transfer->buffer) {
        uint16_t cnt = transfer->length;
        if (cnt > 16) cnt = 16;
        uint8_t *pb = (uint8_t*)transfer->buffer;
        while (cnt--) Serial.printf(" %02x", *pb++);
    }
    Serial.printf("\n");
    send_Control_packet_active_ = false;
    return false;
}

void JoystickController::joystickDataClear() {
    joystickEvent = false;
    anychange = false;
    axis_changed_mask_ = 0;
    axis_mask_ = 0;
}

//*****************************************************************************
// Support for Joysticks that are class specific and do not use HID
// Example: XBox One controller.
//*****************************************************************************

static  uint8_t xboxone_start_input[] = {0x05, 0x20, 0x00, 0x01, 0x00};
static  uint8_t xbox360w_inquire_present[] = {0x08, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//static  uint8_t switch_start_input[] = {0x19, 0x01, 0x03, 0x07, 0x00, 0x00, 0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
static  uint8_t switch_start_input[] = {0x80, 0x02};
bool JoystickController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
    println("JoystickController claim this=", (uint32_t)this, HEX);
    DBGPrintf("JoystickController::claim(%p) Dev:%p Type:%x\n", this, dev, type);

    // Don't try to claim if it is used as USB device or HID device
    if (mydevice != NULL) return false;
    if (device != nullptr) return false;

    // Try claiming at the interface level.
    if (type != 1) return false;
    print_hexbytes(descriptors, len);

    JoystickController::joytype_t jtype = mapVIDPIDtoJoystickType(dev->idVendor, dev->idProduct, true);
    DBGPrintf("\tVID:%x PID:%x Jype:%x\n", dev->idVendor, dev->idProduct, jtype);
    println("Jtype=", (uint8_t)jtype, DEC);
    if (jtype == UNKNOWN)
        return false;

    // XBOX One
    //  0  1  2  3  4  5  6  7  8 *9 10  1  2  3  4  5 *6  7  8  9 20  1  2  3  4  5  6  7  8  9 30  1...
    // 09 04 00 00 02 FF 47 D0 00 07 05 02 03 40 00 04 07 05 82 03 40 00 04 09 04 01 00 00 FF 47 D0 00
    // Lets do some verifications to make sure.

    // XBOX 360 wireless... Has 8 interfaces.  4 joysticks (1, 3, 5, 7) and 4 headphones assume 2,4,6, 8...
    // Shows data for #1 only...
    // Also they have some unknown data type we need to ignore between interface and end points.
    //  0  1  2  3  4  5  6  7  8 *9 10  1  2  3  4  5 *6  7  8  9 20  1  2  3  4  5  6  7  8
    // 09 04 00 00 02 FF 5D 81 00 14 22 00 01 13 81 1D 00 17 01 02 08 13 01 0C 00 0C 01 02 08

    // 29 30  1  2  3  4  5  6  7  8  9 40 41 42
    // 07 05 81 03 20 00 01 07 05 01 03 20 00 08

    // Switch
    // 09 04 00 00 02 FF 5D 01 00
    // 10 21 10 01 01 24 81 14 03 00 03 13 02 00 03 00
    // 07 05 81 03 20 00 08
    // 07 05 02 03 20 00 08



    if (len < 9 + 7 + 7) return false;

    // Some common stuff for both XBoxs
    uint32_t count_end_points = descriptors[4];
    if (count_end_points < 2) return false;
    if (descriptors[5] != 0xff) return false; // bInterfaceClass, 3 = HID
    rx_ep_ = 0;
    uint32_t txep = 0;
    uint8_t rx_interval = 0;
    uint8_t tx_interval = 0;
    rx_size_ = 0;
    tx_size_ = 0;
    uint32_t descriptor_index = 9;
    if (descriptors[descriptor_index + 1] == 0x22)  {
        if (descriptors[descriptor_index] != 0x14) return false; // only support specific versions...
        descriptor_index += descriptors[descriptor_index]; // XBox360w ignore this unknown setup...
    }
    while ((rx_ep_ == 0) || txep == 0) {
        print("  Index:", descriptor_index, DEC);

        if (descriptor_index >= len) return false;          // we ran off the end and did not get end points
        // see if the next data is an end point descript
        if ((descriptors[descriptor_index] == 7) && (descriptors[descriptor_index + 1] == 5)) {
            if ((descriptors[descriptor_index + 3] == 3)            // Type 3...
                    && (descriptors[descriptor_index + 4] <= 64)
                    && (descriptors[descriptor_index + 5] == 0)) {
                // have a bulk EP size
                if (descriptors[descriptor_index + 2] & 0x80 ) {
                    rx_ep_ = descriptors[descriptor_index + 2];
                    rx_size_ = descriptors[descriptor_index + 4];
                    rx_interval = descriptors[descriptor_index + 6];
                } else {
                    txep = descriptors[descriptor_index + 2];
                    tx_size_ = descriptors[descriptor_index + 4];
                    tx_interval = descriptors[descriptor_index + 6];
                }
            }
        }
        descriptor_index += descriptors[descriptor_index];  // setup to look at next one...
    }
    if ((rx_ep_ == 0) || (txep == 0)) return false; // did not find two end points.
    print("JoystickController, rx_ep_=", rx_ep_ & 15);
    print("(", rx_size_);
    print("), txep=", txep);
    print("(", tx_size_);
    println(")");
    rxpipe_ = new_Pipe(dev, 3, rx_ep_ & 15, 1, rx_size_, rx_interval);
    if (!rxpipe_) return false;
    txpipe_ = new_Pipe(dev, 3, txep, 0, tx_size_, tx_interval);
    if (!txpipe_) {
        //free_Pipe(rxpipe_);
        return false;
    }
    rxpipe_->callback_function = rx_callback;
    queue_Data_Transfer_Debug(rxpipe_, rxbuf_, rx_size_, this, __LINE__);

    txpipe_->callback_function = tx_callback;

    if (jtype == XBOXONE) {
        queue_Data_Transfer_Debug(txpipe_, xboxone_start_input, sizeof(xboxone_start_input), this, __LINE__);
        connected_ = true;      // remember that hardware is actually connected...
    } else if (jtype == XBOX360) {
        queue_Data_Transfer_Debug(txpipe_, xbox360w_inquire_present, sizeof(xbox360w_inquire_present), this, __LINE__);
        connected_ = 0;     // remember that hardware is actually connected...
    } else if (jtype == SWITCH) {
        queue_Data_Transfer_Debug(txpipe_, switch_start_input, sizeof(switch_start_input), this, __LINE__);
        connected_ = true;      // remember that hardware is actually connected...
    }
    memset(axis, 0, sizeof(axis));  // clear out any data.
    joystickType_ = jtype;      // remember we are an XBox One.
    DBGPrintf("   JoystickController::claim joystickType_ %d\n", joystickType_);
	return true;
}

void JoystickController::control(const Transfer_t *transfer)
{
}


/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void JoystickController::rx_callback(const Transfer_t *transfer)
{
    if (!transfer->driver) return;
    ((JoystickController *)(transfer->driver))->rx_data(transfer);
}

void JoystickController::tx_callback(const Transfer_t *transfer)
{
    if (!transfer->driver) return;
    ((JoystickController *)(transfer->driver))->tx_data(transfer);
}



/************************************************************/
//  Interrupt-based Data Movement
// XBox one input data when type == 0x20
// Information came from several places on the web including:
// https://github.com/quantus/xbox-one-controller-protocol
/************************************************************/
// 20 00 C5 0E 00 00 00 00 00 00 F0 06 AD FB 7A 0A DD F7 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
// 20 00 E0 0E 40 00 00 00 00 00 F0 06 AD FB 7A 0A DD F7 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
typedef struct {
    uint8_t type;
    uint8_t const_0;
    uint16_t id;
    // From online references button order:
    //     sync, dummy, start, back, a, b, x, y
    //     dpad up, down left, right
    //     lb, rb, left stick, right stick
    // Axis:
    //     lt, rt, lx, ly, rx, ry
    //
    uint16_t buttons;
    int16_t axis[6];
} xbox1data20_t;

typedef struct {
    uint8_t state;
    uint8_t id_or_type;
    uint16_t controller_status;
    uint16_t unknown;
    // From online references button order:
    //     sync, dummy, start, back, a, b, x, y
    //     dpad up, down left, right
    //     lb, rb, left stick, right stick
    // Axis:
    //     lt, rt, lx, ly, rx, ry
    //
    uint16_t buttons;
    uint8_t lt;
    uint8_t rt;
    int16_t axis[4];
} xbox360data_t;

typedef struct {
    uint8_t state;
    uint8_t id_or_type;
    // From online references button order:
    //     sync, dummy, start, back, a, b, x, y
    //     dpad up, down left, right
    //     lb, rb, left stick, right stick
    // Axis:
    //     lt, rt, lx, ly, rx, ry
    //
    uint8_t buttons_h;
    uint8_t buttons_l;
    uint8_t lt;
    uint8_t rt;
    int16_t axis[4];
} switchdataUSB_t;

static const uint8_t xbox_axis_order_mapping[] = {3, 4, 0, 1, 2, 5};

void JoystickController::rx_data(const Transfer_t *transfer)
{
#ifdef  DEBUG_JOYSTICK
    print("JoystickController::rx_data (", joystickType_, DEC);
    print("): ");
    print_hexbytes((uint8_t*)transfer->buffer, transfer->length);
#endif

    if (joystickType_ == XBOXONE) {
        // Process XBOX One data
        axis_mask_ = 0x3f;
        axis_changed_mask_ = 0; // assume none for now
        xbox1data20_t *xb1d = (xbox1data20_t *)transfer->buffer;
        if ((xb1d->type == 0x20) && (transfer->length >= sizeof (xbox1data20_t))) {
            // We have a data transfer.  Lets see what is new...
            if (xb1d->buttons != buttons) {
                buttons = xb1d->buttons;
                anychange = true;
                joystickEvent = true;
                println("  Button Change: ", buttons, HEX);
            }
            for (uint8_t i = 0; i < sizeof (xbox_axis_order_mapping); i++) {
                // The first two values were unsigned.
                int axis_value = (i < 2) ? (int)(uint16_t)xb1d->axis[i] : xb1d->axis[i];
                if (axis_value != axis[xbox_axis_order_mapping[i]]) {
                    axis[xbox_axis_order_mapping[i]] = axis_value;
                    axis_changed_mask_ |= (1 << xbox_axis_order_mapping[i]);
                    anychange = true;
                }
            }
            joystickEvent = true;
        }

    } else if (joystickType_ == XBOX360) {
        // First byte appears to status - if the byte is 0x8 it is a connect or disconnect of the controller.
        xbox360data_t  *xb360d = (xbox360data_t *)transfer->buffer;
        if (xb360d->state == 0x08) {
            if (xb360d->id_or_type != connected_) {
                connected_ = xb360d->id_or_type;    // remember it...
                if (connected_) {
                    println("XBox360w - Connected type:", connected_, HEX);
                    // rx_ep_ should be 1, 3, 5, 7 for the wireless convert to 2-5 on led
                    setLEDs(2 + rx_ep_ / 2); // Right now hard coded to first joystick...

                } else {
                    println("XBox360w - disconnected");
                }
            }
        } else if ((xb360d->id_or_type == 0x00) && (xb360d->controller_status & 0x1300)) {
            // Controller status report - Maybe we should save away and allow the user access?
            println("XBox360w - controllerStatus: ", xb360d->controller_status, HEX);
        } else if (xb360d->id_or_type == 0x01) { // Lets only process report 1.
            //const uint8_t *pbuffer = (uint8_t*)transfer->buffer;
            //for (uint8_t i = 0; i < transfer->length; i++) DBGPrintf("%02x ", pbuffer[i]);
            //DBGPrintf("\n");

            if (buttons != xb360d->buttons) {
                buttons = xb360d->buttons;
                anychange = true;
            }
            axis_mask_ = 0x3f;
            axis_changed_mask_ = 0; // assume none for now

            for (uint8_t i = 0; i < 4; i++) {
                if (axis[i] != xb360d->axis[i]) {
                    axis[i] = xb360d->axis[i];
                    axis_changed_mask_ |= (1 << i);
                    anychange = true;
                }
            }
            // the two triggers show up as 4 and 5
            if (axis[4] != xb360d->lt) {
                axis[4] = xb360d->lt;
                axis_changed_mask_ |= (1 << 4);
                anychange = true;
            }

            if (axis[5] != xb360d->rt) {
                axis[5] = xb360d->rt;
                axis_changed_mask_ |= (1 << 5);
                anychange = true;
            }

            if (anychange) joystickEvent = true;
        }
    } else if (joystickType_ == SWITCH) {
    	uint8_t packet[8];
		if(initialPass_ == true) {
			switch(connectedComplete_pending_) {
				case 0:
					//setup handshake
					DBGPrintf("Send Handshake\n");
                    sw_sendCmdUSB(0x02, SW_CMD_TIMEOUT);
					connectedComplete_pending_ = 1;
					break;
				case 1:
					DBGPrintf("Send Hid only\n");
                    sw_sendCmdUSB(0x04, SW_CMD_TIMEOUT);
					connectedComplete_pending_ = 2;
					break;
				case 2:
					//Send report type
					DBGPrintf("Enable IMU\n");
					packet[0] = 0x01;
					sw_sendSubCmdUSB(0x40, packet, 1);
					connectedComplete_pending_ = 3;
					break;
				case 3:
					DBGPrintf("Enable Rumble\n");
					packet[0] = 0x01;
					sw_sendSubCmdUSB(0x48, packet, 1);
					connectedComplete_pending_ = 4;
					break;
				case 4:
					DBGPrintf("Enable Std Rpt\n");
					packet[0] = 0x30;
					sw_sendSubCmdUSB(0x3f, packet, 1);
					connectedComplete_pending_ = 5;
				case 5:
					connectedComplete_pending_ = 0;
					initialPass_ = false;
					break;
			}
		}
        switchdataUSB_t  *switchd = (switchdataUSB_t *)transfer->buffer;
		//uint32_t cur_buttons = switchd->buttons_l | (switchd->buttons_m << 8) | (switchd->buttons_h << 16);
        uint16_t cur_buttons = (switchd->buttons_h << 8) | switchd->buttons_l;
        if (buttons != cur_buttons) {
            buttons = cur_buttons;
            anychange = true;
        }
        axis_mask_ = 0x3f;
        axis_changed_mask_ = 0; // assume none for now

        for (uint8_t i = 0; i < 4; i++) {
            if (axis[i] != switchd->axis[i]) {
                axis[i] = switchd->axis[i];
                axis_changed_mask_ |= (1 << i);
                anychange = true;
            }
        }

		//apply stick calibration
		float xout, yout;
		CalcAnalogStick(xout, yout, axis[0], axis[1], true);
		//Serial.printf("Correctd Left Stick: %f, %f\n", xout , yout);
		axis[0] = int(round(xout));
		axis[1] = int(round(yout));

		CalcAnalogStick(xout, yout, axis[2], axis[3], true);
		axis[2] = int(round(xout));
		axis[3] = int(round(yout));


        // the two triggers show up as 4 and 5
        if (axis[6] != switchd->lt) {
            axis[6] = switchd->lt;
            axis_changed_mask_ |= (1 << 4);
            anychange = true;
        }

        if (axis[5] != switchd->rt) {
            axis[5] = switchd->rt;
            axis_changed_mask_ |= (1 << 5);
            anychange = true;
        }

        if (anychange) joystickEvent = true;
    }

    queue_Data_Transfer_Debug(rxpipe_, rxbuf_, rx_size_, this, __LINE__);
}

void JoystickController::tx_data(const Transfer_t *transfer)
{
}

void JoystickController::disconnect()
{
    axis_mask_ = 0;
    axis_changed_mask_ = 0;
    // TODO: free resources
}


hidclaim_t JoystickController::claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type)
{
    USBHDBGSerial.printf("JoystickController::claim_bluetooth - Class %x %s\n", bluetooth_class, remoteName);
    // If we are already in use than don't grab another one.  Likewise don't grab if it is used as USB or HID object
    if (btconnect && (btconnection != btconnect)) return CLAIM_NO;
    if (mydevice != NULL) return CLAIM_NO;

    if ((bluetooth_class & 0x0f00) == 0x500) {
        bool name_maps_to_joystick_type = (remoteName && mapNameToJoystickType(remoteName));
        if ((bluetooth_class & 0x3C) == 0x08) {
            bool claim_interface = (type == 1) || (remoteName == nullptr);
            if (name_maps_to_joystick_type) {
                switch (joystickType_) {
                    //case SWITCH:
                    default:
                        // others will experiment with trying for HID.
                        break;

                    case PS3:
                    case PS3_MOTION:
                        special_process_required = SP_PS3_IDS;      // PS3 maybe needs different IDS.
                        // fall through
                    case PS4:
                    case XBOXONE:
                    case SWITCH:
                        claim_interface = true;
                        break;
                }
            }
            if (claim_interface) {
                // They are telling me to grab it now. SO say yes
                USBHDBGSerial.printf("JoystickController::claim_bluetooth Interface\n");
                btconnect = btconnection;
                btdevice = (Device_t*)btconnect->btController_; // remember this way
                btdriver_ = btconnect->btController_;
                btdriver_->useHIDProtocol(true);

                // Another big hack try calling the connectionComplete to maybe update what reports we are working with
               // if (name_maps_to_joystick_type) connectionComplete();
                return CLAIM_INTERFACE;
            }
        }
        return CLAIM_REPORT; // let them know we may be interested if there is a HID REport Descriptor
    }
    return CLAIM_NO;
}

bool JoystickController::process_bluetooth_HID_data(const uint8_t *data, uint16_t length)
{
    // Example data from PS4 controller
    //01 7e 7f 82 84 08 00 00 00 00
    //   LX LY RX RY BT BT PS LT RT
    DBGPrintf("JoystickController::process_bluetooth_HID_data: data[0]=%x\n", data[0]);
    // May have to look at this one with other controllers...
    report_id_ = data[0];


    if (data[0] == 1) {
        //print("  Joystick Data: ");
        // print_hexbytes(data, length);
        if (length > TOTAL_AXIS_COUNT) length = TOTAL_AXIS_COUNT;   // don't overflow arrays...
        DBGPrintf("  Joystick Data: ");
        for (uint16_t i = 0; i < length; i++) DBGPrintf("%02x ", data[i]);
        DBGPrintf("\r\n");
        if (joystickType_ == PS3) {
            // Quick and dirty hack to match PS3 HID data
            uint32_t cur_buttons = data[2] | ((uint16_t)data[3] << 8) | ((uint32_t)data[4] << 16);
            if (cur_buttons != buttons) {
                buttons = cur_buttons;
                joystickEvent = true;   // something changed.
            }

            uint64_t mask = 0x1;
            axis_mask_ = 0x27;  // assume bits 0, 1, 2, 5
            for (uint16_t i = 0; i < 3; i++) {
                if (axis[i] != data[i + 6]) {
                    axis_changed_mask_ |= mask;
                    axis[i] = data[i + 6];
                }
                mask <<= 1; // shift down the mask.
            }
            if (axis[5] != data[9]) {
                axis_changed_mask_ |= (1 << 5);
                axis[5] = data[9];
            }

            if (axis[3] != data[18]) {
                axis_changed_mask_ |= (1 << 3);
                axis[3] = data[18];
            }

            if (axis[4] != data[19]) {
                axis_changed_mask_ |= (1 << 4);
                axis[4] = data[19];
            }

            // Then rest of data
            mask = 0x1 << 10;   // setup for other bits
            for (uint16_t i = 10; i < length; i++ ) {
                axis_mask_ |= mask;
                if (data[i] != axis[i]) {
                    axis_changed_mask_ |= mask;
                    axis[i] = data[i];
                }
                mask <<= 1; // shift down the mask.
            }
        } else if (joystickType_ == PS3_MOTION) {
            // Quick and dirty PS3_Motion data.
            uint32_t cur_buttons = data[1] | ((uint16_t)data[2] << 8) | ((uint32_t)data[3] << 16);
            if (cur_buttons != buttons) {
                buttons = cur_buttons;
                joystickEvent = true;   // something changed.
            }

            // Hard to know what is best here. for now just copy raw data over...
            // will do this for now... Format of thought to be data.
            //  data[1-3] Buttons (mentioned 4 as well but appears to be counter
            // axis[0-1] data[5] Trigger, Previous trigger value
            // 2-5 Unknown probably place holders for Axis like data for other PS3
            // 6 - Time stamp
            // 7 - Battery
            // 8-19 - Accel: XL, XH, YL, YH, ZL, ZH, XL2, XH2, YL2, YH2, ZL2, ZH2
            // 20-31 - Gyro: Xl,Xh,Yl,Yh,Zl,Zh,Xl2,Xh2,Yl2,Yh2,Zl2,Zh2
            // 32 - Temp High
            // 33 - Temp Low (4 bits)  Maybe Magneto x High on other??
            uint64_t mask = 0x1;
            axis_mask_ = 0; // assume bits 0, 1, 2, 5
            // Then rest of data
            mask = 0x1 << 10;   // setup for other bits
            for (uint16_t i = 5; i < length; i++ ) {
                axis_mask_ |= mask;
                if (data[i] != axis[i - 5]) {
                    axis_changed_mask_ |= mask;
                    axis[i - 5] = data[i];
                }
                mask <<= 1; // shift down the mask.
            }

        } else if (joystickType_ == XBOXONE) {
            // Process XBOX One data
            typedef struct __attribute__ ((packed)) {
                uint8_t report_type; // 1
                int16_t axis[6];
                uint32_t buttons;
                // From online references button order:
                //     sync, dummy, start, back, a, b, x, y
                //     dpad up, down left, right
                //     lb, rb, left stick, right stick
                // Axis:
                //     lt, rt, lx, ly, rx, ry
                //
            } xbox1data20bt_t;

            static const uint8_t xbox_bt_axis_order_mapping[] = { 0, 1, 2, 3, 4, 5};
            axis_mask_ = 0x3f;
            axis_changed_mask_ = 0; // assume none for now

            xbox1data20bt_t *xb1d = (xbox1data20bt_t *)data;
            //if ((xb1d->type == 0x20) && (length >= sizeof (xbox1data20bt_t))) {
                // We have a data transfer.  Lets see what is new...
                if (xb1d->buttons != buttons) {
                    buttons = xb1d->buttons;
                    anychange = true;
                    joystickEvent = true;
                    println("  Button Change: ", buttons, HEX);
                }
                for (uint8_t i = 0; i < sizeof (xbox_axis_order_mapping); i++) {
                    // The first two values were unsigned.
                    int axis_value = (i < 4) ? (int)(uint16_t)xb1d->axis[i] : xb1d->axis[i];

					//DBGPrintf(" axis value [ %d ] = %d \n", i, axis_value);

                    if (axis_value != axis[xbox_bt_axis_order_mapping[i]]) {
                        axis[xbox_bt_axis_order_mapping[i]] = axis_value;
                        axis_changed_mask_ |= (1 << xbox_bt_axis_order_mapping[i]);
                        anychange = true;
                    }
                }

                joystickEvent = true;
            //}

        } else {
            uint64_t mask = 0x1;
            axis_mask_ = 0;

            for (uint16_t i = 0; i < length; i++ ) {
                axis_mask_ |= mask;
                if (data[i] != axis[i]) {
                    axis_changed_mask_ |= mask;
                    axis[i] = data[i];
                }
                mask <<= 1; // shift down the mask.
//              DBGPrintf("%02x ", axis[i]);
            }

        }

        if (axis_changed_mask_ & axis_change_notify_mask_)
            joystickEvent = true;
        connected_ = true;
        return true;

    } else if (data[0] == 0x11) {
        DBGPrintf("\n  Joystick Data: ");
        uint64_t mask = 0x1;
        axis_mask_ = 0;
        axis_changed_mask_ = 0;

        //This moves data to be equivalent to what we see for
        //data[0] = 0x01
        uint8_t tmp_data[length - 2];

        for (uint16_t i = 0; i < (length - 2); i++ ) {
            tmp_data[i] = 0;
            tmp_data[i] = data[i + 2];
        }

        /*
         * [1] LX, [2] = LY, [3] = RX, [4] = RY
         * [5] combo, tri, cir, x, sqr, D-PAD (4bits, 0-3
         * [6] R3,L3, opt, share, R2, L2, R1, L1
         * [7] Counter (bit7-2), T-PAD, PS
         * [8] Left Trigger, [9] Right Trigger
         * [10-11] Timestamp
         * [12] Battery (0 to 0xff)
         * [13-14] acceleration x
         * [15-16] acceleration y
         * [17-18] acceleration z
         * [19-20] gyro x
         * [21-22] gyro y
         * [23-24] gyro z
         * [25-29] unknown
         * [30] 0x00,phone,mic, usb, battery level (4bits)
         * rest is trackpad?  to do implement?
         */
        //PS Bit
        tmp_data[7] = (tmp_data[7] >> 0) & 1;
        //set arrow buttons to axis[0]
        tmp_data[10] = tmp_data[5] & ((1 << 4) - 1);
        //set buttons for last 4bits in the axis[5]
        tmp_data[5] = tmp_data[5] >> 4;

        // Lets try mapping the DPAD buttons to high bits
        //                                            up    up/right  right    R DN      DOWN    L DN      Left    LUP
        static const uint32_t dpad_to_buttons[] = {0x10000, 0x30000, 0x20000, 0x60000, 0x40000, 0xC0000, 0x80000, 0x90000};

        // Quick and dirty hack to match PS4 HID data
        uint32_t cur_buttons = ((uint32_t)tmp_data[7] << 12) | (((uint32_t)tmp_data[6] * 0x10)) | ((uint16_t)tmp_data[5] ) ;

        if (tmp_data[10] < 8) cur_buttons |= dpad_to_buttons[tmp_data[10]];

        if (cur_buttons != buttons) {
            buttons = cur_buttons;
            joystickEvent = true;   // something changed.
        }

        mask = 0x1;
        axis_mask_ = 0x27;  // assume bits 0, 1, 2, 5
        for (uint16_t i = 0; i < 3; i++) {
            if (axis[i] != tmp_data[i + 1]) {
                axis_changed_mask_ |= mask;
                axis[i] = tmp_data[i + 1];
            }
            mask <<= 1; // shift down the mask.
        }
        if (axis[5] != tmp_data[4]) {
            axis_changed_mask_ |= (1 << 5);
            axis[5] = tmp_data[4];
        }

        if (axis[3] != tmp_data[8]) {
            axis_changed_mask_ |= (1 << 3);
            axis[3] = tmp_data[8];
        }

        if (axis[4] != tmp_data[9]) {
            axis_changed_mask_ |= (1 << 4);
            axis[4] = tmp_data[9];
        }

        //limit for masking
        mask = 0x1;
        for (uint16_t i = 6; i < (64); i++ ) {
            axis_mask_ |= mask;
            if (tmp_data[i] != axis[i]) {
                axis_changed_mask_ |= mask;
                axis[i] = tmp_data[i];
            }
            mask <<= 1; // shift down the mask.
            DBGPrintf("%02x ", axis[i]);
        }
        DBGPrintf("\n");
        //DBGPrintf("Axis Mask (axis_mask_, axis_changed_mask_; %d, %d\n", axis_mask_,axis_changed_mask_);
        joystickEvent = true;
        connected_ = true;
    } else if (joystickType_ == SWITCH) {
        if (sw_handle_bt_init_of_joystick(data, length, false))
            return true;

        return sw_process_HID_data(data, length);
    }

    return false;
}



//-----------------------------------------------------------------------------
// Process SWITCH controller data messages - split out here as used both for HID
// and Bluetooth HID
//-----------------------------------------------------------------------------
bool JoystickController::sw_handle_bt_init_of_joystick(const uint8_t *data, uint16_t length, bool timer_event)
{

    if (data) {
        if (data[0] != 0x21) return false;
        DBGPrintf("Joystick Acknowledge Command Rcvd! pending: %u SC: %x", connectedComplete_pending_, data[14]);
        if (data[13] & 0x80) DBGPrintf(" ACK(%x)\n", data[13]);
        else DBGPrintf(" ** NACK(%x) **\n", data[13]);
        DBGPrintf("  Joystick Data: ");
        for (uint16_t i = 0; i < length; i++) DBGPrintf("%02x ", data[i]);
        DBGPrintf("\r\n");

        btconnect->stopTimer();

        sw_parseAckMsg(data);

		DBGPrintf("==========> Connection Pending: %d\n",connectedComplete_pending_);


        if (!initialPassBT_) return true; // don't need to process
        // Shold maybe double check the right one...
        connectedComplete_pending_++;
    } else if (timer_event) {
        if (!initialPassBT_) return true; // don't need to process
        DBGPrintf("\t(%u)Timer event - advance\n", (uint32_t)em_sw_);
        connectedComplete_pending_++;
    }
		DBGPrintf("==========> Connection Pending: %d\n",connectedComplete_pending_);

    // only called by BT;
    uint8_t packet_[8];
    switch (connectedComplete_pending_) {
    case 1:
        DBGPrintf("\nSet Shipment Low Power State\n");
        packet_[0] = 0x00;
        sw_sendCmd(0x08, packet_, 1, SW_CMD_TIMEOUT );
        break;
      case 2:
        DBGPrintf("\n Read Left joystick dead zone\n");
        packet_[0] = 0x86;
        packet_[1] = 0x60;
        packet_[2] = 0x00;
        packet_[3] = 0x00;
        packet_[4] = (0x6097 - 0x6086 + 1);
        sw_sendCmd(0x10, packet_, 5, SW_CMD_TIMEOUT);
        break;
    case 3:
        DBGPrintf("\n Read Right stick dead zone\n");
        packet_[0] = 0x98;
        packet_[1] = 0x60;
        packet_[2] = 0x00;
        packet_[3] = 0x00;
        packet_[4] = (0x60A9 - 0x6098 + 1);
        sw_sendCmd(0x10, packet_, 5, SW_CMD_TIMEOUT);
        break;
    case 4:
        DBGPrintf("\n Read: Factory Analog stick calibration\n");
        packet_[0] = 0x3D;
        packet_[1] = 0x60;
        packet_[2] = 0x00;
        packet_[3] = 0x00;
        packet_[4] = (0x604E - 0x603D + 1);
        sw_sendCmd(0x10, packet_, 5, SW_CMD_TIMEOUT);
        break;
    case 5:
        DBGPrintf("\nTry to Get IMU Calibration Data\n");
        packet_[0] = 0x20;
        packet_[1] = 0x60;
        packet_[2] = 0x00;
        packet_[3] = 0x00;
        packet_[4] = (0x6037 - 0x6020 + 1);
        sw_sendCmd(0x10, packet_, 5, SW_CMD_TIMEOUT);
        break;
	case 6:
        DBGPrintf("\nTry to Get IMU Horizontal Offset Data\n");
        packet_[0] = 0x80;
        packet_[1] = 0x60;
        packet_[2] = 0x00;
        packet_[3] = 0x00;
        packet_[4] = (0x6097 - 0x6080 + 1);
        sw_sendCmd(0x10, packet_, 5, SW_CMD_TIMEOUT);
		break;
    case 7:
        DBGPrintf("\nTry to Enable IMU\n");
        packet_[0] = 0x01;
        sw_sendCmd(0x40, packet_, 1, SW_CMD_TIMEOUT);   /* 0x40 IMU, note: 0x00 would disable */
        break;
    case 8:
        DBGPrintf("\nTry to Enable Rumble\n");
        packet_[0] = 0x01;
        sw_sendCmd(0x48, packet_, 1, SW_CMD_TIMEOUT);
        break;
    case 9:
        DBGPrintf("\nTry to set LEDS\n");
        setLEDs(0x1, 0, 0);
        break;
    case 10:
        DBGPrintf("\nSet Report Mode\n");
        packet_[0] = 0x30; //0x3F;
        sw_sendCmd(0x03, packet_, 1, SW_CMD_TIMEOUT);
        break;
    case 11:
        DBGPrintf("\nTry to set Rumble\n");
        setRumble(0xff, 0xff, 0xff);
		initialPassBT_ = false;
        connectedComplete_pending_ = 0xff;
        break;
    }
    return true;
}

void JoystickController::sw_update_axis(uint8_t axis_index, int new_value)
{
    if (axis[axis_index] != new_value) {
        axis[axis_index] = new_value;
        anychange = true;
        axis_changed_mask_ |= (1 << axis_index);
    }
}

bool JoystickController::sw_process_HID_data(const uint8_t *data, uint16_t length)
{
    if (data[0] == 0x3f) {
        // Assume switch:
        //<<(02 15 21):48 20 11 00 0D 00 71 00 A1
        // 16 bits buttons
        // 4 bits hat
        // 4 bits <constant>
        // 16 bits X
        // 16 bits Y
        // 16 bits rx
        // 16 bits ry
        typedef struct __attribute__ ((packed)) {
            uint8_t report_type; // 1
            uint16_t buttons;
            uint8_t hat;
            int16_t axis[4];
            // From online references button order:
            //     sync, dummy, start, back, a, b, x, y
            //     dpad up, down left, right
            //     lb, rb, left stick, right stick
            // Axis:
            //     lt, rt, lx, ly, rx, ry
            //
        } switchbt_t;

        static const uint8_t switch_bt_axis_order_mapping[] = { 0, 1, 2, 3};
        axis_mask_ = 0x1ff;
        axis_changed_mask_ = 0; // assume none for now

        switchbt_t *sw1d = (switchbt_t *)data;
        // We have a data transfer.  Lets see what is new...
        if (sw1d->buttons != buttons) {
            buttons = sw1d->buttons;
            anychange = true;
            joystickEvent = true;
            println("  Button Change: ", buttons, HEX);
        }
        // We will put the HAT into axis 9 for now..
        if (sw1d->hat != axis[9]) {
            axis[9] = sw1d->hat;
            axis_changed_mask_ |= (1 << 9);
            anychange = true;
        }

        //just a hack for a single joycon.
        if(buttons == 0x8000) { //ZL
            axis[6] = 1;
        } else {
            axis[6] = 0;
        }
        if(buttons == 0x8000) {     //ZR
            axis[7] = 1;
        } else {
            axis[7] = 0;
        }


        for (uint8_t i = 0; i < sizeof (switch_bt_axis_order_mapping); i++) {
            // The first two values were unsigned.
            int axis_value = (uint16_t)sw1d->axis[i];

            //DBGPrintf(" axis value [ %d ] = %d \n", i, axis_value);

            if (axis_value != axis[switch_bt_axis_order_mapping[i]]) {
                axis[switch_bt_axis_order_mapping[i]] = axis_value;
                axis_changed_mask_ |= (1 << switch_bt_axis_order_mapping[i]);
                anychange = true;
            }
        }

        joystickEvent = true;

    } else if (data[0] == 0x30) {
        // Assume switch full report
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48
        // 30 E0 80 00 00 00 D9 37 79 19 98 70 00 0D 0B F1 02 F0 0A 41 FE 25 FC 89 00 F8 0A F0 02 F2 0A 41 FE D9 FB 99 00 D4 0A F6 02 FC 0A 3C FE 69 FB B8 00
        //<<(02 15 21):48 20 11 00 0D 00 71 00 A1
        //static const uint8_t switch_bt_axis_order_mapping[] = { 0, 1, 2, 3};
        axis_mask_ = 0x7fff;  // have all of the fields.
        axis_changed_mask_ = 0; // assume none for now
        // We have a data transfer.  Lets see what is new...
        uint32_t cur_buttons = data[3] | (data[4] << 8) | (data[5] << 16);

        //DBGPrintf("BUTTONS: %x\n", cur_buttons);
        if(initialPassButton_ == true) {
            if(cur_buttons == 0x8000) {
                buttonOffset_ = 0x8000;
            } else {
                buttonOffset_ = 0;
            }
            initialPassButton_ = false;
        }

        cur_buttons = cur_buttons - buttonOffset_;
        //Serial.printf("Buttons (3,4,5): %x, %x, %x, %x, %x, %x\n", buttonOffset_, cur_buttons, buttons, data[3], data[4], data[5]);

        if (cur_buttons != buttons) {
            buttons = cur_buttons;
            anychange = true;
            joystickEvent = true;
            println("  Button Change: ", buttons, HEX);
        }
        // We will put the HAT into axis 9 for now..
        /*
        if (sw1d->hat != axis[9]) {
            axis[9] = sw1d->hat;
            axis_changed_mask_ |= (1 << 9);
            anychange = true;
        }
        */

        uint16_t new_axis[14];
        //Joystick data
        new_axis[0] = data[6] | ((data[7] & 0xF) << 8);   //xl
        new_axis[1] = (data[7] >> 4) | (data[8] << 4);    //yl
        new_axis[2] = data[9] | ((data[10] & 0xF) << 8);  //xr
        new_axis[3] = (data[10] >> 4) | (data[11] << 4);  //yr

        //Kludge to get trigger buttons tripping
        if(buttons == 0x40) {   //R1
            new_axis[5] = 1;
        } else {
            new_axis[5] = 0;
        }
        if(buttons == 0x400000) {   //L1
            new_axis[4] = 1;
        } else {
            new_axis[4] = 0;
        }
        if(buttons == 0x400040) {
            new_axis[4] = 0xff;
            new_axis[5] = 0xff;
        }
        if(buttons == 0x800000) {   //ZL
            new_axis[6] = 0xff;
        } else {
            new_axis[6] = 0;
        }
        if(buttons == 0x80) {       //ZR
            new_axis[7] = 0xff;
        } else {
            new_axis[7] = 0;
        }
        if(buttons == 0x800080) {
            new_axis[6] = 0xff;
            new_axis[7] = 0xff;
        }

        sw_update_axis(8, (int16_t)(data[13]  | (data[14] << 8))); //ax
        sw_update_axis(9, (int16_t)(data[15]  | (data[16] << 8))); //ay
        sw_update_axis(10,  (int16_t)(data[17] | (data[18] << 8))); //az
        sw_update_axis(11,  (int16_t)(data[19] | (data[20] << 8)));  //gx
        sw_update_axis(12,  (int16_t)(data[21] | (data[22] << 8))); //gy
        sw_update_axis(13,  (int16_t)(data[23] | (data[24] << 8))); //gz

        sw_update_axis(14,  data[2] >> 4);  //Battery level, 8=full, 6=medium, 4=low, 2=critical, 0=empty

        //map axes
        for (uint8_t i = 0; i < 8; i++) {
            // The first two values were unsigned.
            if (new_axis[i] != axis[i]) {
                axis[i] = new_axis[i];
                axis_changed_mask_ |= (1 << i);
                anychange = true;
            }
        }

		//apply stick calibration
		float xout, yout;
		CalcAnalogStick(xout, yout, new_axis[0], new_axis[1], true);
		//Serial.printf("Correctd Left Stick: %f, %f\n", xout , yout);
		axis[0] = int(round(xout));
		axis[1] = int(round(yout));

		CalcAnalogStick(xout, yout, new_axis[2], new_axis[3], true);
		axis[2] = int(round(xout));
		axis[3] = int(round(yout));

        joystickEvent = true;
        initialPass_ = false;

    }
    return false;
}

hidclaim_t JoystickController::bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage)
{
    USBHDBGSerial.printf("JoystickController::bt_claim_collection(%p) Connection:%p class:%x Top:%x\n", this, btconnection, bluetooth_class, topusage);


    if (mydevice != NULL) return CLAIM_NO;  // claimed by some other...
    if (btconnect && (btconnect != btconnection)) return CLAIM_NO;
    // We will claim if BOOT Keyboard.

    if (topusage != 0x10004 && topusage != 0x10005 && topusage != 0x10008) return CLAIM_NO;
    // only claim from one physical device

    USBHDBGSerial.printf("\tJoystickController claim collection\n");
    btconnect = btconnection;
    btdevice = (Device_t*)btconnect->btController_; // remember this way

    // experiment?  See if we can now tell system to maybe set which report we want
    connectionComplete();
    return CLAIM_REPORT;
}

void JoystickController::bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
    hid_input_begin(topusage, type, lgmin, lgmax);
}

void JoystickController::bt_hid_input_data(uint32_t usage, int32_t value)
{
    hid_input_data(usage, value);
}

void JoystickController::bt_hid_input_end()
{
    hid_input_end();
}

void JoystickController::bt_disconnect_collection(Device_t *dev)
{
    disconnect_collection(dev);
}

bool JoystickController::mapNameToJoystickType(const uint8_t *remoteName)
{
    // Sort of a hack, but try to map the name given from remote to a type...
    if (strncmp((const char *)remoteName, "Wireless Controller", 19) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %s - set to PS4\n", remoteName);
        joystickType_ = PS4;
    } else if (strncmp((const char *)remoteName, "PLAYSTATION(R)3", 15) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3\n", (uint32_t)this, remoteName);
        joystickType_ = PS3;
    } else if (strncmp((const char *)remoteName, "Navigation Controller", 21) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3\n", (uint32_t)this, remoteName);
        joystickType_ = PS3;
    } else if (strncmp((const char *)remoteName, "Motion Controller", 17) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3 Motion\n", (uint32_t)this, remoteName);
        joystickType_ = PS3_MOTION;
    } else if (strncmp((const char *)remoteName, "Xbox Wireless", 13) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to XBOXONE\n", (uint32_t)this, remoteName);
        joystickType_ = XBOXONE;
    } else if (strncmp((const char *)remoteName, "Pro Controller", 13) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to Nintendo Pro Controller\n", (uint32_t)this, remoteName);
        joystickType_ = SWITCH;
    } else if(strncmp((const char *)remoteName, "Joy-Con (R)", 11) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to Nintendo Joy-Con (R) Controller\n", (uint32_t)this, remoteName);
        joystickType_ = SWITCH;
    } else if(strncmp((const char *)remoteName, "Joy-Con (L)", 11) == 0) {
        DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to Nintendo Joy-Con (L) Controller\n", (uint32_t)this, remoteName);
        joystickType_ = SWITCH;
    } else {
        DBGPrintf("  JoystickController::mapNameToJoystickType %s - Unknown\n", remoteName);
    }
    DBGPrintf("  Joystick Type: %d\n", joystickType_);
    return true;
}


bool JoystickController::remoteNameComplete(const uint8_t *remoteName)
{
    // Sort of a hack, but try to map the name given from remote to a type...
    if (mapNameToJoystickType(remoteName)) {
        switch (joystickType_) {
        case PS4: special_process_required = SP_NEED_CONNECT; break;
        case PS3: special_process_required = SP_PS3_IDS; break;
        case PS3_MOTION: special_process_required = SP_PS3_IDS; break;
        default:
            break;
        }
    }
    return true;
}

void JoystickController::connectionComplete()
{
    connectedComplete_pending_ = 0;

    DBGPrintf("  JoystickController::connectionComplete %x joystick type %d\n", (uint32_t)this, joystickType_);
    switch (joystickType_) {
    case PS4:
    {
        uint8_t packet[2];
        packet[0] = 0x43; // HID BT Get_report (0x40) | Report Type (Feature 0x03)
        packet[1] = 0x02; // Report ID
        DBGPrintf("Set PS4 report\n");
        delay(1);
        btdriver_->sendL2CapCommand(packet, sizeof(packet), BluetoothController::CONTROL_SCID /*0x40*/);
    }
    break;
    case PS3:
    {
        uint8_t packet[6];
        packet[0] = 0x53; // HID BT Set_report (0x50) | Report Type (Feature 0x03)
        packet[1] = 0xF4; // Report ID
        packet[2] = 0x42; // Special PS3 Controller enable commands
        packet[3] = 0x03;
        packet[4] = 0x00;
        packet[5] = 0x00;

        DBGPrintf("enable six axis\n");
        delay(1);
        btdriver_->sendL2CapCommand(packet, sizeof(packet), BluetoothController::CONTROL_SCID);
    }
    break;
    case PS3_MOTION:
        setLEDs(0, 0xff, 0);    // Maybe try setting to green?
        break;
    case SWITCH:
    {
        // See if we can set a specific report
#if 1
		uint8_t packet_[8];
        DBGPrintf("Request Device Info......\n");
		packet_[0] = 0x00;
		sw_sendCmd(0x02, packet_, 1, SW_CMD_TIMEOUT);
        connectedComplete_pending_ = 0;

		DBGPrintf("Config Complete!\n");


#endif
    }

    default:
        break;
    }
}

void JoystickController::release_bluetooth()
{
    btdevice = nullptr; // remember this way
    btdriver_ = nullptr;
    connected_ = false;
    special_process_required = false;

}


bool JoystickController::PS3Pair(uint8_t* bdaddr) {
    if (!driver_) return false;
    if (joystickType_ == PS3) {
        /* Set the internal Bluetooth address */
        txbuf_[0] = 0x01;
        txbuf_[1] = 0x00;

        for (uint8_t i = 0; i < 6; i++)
            txbuf_[i + 2] = bdaddr[5 - i]; // Copy into buffer, has to be written reversed, so it is MSB first

        // bmRequest = Host to device (0x00) | Class (0x20) | Interface (0x01) = 0x21, bRequest = Set Report (0x09), Report ID (0xF5), Report Type (Feature 0x03), interface (0x00), datalength, datalength, data
        return driver_->sendControlPacket(0x21, 9, 0x3f5, 0, 8, txbuf_);
    } else if (joystickType_ == PS3_MOTION) {
        // Slightly different than other PS3 units...
        txbuf_[0] = 0x05;
        for (uint8_t i = 0; i < 6; i++)
            txbuf_[i + 1] = bdaddr[i]; // Order different looks like LSB First?

        txbuf_[7] = 0x10;
        txbuf_[8] = 0x01;
        txbuf_[9] = 0x02;
        txbuf_[10] = 0x12;
        // bmRequest = Host to device (0x00) | Class (0x20) | Interface (0x01) = 0x21, bRequest = Set Report (0x09), Report ID (0xF5), Report Type (Feature 0x03), interface (0x00), datalength, datalength, data
        return driver_->sendControlPacket(0x21, 9, 0x305, 0, 11, txbuf_);
    }
    return false;
}

//=============================================================================
// Retrieve the current pairing information for a PS4...
//=============================================================================
bool JoystickController::PS4GetCurrentPairing(uint8_t* bdaddr) {
    if (!driver_ || (joystickType_ != PS4)) return false;
    // Try asking PS4 for information
    memset(txbuf_, 0, 0x10);
    send_Control_packet_active_ = true;
    if (!driver_->sendControlPacket(0xA1, 1, 0x312, 0, 0x10, txbuf_))
        return false;
    elapsedMillis em = 0;
    while ((em < 500) && send_Control_packet_active_) ;
    memcpy(bdaddr, &txbuf_[10], 6);
    return true;
}

bool JoystickController::PS4Pair(uint8_t* bdaddr) {
    if (!driver_ || (joystickType_ != PS4)) return false;
    // Lets try to setup a message to send...
    static const uint8_t ps4_pair_msg[] PROGMEM = {0x13, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                   0x56, 0xE8, 0x81, 0x38, 0x08, 0x06, 0x51, 0x41, 0xC0, 0x7F, 0x12, 0xAA, 0xD9, 0x66, 0x3C, 0xCE
                                                  };

    // Note the above 0xff sare place holders for the bdaddr
    memcpy(txbuf_, ps4_pair_msg, sizeof(ps4_pair_msg));
    for (uint8_t i = 0; i < 6; i++)
        txbuf_[i + 1] = bdaddr[i];

    send_Control_packet_active_ = true;
    return driver_->sendControlPacket(0x21, 0x09, 0x0313, 0, sizeof(ps4_pair_msg), txbuf_);
}

//Nintendo Switch functions
void JoystickController::sw_sendCmd(uint8_t cmd, uint8_t *data, uint16_t size, uint32_t timeout) {
	struct SWProBTSendConfigData *packet =  (struct SWProBTSendConfigData *)txbuf_ ;
	memset((void*)packet, 0, sizeof(struct SWProBTSendConfigData));
	packet->hid_hdr = 0xA2; // HID BT Get_report (0xA0) | Report Type (Output)
	packet->id = 1;
	packet->gpnum = switch_packet_num;
	switch_packet_num = (switch_packet_num + 1) & 0x0f;
	// 2-9 rumble data;
	packet->rumbleDataL[0] = 0x00;
	packet->rumbleDataL[1] = 0x01;
	packet->rumbleDataL[2] = 0x40;
	packet->rumbleDataL[3] = 0x40;
	packet->rumbleDataR[0] = 0x00;
	packet->rumbleDataR[1] = 0x01;
	packet->rumbleDataR[2] = 0x40;
	packet->rumbleDataR[3] = 0x40;

	packet->subCommand = cmd; // Report ID
	for(uint16_t i = 0; i < size; i++) {
		packet->subCommandData[i] = data[i];
	}
	if (btdriver_) {
        if (timeout != 0) {
            btconnect->startTimer(timeout);
        }
        btdriver_->sendL2CapCommand((uint8_t *)packet, sizeof(struct SWProBTSendConfigData), BluetoothController::INTERRUPT_SCID /*0x40*/);
    }
    else Serial.printf("\n####### sw_sendCMD(%x %p %u) called with btdriver_ == 0 ", cmd, data, size);
    em_sw_ = 0;
}

void JoystickController::sw_sendCmdUSB(uint8_t cmd, uint32_t timeout) {
    DBGPrintf("sw_sendCmdUSB: cmd:%x, timeout:%x\n",  cmd, timeout);
	//sub-command
    txbuf_[0] = 0x80;
	txbuf_[1] = cmd;
    sw_last_cmd_sent_ = cmd; // remember which command we sent
	if(driver_) {
        if (timeout != 0) {
            driver_->startTimer(timeout);
        }
		driver_->sendPacket(txbuf_, 2);
        em_sw_ = 0;
	} else {
		if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 18, this, __LINE__)) {
			println("switch transfer fail");
		}
	}
}

void JoystickController::sw_sendSubCmdUSB(uint8_t sub_cmd, uint8_t *data, uint8_t size, uint32_t timeout) {
        DBGPrintf("sw_sendSubCmdUSB(%x, %p, %u): ",  sub_cmd, size);
        for (uint8_t i = 0; i < size; i++) DBGPrintf(" %02x", data[i]);
        DBGPrintf("\n");
        memset(txbuf_, 0, 32);  // make sure it is cleared out

		txbuf_[0] = 0x01;
        // Now add in subcommand data:
        // Probably do this better soon
        txbuf_[ 1] = switch_packet_num = (switch_packet_num + 1) & 0x0f; //

        txbuf_[ 2] = 0x00;
        txbuf_[ 3] = 0x01;
        txbuf_[ 4] = 0x40;
        txbuf_[ 5] = 0x40;
        txbuf_[ 6] = 0x00;
        txbuf_[ 7] = 0x01;
        txbuf_[ 8] = 0x40;
        txbuf_[ 9] = 0x40;

		txbuf_[ 10] = sub_cmd;

		//sub-command
		for(uint16_t i = 0; i < size; i++) {
			txbuf_[i + 11] = data[i];
		}

		println("USB send sub cmd: driver? ", (uint32_t)driver_, HEX);
		print_hexbytes((uint8_t*)txbuf_, 32);

		if(driver_) {
			driver_->sendPacket(txbuf_, 32);
            if (timeout != 0) {
                driver_->startTimer(timeout);
            }
		} else if (txpipe_) {
			if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, 32, this, __LINE__)) {
				println("switch transfer fail");
			}
		}
        em_sw_ = 0;
		if (!timeout) delay(100);
}

void JoystickController::sw_parseAckMsg(const uint8_t *buf_)
{
	int16_t data[6];
	uint8_t offset = 20;
	uint8_t icount = 0;
	//uint8_t packet_[8];

	if((buf_[14] == 0x10 && buf_[15] == 0x20 && buf_[16] == 0x60)) {
		//parse IMU calibration
		DBGPrintf("===>  IMU Calibration \n");
		for(uint8_t i = 0; i < 3; i++) {
			SWIMUCal.acc_offset[i] = (int16_t)(buf_[icount+offset] | (buf_[icount+offset+1] << 8));
			SWIMUCal.acc_sensitivity[i] = (int16_t)(buf_[icount+offset+6] | (buf_[icount+offset+1+6] << 8));
			SWIMUCal.gyro_offset[i] = (int16_t)(buf_[icount+offset+12] | (buf_[icount+offset+1+12] << 8));
			SWIMUCal.gyro_sensitivity[i] = (int16_t)(buf_[icount+offset+18] | (buf_[icount+offset+1+18] << 8));
			icount = i * 2;
		}
		for(uint8_t i = 0; i < 3; i++) {
			DBGPrintf("\t %d, %d, %d, %d\n", SWIMUCal.acc_offset[i], SWIMUCal.acc_sensitivity[i],
				SWIMUCal.gyro_offset[i], SWIMUCal.gyro_sensitivity[i]);
		}
	} else if((buf_[14] == 0x10 && buf_[15] == 0x80 && buf_[16] == 0x60)) {
		//parse IMU calibration
		DBGPrintf("===>  IMU Calibration Offsets \n");
		for(uint8_t i = 0; i < 3; i++) {
			SWIMUCal.acc_offset[i] = (int16_t)(buf_[i+offset] | (buf_[i+offset+1] << 8));
		}
		for(uint8_t i = 0; i < 3; i++) {
			DBGPrintf("\t %d\n", SWIMUCal.acc_offset[i]);
		}
	} else if((buf_[14] == 0x10 && buf_[15] == 0x3D && buf_[16] == 0x60)){		//left stick
		offset = 20;
		data[0] = ((buf_[1+offset] << 8) & 0xF00) | buf_[0+offset];
		data[1] = (buf_[2+offset] << 4) | (buf_[1+offset] >> 4);
		data[2] = ((buf_[4+offset] << 8) & 0xF00) | buf_[3+offset];
		data[3] = (buf_[5+offset] << 4) | (buf_[4+offset] >> 4);
		data[4] = ((buf_[7+offset] << 8) & 0xF00) | buf_[6+offset];
		data[5] = (buf_[8+offset] << 4) | (buf_[7+offset] >> 4);

		SWStickCal.lstick_center_x = data[2];
		SWStickCal.lstick_center_y = data[3];
		SWStickCal.lstick_x_min = SWStickCal.lstick_center_x - data[0];
		SWStickCal.lstick_x_max = SWStickCal.lstick_center_x + data[4];
		SWStickCal.lstick_y_min = SWStickCal.lstick_center_y - data[1];
		SWStickCal.lstick_y_max = SWStickCal.lstick_center_y + data[5];

		DBGPrintf("Left Stick Calibrataion\n");
		DBGPrintf("center: %d, %d\n", SWStickCal.lstick_center_x, SWStickCal.lstick_center_y );
		DBGPrintf("min/max x: %d, %d\n", SWStickCal.lstick_x_min, SWStickCal.lstick_x_max);
		DBGPrintf("min/max y: %d, %d\n", SWStickCal.lstick_y_min, SWStickCal.lstick_y_max);

		//right stick
		offset = 29;
		data[0] = ((buf_[1+offset] << 8) & 0xF00) | buf_[0+offset];
		data[1] = (buf_[2+offset] << 4) | (buf_[1+offset] >> 4);
		data[2] = ((buf_[4+offset] << 8) & 0xF00) | buf_[3+offset];
		data[3] = (buf_[5+offset] << 4) | (buf_[4+offset] >> 4);
		data[4] = ((buf_[7+offset] << 8) & 0xF00) | buf_[6+offset];
		data[5] = (buf_[8+offset] << 4) | (buf_[7+offset] >> 4);

		SWStickCal.rstick_center_x = data[0];
		SWStickCal.rstick_center_y = data[1];
		SWStickCal.rstick_x_min = SWStickCal.rstick_center_x - data[2];
		SWStickCal.rstick_x_max = SWStickCal.rstick_center_x + data[4];
		SWStickCal.rstick_y_min = SWStickCal.rstick_center_y - data[3];
		SWStickCal.rstick_y_max = SWStickCal.rstick_center_y + data[5];

		DBGPrintf("\nRight Stick Calibrataion\n");
		DBGPrintf("center: %d, %d\n", SWStickCal.rstick_center_x, SWStickCal.rstick_center_y );
		DBGPrintf("min/max x: %d, %d\n", SWStickCal.rstick_x_min, SWStickCal.rstick_x_max);
		DBGPrintf("min/max y: %d, %d\n", SWStickCal.rstick_y_min, SWStickCal.rstick_y_max);
	}  else if((buf_[14] == 0x10 && buf_[15] == 0x86 && buf_[16] == 0x60)){			//left stick deadzone_left
		offset = 20;
		SWStickCal.deadzone_left = (((buf_[4 + offset] << 8) & 0xF00) | buf_[3 + offset]);
		DBGPrintf("\nLeft Stick Deadzone\n");
		DBGPrintf("deadzone: %d\n", SWStickCal.deadzone_left);
	}   else if((buf_[14] == 0x10 && buf_[15] == 0x98 && buf_[16] == 0x60)){			//left stick deadzone_left
		offset = 20;
		SWStickCal.deadzone_left = (((buf_[4 + offset] << 8) & 0xF00) | buf_[3 + offset]);
		DBGPrintf("\nRight Stick Deadzone\n");
		DBGPrintf("deadzone: %d\n", SWStickCal.deadzone_right);
	} else if((buf_[14] == 0x10 && buf_[15] == 0x10 && buf_[16] == 0x80)){
		DBGPrintf("\nUser Calibration Rcvd!\n");
	}

}

bool JoystickController::sw_getIMUCalValues(float *accel, float *gyro)
{
    // Fail if we don't have actually have those fields. We need axis 8-13 for this
    if ((axis_mask_ & 0x3f00) != 0x3f00) return false;
	for(uint8_t i = 0; i < 3; i++) {
		accel[i] = (float)(axis[8+i] - SWIMUCal.acc_offset[i]) * (1.0f / (float)SWIMUCal.acc_sensitivity[i]) * 4.0f;
		gyro[i]  = (float)(axis[11+i] - SWIMUCal.gyro_offset[i]) * (816.0f / (float)SWIMUCal.gyro_sensitivity[i]);
	}
    return true;
}


#define sw_scale 2048
void JoystickController::CalcAnalogStick
(
	float &pOutX,       // out: resulting stick X value
	float &pOutY,       // out: resulting stick Y value
	int16_t x,         // in: initial stick X value
	int16_t y,         // in: initial stick Y value
	bool isLeft			// are we dealing with left or right Joystick
)
{
//		uint16_t x_calc[3],      // calc -X, CenterX, +X
//		uint16_t y_calc[3]       // calc -Y, CenterY, +Y

	int16_t min_x;
	int16_t max_x;
	int16_t center_x;
	int16_t min_y;		// analog joystick calibration
	int16_t max_y;
	int16_t center_y;
	if(isLeft) {
		min_x = SWStickCal.lstick_x_min;
		max_x = SWStickCal.lstick_x_max;
		center_x = SWStickCal.lstick_center_x;
		min_y = SWStickCal.lstick_y_min;
		max_y = SWStickCal.lstick_y_max;
		center_y = SWStickCal.lstick_center_y;
	} else {
		min_x = SWStickCal.rstick_x_min;
		max_x = SWStickCal.rstick_x_max;
		center_x = SWStickCal.rstick_center_x;
		min_y = SWStickCal.rstick_y_min;
		max_y = SWStickCal.rstick_y_max;
		center_y = SWStickCal.rstick_center_y;
	}


	float x_f, y_f;
	// Apply Joy-Con center deadzone. 0xAE translates approx to 15%. Pro controller has a 10% () deadzone
	float deadZoneCenter = 0.15f;
	// Add a small ammount of outer deadzone to avoid edge cases or machine variety.
	float deadZoneOuter = 0.0f;

	// convert to float based on calibration and valid ranges per +/-axis
	x = clamp(x, min_x, max_x);
	y = clamp(y, min_y, max_y);
	if (x >= center_x) {
		x_f = (float)(x - center_x) / (float)(max_x - center_x);
	} else {
		x_f = -((float)(x - center_x) / (float)(min_x - center_x));
	}
	if (y >= center_y) {
		y_f = (float)(y - center_y) / (float)(max_y - center_y);
	} else {
		y_f = -((float)(y - center_y) / (float)(min_y - center_y));
	}

	// Interpolate zone between deadzones
	float mag = sqrtf(x_f*x_f + y_f*y_f);
	if (mag > deadZoneCenter) {
		// scale such that output magnitude is in the range [0.0f, 1.0f]
		float legalRange = 1.0f - deadZoneOuter - deadZoneCenter;
		float normalizedMag = min(1.0f, (mag - deadZoneCenter) / legalRange);
		float scale = normalizedMag / mag;
		pOutX = (x_f * scale * sw_scale);
		pOutY = (y_f * scale * sw_scale);
	} else {
		// stick is in the inner dead zone
		pOutX = 0.0f;
		pOutY = 0.0f;
	}


}
