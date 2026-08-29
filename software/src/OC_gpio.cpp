#include <Arduino.h>
#include "OC_gpio.h"
#include "HSUtils.h"
#include "OC_ADC.h"
#include "OC_DAC.h"
#include "OC_digital_inputs.h"

// default settings for traditional O_C hardware wired for Teensy 3.2
// uint8_t CV1=19, CV2=18, CV3=20, CV4=17;
uint8_t TR1 = 0, TR2 = 1, TR3 = 2, TR4 = 3;
uint8_t encL1 = 22, encL2 = 21, butL = 23, encR1 = 16, encR2 = 15, butR = 14;
uint8_t but_top = 5, but_bot = 4, but_mid = 9, but_top2 = 255, but_bot2 = 255;

FLASHMEM
void OC::SetFlipMode(bool flip_180) {
  if (flip_180) {
    // reversed
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
    butR = 23;
    encL2 = 16;
    encL1 = 15;
    butL = 14;

    // CV4 = 19;
    // CV3 = 18;
    // CV2 = 20;
    // CV1 = 17;
    TR4 = 0;
    TR3 = 1;
    TR2 = 2;
    TR1 = 3;
  } else {
    // default orientation
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
    butL = 23;
    encR1 = 16;
    encR2 = 15;
    butR = 14;

    // CV1 = 19;
    // CV2 = 18;
    // CV3 = 20;
    // CV4 = 17;
    TR1 = 0;
    TR2 = 1;
    TR3 = 2;
    TR4 = 3;
  }
}
