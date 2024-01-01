#include <Arduino.h>
#include <algorithm>
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_ADC.h"
//#include "OC_options.h"

// custom pins only for Teensy 4.1
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

// default settings for traditional O_C hardware wired for Teensy 3.2
uint8_t CV1=19, CV2=18, CV3=20, CV4=17;
uint8_t TR1=0, TR2=1, TR3=2, TR4=3;
uint8_t OLED_DC=6, OLED_RST=7, OLED_CS=8;
uint8_t DAC_CS=10, DAC_RST=9;
uint8_t encL1=22, encL2=21, butL=23, encR1=16, encR2=15, butR=14;
uint8_t but_top=5, but_bot=4, but_mid=255, but_top2=255, but_bot2=255;
uint8_t OC_GPIO_DEBUG_PIN1=24, OC_GPIO_DEBUG_PIN2=25;
bool ADC33131D_Uses_FlexIO=false;
bool OLED_Uses_SPI1=false;
bool DAC8568_Uses_SPI=false;
bool I2S2_Audio_ADC=false;
bool I2S2_Audio_DAC=false;
bool I2C_Expansion=false;
bool MIDI_Uses_Serial8=false;

FLASHMEM
void OC::Pinout_Detect() {
  float id_voltage = OC::ADC::Read_ID_Voltage();
  Serial.printf("ID voltage (pin A17) = %.3f\n", id_voltage);

  if (id_voltage >= 0.05f && id_voltage < 0.15f) {
    CV1 = 255;
    CV2 = 255;               // CV inputs with ADC33131D
    CV3 = 255;
    CV4 = 255;
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

}

#endif // __IMXRT1062__ && ARDUINO_TEENSY41
