#ifndef FreqMeasureMultiIMXRT_h
#define FreqMeasureMultiIMXRT_h

#if defined(__IMXRT1062__)

struct freq_pwm_pin_info_struct {
	IMXRT_FLEXPWM_t *pflexpwm;
	uint8_t 		module;  	// 0-3, 0-3
	uint8_t 		channel; 	// 0=X, 1=A, 2=B
	uint8_t 		muxval;  	//
	uint8_t			irq;		// which IRQ to use
	void			(*isr)();  // interrupt handler. 
	volatile 		uint32_t	*select_input_register; // Which register controls the selection
	const uint32_t	select_val;	// Value for that selection
};



class FreqMeasureMulti
{
public:
	bool begin(uint32_t pin);
	bool begin(uint32_t pin, uint8_t mode);
	uint32_t available(void);
	uint32_t read(void);
	uint8_t readLevel(void);
	static float countToFrequency(uint32_t count);
	static float countToNanoseconds(uint32_t count);
	void end(void);
private:
	static const struct freq_pwm_pin_info_struct freq_pwm_pin_info[];
	static FreqMeasureMulti *list[16];
	fmultiRecord buffer_value[FREQMEASUREMULTI_BUFFER_LEN];
	uint8_t buffer_head;
	uint8_t buffer_tail;

	IMXRT_FLEXPWM_t *_pflexpwm;
	void processChannelISR(uint8_t channel, uint32_t capture, uint8_t edge);
	void isr();
	uint8_t last_read_level;

	uint8_t _pin;	// remember the pin number;
	uint8_t _mode;	// remember the mode we are using. 
	uint8_t _channel;
	FreqMeasureMulti *_next = nullptr; // in case we have multiple ones who are on the same module. 
	uint32_t _capture_previous[2];	// Not sure yet
	uint16_t capture_msw = 0;

	static void flexpwm1_0_isr();
	static void flexpwm1_1_isr();
	static void flexpwm1_2_isr();
	static void flexpwm1_3_isr();
	static void flexpwm2_0_isr();
	static void flexpwm2_1_isr();
	static void flexpwm2_2_isr();
	static void flexpwm2_3_isr();
	static void flexpwm3_0_isr();
	static void flexpwm3_1_isr();
	static void flexpwm3_3_isr();
	static void flexpwm4_0_isr();
	static void flexpwm4_1_isr();
	static void flexpwm4_2_isr();
};

#endif // IMXRT	
#endif

