/* Audio Library Note Frequency Detection & Guitar/Bass Tuner
 * Copyright (c) 2015, Colin Duffy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef ARDUINO_TEENSY41

#include <Arduino.h>
#include "SafeNoteFrequencyAnalyzer.h"
#include "utility/dspinst.h"
#include "arm_math.h"
#include <cmath>

// #define HALF_BLOCKS AUDIO_GUITARTUNER_BLOCKS * 64

// initial declarations of low-pass filter variables (FILTER CURRENTLY DISABLED)
// float SafeNoteFrequencyAnalyzer::lpf_alpha = 0.0f;
// float SafeNoteFrequencyAnalyzer::lpf_state = 0.0f;
// float SafeNoteFrequencyAnalyzer::lpf_cutoff = 0.0f;
// bool  SafeNoteFrequencyAnalyzer::lpf_initiated = false;

/**
 *  Copy internal blocks of data to class buffer
 *
 *  @param destination destination address
 *  @param source      source address
 */
static void copy_buffer(void *destination, const void *source) {
    memcpy(destination, source, AUDIO_BLOCK_SAMPLES * sizeof(int16_t)); // AUDIO_BLOCK_SAMPLES samples copied are 2 bytes each
}

// Derive alpha for LPF from cutoff (Hz) and sample rate.
// α = 1 - exp(-2π fc / Fs)
static inline float lpf_alpha_from_cutoff(float cutoff_hz, float sample_rate_hz) {
  if (cutoff_hz <= 0.0f || sample_rate_hz <= 0.0f) return 0.0f; // disabled / invalid
  if (cutoff_hz > 0.45f * sample_rate_hz) cutoff_hz = 0.45f * sample_rate_hz; // below Nyquist
  const float x = -2.0f * 3.1415926535f * cutoff_hz / sample_rate_hz;
  float a = 1.0f - expf(x);                       // 0 < a < 1
  if (a < 1.0e-6f) a = 1.0e-6f;                   // numeric floor
  if (a > 1.0f)    a = 1.0f;
  return a;
}

/**
 *  Virtual function to override from Audio Library
 */
void SafeNoteFrequencyAnalyzer::update( void ) {
    
    audio_block_t *block;

    // lightweight timing: record first block arrival for the current buffer,
    // and compute buffer-fill latency when the buffer becomes full.
    const uint32_t now_us = (uint32_t)micros();
    
    block = receiveReadOnly();
    if (!block) return;
    if (state==0) first_block_time_us = now_us;
    
    if ( !enabled ) {
        release( block );
        return;
    }
    
    if ( next_buffer ) {
        blocklist1[state++] = block;
    } else {
        blocklist2[state++] = block;
    }

    if (state >= needed_blocks) {
        // compute start index so we copy the tail window
        int start = state - needed_blocks;  // 0-based within the current list
        int dst_idx = 0;
        for (int i = 0; i < needed_blocks; ++i) {
            const audio_block_t* srcblk = next_buffer ? blocklist1[start + i]
                                                      : blocklist2[start + i];
            copy_buffer(AudioBuffer + dst_idx, srcblk->data);
            // optionally enable this to prefilter: filter_inplace_block(AudioBuffer + dst_idx);
            dst_idx += AUDIO_BLOCK_SAMPLES;
        }

        // mark that we are ready to process this sliding window
        process_buffer = true;
        process();
        first_run = false;
        // When we’ve consumed a block, release the **oldest** one to avoid leaks:
        // (only if state has grown beyond needed_blocks)
        if (state > needed_blocks) {
            int release_idx = 0;  // release the oldest of the queue
            if (next_buffer) { release(blocklist1[release_idx]); blocklist1[release_idx] = nullptr; }
            else              { release(blocklist2[release_idx]); blocklist2[release_idx] = nullptr; }

            // shift remaining pointers left by one (cheap memmove of pointers, not audio)
            if (next_buffer) memmove(&blocklist1[0], &blocklist1[1], (state-1)*sizeof(audio_block_t*));
            else             memmove(&blocklist2[0], &blocklist2[1], (state-1)*sizeof(audio_block_t*));
            state -= 1;
        }
    }
    
}

// low pass filtering for YIN optimization. filters audio buffer in place to save memory
void SafeNoteFrequencyAnalyzer::filter_inplace_block(int16_t *dst) {
  if (lpf_cutoff <= 0.0f || lpf_alpha <= 0.0f) return; // disabled
  if (!lpf_initiated) { lpf_state = (float)dst[0]; lpf_initiated = true; } // ensure state is set

  float y = lpf_state; // scratch variable for state

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    y += lpf_alpha * ((float)dst[i] - y);  // one-pole LPF
    int32_t out = (int32_t)lrintf(y);      // clamp to int16
    if (out > INT16_MAX) out = INT16_MAX;
    else if (out < INT16_MIN) out = INT16_MIN;
    dst[i] = (int16_t)out;
  }

  lpf_state = y; // state taken from last output block, persist to next buffer chunk 
}

/**
 *  Start the Yin algorithm
 *
 *  TODO: Significant speed up would be to use spectral domain to find fundamental frequency.
 *  This paper explains: https://aubio.org/phd/thesis/brossier06thesis.pdf -> Section 3.2.4
 *  page 79. Might have to downsample for low fundmental frequencies because of fft buffer
 *  size limit.
 */
void SafeNoteFrequencyAnalyzer::process( void ) {

    // const uint32_t now_us = (uint32_t)micros();
    // first_block_time_us = now_us;
    const int16_t *p = AudioBuffer;
    uint16_t cycles = 64;
    uint16_t tau = tau_global;

    do {
        uint16_t x = 0;
        uint64_t sum = 0;
        // using XMAX as the bound for the below while loop introduces instability at low pitches
        // uint16_t XMAX = (tau < half_window_samples) ? (uint16_t)(half_window_samples - tau) : 0;
        // if (XMAX == 0) break; // nothing valid to sum for this tau
        do {
            int16_t current, lag, delta;

            lag = *((int16_t *)p + (x + tau));
            current = *((int16_t *)p + x);
            delta = (current - lag);
            sum += delta * delta;
            x += 4;

            lag = *((int16_t *)p + (x + tau));
            current = *((int16_t *)p + x);
            delta = (current - lag);
            sum += delta * delta;
            x += 4;

            lag = *((int16_t *)p + (x + tau));
            current = *((int16_t *)p + x);
            delta = (current - lag);
            sum += delta * delta;
            x += 4;

            lag = *((int16_t *)p + (x + tau));
            current = *((int16_t *)p + x);
            delta = (current - lag);
            sum += delta * delta;
            x += 4;
        } while (x < half_window_samples);

        // update running stats, estimate tau for next cycle
        uint64_t rs = running_sum;
        rs += sum;
        yin_buffer[yin_idx] = sum * tau;
        rs_buffer[yin_idx] = rs;
        running_sum = rs;
        yin_idx = (++yin_idx >= 5) ? 0 : yin_idx;
        tau = estimate(yin_buffer, rs_buffer, yin_idx, tau);

        // tau = 0 means we've found a pitch
        if (tau == 0) {
            tau_global = 1; // reset to 1 for now.
            process_buffer = false;
            new_output = true;
            yin_idx = 1;
            running_sum = 0;
            return;
        }
    } while (--cycles);

    // Reset per-buffer state before returning to caller
    if (tau >= half_window_samples) {
        process_buffer = false;
        new_output = false;
        yin_idx = 1;
        running_sum = 0;
        tau_global = 1; // conservative restart on miss
        return;
    }

    tau_global = tau;
    // last_buffer_latency_us = (uint32_t)((uint32_t)micros() - first_block_time_us );
}


/**
 *  check the sampled data for fundamental frequency
 *
 *  @param yin  buffer to hold sum*tau value
 *  @param rs   buffer to hold running sum for sampled window
 *  @param head buffer index
 *  @param tau  lag we are currently working on gets incremented
 *
 *  @return tau
 */
uint16_t SafeNoteFrequencyAnalyzer::estimate( uint64_t *yin, uint64_t *rs, uint16_t head, uint16_t tau ) {
    const uint64_t *y = ( uint64_t * )yin;
    const uint64_t *r = ( uint64_t * )rs;
    uint16_t _tau, _head;
    const float thresh = yin_threshold;
    _tau = tau;
    _head = head;

    if ( _tau > 4 ) {
        
        uint16_t idx0, idx1, idx2;
        idx0 = _head;
        idx1 = _head + 1;
        idx1 = ( idx1 >= 5 ) ? 0 : idx1;
        idx2 = head + 2;
        idx2 = ( idx2 >= 5 ) ? 0 : idx2;
        
        float s0, s1, s2;
        s0 = ( ( float )*( y+idx0 ) / *( r+idx0 ) );
        s1 = ( ( float )*( y+idx1 ) / *( r+idx1 ) );
        s2 = ( ( float )*( y+idx2 ) / *( r+idx2 ) );
        
        if ( s1 < thresh && s1 < s2 ) {
            uint16_t period = _tau - 3;
            periodicity = 1 - s1;
            // do not update data if periodicity drops by at least 0.05
            // this helps with low frequency jitteriness as YIN struggles to find a good match the lower we get our latency
            if ((periodicity - p_last_accepted) <= -0.05f || periodicity < 0.90f){
                return 0;
            }
            p_last_accepted = periodicity; // now that we know we aren't accepting a dud, save this value
            data = period + 0.5f * ( s0 - s2 ) / ( s0 - 2.0f * s1 + s2 );
            return 0;
        }
    }
    return _tau + 1;
}

/**
 *  Initialise
 *
 *  @param threshold Allowed uncertainty
 */
void SafeNoteFrequencyAnalyzer::begin( float threshold , float cutoff_hz) {

    size_t n = AUDIO_GUITARTUNER_BLOCKS * 128;
    AudioBuffer = reinterpret_cast<int16_t*>(
        ::operator new[](n * sizeof(int16_t), std::align_val_t(4)));

    // choosing a window in ms based on the lowest note we care to detect. higher values increase latency
    const float window_ms = 42.0f;   // this is what's going to affect the lowest pitch we can track; use 18–24 ms range
    window_samples = (uint16_t)lrintf(AUDIO_SAMPLE_RATE_EXACT * (window_ms / 1000.0f));

    // round up to blocks of 128
    needed_blocks = (window_samples + AUDIO_BLOCK_SAMPLES - 1) / AUDIO_BLOCK_SAMPLES;
    if (needed_blocks < 4) needed_blocks = 4;                                                // safety
    if (needed_blocks > AUDIO_GUITARTUNER_BLOCKS) needed_blocks = AUDIO_GUITARTUNER_BLOCKS;  // cap at our buffer
    HALF_BLOCKS = (uint16_t)((uint32_t)needed_blocks * AUDIO_BLOCK_SAMPLES / 2);

    // Derive sample counts
    window_samples      = (uint16_t)(needed_blocks * AUDIO_BLOCK_SAMPLES);
    half_window_samples = (uint16_t)(window_samples >> 1);

    // initialize actual YIN parameters
    __disable_irq( );
    process_buffer = false;
    yin_threshold  = threshold;
    lpf_cutoff     = (float)cutoff_hz;
    lpf_alpha      = lpf_alpha_from_cutoff(lpf_cutoff, AUDIO_SAMPLE_RATE_EXACT);             // Cache alpha unless cutoff changes
    periodicity    = 0.0f;
    next_buffer    = true;
    running_sum    = 0;
    tau_global     = 1;
    first_run      = true;
    yin_idx        = 1;
    enabled        = true;
    state          = 0;
    data           = 0.0f;
    __enable_irq( );
}

/**
 *  available
 *
 *  @return true if data is ready else false
 */
bool SafeNoteFrequencyAnalyzer::available( void ) {
    __disable_irq( );
    bool flag = new_output;
    if ( flag ) new_output = false;
    __enable_irq( );
    return flag;
}

/**
 *  read processes the data samples for the Yin algorithm.
 *
 *  @return frequency in hertz
 */
float SafeNoteFrequencyAnalyzer::read( void ) {
    __disable_irq( );
    float d = data;
    __enable_irq( );
    return AUDIO_SAMPLE_RATE_EXACT / d;
}

/**
 *  Periodicity of the sampled signal from Yin algorithm from read function.
 *
 *  @return periodicity
 */
float SafeNoteFrequencyAnalyzer::probability( void ) {
    __disable_irq( );
    float p = periodicity;
    __enable_irq( );
    return p;
}

/**
 *  Initialise parameters.
 *
 *  @param thresh    Allowed uncertainty
 */
void SafeNoteFrequencyAnalyzer::threshold( float p ) {
    __disable_irq( );
    yin_threshold = p;
    __enable_irq( );
}

// get delay in milliseconds from buffer processing
uint32_t SafeNoteFrequencyAnalyzer::get_buffer_delay( void ){
    return last_buffer_latency_us / 1000; // convert to ms
}

void SafeNoteFrequencyAnalyzer::end() {
    
    // Prevent update() from accepting/queuing new blocks while we clean up
    enabled = false;

    __disable_irq();

    // drain any queued input blocks (non-owning pointers returned by receiveReadOnly)
    audio_block_t *b;
    while ((b = receiveReadOnly()) != nullptr) {
        release(b);   // return block to audio pool
    }

    // Release any queued blocks while IRQs are still disabled to avoid races/double-free.
    for (int i = 0; i < AUDIO_GUITARTUNER_BLOCKS; ++i) {
        if (blocklist1[i]) { release(blocklist1[i]); blocklist1[i] = nullptr; }
        if (blocklist2[i]) { release(blocklist2[i]); blocklist2[i] = nullptr; }
    }

    state = 0;
    process_buffer = false;
    first_run = true;
    new_output = false;
    lpf_initiated = false;
    yin_idx = 1;
    running_sum = 0;
    tau_global = 1;

    int16_t *buf_to_free = AudioBuffer;
    AudioBuffer = nullptr;
    __enable_irq();

    // Free heap memory outside IRQ-disabled region
    if (buf_to_free) ::operator delete[](buf_to_free, std::align_val_t(4));

} // flush and disable to save CPU usage
void SafeNoteFrequencyAnalyzer::pause_switch(bool p) {} // quick on/off switch without teardown (like for audio being unplugged)
bool SafeNoteFrequencyAnalyzer::isEnabled() { return enabled; } // get enabled state for TuneTracker functionality

#endif
