// Xenomorphicle: whole-file guard, F32 core is used on T4.1 audio builds only (see README.md)
#ifdef ARDUINO_TEENSY41

#include <Arduino.h>
#include "AudioSettings_F32.h"
#include "AudioStream_F32.h"

float AudioSettings_F32::cpu_load_percent(const int n) {
    //n is the number of cycles
    // #define CYCLE_COUNTER_APPROX_PERCENT(n) (((n) + (F_CPU / 32 / AUDIO_SAMPLE_RATE * AUDIO_BLOCK_SAMPLES / 100)) / (F_CPU / 16 / AUDIO_SAMPLE_RATE * AUDIO_BLOCK_SAMPLES / 100))  removed per @jcj83429 21 Dec 20  defined double
    float foo1 = ((float)(F_CPU / 32))/sample_rate_Hz;
    foo1 *= ((float)audio_block_samples);
    foo1 /= 100.f;
    foo1 += (float)n;
    float foo2 = (float)(F_CPU / 16)/sample_rate_Hz;
    foo2 *= ((float)audio_block_samples);
    foo2 /= 100.f;
    return  foo1 / foo2;

    //return (((n) + (F_CPU / 32 / sample_rate_Hz * audio_block_samples / 100)) / (F_CPU / 16 / sample_rate_Hz * audio_block_samples / 100));
}

float AudioSettings_F32::processorUsage(void) {
    return cpu_load_percent(AudioStream::cpu_cycles_total);
};
float AudioSettings_F32::processorUsageMax(void) {
    return cpu_load_percent(AudioStream::cpu_cycles_total_max);
}
void AudioSettings_F32::processorUsageMaxReset(void) {
    AudioStream::cpu_cycles_total_max = AudioStream::cpu_cycles_total;
}

#endif // ARDUINO_TEENSY41
