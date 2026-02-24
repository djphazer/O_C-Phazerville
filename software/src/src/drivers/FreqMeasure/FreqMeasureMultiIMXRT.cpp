/* FreqMeasure Library, for measuring relatively low frequencies
 * http://www.pjrc.com/teensy/td_libs_FreqMeasure.html
 * Copyright (c) 2015 PJRC.COM, LLC - Paul Stoffregen <paul@pjrc.com>
 * Extended 2016 by Thierry Frenkel <theremingenieur@gmail.com>
 *
 * Version 0.22
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

#include "FreqMeasureMulti.h"
#if defined(__IMXRT1062__)
// This file is to handle the Teensy 4.x boards. 
#define M(a, b) ((((a) - 1) << 2) | (b))   // should translate from 0-15
FreqMeasureMulti * FreqMeasureMulti::list[16] = {nullptr, nullptr, nullptr, nullptr};

void FreqMeasureMulti::flexpwm1_0_isr() {list[M(1,0)]->isr();}
void FreqMeasureMulti::flexpwm1_1_isr() {list[M(1,1)]->isr();}
void FreqMeasureMulti::flexpwm1_2_isr() {list[M(1,2)]->isr();}
void FreqMeasureMulti::flexpwm1_3_isr() {list[M(1,3)]->isr();}
void FreqMeasureMulti::flexpwm2_0_isr() {list[M(2,0)]->isr();}
void FreqMeasureMulti::flexpwm2_1_isr() {list[M(2,1)]->isr();}
void FreqMeasureMulti::flexpwm2_2_isr() {list[M(2,2)]->isr();}
void FreqMeasureMulti::flexpwm2_3_isr() {list[M(2,3)]->isr();}
void FreqMeasureMulti::flexpwm3_0_isr() {list[M(3,0)]->isr();}
void FreqMeasureMulti::flexpwm3_1_isr() {list[M(3,1)]->isr();}
void FreqMeasureMulti::flexpwm3_3_isr() {list[M(3,3)]->isr();}
void FreqMeasureMulti::flexpwm4_0_isr() {list[M(4,0)]->isr();}
void FreqMeasureMulti::flexpwm4_1_isr() {list[M(4,1)]->isr();}
void FreqMeasureMulti::flexpwm4_2_isr() {list[M(4,2)]->isr();}


const struct freq_pwm_pin_info_struct FreqMeasureMulti::freq_pwm_pin_info[] = {
	{&IMXRT_FLEXPWM1, M(1,1), 0, 4 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, nullptr, 0},  // FlexPWM1_1_X   0  // AD_B0_03
	{&IMXRT_FLEXPWM1, M(1,0), 0, 4 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, nullptr, 0},  // FlexPWM1_0_X   1  // AD_B0_02
	{&IMXRT_FLEXPWM4, M(4,2), 1, 1 | 0x10, IRQ_FLEXPWM4_2, &FreqMeasureMulti::flexpwm4_2_isr, &IOMUXC_FLEXPWM4_PWMA2_SELECT_INPUT, 0},  // FlexPWM4_2_A   2  // EMC_04
	{&IMXRT_FLEXPWM4, M(4,2), 2, 1 | 0x10, IRQ_FLEXPWM4_2, &FreqMeasureMulti::flexpwm4_2_isr, nullptr, 0},  // FlexPWM4_2_B   3  // EMC_05
	{&IMXRT_FLEXPWM2, M(2,0), 1, 1 | 0x10, IRQ_FLEXPWM2_0, &FreqMeasureMulti::flexpwm2_0_isr, &IOMUXC_FLEXPWM2_PWMA0_SELECT_INPUT, 0},  // FlexPWM2_0_A   4  // EMC_06
	{&IMXRT_FLEXPWM2, M(2,1), 1, 1 | 0x10, IRQ_FLEXPWM2_1, &FreqMeasureMulti::flexpwm2_1_isr, &IOMUXC_FLEXPWM2_PWMA1_SELECT_INPUT, 0},  // FlexPWM2_1_A   5  // EMC_08
	{&IMXRT_FLEXPWM2, M(2,2), 1, 2 | 0x10, IRQ_FLEXPWM2_2, &FreqMeasureMulti::flexpwm2_2_isr, &IOMUXC_FLEXPWM2_PWMA2_SELECT_INPUT, 1},  // FlexPWM2_2_A   6  // B0_10
	{&IMXRT_FLEXPWM1, M(1,3), 2, 6 | 0x10, IRQ_FLEXPWM1_3, &FreqMeasureMulti::flexpwm1_3_isr, &IOMUXC_FLEXPWM1_PWMB3_SELECT_INPUT, 4},  // FlexPWM1_3_B   7  // B1_01
	{&IMXRT_FLEXPWM1, M(1,3), 1, 6 | 0x10, IRQ_FLEXPWM1_3, &FreqMeasureMulti::flexpwm1_3_isr, &IOMUXC_FLEXPWM1_PWMA3_SELECT_INPUT, 4},  // FlexPWM1_3_A   8  // B1_00
	{&IMXRT_FLEXPWM2, M(2,2), 2, 2 | 0x10, IRQ_FLEXPWM2_2, &FreqMeasureMulti::flexpwm2_2_isr, &IOMUXC_FLEXPWM2_PWMB2_SELECT_INPUT, 1},  // FlexPWM2_2_B   9  // B0_11
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer1_0  10  // B0_00
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer1_2  11  // B0_02
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer1_1  12  // B0_01
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer2_0  13  // B0_03
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer3_2  14  // AD_B1_02
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer3_3  15  // AD_B1_03
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer3_1  18  // AD_B1_01
	{nullptr, 0, 0, 1 | 0x10, 0, nullptr, nullptr, 0},  // QuadTimer3_0  19  // AD_B1_00
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{&IMXRT_FLEXPWM4, M(4,0), 1, 1 | 0x10, IRQ_FLEXPWM4_0, &FreqMeasureMulti::flexpwm4_0_isr, &IOMUXC_FLEXPWM4_PWMA0_SELECT_INPUT, 1},  // FlexPWM4_0_A  22  // AD_B1_08
	{&IMXRT_FLEXPWM4, M(4,1), 1, 1 | 0x10, IRQ_FLEXPWM4_1, &FreqMeasureMulti::flexpwm4_1_isr, &IOMUXC_FLEXPWM4_PWMA1_SELECT_INPUT, 1},  // FlexPWM4_1_A  23  // AD_B1_09
	{&IMXRT_FLEXPWM1, M(1,2), 0, 4 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, nullptr, 0},  // FlexPWM1_2_X  24  // AD_B0_12
	{&IMXRT_FLEXPWM1, M(1,3), 0, 4 | 0x10, IRQ_FLEXPWM1_3, &FreqMeasureMulti::flexpwm1_3_isr, nullptr, 0},  // FlexPWM1_3_X  25  // AD_B0_13
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{&IMXRT_FLEXPWM3, M(3,1), 2, 1 | 0x10, IRQ_FLEXPWM3_1, &FreqMeasureMulti::flexpwm3_1_isr, nullptr, 0},  // FlexPWM3_1_B  28  // EMC_32
	{&IMXRT_FLEXPWM3, M(3,1), 1, 1 | 0x10, IRQ_FLEXPWM3_1, &FreqMeasureMulti::flexpwm3_1_isr, nullptr, 0},  // FlexPWM3_1_A  29  // EMC_31
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{&IMXRT_FLEXPWM2, M(2,0), 2, 1 | 0x10, IRQ_FLEXPWM2_0, &FreqMeasureMulti::flexpwm2_0_isr, &IOMUXC_FLEXPWM2_PWMB0_SELECT_INPUT, 0},  // FlexPWM2_0_B  33  // EMC_07
#ifdef ARDUINO_TEENSY40
	{&IMXRT_FLEXPWM1, M(1,1), 2, 1 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMB1_SELECT_INPUT, 1},  // FlexPWM1_1_B  34  // SD_B0_03
	{&IMXRT_FLEXPWM1, M(1,1), 1, 1 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMA1_SELECT_INPUT, 1},  // FlexPWM1_1_A  35  // SD_B0_02
	{&IMXRT_FLEXPWM1, M(1,0), 2, 1 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, &IOMUXC_FLEXPWM1_PWMB0_SELECT_INPUT, 1},  // FlexPWM1_0_B  36  // SD_B0_01
	{&IMXRT_FLEXPWM1, M(1,0), 1, 1 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, &IOMUXC_FLEXPWM1_PWMA0_SELECT_INPUT, 1},  // FlexPWM1_0_A  37  // SD_B0_00
	{&IMXRT_FLEXPWM1, M(1,2), 2, 1 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMB2_SELECT_INPUT, 1},  // FlexPWM1_2_B  38  // SD_B0_05
	{&IMXRT_FLEXPWM1, M(1,2), 1, 1 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMA2_SELECT_INPUT, 1},  // FlexPWM1_2_A  39  // SD_B0_04
#endif
#ifdef  ARDUINO_TEENSY41
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{&IMXRT_FLEXPWM2, M(2,3), 1, 6 | 0x10, IRQ_FLEXPWM2_3, &FreqMeasureMulti::flexpwm2_3_isr, &IOMUXC_FLEXPWM2_PWMA3_SELECT_INPUT, 4},  // FlexPWM2_3_A  36  // B1_02
	{&IMXRT_FLEXPWM2, M(2,3), 2, 6 | 0x10, IRQ_FLEXPWM2_3, &FreqMeasureMulti::flexpwm2_3_isr, &IOMUXC_FLEXPWM2_PWMB3_SELECT_INPUT, 3},  // FlexPWM2_3_B  37  // B1_03
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{nullptr, 0, 0, 0 | 0x10, 0, nullptr, nullptr, 0},
	{&IMXRT_FLEXPWM1, M(1,1), 2, 1 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMB1_SELECT_INPUT, 1},  // FlexPWM1_1_B  42  // SD_B0_03
	{&IMXRT_FLEXPWM1, M(1,1), 1, 1 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMA1_SELECT_INPUT, 1},  // FlexPWM1_1_A  43  // SD_B0_02
	{&IMXRT_FLEXPWM1, M(1,0), 2, 1 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, &IOMUXC_FLEXPWM1_PWMB0_SELECT_INPUT, 1},  // FlexPWM1_0_B  44  // SD_B0_01
	{&IMXRT_FLEXPWM1, M(1,0), 1, 1 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, &IOMUXC_FLEXPWM1_PWMA0_SELECT_INPUT, 1},  // FlexPWM1_0_A  45  // SD_B0_00
	{&IMXRT_FLEXPWM1, M(1,2), 2, 1 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMB2_SELECT_INPUT, 1},  // FlexPWM1_2_B  46  // SD_B0_05
	{&IMXRT_FLEXPWM1, M(1,2), 1, 1 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMA2_SELECT_INPUT, 1},  // FlexPWM1_2_A  47  // SD_B0_04
	{&IMXRT_FLEXPWM1, M(1,0), 0, 0 | 0x10, IRQ_FLEXPWM1_0, &FreqMeasureMulti::flexpwm1_0_isr, &IOMUXC_FLEXPWM1_PWMB0_SELECT_INPUT, 0},  // duplicate FlexPWM1_0_B
	{&IMXRT_FLEXPWM1, M(1,0), 0, 0 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMA2_SELECT_INPUT, 0},  // duplicate FlexPWM1_2_A
	{&IMXRT_FLEXPWM1, M(1,0), 0, 0 | 0x10, IRQ_FLEXPWM1_2, &FreqMeasureMulti::flexpwm1_2_isr, &IOMUXC_FLEXPWM1_PWMB2_SELECT_INPUT, 0},  // duplicate FlexPWM1_2_B
	{&IMXRT_FLEXPWM3, M(3,3), 2, 1 | 0x10, IRQ_FLEXPWM3_3, &FreqMeasureMulti::flexpwm3_3_isr, nullptr, 0},  // FlexPWM3_3_B  51  // EMC_22
	{&IMXRT_FLEXPWM1, M(1,0), 0, 0 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMB1_SELECT_INPUT, 0},  // duplicate FlexPWM1_1_B
	{&IMXRT_FLEXPWM1, M(1,0), 0, 0 | 0x10, IRQ_FLEXPWM1_1, &FreqMeasureMulti::flexpwm1_1_isr, &IOMUXC_FLEXPWM1_PWMA1_SELECT_INPUT, 0},  // duplicate FlexPWM1_1_A
	{&IMXRT_FLEXPWM3, M(3,0), 1, 1 | 0x10, IRQ_FLEXPWM3_0, &FreqMeasureMulti::flexpwm3_0_isr, nullptr, 0},  // FlexPWM3_0_A  54  // EMC_29
#endif
};

#if 0
#define FTM_SC_VALUE (FTM_SC_TOIE | FTM_SC_CLKS(1) | FTM_SC_PS(0))
#define FTM_CSC_RAISING (FTM_CSC_CHIE | FTM_CSC_ELSA)
#define FTM_CSC_FALLING (FTM_CSC_CHIE | FTM_CSC_ELSB)
#define UPDATE_ON_RAISING 1
#define UPDATE_ON_FALLING 2
#define UPDATE_DIFFERENCE 4
#endif

//static uint8_t channelmask = 0;

bool FreqMeasureMulti::begin(uint32_t pin) {
	return begin(pin, FREQMEASUREMULTI_RAISING);
}

bool FreqMeasureMulti::begin(uint32_t pin, uint8_t mode)
{
	uint16_t capture_mode;
	if (pin > (sizeof(freq_pwm_pin_info)/sizeof(freq_pwm_pin_info[0]))) return false;
	if (freq_pwm_pin_info[pin].pflexpwm == nullptr) return false;	
	_pin = pin;	// remember the pin
	_mode = mode;	
	_channel = freq_pwm_pin_info[pin].channel; // remember the channel 

	// So we may now have a valid pin, lets configure it. 
	*(portConfigRegister(pin)) = freq_pwm_pin_info[pin].muxval;
	if (freq_pwm_pin_info[pin].select_input_register)
		*freq_pwm_pin_info[pin].select_input_register = freq_pwm_pin_info[pin].select_val;

	IMXRT_FLEXPWM_t *pflexpwm = freq_pwm_pin_info[pin].pflexpwm;
	uint8_t sub_module = freq_pwm_pin_info[pin].module & 3;
	uint8_t sub_module_bit = 1 << sub_module;

	pflexpwm->FCTRL0 |= FLEXPWM_FCTRL0_FLVL(sub_module_bit);
	pflexpwm->FSTS0 = sub_module_bit;
	pflexpwm->MCTRL |= FLEXPWM_MCTRL_CLDOK(sub_module_bit);
	pflexpwm->SM[sub_module].CTRL2 = FLEXPWM_SMCTRL2_INDEP;
	pflexpwm->SM[sub_module].CTRL = FLEXPWM_SMCTRL_HALF;
	pflexpwm->SM[sub_module].INIT = 0;
	pflexpwm->SM[sub_module].VAL0 = 0;
	pflexpwm->SM[sub_module].VAL1 = 65535;
	pflexpwm->SM[sub_module].VAL2 = 0;
	pflexpwm->SM[sub_module].VAL3 = 0;
	pflexpwm->SM[sub_module].VAL4 = 0;
	pflexpwm->SM[sub_module].VAL5 = 0;
	pflexpwm->MCTRL |= FLEXPWM_MCTRL_LDOK(sub_module_bit) | FLEXPWM_MCTRL_RUN(sub_module_bit);


	// How do we handle the three different channels? 
    // Can maybe capture two different values. 
    //00b - Disabled
	//01b - Capture falling edges
	//10b - Capture rising edges
	//11b - Capture any edge
	// BUGBUG: FLEXPWM_SMCAPTCTRLA_EDGA0(1) is used in cases wehre it is also _EDGB0 or _EDGX0, but
	// macros are the same
	// Quick and dirty way to set which interrupt bits we want...
	uint16_t inten = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CX0IE;  // one of the 0 edge bits.
	switch (mode) {
		case FREQMEASUREMULTI_FALLING:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(1);
			break;
		case FREQMEASUREMULTI_INTERLEAVE:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(2) | FLEXPWM_SMCAPTCTRLA_EDGA1(1);
			inten = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CX0IE 
				| FLEXPWM_SMINTEN_CA1IE | FLEXPWM_SMINTEN_CB1IE | FLEXPWM_SMINTEN_CX1IE;
			break;
		case FREQMEASUREMULTI_MARK_ONLY:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(1) | FLEXPWM_SMCAPTCTRLA_EDGA1(2);
			inten = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CX0IE 
				| FLEXPWM_SMINTEN_CA1IE | FLEXPWM_SMINTEN_CB1IE | FLEXPWM_SMINTEN_CX1IE;
			break;
		case FREQMEASUREMULTI_SPACE_ONLY:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(2) | FLEXPWM_SMCAPTCTRLA_EDGA1(1);
			inten = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CX0IE 
				| FLEXPWM_SMINTEN_CA1IE | FLEXPWM_SMINTEN_CB1IE | FLEXPWM_SMINTEN_CX1IE;
			break;	
		// ? Both edges? 
		case FREQMEASUREMULTI_ALTERNATE:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(2) | FLEXPWM_SMCAPTCTRLA_EDGA1(1);
			inten = FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CX0IE 
				| FLEXPWM_SMINTEN_CA1IE | FLEXPWM_SMINTEN_CB1IE | FLEXPWM_SMINTEN_CX1IE;
			break;
		default:
			capture_mode = FLEXPWM_SMCAPTCTRLA_EDGA0(2);
	}
	switch (_channel) {
		case 0: //  X channel
			pflexpwm->SM[sub_module].CAPTCTRLX = capture_mode | FLEXPWM_SMCAPTCTRLA_ARMA;
			pflexpwm->SM[sub_module].INTEN |= (inten & (FLEXPWM_SMINTEN_CX0IE | FLEXPWM_SMINTEN_CX1IE)) | FLEXPWM_SMINTEN_RIE;
			break;

		case 1: // A Channel
			pflexpwm->SM[sub_module].CAPTCTRLA = capture_mode | FLEXPWM_SMCAPTCTRLA_ARMA;
			pflexpwm->SM[sub_module].INTEN |= (inten & (FLEXPWM_SMINTEN_CA0IE | FLEXPWM_SMINTEN_CA1IE)) | FLEXPWM_SMINTEN_RIE;
			break;
		case 2: // B Channel;
			pflexpwm->SM[sub_module].CAPTCTRLB = capture_mode | FLEXPWM_SMCAPTCTRLA_ARMA;
			pflexpwm->SM[sub_module].INTEN |= (inten & (FLEXPWM_SMINTEN_CB0IE | FLEXPWM_SMINTEN_CB1IE)) | FLEXPWM_SMINTEN_RIE;
			break;
	}

	__disable_irq();
	if (list[freq_pwm_pin_info[pin].module] == nullptr) {
		list[freq_pwm_pin_info[pin].module] = this;	// remember this object
		_next = nullptr;
		attachInterruptVector((IRQ_NUMBER_t)freq_pwm_pin_info[pin].irq, freq_pwm_pin_info[pin].isr);
		NVIC_SET_PRIORITY(freq_pwm_pin_info[pin].irq, 48);
		NVIC_ENABLE_IRQ(freq_pwm_pin_info[pin].irq);
	} else {
		// BUGBUG:: Maybe should check for duplicate signals, but... 
		_next = list[freq_pwm_pin_info[pin].module];
		list[freq_pwm_pin_info[pin].module] = this;
	}
	__enable_irq();

	return true;
}

uint32_t FreqMeasureMulti::available(void)
{
	uint32_t head = buffer_head;
	uint32_t tail = buffer_tail;
	if (head >= tail) return head - tail;
	return FREQMEASUREMULTI_BUFFER_LEN + head - tail;
}

uint32_t FreqMeasureMulti::read(void)
{
	uint32_t head = buffer_head;
	uint32_t tail = buffer_tail;
	if (head == tail) return 0xFFFFFFFF;
	tail = tail + 1;
	if (tail >= FREQMEASUREMULTI_BUFFER_LEN) tail = 0;
	uint32_t value = buffer_value[tail].count;
	last_read_level = buffer_value[tail].level;
	buffer_tail = tail;
	return value;
}

uint8_t FreqMeasureMulti::readLevel(void)
{
	return last_read_level;
}

float FreqMeasureMulti::countToFrequency(uint32_t count)
{
	return (float)F_BUS_ACTUAL / (float)count;
}

float FreqMeasureMulti::countToNanoseconds(uint32_t count)
{
	return (float)count * 1000000000.0f / (float)F_BUS_ACTUAL;
}

void FreqMeasureMulti::end(void)
{
	if (list[freq_pwm_pin_info[_pin].module] == this) {
		list[freq_pwm_pin_info[_pin].module] = _next;	// removed us from head of list
		if (list[freq_pwm_pin_info[_pin].module] == nullptr) {
			// no one left active on this one. 
			IMXRT_FLEXPWM_t *pflexpwm = freq_pwm_pin_info[_pin].pflexpwm;
			uint8_t sub_module = freq_pwm_pin_info[_pin].module & 3;
			pflexpwm->SM[sub_module].INTEN = 0;
			pflexpwm->SM[sub_module].CAPTCTRLA = 0;
			NVIC_DISABLE_IRQ(freq_pwm_pin_info[_pin].irq);		
		}
	} else {
		if (list[freq_pwm_pin_info[_pin].module]->_next == this) list[freq_pwm_pin_info[_pin].module]->_next = _next;
		if (list[freq_pwm_pin_info[_pin].module]->_next->_next == this) list[freq_pwm_pin_info[_pin].module]->_next->_next = _next;
	}	
}


void FreqMeasureMulti::processChannelISR(uint8_t channel, uint32_t capture, uint8_t edge)
{
	uint8_t level = LEVEL_UNDEFINED;

	uint32_t i;
	bool add_item_to_buffer = true;
	uint32_t period = capture - _capture_previous[edge];

	if (channel == _channel) {
		// compute the waveform period
		switch (_mode) {
			case FREQMEASUREMULTI_RAISING:
				level = LEVEL_MARK_SPACE;
				break;
			case FREQMEASUREMULTI_FALLING:
				level = LEVEL_SPACE_MARK;
				break;
			case FREQMEASUREMULTI_INTERLEAVE:
				level = edge? LEVEL_SPACE_MARK : LEVEL_MARK_SPACE; 
				break;
			case FREQMEASUREMULTI_SPACE_ONLY:
				level = LEVEL_SPACE_ONLY;
				if (edge) add_item_to_buffer = false;
				else period = capture - _capture_previous[1];
				break;
			case FREQMEASUREMULTI_MARK_ONLY:
				level = LEVEL_MARK_ONLY;
				if (edge) add_item_to_buffer = false;
				else period = capture - _capture_previous[1];
				break;
			case FREQMEASUREMULTI_ALTERNATE:
				if (edge) {
					period = capture - _capture_previous[0];
					level = LEVEL_MARK_ONLY;
				} else {
					period = capture - _capture_previous[1];
					level = LEVEL_SPACE_ONLY;
				}
				break;

		}
		_capture_previous[edge] = capture;
		// store it into the buffer
		if (add_item_to_buffer) {
			i = buffer_head + 1;
			if (i >= FREQMEASUREMULTI_BUFFER_LEN) i = 0;
			if (i != buffer_tail) {
				buffer_value[i].level = level;
				buffer_value[i].count = period;
				//buffer_value[i] = period;
				buffer_head = i;
			}
		}
	} else if (_next) {
		_next->processChannelISR(channel, capture, edge);
	}
}

void FreqMeasureMulti::isr()
{
	IMXRT_FLEXPWM_t *pflexpwm = freq_pwm_pin_info[_pin].pflexpwm;
	uint8_t sub_module = freq_pwm_pin_info[_pin].module & 3;

	uint32_t capture;
	bool inc = false;

	// See if we have an overflow
	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_RF) {
		pflexpwm->SM[sub_module].STS  = FLEXPWM_SMSTS_RF;	// clear the status
		capture_msw++;
		inc = true;
	}
	// This section handels inputs on Pin A
	// We can probably combine and need to handle multiple pins on same one...
	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFA0) {	// CAPTURE FLAG A0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL2;
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFA0;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(1, capture, 0);
	}

	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFA1) {	// CAPTURE FLAG A0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL3;
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFA1;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(1, capture, 1);
	}
	// This section handels inputs on Pin B
	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFB0) {	// CAPTURE FLAG B0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL4;				// Guessing ? 
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFB0;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(2, capture, 0);
	}
	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFB1) {	// CAPTURE FLAG B0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL5;				// Guessing ? 
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFB1;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(2, capture, 1);
	}

	// This section handels inputs on Pin X
	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFX0) {	// CAPTURE FLAG X0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL0;				// 
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFX0;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(0, capture, 0);
	}

	if (pflexpwm->SM[sub_module].STS & FLEXPWM_SMSTS_CFX1) {	// CAPTURE FLAG X0 (HINT)
		capture = pflexpwm->SM[sub_module].CVAL1;				// Guessing? 
		pflexpwm->SM[sub_module].STS = FLEXPWM_SMSTS_CFX1;		// clear status
		if (capture <= 0xE000 || !inc) {
			capture |= (capture_msw << 16);
		} else {
			capture |= ((capture_msw - 1) << 16);
		}
		processChannelISR(0, capture, 1);
	}
}
#endif
