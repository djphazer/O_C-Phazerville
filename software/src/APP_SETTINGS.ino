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

class Settings : public HSApplication {
public:
	void Start() {
	}
	
	void Resume() {
	}

    void Controller() {
    #ifdef PEWPEWPEW
        HS::frame.Load();PewPewTime.PEWPEW(Clock(3)<<1|Clock(0));}
        struct{bool go=0;int idx=0;struct{uint8_t x,y;int x_v,y_v;}pewpews[8];
        void PEWPEW(uint8_t mask){uint32_t t=OC::CORE::ticks;for(int i=0;i<8;++i){auto &p=pewpews[i];
          if(mask>>i&0x01){auto &pp=pewpews[idx++];pp.x=0+120*i;pp.y=55;pp.x_v=(6+random(3))*(i?-1:1);pp.y_v=-9;idx%=8;}
          if(t%500==0){p.x+=p.x_v;p.y+=p.y_v;if(p.y>=55&&p.y_v>0)p.y_v=-p.y_v;else ++p.y_v;}
          if(t%10000==0){p.x_v=p.x_v*100/101;p.y_v=p.y_v*10/11;}}}}PewPewTime;
        void PEWPEW(){for(int i=0;i<8;++i){auto &p=PewPewTime.pewpews[i];gfxIcon(p.x%128,p.y%64,ZAP_ICON);}
    #endif
    }

    void View() {
        gfxHeader("Setup / About");

        #if defined(ARDUINO_TEENSY40)
        gfxPrint(100, 0, "T4.0");
        //gfxPrint(0, 45, "E2END="); gfxPrint(E2END);
        #elif defined(ARDUINO_TEENSY41)
        gfxPrint(100, 0, "T4.1");
        #else
        gfxPrint(100, 0, "T3.2");
        #endif

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

};

Settings Settings_instance;

// App stubs
void Settings_init() {
    Settings_instance.BaseStart();
}

// Not using O_C Storage
constexpr size_t Settings_storageSize() {return 0;}
size_t Settings_save(void *storage) {return 0;}
size_t Settings_restore(const void *storage) {return 0;}

void Settings_isr() {
#ifdef PEWPEWPEW
  Settings_instance.Controller();
#endif
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

void Settings_screensaver() {
#ifdef PEWPEWPEW
    Settings_instance.PEWPEW();
#endif
}

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
