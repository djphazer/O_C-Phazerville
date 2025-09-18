#ifndef OC_GPIO_H_
#define OC_GPIO_H_

#include "OC_options.h"

// All platforms now have dynamic pinouts.
// Controls can be remapped, and flip_180 is a calibration flag.
// Teensy 4.1 has different pinouts depending on voltage at pin 41/A17

//extern uint8_t CV1, CV2, CV3, CV4;
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
extern bool SDcard_Ready;
#ifdef NORTHERNLIGHT
static constexpr bool NorthernLightModular = true;
#elif !defined(ARDUINO_TEENSY41)
static constexpr bool NorthernLightModular = false;
#else
extern bool NorthernLightModular;
#endif

// OLED CS is active low
#define OLED_CS_ACTIVE LOW
#define OLED_CS_INACTIVE HIGH

#define OC_GPIO_BUTTON_PINMODE INPUT_PULLUP
#define OC_GPIO_TRx_PINMODE INPUT_PULLUP
#define OC_GPIO_ENC_PINMODE INPUT_PULLUP

/* local copy of pinMode (cf. cores/pins_teensy.c), using faster slew rate */
// TODO: is this necessary? -NJM

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
void SetFlipMode(bool flip_180);

}

#endif // OC_GPIO_H_
