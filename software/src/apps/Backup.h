// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

OC_APP_CLASS(AppBackup, TWOCCS("BU"), "Back It Up!", "Backup / Restore"),
  public SystemExclusiveHandler {
public:
  OC_APP_INTERFACE_DECLARE(AppBackup, 0);

  void Resume() {
    receiving = 0;
    packet = 0;
  }

  void Controller() {
    if (receiving) ListenForSysEx();
  }

  void View() const {
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

      uint8_t start = calibration ? 0 : (EEPROM_CALIBRATIONDATA_END / 32);
      uint8_t end = calibration ? (EEPROM_CALIBRATIONDATA_END / 32)
                             : (EEPROMStorage::LENGTH / 32);
      for (uint8_t p = start; p < end; p++) {
        uint16_t address = p * 32;
        uint8_t ix = 0;
        V[ix++] = p; // Packet number
        packet = p;

        // Wrap into 32-byte packets
        for (int b = 0; b < 32; b++) V[ix++] = EEPROM.read(address++);

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
      for (int b = 0; b < 32; b++) EEPROM.write(address++, V[ix++]);

      // Reset on last packet
      if (p == ((EEPROM_CALIBRATIONDATA_END / 32) - 1)
          || p == ((EEPROMStorage::LENGTH / 32) - 1)) {
        receiving = 0;
        OC::app_switcher.Init(0);
      }
    }
  }

private:
  bool calibration = 0;
  bool receiving = 0;
  uint8_t packet = 0;

  void DrawInterface() const {
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
      } else graphics.print("Listening...");
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

FLASHMEM
void AppBackup::Init() {
    Resume();
}
FLASHMEM
void AppBackup::DrawMenu() const {
    View();
}
void AppBackup::Process(OC::IOFrame* ioframe) {
    Controller();
}
void AppBackup::GetIOConfig(OC::IOConfig &ioconfig) const {
}
void AppBackup::DrawDebugInfo() const {
}

// Storage not used for this app
size_t AppBackup::SaveAppData(util::StreamBufferWriter &) const { return 0; }
size_t AppBackup::RestoreAppData(util::StreamBufferReader &) { return 0; }

FLASHMEM
void AppBackup::HandleAppEvent(OC::AppEvent event) {
  if (event == OC::APP_EVENT_RESUME) Resume();
}
void AppBackup::Loop() {} // Deprecated
FLASHMEM
void AppBackup::DrawScreensaver() const {
  View();
}
FLASHMEM
void AppBackup::HandleEncoderEvent(const UI::Event& event) {
  ToggleCalibration();
}
FLASHMEM
void AppBackup::HandleButtonEvent(const UI::Event& event) {
  if (event.type == UI::EVENT_BUTTON_PRESS) {
    if (event.control == OC::CONTROL_BUTTON_L) ToggleReceiveMode();
    if (event.control == OC::CONTROL_BUTTON_R) OnSendSysEx();
  }
}
