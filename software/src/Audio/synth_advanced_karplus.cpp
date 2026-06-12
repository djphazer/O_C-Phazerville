// Out-of-line definitions for AudioSynthAdvancedKarplus.
//
// Kept in a .cpp file so that FLASHMEM is honoured by the linker. Inline
// class-member definitions in headers end up in COMDAT (.gnu.linkonce)
// sections and the section attribute is silently ignored; out-of-line
// definitions in a translation unit do not have this problem.

/* ^ actually, let's not put this one in FLASHMEM,
 * because we want it to be fast, yeah?
 * - djphazer
 */

#ifdef ARDUINO_TEENSY41

#include "synth_advanced_karplus.h"

// --- Lifecycle -----------------------------------------------------------

void AudioSynthAdvancedKarplus::Acquire() {
    if (delay_line_) return;
    delay_line_ = static_cast<float*>(calloc(BUFFER_SIZE, sizeof(float)));
    if (delay_line_) {
        write_idx_        = 0;
        iir_state_        = 0.0f;
        trigger_pending_  = false;
        excite_remaining_ = 0;
    }
}

void AudioSynthAdvancedKarplus::Release() {
    if (!delay_line_) return;
    free(delay_line_);
    delay_line_ = nullptr;
}

// --- Parameter setters ---------------------------------------------------

void AudioSynthAdvancedKarplus::setFrequency(float hz) {
    target_hz_ = constrain(hz, MIN_HZ, MAX_HZ);
    recalculateDelay();
}

void AudioSynthAdvancedKarplus::setDecay(float d) {
    decay_param_ = constrain(d, 0.0f, 1.0f);
}

void AudioSynthAdvancedKarplus::setBrightness(float b) {
    brightness_param_ = constrain(b, 0.0f, 1.0f);
    iir_alpha_ = 0.05f + brightness_param_ * 0.95f;
    recalculateDelay();
}

void AudioSynthAdvancedKarplus::setBody(float body) {
    body_param_ = constrain(body, 0.0f, 1.0f);
}

// --- Trigger -------------------------------------------------------------

void AudioSynthAdvancedKarplus::noteOn(float velocity) {
    trigger_velocity_ = constrain(velocity, 0.0f, 1.0f);
    trigger_pending_  = true;
}

// --- Private helper ------------------------------------------------------

void AudioSynthAdvancedKarplus::recalculateDelay() {
    float filter_delay = (1.0f - iir_alpha_) / iir_alpha_;
    float d = AUDIO_SAMPLE_RATE_EXACT / target_hz_ - filter_delay;
    if (d < 2.0f) d = 2.0f;
    target_delay_ = (d < BUFFER_SIZE - 2) ? d : BUFFER_SIZE - 2;
}

// --- Audio DSP (hot path) ------------------------------------------------

__attribute__((noinline)) void AudioSynthAdvancedKarplus::updateCore() {
    audio_block_t* out = allocate();
    if (!out) return;

    // --- Pitch-compensated loop gain (computed once per block) -----------
    // Map decay param to target time: T = 0.02 * e^(d * 8.699)  [0.02 s…6 s]
    // ρ^L = exp(-L/(T_s*fs)) gives perceptual decay independent of pitch.
    float T_s = 0.02f * fastexp(decay_param_ * 8.699f);
    float loop_gain = fastexp(-target_delay_ / (T_s * AUDIO_SAMPLE_RATE_EXACT));
    if (loop_gain > 0.9999990f) loop_gain = 0.9999990f;

    // --- Handle pending trigger (sets up progressive excitation) ----------
    if (trigger_pending_) {
        trigger_pending_ = false;
        int n = static_cast<int>(target_delay_);
        if (n < 2)                              n = 2;
        if (n >= static_cast<int>(BUFFER_SIZE)) n = BUFFER_SIZE - 1;
        excite_remaining_ = n;
        excite_phase_     = 0.0f;
        excite_phase_inc_ = KS_TWO_PI / static_cast<float>(n);
        excite_scale_     = trigger_velocity_ * 32767.0f;
        iir_state_        = 0.0f;
    }

    // --- Per-sample KS feedback loop -------------------------------------
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {

        float read_pos = static_cast<float>(write_idx_) - target_delay_;
        if (read_pos < 0.0f) read_pos += static_cast<float>(BUFFER_SIZE);

        uint32_t r0_raw = static_cast<uint32_t>(read_pos);
        float    frac   = read_pos - static_cast<float>(r0_raw);
        uint32_t r0     = r0_raw & BUFFER_MASK;
        uint32_t r1     = (r0 + 1) & BUFFER_MASK;

        float raw = delay_line_[r0] + frac * (delay_line_[r1] - delay_line_[r0]);

        float new_sample;

        if (excite_remaining_ > 0) {
            noise_seed_ = noise_seed_ * 1664525u + 1013904223u;
            float noise = static_cast<float>(static_cast<int32_t>(noise_seed_))
                          * (1.0f / 2147483648.0f);
            float sine  = arm_sin_f32(excite_phase_);
            float excite = ((1.0f - body_param_) * noise + body_param_ * sine)
                           * excite_scale_;
            if (excite >  32767.0f) excite =  32767.0f;
            if (excite < -32767.0f) excite = -32767.0f;

            iir_state_ = iir_alpha_ * excite + (1.0f - iir_alpha_) * iir_state_;
            new_sample  = excite;
            excite_phase_ += excite_phase_inc_;
            excite_remaining_--;
        } else {
            float filtered = iir_alpha_ * raw + (1.0f - iir_alpha_) * iir_state_;
            iir_state_     = filtered;
            new_sample     = filtered * loop_gain;
        }

        delay_line_[write_idx_] = new_sample;
        write_idx_ = (write_idx_ + 1) & BUFFER_MASK;

        out->data[i] = Clip16(new_sample);
    }

    transmit(out);
    release(out);
}

#endif // ARDUINO_TEENSY41
