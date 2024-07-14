#ifndef OC_FREQMEASURE_H
#define OC_FREQMEASURE_H

#include <Arduino.h>

#if defined(__MK20DX256__)

class FreqMeasureClass {
public:
	// single static instance
	static void begin(void); // pin is fixed to ? (TR?)
	static uint8_t available(void);
	static uint32_t read(void);
	static float countToFrequency(uint32_t count);
	static void end(void);
};

#elif defined(__IMXRT1062__)

class FreqMeasureClass {
public:
	// supports up to 4 simultaneously running instances
	FreqMeasureClass() { running = false; }
	void begin(uint8_t pin = 23); // pin can be 0 (TR1), 1 (TR2), 23 (TR3), 22 (TR4)
	uint8_t available(void);
	uint32_t read(void);
	float countToFrequency(uint32_t count) { return (float)F_BUS_ACTUAL / (float)count; }
	void end(void);
	~FreqMeasureClass() { if (running) end(); }
private:
	static FreqMeasureClass *pin_inst[4];
	static void pin0_isr() { pin_inst[0]->isr(); }
	static void pin1_isr() { pin_inst[1]->isr(); }
	static void pin23_isr() { pin_inst[2]->isr(); }
	static void pin22_isr() { pin_inst[3]->isr(); }
	void isr();
	union {
		IMXRT_FLEXPWM_t *flex;
		IMXRT_TMR_t *quad;
	} timer;
	uint8_t ch;
	uint8_t type; // 0 = FlexPWM, 1 = QuadTimer
	bool running;
	IRQ_NUMBER_t irq;
	volatile uint32_t *muxreg;
	static const int FREQMEASURE_BUFFER_LEN = 12;
	volatile uint32_t buffer_value[FREQMEASURE_BUFFER_LEN];
	volatile uint8_t buffer_head;
	volatile uint8_t buffer_tail;
	uint16_t capture_msw;
	uint32_t capture_previous;
};

#endif

extern FreqMeasureClass FreqMeasure;

#endif

