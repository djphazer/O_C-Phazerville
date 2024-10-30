/* FreqMeasure Library, for measuring relatively low frequencies
 * http://www.pjrc.com/teensy/td_libs_FreqMeasure.html
 * Copyright (c) 2011 PJRC.COM, LLC - Paul Stoffregen <paul@pjrc.com>
 *
 * Version 1.1
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

#include "OC_FreqMeasure.h"
#include "OC_FreqMeasureCapture.h"

#if defined(__MK20DX256__)

#define FREQMEASURE_BUFFER_LEN 12
static volatile uint32_t buffer_value[FREQMEASURE_BUFFER_LEN];
static volatile uint8_t buffer_head;
static volatile uint8_t buffer_tail;
static uint16_t capture_msw;
static uint32_t capture_previous;

void FreqMeasureClass::begin(void)
{
	capture_init();
	capture_msw = 0;
	capture_previous = 0;
	buffer_head = 0;
	buffer_tail = 0;
	capture_start();
}

uint8_t FreqMeasureClass::available(void)
{
	uint8_t head, tail;

	head = buffer_head;
	tail = buffer_tail;

	if (head >= tail) 
		return (head - tail);

	return FREQMEASURE_BUFFER_LEN + head - tail;
}

uint32_t FreqMeasureClass::read(void)
{
	uint8_t head, tail;
	uint32_t value;

	head = buffer_head;
	tail = buffer_tail;
	if (head == tail) return 0xFFFFFFFF;
	tail = tail + 1;
	if (tail >= FREQMEASURE_BUFFER_LEN) tail = 0;
	value = buffer_value[tail];
	buffer_tail = tail;
	return value;
}

float FreqMeasureClass::countToFrequency(uint32_t count)
{
#if defined(__AVR__)
	return (float)F_CPU / (float)count;
#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISK)
	return (float)F_BUS / (float)count;
#elif defined(__arm__) && defined(TEENSYDUINO) && defined(KINETISL)
	return (float)(F_PLL/2) / (float)count;
#endif
}

void FreqMeasureClass::end(void)
{
	capture_shutdown();
}

void FTM_ISR_NAME (void)
{
	uint32_t capture, period, i;
	bool inc = false;

	if (capture_overflow()) {
		capture_overflow_reset();
		capture_msw++;
		inc = true;
	}

	if (capture_event()) {

		capture = capture_read();
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		// compute the waveform period
		period = capture - capture_previous;
		capture_previous = capture;
		// store it into the buffer
		i = buffer_head + 1;
		if (i >= FREQMEASURE_BUFFER_LEN) i = 0;
		if (i != buffer_tail) {
			buffer_value[i] = period;
			buffer_head = i;
		}
	}
}

#elif defined(__IMXRT1062__)

FreqMeasureClass *FreqMeasureClass::pin_inst[4];
extern "C" void xbar_connect(unsigned int input, unsigned int output); // in pwm.c

FLASHMEM
void FreqMeasureClass::begin(uint8_t pin /*= 0 TR1*/)
{
	switch (pin) {
	  case 0: // TR1=0  AD_B0_03  XBAR1_INOUT17 -> XBAR1 -> QTIMER1_TIMER3
		type = 1;
		timer.quad = &IMXRT_TMR1;
		ch = 3;
		pin_inst[0] = this;
		IOMUXC_GPR_GPR6 &= ~IOMUXC_GPR_GPR6_IOMUXC_XBAR_DIR_SEL_17;
		muxreg = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_03; // page 473
		*muxreg = 1 | 0x10;
		IOMUXC_XBAR1_IN17_SELECT_INPUT = 1; // page 904
		CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
		xbar_connect(XBARA1_IN_IOMUX_XBAR_INOUT17, XBARA1_OUT_QTIMER1_TIMER3);
		IOMUXC_GPR_GPR6 |= IOMUXC_GPR_GPR6_QTIMER1_TRM3_INPUT_SEL;
		irq = IRQ_QTIMER1;
		attachInterruptVector(irq, &pin0_isr);
		break;
	  case 1: // TR2=1  AD_B0_02  XBAR1_INOUT16 -> XBAR1 -> QTIMER2_TIMER3
		type = 1;
		timer.quad = &IMXRT_TMR2;
		ch = 3;
		pin_inst[1] = this;
		IOMUXC_GPR_GPR6 &= ~IOMUXC_GPR_GPR6_IOMUXC_XBAR_DIR_SEL_16;
		muxreg = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_02; // page 472
		*muxreg = 1 | 0x10;
		IOMUXC_XBAR1_IN16_SELECT_INPUT = 0; // page 911
		CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
		xbar_connect(XBARA1_IN_IOMUX_XBAR_INOUT16, XBARA1_OUT_QTIMER2_TIMER3);
		IOMUXC_GPR_GPR6 |= IOMUXC_GPR_GPR6_QTIMER2_TRM3_INPUT_SEL;
		irq = IRQ_QTIMER2;
		attachInterruptVector(irq, &pin1_isr);
		break;
	  case 23: // TR3=23  AD_B1_09  FlexPWM4_1_A
		type = 0;
		timer.flex = &IMXRT_FLEXPWM4;
		ch = 1;
		pin_inst[2] = this;
		muxreg = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09; // page 497
		*muxreg = 1 | 0x10;
		IOMUXC_FLEXPWM4_PWMA1_SELECT_INPUT = 1; // page 812
		irq = IRQ_FLEXPWM4_1;
		attachInterruptVector(irq, &pin23_isr);
		break;
	  case 22: // TR4=22  AD_B1_08  FlexPWM4_0_A
		type = 0;
		timer.flex = &IMXRT_FLEXPWM4;
		ch = 0;
		pin_inst[3] = this;
		muxreg = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08; // page 496
		*muxreg = 1 | 0x10;
		IOMUXC_FLEXPWM4_PWMA0_SELECT_INPUT = 1; // page 811
		irq = IRQ_FLEXPWM4_0;
		attachInterruptVector(irq, &pin22_isr);
		break;
	  default:
		return;
	}
	if (type == 0) {
		timer.flex->FCTRL0 |= FLEXPWM_FCTRL0_FLVL(1 << ch);
		timer.flex->FSTS0 |= FLEXPWM_FSTS0_FFLAG(1 << ch);
		timer.flex->MCTRL |= FLEXPWM_MCTRL_CLDOK(1 << ch);
		// Counter Synchronization, page 3109
		// CTRL2 register, page 3144
		timer.flex->SM[ch].CTRL2 = FLEXPWM_SMCTRL2_INIT_SEL(0) | FLEXPWM_SMCTRL2_INDEP;
		timer.flex->SM[ch].CTRL = FLEXPWM_SMCTRL_FULL;
		timer.flex->SM[ch].INIT = 0;
		timer.flex->SM[ch].VAL0 = 0;
		timer.flex->SM[ch].VAL1 = 65535;
		timer.flex->SM[ch].VAL2 = 0;
		timer.flex->SM[ch].VAL3 = 0;
		timer.flex->SM[ch].VAL4 = 0;
		timer.flex->SM[ch].VAL5 = 0;
		timer.flex->MCTRL |= FLEXPWM_MCTRL_LDOK(1 << ch) | FLEXPWM_MCTRL_RUN(1 << ch);
		NVIC_SET_PRIORITY(irq, 48);
		timer.flex->SM[ch].INTEN = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_RIE;
		capture_msw = 0;
		capture_previous = 0;
		buffer_head = 0;
		buffer_tail = 0;
		timer.flex->SM[ch].CAPTCTRLA = FLEXPWM_SMCAPTCTRLA_EDGA0(2)
			| FLEXPWM_SMCAPTCTRLA_ARMA;
		timer.flex->SM[ch].STS = FLEXPWM_SMSTS_CFA0 | FLEXPWM_SMSTS_RF;
		NVIC_ENABLE_IRQ(irq);
	} else /*if (type == 1)*/ {
		timer.quad->ENBL &= ~(1 << ch);
		timer.quad->CH[ch].CTRL = 0;
		timer.quad->CH[ch].CNTR = 0;
		timer.quad->CH[ch].COMP1 = 65535;
		timer.quad->CH[ch].COMP2 = 40000;
		timer.quad->CH[ch].LOAD = 0;
		timer.quad->CH[ch].CSCTRL = 0;
		timer.quad->CH[ch].FILT = 0;
		timer.quad->CH[ch].DMA = 0;
		timer.quad->CH[ch].CTRL = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) // page 3079
			| TMR_CTRL_SCS(ch) /* | TMR_CTRL_ONCE */ /*| TMR_CTRL_LENGTH */;
		timer.quad->CH[ch].SCTRL = TMR_SCTRL_TCFIE | TMR_SCTRL_IEFIE
			| TMR_SCTRL_CAPTURE_MODE(1);
		capture_msw = 0;
		capture_previous = 0;
		buffer_head = 0;
		buffer_tail = 0;
		timer.quad->ENBL |= (1 << ch);
		NVIC_SET_PRIORITY(irq, 48);
		NVIC_ENABLE_IRQ(irq);
	}
	running = true;
}

uint8_t FreqMeasureClass::available(void)
{
	if (!running) return 0;
	uint8_t head, tail;
	head = buffer_head;
	tail = buffer_tail;
	if (head >= tail) return head - tail;
	return FREQMEASURE_BUFFER_LEN + head - tail;
}

uint32_t FreqMeasureClass::read(void)
{
	uint8_t head, tail;
	uint32_t value;

	if (!running) return 0;
	head = buffer_head;
	tail = buffer_tail;
	if (head == tail) return 0xFFFFFFFF;
	tail = tail + 1;
	if (tail >= FREQMEASURE_BUFFER_LEN) tail = 0;
	value = buffer_value[tail];
	buffer_tail = tail;
	return value;
}

FLASHMEM
void FreqMeasureClass::end(void)
{
	if (!running) return;
	if (type == 0) {
		timer.flex->SM[ch].INTEN = 0;
		timer.flex->SM[ch].CAPTCTRLA = 0;
	} else if (type == 1) {
		timer.quad->ENBL &= ~(1 << ch);
		timer.quad->CH[ch].CTRL = 0;
		timer.quad->CH[ch].SCTRL = 0;
	}
	NVIC_DISABLE_IRQ(irq);
	*muxreg = 5 | 0x10;
	running = false;
}

void FreqMeasureClass::isr(void)
{
	if (!running) return;
	bool inc = false;
	uint32_t capture;

	if (type == 0) {
		uint32_t sts = timer.flex->SM[ch].STS;
		if (sts & FLEXPWM_SMSTS_RF) { // counter 16 bit overflow
			timer.flex->SM[ch].STS = FLEXPWM_SMSTS_RF;
			capture_msw++;
			inc = true;
		}
		if (sts & FLEXPWM_SMSTS_CFA0) {
			capture = timer.flex->SM[ch].CVAL2;
			timer.flex->SM[ch].STS = FLEXPWM_SMSTS_CFA0;
		} else {
			return;
		}
	} else /* type == 1 */ {
		uint32_t sctrl = timer.quad->CH[ch].SCTRL;
		if (sctrl & TMR_SCTRL_TCF) {
			timer.quad->CH[ch].SCTRL = sctrl & ~TMR_SCTRL_TCF;
			capture_msw++;
			inc = true;
		}
		if (sctrl & TMR_SCTRL_IEF) {
			capture = timer.quad->CH[ch].CAPT;
			timer.quad->CH[ch].SCTRL = sctrl & ~TMR_SCTRL_IEF;
		} else {
			return;
		}
	}

	if (capture <= 0xE000 || !inc) {
		capture |= (capture_msw << 16);
	} else {
		capture |= ((capture_msw - 1) << 16);
	}

	// compute the waveform period
	uint32_t period = capture - capture_previous;
	capture_previous = capture;
	// store it into the buffer
	uint32_t i = buffer_head + 1;
	if (i >= FREQMEASURE_BUFFER_LEN) i = 0;
	if (i != buffer_tail) {
		buffer_value[i] = period;
		buffer_head = i;
	}
}

#endif // __IMXRT1062__

FreqMeasureClass FreqMeasure;

