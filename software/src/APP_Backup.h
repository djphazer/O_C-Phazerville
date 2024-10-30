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

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include <stdint.h>
#include "OC_config.h"
#include "OC_apps.h"
#include "OC_ui.h"
#include "src/drivers/display.h"
#include "HSMIDI.h"

class Backup: public SystemExclusiveHandler {
public:
    void Init() {
        Resume();
    }
    
    void Resume() {
        receiving = 0;
        packet = 0;
    }

    void Controller() {
        if (receiving) ListenForSysEx();
    }
    
    void View() {
        DrawInterface();
    }
    
    void ToggleReceiveMode() {
        receiving = 1 - receiving;
        packet = 0;
    }
    
    void ToggleCalibration() {
        if (!receiving) {
            calibration = 1 - calibration;
            packet = 0;
        }
    }

    void OnSendSysEx() {
        if (!receiving) {
            packet = 0;
            uint8_t V[33];
            
            byte start = calibration ? 0 : (EEPROM_CALIBRATIONDATA_END / 32);
            byte end = calibration ?  (EEPROM_CALIBRATIONDATA_END / 32) : 64;
            for (byte p = start; p < end; p++)
            {
                uint16_t address = p * 32;
                uint8_t ix = 0;
                V[ix++] = p; // Packet number
                packet = p;
                
                // Wrap into 32-byte packets
                for (byte b = 0; b < 32; b++) V[ix++] = EEPROM.read(address++);
                
                UnpackedData unpacked;
                unpacked.set_data(ix, V);
                PackedData packed = unpacked.pack();
                SendSysEx(packed, 'B');
            }
        }
    }
    
    void OnReceiveSysEx() {
        uint8_t V[33];
        if (ExtractSysExData(V, 'B')) {
            uint8_t ix = 0;
            uint8_t p = V[ix++]; // Get packet number
            packet = p;
            uint16_t address = p * 32;
            for (byte b = 0; b < 32; b++) EEPROM.write(address++, V[ix++]);

            // Reset on last packet
            if (p == ((EEPROM_CALIBRATIONDATA_END / 32) - 1) || p == 63) {
                receiving = 0;
                OC::apps::Init(0);
            }
        }
    }
        
private:
    bool calibration = 0;
    bool receiving = 0;
    uint8_t packet = 0;
    
    void DrawInterface() {
        graphics.drawLine(0, 10, 127, 10);
        graphics.drawLine(0, 12, 127, 12);
        graphics.setPrintPos(0, 1);
        graphics.print("Backup / Restore");
        
        graphics.setPrintPos(0, 15);
        if (receiving) {
            if (packet > 0) {
                graphics.print("Receiving...");

                // Progress bar
                graphics.drawRect(0, 33, (packet + 4) * 2, 8);
            }
            else graphics.print("Listening...");
        } else {
            if (packet > 0) graphics.print("Done!");
            else graphics.print("Restore or Backup?");
        }
        
        graphics.setPrintPos(0, 55);
        if (receiving) graphics.print("[CANCEL]");
        else {
            graphics.print("[RESTORE]");
            graphics.setPrintPos(78, 55);
            graphics.print("[BACKUP]");
            graphics.setPrintPos(6, 35);
            graphics.print("Backup: ");
            graphics.print(calibration ? "Calibration" : "Data");
        }
    }
    
};

Backup Backup_instance;

void Backup_init() {}
void Backup_menu() {Backup_instance.View();}
void Backup_isr() {Backup_instance.Controller();}

// Storage not used for this app
static constexpr size_t Backup_storageSize() {return 0;}
static size_t Backup_save(void *storage) {return 0;}
static size_t Backup_restore(const void *storage) {return 0;}

void Backup_handleAppEvent(OC::AppEvent event) {
    if (event == OC::APP_EVENT_RESUME) Backup_instance.Resume();
}
void Backup_loop() {} // Deprecated
void Backup_screensaver() {Backup_instance.View();}
void Backup_handleEncoderEvent(const UI::Event &event) {
    Backup_instance.ToggleCalibration();
}
void Backup_handleButtonEvent(const UI::Event &event) {
    if (event.type == UI::EVENT_BUTTON_PRESS) {
        if (event.control == OC::CONTROL_BUTTON_L) Backup_instance.ToggleReceiveMode();
        if (event.control == OC::CONTROL_BUTTON_R) Backup_instance.OnSendSysEx();
    }
}
