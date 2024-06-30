/*
*   (C) Copyright 2013, Andrew Kroll (xxxajk)
*   (C) Copyright 2015, 2016, Patrick Dowling, Max Stadler
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of version 3 of the GNU General Public License as
*   published by the Free Software Foundation at http://www.gnu.org/licenses,
*   with Additional Permissions under term 7(b) that the original copyright
*   notice and author attibution must be preserved and under term 7(c) that
*   modified versions be marked as different from the original.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*
*   DAC8565 basics.
*
*   chip select/CS : 10
*   reset/RST : 9
*
*   DB23 = 0 :: always 0
*   DB22 = 0 :: always 0
*   DB21 = 0 && DB20 = 1 :: single-channel update
*   DB19 = 0 :: always 0
*   DB18/17  :: DAC # select
*   DB16 = 0 :: power down mode
*   DB15-DB0 :: data
*
*   jacks map to channels A-D as follows:
*   top left (B) - > top right (A) - > bottom left (D) - > bottom right (C) 
*
*/

#include "util/util_SPIFIFO.h"
#include "OC_DAC.h"
#include "OC_gpio.h"
#include "OC_options.h"
#include "OC_calibration.h"
#if defined(__IMXRT1062__)
#include <SPI.h>
#endif

#define SPICLOCK_30MHz   (SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR) //(60 / 2) * ((1+1)/2) = 30 MHz (= 24MHz, when F_BUS == 48000000)

namespace OC {

#ifdef VOR
int DAC::kOctaveZero = 0;
#endif

/*static*/
void DAC::Init(const CalibrationData *calibration_data, const AutotuneCalibrationData *autotune_calibration_data) {

  calibration_data_ = calibration_data;
  autotune_calibration_data_ = autotune_calibration_data;

  // set up DAC pins 
  pinMode(DAC_CS, OUTPUT);

  // set Vbias, using onboard DAC - does nothing on non-VOR hardware
  init_Vbias();
  set_Vbias(2760); // default to Asymmetric
  delay(10);
  
#ifndef VOR
  // VOR button uses the same pin as DAC_RST
  pinMode(DAC_RST,OUTPUT);
  #ifdef DAC8564 // A0 = 0, A1 = 0
    digitalWrite(DAC_RST, LOW); 
  #else  // default to DAC8565 - pull RST high 
    digitalWrite(DAC_RST, HIGH);
  #endif
#endif

  history_tail_ = 0;
  memset(history_, 0, sizeof(uint16_t) * kHistoryDepth * DAC_CHANNEL_LAST);

#if defined(__MK20DX256__)
  if (F_BUS == 60000000 || F_BUS == 48000000) 
    SPIFIFO.begin(DAC_CS, SPICLOCK_30MHz, SPI_MODE0);  

#elif defined(__IMXRT1062__)
  #if defined(ARDUINO_TEENSY40)
  SPI.begin();
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 3; // DAC CS pin controlled by SPI
  #elif defined(ARDUINO_TEENSY41)
  if (DAC8568_Uses_SPI) {
    // Assume DAC8568_Vref_enable() already called by  main program startup,
    // so we don't need to do any more hardware init, and calling SPI.begin()
    // again could cause problems.
  } else {
    SPI.begin();
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 3; // DAC CS pin controlled by SPI
  }
  if (ADC33131D_Uses_FlexIO) {
    // ADC33131D wants pin 12 for data input, but SPI.begin() takes it away
    // reconfigure pin mux to return pin 12 control to FlexIO
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 4 | 0x10;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_B0_01 = 0x100C0;
  }
  #endif
#endif

  set_all(0xffff);
  Update();
}

#if defined(__MK20DX256__)
/*static*/ 
void DAC::init_Vbias() {
  /* using MK20 DAC0 for Vbias*/
  VREF_TRM = 0x60; VREF_SC = 0xE1; // enable 1v2 reference
  SIM_SCGC2 |= SIM_SCGC2_DAC0; // DAC clock
  DAC0_C0 = DAC_C0_DACEN; // enable module + use internal 1v2 reference
}
/*static*/ 
void DAC::set_Vbias(uint32_t data) {
  *(volatile int16_t *)&(DAC0_DAT0L) = data;
}

#elif defined(__IMXRT1062__)
void DAC::init_Vbias() {
  // TODO Teensy 4.1
}
void DAC::set_Vbias(uint32_t data) {
  // TODO Teensy 4.1
}

#endif

/*static*/
const DAC::CalibrationData *DAC::calibration_data_ = nullptr;

/*static*/
const DAC::AutotuneCalibrationData *DAC::autotune_calibration_data_ = nullptr;

/*static*/
uint32_t DAC::values_[DAC_CHANNEL_LAST];

/*static*/
uint16_t DAC::history_[DAC_CHANNEL_LAST][DAC::kHistoryDepth];

/*static*/ 
volatile size_t DAC::history_tail_;

} // namespace OC

void DAC8565::Write(uint32_t cmd, uint32_t data) {
#ifdef BUCHLA_cOC
#else
  data = kMaxValue - data;
#endif

#if defined(__MK20DX256__)
  SPIFIFO.write(cmd, SPI_CONTINUE);
  SPIFIFO.write16(data);
  SPIFIFO.read();
  SPIFIFO.read();
#elif defined(__IMXRT1062__) // Teensy 4.x
  data = (cmd << 16) | (data & 0xFFFF);
  LPSPI4_TDR = data;
#endif // __IMXRT1062__

}

// adapted from https://github.com/xxxajk/spi4teensy3 (MISO disabled) : 

#if defined(__MK20DX256__)
void SPI_init() {

  uint32_t ctar0, ctar1;

  SIM_SCGC6 |= SIM_SCGC6_SPI0;
  CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
  CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
  
  ctar0 = SPI_CTAR_DBR; // default
  #if   F_BUS == 60000000
      ctar0 = (SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR); //(60 / 2) * ((1+1)/2) = 30 MHz
  #elif F_BUS == 48000000
      ctar0 = (SPI_CTAR_PBR(0) | SPI_CTAR_BR(0) | SPI_CTAR_DBR); //(48 / 2) * ((1+1)/2) = 24 MHz          
  #endif
  ctar1 = ctar0;
  ctar0 |= SPI_CTAR_FMSZ(7);
  ctar1 |= SPI_CTAR_FMSZ(15);
  SPI0_MCR = SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F);
  SPI0_MCR |= SPI_MCR_CLR_RXF | SPI_MCR_CLR_TXF;

  // update ctars
  uint32_t mcr = SPI0_MCR;
  if (mcr & SPI_MCR_MDIS) {
    SPI0_CTAR0 = ctar0;
    SPI0_CTAR1 = ctar1;
  } else {
    SPI0_MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
    SPI0_CTAR0 = ctar0;
    SPI0_CTAR1 = ctar1;
    SPI0_MCR = mcr;
  }
}

#elif defined(__IMXRT1062__)
void SPI_init() {
  // TODO Teensy 4.1
}

#if defined(ARDUINO_TEENSY41)
void OC::DAC::DAC8568_Vref_enable() {
  Serial.println("DAC8568 Vref enable");
  SPI.begin();
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 3; // DAC CS pin controlled by SPI
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE1));
  LPSPI4_TCR = (LPSPI4_TCR & 0xF8000000) | LPSPI_TCR_FRAMESZ(31)
    | LPSPI_TCR_PCS(0) | LPSPI_TCR_RXMSK;
  delayMicroseconds(10);
  dac8568_raw_write(0x08000001); // turn on Vref (2.5V ±0.004%), static mode
  delay(5);
}
#endif

#endif // __IMXRT1062__

// OC_DAC
