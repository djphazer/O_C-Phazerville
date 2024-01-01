/* Simple compatibility headers for AVR code used with ARM chips
 * Copyright (c) 2015 Paul Stoffregen <paul@pjrc.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Guidelines for editing this file:
// https://forum.pjrc.com/threads/34537-Teensy-LC-Increase-EEPROM-Size/page2

// modified version for Phazerville Suite
// This is a combo of the T3 and T4 versions of this file, with the 4.0 value bumped up to 3071

#ifndef _AVR_EEPROM_H_
#define _AVR_EEPROM_H_ 1

#include <stddef.h>
#include <stdint.h>

#include "avr_functions.h"

#if defined(ARDUINO_TEENSY40)
  #define E2END 0xBFF // 0x437
#elif defined(ARDUINO_TEENSY41)
  #define E2END 0x10BB
#elif defined(ARDUINO_TEENSY_MICROMOD)
  #define E2END 0x10BB
#elif defined(__MK20DX128__) || defined(__MK20DX256__)
  #define E2END 0x7FF
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  #define E2END 0xFFF
#elif defined(__MKL26Z64__)
  #define E2END 0x7F
#else
  #define E2END 0
#endif

#endif
