#include <Arduino.h>
#include <algorithm>
#include "HSUtils.h"
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_ADC.h"

// default settings for traditional O_C hardware wired for Teensy 3.2
//uint8_t CV1=19, CV2=18, CV3=20, CV4=17;
uint8_t TR1=0, TR2=1, TR3=2, TR4=3;
uint8_t OLED_DC=6, OLED_RST=7, OLED_CS=8;
uint8_t DAC_CS=10, DAC_RST=9;
uint8_t encL1=22, encL2=21, butL=23, encR1=16, encR2=15, butR=14;
uint8_t but_top=5, but_bot=4, but_mid=9, but_top2=255, but_bot2=255;
uint8_t OC_GPIO_DEBUG_PIN1=24, OC_GPIO_DEBUG_PIN2=25;
bool ADC33131D_Uses_FlexIO=false;
bool OLED_Uses_SPI1=false;
bool DAC8568_Uses_SPI=false;
#if !defined(NORTHERNLIGHT) && defined(ARDUINO_TEENSY41)
bool NorthernLightModular=false;
#endif
bool I2S2_Audio_ADC=false;
bool I2S2_Audio_DAC=false;
bool I2C_Expansion=false;
bool MIDI_Uses_Serial8=false;
float id_voltage = 0.0;
bool flip_mode = false;
bool SDcard_Ready = false;

FLASHMEM
void OC::SetFlipMode(bool flip_180) {
  flip_mode = flip_180;
  if (flip_180) {
    // reversed
    if (id_voltage >= 0.05f) {
      // new hardware
      but_top  = 15;
      but_top2 = 14;
      but_bot  = 28;
      but_bot2 = 29;

      encR1 = 30;
      encR2 = 31;
      butR  = 24;
      encL1 = 36;
      encL2 = 37;
      butL  = 25;

      TR4 = 0;
      TR3 = 1;
      TR2 = 23;
      TR1 = 22;
    } else {
      // old hardware
#ifdef NLM_hOC
      but_top = 5;
      but_bot = 4;
#else
      but_top = 4;
      but_bot = 5;
#endif

      encR2 = 22;
      encR1 = 21;
      butR  = 23;
      encL2 = 16;
      encL1 = 15;
      butL  = 14;

      //CV4 = 19;
      //CV3 = 18;
      //CV2 = 20;
      //CV1 = 17;
      TR4 = 0;
      TR3 = 1;
      TR2 = 2;
      TR1 = 3;
    }
  } else {
    // default orientation
    if (id_voltage >= 0.05f) {
      // new hardware
      but_top  = 29;
      but_top2 = 28;
      but_bot  = 14;
      but_bot2 = 15;

      encL1 = 30;
      encL2 = 31;
      butL  = 24;
      encR1 = 36;
      encR2 = 37;
      butR  = 25;

      TR1 = 0;
      TR2 = 1;
      TR3 = 23;
      TR4 = 22;
    } else {
      // old hardware
#ifdef NLM_hOC
      but_top = 4;
      but_bot = 5;
#else
      but_top = 5;
      but_bot = 4;
#endif

      encL1 = 22;
      encL2 = 21;
      butL  = 23;
      encR1 = 16;
      encR2 = 15;
      butR  = 14;

      //CV1 = 19;
      //CV2 = 18;
      //CV3 = 20;
      //CV4 = 17;
      TR1 = 0;
      TR2 = 1;
      TR3 = 2;
      TR4 = 3;
    }
  }
}

#if defined(ARDUINO_TEENSY41)
FLASHMEM
void OC::Pinout_Detect() {
  id_voltage = OC::ADC::Read_ID_Voltage();
  const int f = int(floor(id_voltage * 1000));
  const int value = f / 1000;
  const int cents = f % 1000;
  Serial.printf("ID voltage (pin A17) = %1d.%03d\n", value, cents);

  if (id_voltage >= 0.05f) { // && id_voltage < 0.15f) {
    //CV1 = 255;
    //CV2 = 255;               // CV inputs with ADC33131D
    //CV3 = 255;
    //CV4 = 255;
    TR1 = 0;
    TR2 = 1;
    TR3 = 23;
    TR4 = 22;
    but_top = 29;
    but_top2 = 28;
    but_bot = 14;
    but_bot2 = 15;
    but_mid = 20;
    OLED_Uses_SPI1 = true;   // pins 26=MOSI, 27=SCK
    OLED_DC = 39;
    OLED_RST = 38;
    OLED_CS = 40;
    DAC8568_Uses_SPI = true; // pins 10=CS, 11=MOSI, 13=SCK
    DAC_CS = 255;            // pin 10 controlled by SPI hardware
    DAC_RST = 255;
    encR1 = 36;
    encR2 = 37;
    butR  = 25;
    encL1 = 30;
    encL2 = 31;
    butL  = 24;
    OC_GPIO_DEBUG_PIN1 = 16;
    OC_GPIO_DEBUG_PIN2 = 17;
    ADC33131D_Uses_FlexIO = true; // pins 7=A0, 6=A1, 8=SCK, 9=CS, 12=DATA, 32=A2
    I2S2_Audio_ADC = true;        // pins 3=LRCLK, 4=BCLK, 5=DATA, 33=MCLK
    I2S2_Audio_DAC = true;        // pins 2=DATA, 3=LRCLK, 4=BCLK, 33=MCLK
    I2C_Expansion = true;         // pins 18=SDA, 19=SCL
    MIDI_Uses_Serial8 = true;     // pins 34=IN, 35=OUT
  }

  NorthernLightModular = NorthernLightModular || (id_voltage >= 0.45f);
  if (NorthernLightModular) {
    DAC::kOctaveZero = 0;
    HS::octave_max = 10;
  }
}
#endif
