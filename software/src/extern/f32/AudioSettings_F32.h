
#ifndef _AudioSettings_F32_
#define _AudioSettings_F32_

#include <AudioStream.h> // 16-bit audio for AUDIO_SAMPLE_RATE_EXACT, AUDIO_BLOCK_SAMPLES

class AudioSettings_F32 {
	public:
		AudioSettings_F32(float fs_Hz=AUDIO_SAMPLE_RATE_EXACT, int block_size=AUDIO_BLOCK_SAMPLES) :
			sample_rate_Hz(fs_Hz), audio_block_samples(block_size) {}
		const float sample_rate_Hz;
		const int audio_block_samples;

		float cpu_load_percent(const int n);
		float processorUsage(void);
		float processorUsageMax(void);
		void processorUsageMaxReset(void);
};

#endif
