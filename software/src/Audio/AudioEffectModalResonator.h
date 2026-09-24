#pragma once

// AudioEffectModalResonator — parallel bank of modal resonators
//
// Implements a modal synthesis resonator bank: NUM_MODES parallel bandpass
// filters (Chamberlin state-variable filter) excited by the input signal.
// All coefficient math runs in updateCoeffs() at ~150 Hz (Controller rate).
// The audio-interrupt update() loop is pure multiply-accumulate.
//
// Filter topology: Chamberlin SVF (not DF1 biquad).
//   notch = x - damp * bp
//   lp   += f * bp
//   bp   += f * (notch - lp)   → bp is the bandpass output
// The SVF bandpass has no DC path by construction — no per-mode x_prev[]
// differencing required. A single-pole DC blocker (z=0.995) is applied to the
// summed output to remove any residual DC from floating-point accumulation.
//
// Stiffness model (Rings-style additive stretch):
//   stretch starts at 1.0, increments by stiffness each mode.
//   stiffness is derived from structure: negative = compressed (flute/clarinet),
//   positive = stretched (piano/bell). Self-limits via per-step decay.
//
// Q model:
//   Base Q spans ~500 to ~500 000 across the damping range.
//   Q-loss (derived from brightness + structure) compounds per mode so
//   upper modes decay faster — physically correct for resonant structures.
//
// Parameters (all normalised 0–1):
//   freq_hz    — fundamental frequency in Hz
//   structure  — spectrum stiffness: 0 = slightly compressed, 0.5 = harmonic,
//                1 = strongly stretched inharmonic
//   brightness — Q-loss per mode: 1 = all modes ring equally, 0 = only mode 0
//   damping    — base Q / decay time: 0 = short click, 1 = long sustain
//   position   — excitation-point comb notch (Rings-style): 0.0–1.0
//
// Thread safety: same assumption as AudioSynthAdvancedKarplus — aligned 32-bit
// float writes from Controller() are naturally atomic on Cortex-M7.
//
// Denormals: FPU flush-to-zero (FPSCR FZ+DN) enabled for update() duration.

#include <Audio.h>
#include "../dsputils.h"

class AudioEffectModalResonator : public AudioStream {
public:
    static constexpr int NUM_MODES = 12;

    AudioEffectModalResonator() : AudioStream(1, input_queue_array_) {}

    // --- Lifecycle -----------------------------------------------------------

    void Acquire() {
        for (int k = 0; k < NUM_MODES; k++) {
            lp_[k] = bp_[k] = 0.0f;
            f_[k]    = 0.0f;
            damp_[k] = 0.1f;
            mode_gain_[k] = 0.0f;
        }
        excite_peak_    = 0;
        output_peak_    = 0;
        strike_pending_ = false;
        strike_vel_     = 0.0f;
        strike_remain_  = 0;
        excite_lp_      = 0.0f;
        excite_bp_      = 0.0f;
        excite_f_       = 0.0f;
        excite_damp_    = 0.0f;
        dc_x1_ = dc_y1_ = 0.0f;
        updateCoeffs(261.63f, 0.5f, 0.7f, 0.5f, 0.25f);
    }

    void Release() {
        for (int k = 0; k < NUM_MODES; k++) lp_[k] = bp_[k] = 0.0f;
        dc_x1_ = dc_y1_ = 0.0f;
    }

    // --- Parameter update (call from Controller, ~150 Hz) -------------------

    __attribute__((noinline))
    void updateCoeffs(float freq_hz, float structure, float brightness,
                      float damping, float position) {

        // --- Rings-Style Exciter Filter Calculation ---
        // Cutoff tracks fundamental frequency and scales up with brightness
        float exciter_hz = freq_hz * (1.0f + brightness * 6.0f); 
        const float FREQ_CEIL = AUDIO_SAMPLE_RATE_EXACT * 0.40f;
        if (exciter_hz > FREQ_CEIL) exciter_hz = FREQ_CEIL;
        
        excite_f_ = 2.0f * sinf(3.14159265f * exciter_hz / AUDIO_SAMPLE_RATE_EXACT);
        // Rings internal exciter uses Q = 1.5. Chamberlin damp = fcoef / Q
        excite_damp_ = excite_f_ / 1.5f; 

        // --- Resonator Bank Calculation ---
        const float Q_MIN  = 1.2f;
        const float Q_LOGR = 6.675f; 
        float q_base = Q_MIN * expf(damping * Q_LOGR);
        float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
        float q_loss_rate = structure * (2.0f - structure) * 0.1f;
        float stiffness = structure * 1.55f - 0.05f;
        float stretch   = 1.0f;
        float gain_sum = 0.0f;

        for (int k = 0; k < NUM_MODES; k++) {
            float f_k = freq_hz * stretch;
            if (f_k > FREQ_CEIL) f_k = FREQ_CEIL;
            if (f_k <      20.0f) f_k =      20.0f;

            f_[k] = 2.0f * sinf(3.14159265f * f_k / AUDIO_SAMPLE_RATE_EXACT);

            float q_safe = q_base;
            if (q_safe < 0.01f) q_safe = 0.01f; 
            float damp_k = f_[k] / q_safe;
            if (damp_k > 1.9f)  damp_k = 1.9f;  
            if (damp_k < 1e-4f) damp_k = 1e-4f; 
            damp_[k] = damp_k;

            stretch += stiffness;
            if (stiffness < 0.0f) stiffness *= 0.93f;
            else                  stiffness *= 0.98f;

            q_loss += q_loss_rate * (1.0f - q_loss);
            q_base *= q_loss;

            float pos_safe = position * 0.98f + 0.01f;
            float sg = sinf(3.14159265f * pos_safe * (float)(k + 1));
            mode_gain_[k] = sg * sg;
            gain_sum += mode_gain_[k];
        }

        if (gain_sum > 0.0f) {
            float inv = 1.0f / gain_sum;
            for (int k = 0; k < NUM_MODES; k++) mode_gain_[k] *= inv;
        }
    }

    // --- Manual strike -------------------------------------------------------

    void strike(float velocity = 1.0f) {
        strike_vel_     = velocity;
        strike_pending_ = true;
    }

    // --- VU readback ---------------------------------------------------------

    uint16_t readPeak() {
        uint16_t p = excite_peak_;
        excite_peak_ = (uint16_t)(p * 220 / 256);
        return p;
    }

    uint16_t readOutputPeak() {
        uint16_t p = output_peak_;
        output_peak_ = (uint16_t)(p * 220 / 256);
        return p;
    }

    // --- AudioStream update --------------------------------------------------

    void update() override {
        audio_block_t* in  = receiveReadOnly(0);
        audio_block_t* out = allocate();

        if (!out) {
            if (in) release(in);
            return;
        }

        // Enable FPU flush-to-zero + default-NaN
        uint32_t fpscr_save;
        __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr_save));
        __asm__ volatile("vmsr fpscr, %0" :: "r"(fpscr_save | 0x03000000u));

        const int16_t* src = in ? in->data : nullptr;
        int16_t*       dst = out->data;

        bool  do_strike = strike_pending_;
        float vel       = strike_vel_;
        if (do_strike) {
            strike_pending_ = false;
            strike_remain_  = AUDIO_BLOCK_SAMPLES;
        }

        // Load into locals for register-friendly inner loop
        float fk[NUM_MODES], dk[NUM_MODES], mg[NUM_MODES];
        float lp[NUM_MODES], bp[NUM_MODES];
        for (int k = 0; k < NUM_MODES; k++) {
            fk[k] = f_[k];
            dk[k] = damp_[k];
            mg[k] = mode_gain_[k];
            float lp_v = lp_[k];
            float bp_v = bp_[k];
            if (lp_v != lp_v || bp_v != bp_v ||
                lp_v > 1e10f || lp_v < -1e10f ||
                bp_v > 1e10f || bp_v < -1e10f) {
                lp_v = 0.0f; bp_v = 0.0f;
            }
            lp[k] = lp_v;
            bp[k] = bp_v;
        }

        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float x = src ? (float)src[i] * (0.125f / 32768.0f) : 0.0f;

            if (strike_remain_ > 0) {
                const bool strike_sample = (strike_remain_ == AUDIO_BLOCK_SAMPLES);
                --strike_remain_;

                // Send a single full-scale impulse into the exciter filter
                float impulse = strike_sample ? vel : 0.0f;

                float excite_notch = impulse - excite_damp_ * excite_bp_;
                excite_lp_ += excite_f_ * excite_bp_;
                excite_bp_ += excite_f_ * (excite_notch - excite_lp_);

                // Feed the filtered impulse into the main modal bank
                x += excite_lp_;
            }

            {
                float ax = x < 0.0f ? -x : x;
                uint32_t ax16 = (uint32_t)(ax * (8.0f * 32767.0f));
                if (ax16 > 32767) ax16 = 32767;
                if (ax16 > excite_peak_) excite_peak_ = (uint16_t)ax16;
            }

            float y = 0.0f;
            for (int k = 0; k < NUM_MODES; k++) {
                float notch = x - dk[k] * bp[k];
                lp[k]      += fk[k] * bp[k];
                bp[k]      += fk[k] * (notch - lp[k]);
                y           += mg[k] * bp[k];
            }

            float y_out = y - dc_x1_ + 0.995f * dc_y1_;
            dc_x1_ = y;
            dc_y1_ = y_out;
            int16_t s = Clip16(y_out * 32767.0f);
            dst[i] = s;
            
            {
                uint16_t av = s < 0 ? (uint16_t)(-s) : (uint16_t)s;
                if (av > output_peak_) output_peak_ = av;
            }
        }

        for (int k = 0; k < NUM_MODES; k++) {
            if (lp[k] >  2.0f) lp[k] =  2.0f;
            if (lp[k] < -2.0f) lp[k] = -2.0f;
            if (bp[k] >  2.0f) bp[k] =  2.0f;
            if (bp[k] < -2.0f) bp[k] = -2.0f;
            lp_[k] = lp[k];
            bp_[k] = bp[k];
        }

        __asm__ volatile("vmsr fpscr, %0" :: "r"(fpscr_save));

        transmit(out, 0);
        release(out);
        if (in) release(in);
    }

private:
    audio_block_t* input_queue_array_[1];

    float f_[NUM_MODES];         
    float damp_[NUM_MODES];      
    float mode_gain_[NUM_MODES]; 

    float lp_[NUM_MODES];        
    float bp_[NUM_MODES];        

    volatile bool  strike_pending_ = false;
    volatile float strike_vel_     = 1.0f;
    int            strike_remain_  = 0;

    // Rings-style internal excitation filter state
    float excite_lp_   = 0.0f;
    float excite_bp_   = 0.0f;
    float excite_f_    = 0.0f;
    float excite_damp_ = 0.0f;

    volatile uint16_t excite_peak_  = 0;
    volatile uint16_t output_peak_  = 0;

    float dc_x1_ = 0.0f;
    float dc_y1_ = 0.0f;
};