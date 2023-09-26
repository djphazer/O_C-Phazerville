// Copyright (c) 2018, Jason Justian
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

#include "HSApplication.h"
#include "OC_strings.h"

// Bitmap representation of QR code for access to http://www.beigemaze.com/hs, which
// redirects to Hemisphere Suite documentation.
//
// And no, the QR code doesn't seem to work. I'm sure that the pixels are too small,
// and that the dark spots are too illuminated by adjacent pixels, or whatever.
// But I'm leaving this in because with the right phone and the right display, who
// knows?
// Ah, the heck with it. Commenting it out.
/*
const uint32_t QR[25] = {
        0x1fdeb7f,
        0x1042d41,
        0x174455d,
        0x174f75d,
        0x174ad5d,
        0x105c441,
        0x1fd557f,
        0x8500,
        0x1f6536a,
        0x9cb1b8,
        0x9356cb,
        0x13b29a0,
        0x131cb6d,
        0x1757138,
        0x1d94d5c,
        0x92d5a6,
        0x9f6ef7,
        0x314f00,
        0xb5147f,
        0x1f1ff41,
        0x1bf545d,
        0x19ee55d,
        0x177105d,
        0x12c7741,
        0x1e4dc7f
};
*/

class Settings : public HSApplication {
public:
	void Start() {
	}
	
	void Resume() {
	}

    void Controller() {
    }

    void View() {
        gfxHeader("Setup / About");
        gfxIcon(0, 15, ZAP_ICON);
        gfxIcon(120, 15, ZAP_ICON);
        #ifdef PEWPEWPEW
        gfxPrint(21, 15, "PEW! PEW! PEW!");
        #else
        gfxPrint(12, 15, "Phazerville Suite");
        #endif
        gfxPrint(0, 25, OC::Strings::VERSION);
        gfxPrint(0, 35, "github.com/djphazer");
        gfxPrint(0, 55, "[CALIBRATE]   [RESET]");

        //DrawQRAt(103, 15);
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void Calibration() {
        OC::ui.Calibrate();
    }

    void FactoryReset() {
        OC::apps::Init(1);
    }

private:
/*
    void DrawQRAt(byte x, byte y) {
        for (byte c = 0; c < 25; c++) // Column
        {
            uint32_t col = QR[c];
            for (byte b = 0; b < 25; b++) // Bit
            {
                if (col & (1 << b)) gfxPixel(x + c, y + b);
            }
        }
    }
*/

};

Settings Settings_instance;

// App stubs
void Settings_init() {
    Settings_instance.BaseStart();
}

// Not using O_C Storage
size_t Settings_storageSize() {return 0;}
size_t Settings_save(void *storage) {return 0;}
size_t Settings_restore(const void *storage) {return 0;}

void Settings_isr() {
// skip the Controller to avoid I/O conflict with Calibration
  return;
}

void Settings_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        Settings_instance.Resume();
    }
}

void Settings_loop() {} // Deprecated

void Settings_menu() {
    Settings_instance.BaseView();
}

void Settings_screensaver() {} // Deprecated

void Settings_handleButtonEvent(const UI::Event &event) {
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_PRESS) Settings_instance.Calibration();
    }

    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) Settings_instance.FactoryReset();
}

void Settings_handleEncoderEvent(const UI::Event &event) {
    (void)event;
    /*
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) Settings_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) Settings_instance.OnRightEncoderMove(event.value);
    */
}
