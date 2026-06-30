// #pragma once
// #include <Audio.h>  // pulls in SafeNoteFrequencyAnalyzer etc.

// class SafeNoteFrequencyAnalyzer : public SafeNoteFrequencyAnalyzer {
// public:
//     using SafeNoteFrequencyAnalyzer::SafeNoteFrequencyAnalyzer; // inherit ctor

//     void end(); // flush and disable to save CPU usage
//     void pause(bool p = true); // quick on/off switch without teardown (like for audio being unplugged)
// };

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

#ifndef SafeNoteFrequencyAnalyzer_h_
#define SafeNoteFrequencyAnalyzer_h_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
/***********************************************************************
 *              Safe to adjust these values below                      *
 *                                                                     *
 *  This parameter defines the size of the buffer.                     *
 *                                                                     *
 *  1.  AUDIO_GUITARTUNER_BLOCKS -  Buffer size is 128 * AUDIO_BLOCKS. *
 *                      The more AUDIO_GUITARTUNER_BLOCKS the lower    *
 *                      the frequency you can detect. The default      *
 *                      (24) is set to measure down to 29.14 Hz        *
 *                      or B(flat)0.                                   *
 *                                                                     *
 ***********************************************************************/
//one block = ~2.9ms of signal time per https://forum.pjrc.com/index.php?threads/different-range-fft-algorithm.32252/page-2
#define AUDIO_GUITARTUNER_BLOCKS  24
/***********************************************************************/
class SafeNoteFrequencyAnalyzer : public AudioStream {
public:
    /**
     *  constructor to setup Audio Library and initialize
     *
     *  @return none
     */
    SafeNoteFrequencyAnalyzer(void) : AudioStream(1, inputQueueArray), enabled(false), new_output(false) {
        inputQueueArray[0] = nullptr;
        for (int i = 0; i < AUDIO_GUITARTUNER_BLOCKS; ++i) {
            blocklist1[i] = nullptr;
            blocklist2[i] = nullptr;
        }
    }
    
    /**
     *  initialize variables and start conversion
     *
     *  @param threshold Allowed uncertainty
     *  @param cpu_max   How much cpu usage before throttling
     *
     *  @return none
     */
    void begin(float threshold, float lp_cutoff_hz);
    
    /**
     *  sets threshold value
     *
     *  @param thresh
     *  @return none
     */
    void threshold(float p);
    
    /**
     *  triggers true when valid frequency is found
     *
     *  @return flag to indicate valid frequency is found
     */
    bool available(void);
    /**
     *  get frequency
     *
     *  @return frequency in hertz
     */
    float read(void);
    
    /**
     *  get predicitity
     *
     *  @return probability of frequency found
     */
    float probability(void);
    
    /**
     *  Audio Library calls this update function ~2.9ms
     *
     *  @return none
     */
    virtual void update(void);

    /**
     *  Returns delay
     * 
     *  @return delay in milliseconds
     */
    uint32_t get_buffer_delay(void);

    void end(); // flush and disable to save CPU usage
    void pause_switch(bool p); // quick on/off switch without teardown (like for audio being unplugged)
    bool isEnabled(); // get enabled state for TuneTracker functionality
    
private:
    /**
     *  check the sampled data for fundamental frequency
     *
     *  @param yin  buffer to hold sum*tau value
     *  @param rs   buffer to hold running sum for sampled window
     *  @param head buffer index
     *  @param tau  lag we are currently working on this gets incremented
     *
     *  @return tau
     */
    uint16_t estimate( uint64_t *yin, uint64_t *rs, uint16_t head, uint16_t tau );

    /**
     *  compartamentalized function to adapt frequency search area based on how high or low we are
     */
    void adapt_window_from_tau(float tau_samples);

    // recovery path functions for tau window shortening/lengthening
    void set_window_blocks(uint16_t nb);
    void widen_to_max_window();

    /**
     *  process audio data
     *
     *  @return none
     */
    void process( void );

    /**
     *  performs LPF on audio buffer based on specified cuttoff freq,
     *  before it is processed on the next run into the YIN algorithm
     *
     *  @return none
     */
    void filter_inplace_block(int16_t *dst);

    /**
     *  Variables
     */
    uint64_t running_sum = 0;
    uint16_t tau_global = 0;
    uint64_t  yin_buffer[5] = {0};
    uint64_t  rs_buffer[5] = {0};
    // comment out old definition for now to test new way
    //    int16_t  AudioBuffer[AUDIO_GUITARTUNER_BLOCKS*128] __attribute__ ( ( aligned ( 4 ) ) );
    // heap-managed buffer so it can be freed on end()/Unload()
    int16_t *AudioBuffer = nullptr;
    uint8_t  yin_idx = 0, state = 0;
    float    periodicity = 0.0, yin_threshold = 0.0, data = 0.0;
    bool     enabled = 0, next_buffer = 0, first_run = 0;
    volatile bool new_output = 0, process_buffer = 0;
    audio_block_t *blocklist1[AUDIO_GUITARTUNER_BLOCKS];
    audio_block_t *blocklist2[AUDIO_GUITARTUNER_BLOCKS];
    audio_block_t *inputQueueArray[1];

    //custom params for optimizing YIN
    uint16_t needed_blocks = 0;     // how many 128-sample blocks our window needs
    uint16_t window_samples = 0;    // number of samples our analysis uses
    uint16_t half_window_samples = 0; // half the number of samples for processing
    uint16_t HALF_BLOCKS = 0;       // redefinition of constant, to accomodate sliding window algo

    // one-pole low-pass params (simple, cheap smoothing)
    float lpf_alpha = 0.0;   // default smoothing coefficient (0..1)
    float lpf_state = 0.0;     // persistent filter state across samples/blocks
    float lpf_cutoff = 0.0;    // low-pass filter cutoff frequency modifiable by user
    bool lpf_initiated = false; // initialize LPF state on first use to avoid a startup step

    // delay calculation params
    uint32_t first_block_time_us = 0;
    uint32_t last_buffer_latency_us = 0; // can read via debugger/Serial if needed

    // --- confidence/holding & smoothing state ---
    float sm_logf = NAN;           // log2 smoothing state (initialized lazy)
    float last_good_period = 0.0f; // period (samples) of last accepted lock
    bool  have_good = false;       // have we accepted at least one good lock?
    uint8_t hold_count = 0;        // how many frames left to hold last_good_period

    // tuning constants for gating/hold
    static constexpr float   CONF_RISE = 0.97f;  // need this to accept a *new* lock
    static constexpr float   CONF_FALL = 0.95f;  // lower bar to keep an *existing* lock (hysteresis)
    static constexpr uint8_t HOLD_MAX  = 8;      // frames to keep last good on low confidence (≈ 6×process cadence)

    // octave / decay guards
    uint8_t octave_suspect = 0;     // consecutive potential octave-down hits
    static constexpr float  OCTAVE_DOWN_RATIO = 1.6f; // period growth treated as octave-ish
    static constexpr uint8_t OCTAVE_CONFIRM   = 4;    // need this many consecutive before accepting

    uint64_t tau_min_next = 0;
    uint64_t tau_max_next = 0;
    float p_last_accepted = 1.00f; // last accepted periodicity, for gating what values we accept
};
#endif
