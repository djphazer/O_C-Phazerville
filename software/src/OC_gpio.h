#ifndef OC_GPIO_H_
#define OC_GPIO_H_

#include "OC_options.h"

// Teensy 3.2 or Teensy 4.0 have fixed pinout
//
#if defined(__MK20DX256__) || (defined(__IMXRT1062__) && defined(ARDUINO_TEENSY40))

#ifdef FLIP_180
  #define CV4 19
  #define CV3 18
  #define CV2 20
  #define CV1 17
  
  #define TR4 0
  #define TR3 1
  #define TR2 2
  #define TR1 3
  
  #define but_top 4
  #define but_bot 5
#else
  #define CV1 19
  #define CV2 18
  #define CV3 20
  #define CV4 17
  
  #define TR1 0
  #define TR2 1
  #define TR3 2
  #define TR4 3
  
  #define but_top 5
  #define but_bot 4
#endif

#define OLED_DC 6
#define OLED_RST 7
#define OLED_CS 8

#define DAC_CS 10

#ifdef VOR
  #define but_mid 9
#else
  #define DAC_RST 9
#endif

// NOTE: encoder pins R1/R2 changed for rev >= 2c
#ifdef FLIP_180
  #define encL1 16
  #define encL2 15
  #define butL  14
  
  #define encR1 22
  #define encR2 21
  #define butR  23
#else
  #define encR1 16
  #define encR2 15
  #define butR  14
  
  #define encL1 22
  #define encL2 21
  #define butL  23
#endif

// NOTE: back side :(
#define OC_GPIO_DEBUG_PIN1 24
#define OC_GPIO_DEBUG_PIN2 25

#endif // Teensy 3.2 or Teensy 4.0

// Teensy 4.1 has different pinouts depending on voltage at pin 41/A17
//
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

extern uint8_t CV1, CV2, CV3, CV4;
extern uint8_t TR1, TR2, TR3, TR4;
extern uint8_t OLED_DC, OLED_RST, OLED_CS;
extern uint8_t DAC_CS, DAC_RST;
extern uint8_t encL1, encL2, butL, encR1, encR2, butR;
extern uint8_t but_top, but_bot, but_mid, but_top2, but_bot2;
extern uint8_t OC_GPIO_DEBUG_PIN1, OC_GPIO_DEBUG_PIN2;
extern bool ADC33131D_Uses_FlexIO;
extern bool OLED_Uses_SPI1;
extern bool DAC8568_Uses_SPI;
extern bool I2S2_Audio_ADC;
extern bool I2S2_Audio_DAC;
extern bool I2C_Expansion;
extern bool MIDI_Uses_Serial8;

#endif

// OLED CS is active low
#define OLED_CS_ACTIVE LOW
#define OLED_CS_INACTIVE HIGH

#define OC_GPIO_BUTTON_PINMODE INPUT_PULLUP
#define OC_GPIO_TRx_PINMODE INPUT_PULLUP
#define OC_GPIO_ENC_PINMODE INPUT_PULLUP

/* local copy of pinMode (cf. cores/pins_teensy.c), using faster slew rate */

namespace OC { 
  
void inline pinMode(uint8_t pin, uint8_t mode) {
  
#if defined(__MK20DX256__)
    volatile uint32_t *config;
  
    if (pin >= CORE_NUM_DIGITAL) return;
    config = portConfigRegister(pin);
  
    if (mode == OUTPUT || mode == OUTPUT_OPENDRAIN) {
  #ifdef KINETISK
      *portModeRegister(pin) = 1;
  #else
      *portModeRegister(pin) |= digitalPinToBitMask(pin); // TODO: atomic
  #endif
      /* use fast slew rate for output */
      *config = PORT_PCR_DSE | PORT_PCR_MUX(1);
      if (mode == OUTPUT_OPENDRAIN) {
          *config |= PORT_PCR_ODE;
      } else {
          *config &= ~PORT_PCR_ODE;
                  }
    } else {
  #ifdef KINETISK
      *portModeRegister(pin) = 0;
  #else
      *portModeRegister(pin) &= ~digitalPinToBitMask(pin);
  #endif
      if (mode == INPUT) {
        *config = PORT_PCR_MUX(1);
      } else if (mode == INPUT_PULLUP) {
        *config = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;
      } else if (mode == INPUT_PULLDOWN) {
        *config = PORT_PCR_MUX(1) | PORT_PCR_PE;
      } else { // INPUT_DISABLE
        *config = 0;
      }
    }
#else
    ::pinMode(pin, mode); // for Teensy 4.x, just use normal pinMode
#endif
  }

void Pinout_Detect();

}

#endif // OC_GPIO_H_
