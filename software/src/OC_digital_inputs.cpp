#include <Arduino.h>
#include <algorithm>
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_options.h"

#if defined(__MK20DX256__) // Teensy 3.2

/*static*/
uint32_t OC::DigitalInputs::clocked_mask_;

/*static*/
volatile uint32_t OC::DigitalInputs::clocked_[DIGITAL_INPUT_LAST];

void FASTRUN OC::tr1_ISR() {
  OC::DigitalInputs::clock<OC::DIGITAL_INPUT_1>();
}  // main clock

void FASTRUN OC::tr2_ISR() {
  OC::DigitalInputs::clock<OC::DIGITAL_INPUT_2>();
}

void FASTRUN OC::tr3_ISR() {
  OC::DigitalInputs::clock<OC::DIGITAL_INPUT_3>();
}

void FASTRUN OC::tr4_ISR() {
  OC::DigitalInputs::clock<OC::DIGITAL_INPUT_4>();
}

/*static*/
void OC::DigitalInputs::Init() {

  static const struct {
    uint8_t pin;
    void (*isr_fn)();
  } pins[DIGITAL_INPUT_LAST] =  {
    {TR1, tr1_ISR},
    {TR2, tr2_ISR},
    {TR3, tr3_ISR},
    {TR4, tr4_ISR},
  };

  for (auto pin : pins) {
    pinMode(pin.pin, OC_GPIO_TRx_PINMODE);
    attachInterrupt(pin.pin, pin.isr_fn, FALLING);
  }

  clocked_mask_ = 0;
  std::fill(clocked_, clocked_ + DIGITAL_INPUT_LAST, 0);

  // Assume the priority of pin change interrupts is lower or equal to the
  // thread where ::Scan function is called. Otherwise a safer mechanism is
  // required to avoid conflicts (LDREX/STREX or store ARM_DWT_CYCCNT in the
  // array to check for changes.
  //
  // A really nice approach would be to use the FTM timer mechanism and avoid
  // the ISR altogether, but this only works for one of the pins. Using more
  // explicit interrupt grouping might also improve the handling a bit (if
  // implemented on the DX)
  //
  // It's still not guaranteed that 4 simultaneous triggers will be handled
  // exactly simultaneously though, but that's micro-timing dependent even if
  // the pins have higher prio.

  //  NVIC_SET_PRIORITY(IRQ_PORTB, 0); // TR1 = 0 = PTB16
  // Defaults is 0, or set OC_GPIO_ISR_PRIO for all ports
}

void OC::DigitalInputs::reInit() {
  // re-init TR4, to avoid conflict with the FTM
  #ifdef FLIP_180
    pinMode(TR1, OC_GPIO_TRx_PINMODE);
    attachInterrupt(TR1, tr1_ISR, FALLING);
  #else
    pinMode(TR4, OC_GPIO_TRx_PINMODE);
    attachInterrupt(TR4, tr4_ISR, FALLING);
  #endif
}

/*static*/
void OC::DigitalInputs::Scan() {
  clocked_mask_ =
    ScanInput<DIGITAL_INPUT_1>() |
    ScanInput<DIGITAL_INPUT_2>() |
    ScanInput<DIGITAL_INPUT_3>() |
    ScanInput<DIGITAL_INPUT_4>();
}

#endif // Teensy 3.2


#if defined(__IMXRT1062__) // Teensy 4.0 or 4.1

uint8_t OC::DigitalInputs::clocked_mask_;
IMXRT_GPIO_t * OC::DigitalInputs::port[DIGITAL_INPUT_LAST];
uint32_t  OC::DigitalInputs::bitmask[DIGITAL_INPUT_LAST];

FLASHMEM
void OC::DigitalInputs::Init() {
  pinMode(TR1, INPUT_PULLUP);
  pinMode(TR2, INPUT_PULLUP);
  pinMode(TR3, INPUT_PULLUP);
  pinMode(TR4, INPUT_PULLUP);
  port[0] = (IMXRT_GPIO_t *)digitalPinToPortReg(TR1);
  port[1] = (IMXRT_GPIO_t *)digitalPinToPortReg(TR2);
  port[2] = (IMXRT_GPIO_t *)digitalPinToPortReg(TR3);
  port[3] = (IMXRT_GPIO_t *)digitalPinToPortReg(TR4);
  bitmask[0] = digitalPinToBitMask(TR1);
  bitmask[1] = digitalPinToBitMask(TR2);
  bitmask[2] = digitalPinToBitMask(TR3);
  bitmask[3] = digitalPinToBitMask(TR4);
  for (unsigned int i=0; i < 4; i++) {
    unsigned int bitnum = __builtin_ctz(bitmask[i]);
    if (bitnum < 16) {
      port[i]->ICR1 |= (0x03 << (bitnum * 2)); // falling edge detect, bits 0-15
    } else {
      port[i]->ICR2 |= (0x03 << ((bitnum - 16) * 2)); // falling edge detect, bits 16-31
    }
    port[i]->ISR = bitmask[i]; // clear any prior detected edge
  }
}

void OC::DigitalInputs::Scan() {
  uint32_t mask[4];
  noInterrupts();
  mask[0] = port[0]->ISR & bitmask[0];
  port[0]->ISR = mask[0];
  mask[1] = port[1]->ISR & bitmask[1];
  port[1]->ISR = mask[1];
  mask[2] = port[2]->ISR & bitmask[2];
  port[2]->ISR = mask[2];
  mask[3] = port[3]->ISR & bitmask[3];
  port[3]->ISR = mask[3];
  interrupts();
  uint8_t new_clocked_mask = 0;
  if (mask[0]) new_clocked_mask |= 0x01;
  if (mask[1]) new_clocked_mask |= 0x02;
  if (mask[2]) new_clocked_mask |= 0x04;
  if (mask[3]) new_clocked_mask |= 0x08;
  clocked_mask_ = new_clocked_mask;
  #if 0
  if (clocked_mask_) {
    static elapsedMicros usec;
    Serial.printf("%u  %u\n", clocked_mask_, (int)usec);
    usec = 0;
  }
  #endif
}

#endif // Teensy 4.0 or 4.1
