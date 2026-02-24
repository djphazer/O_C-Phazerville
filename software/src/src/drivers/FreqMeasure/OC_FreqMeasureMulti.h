#ifndef FreqMeasureMulti_h
#define FreqMeasureMulti_h

#include <Arduino.h>

#define FREQMEASUREMULTI_BUFFER_LEN 24
// capture modes
#define FREQMEASUREMULTI_RAISING 1
#define FREQMEASUREMULTI_FALLING 2
#define FREQMEASUREMULTI_INTERLEAVE 3
#define FREQMEASUREMULTI_SPACE_ONLY 5
#define FREQMEASUREMULTI_MARK_ONLY 6
#define FREQMEASUREMULTI_ALTERNATE 7
// result constants for fmultiRecord.level
#define LEVEL_SPACE_ONLY 0
#define LEVEL_SPACE_MARK 1
#define LEVEL_MARK_SPACE 2
#define LEVEL_MARK_ONLY 3
#define LEVEL_UNDEFINED 255

typedef struct {
	uint8_t level;
	uint32_t count;
} fmultiRecord;

#if defined(__IMXRT1062__)
#include "FreqMeasureMultiIMXRT.h"
#else

// Teensy 3.x and LC
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
	fmultiRecord buffer_value[FREQMEASUREMULTI_BUFFER_LEN];
	uint8_t buffer_head;
	uint8_t buffer_tail;

	friend void ftm0_isr(void);
	void isr(bool inc);
	uint8_t channel;
	uint8_t last_read_level;
	uint32_t raiscap_previous;
	uint32_t fallcap_previous;
	bool act_on_fall, act_on_raise, read_diff;
	bool next_is_falling;
};
#endif //defined(__IMXRT1062__)

extern FreqMeasureMulti FreqMeasureMult;

#endif

