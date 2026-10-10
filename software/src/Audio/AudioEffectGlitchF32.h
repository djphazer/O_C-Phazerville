#pragma once

// Float32 port of AudioEffectGlitch (see AudioEffectGlitch.h): same slice
// capture, loop modes, micro-fades, and Bit/Smp/Off logic, but the circular
// buffer stores float32 and the wet path stays float end to end, so held
// slices and fades never truncate to int16. Bit crush is the one deliberately
// int-domain operation: it quantizes through the same 16-bit shift math as the
// original ((raw >> bits) << bits) so each Bit setting sounds identical; at
// Bit=0 it is a true bypass and full float precision passes through.

#include "AudioBuffer.h"
#include "AudioEffectGlitch.h" // for the GlitchBuffer<T> template
#include "../dsputils.h"
#include "../extern/f32/AudioStream_F32.h"
#include <Audio.h>

class AudioEffectGlitchF32 : public AudioStream_F32 {
public:
    static const size_t GLITCH_BUFFER_SAMPLES = 48000; // 1 sec at 48 kHz
    static const size_t FADE_SAMPLES = 64;             // ~1.3 ms micro-fade

    static const uint8_t MODE_FWD    = 0;
    static const uint8_t MODE_REV    = 1;
    static const uint8_t MODE_PING   = 2;
    static const uint8_t MODE_RATCHET = 3;

    AudioEffectGlitchF32(size_t buf_len = GLITCH_BUFFER_SAMPLES)
        : AudioStream_F32(1, input_queue_array), g_buffer(buf_len) {}

    void Acquire() { g_buffer.Acquire(); }
    void Release() { g_buffer.Release(); }
    bool IsReady() const { return g_buffer.IsReady(); }

    void setHold(bool h)   { hold_   = h; }
    void setFreeze(bool f) { freeze_ = f; }

    void setSliceSamples(size_t n) {
        size_t max_slice = g_buffer.NumSamples / 2;
        if (max_slice < (size_t)AUDIO_BLOCK_SAMPLES) max_slice = AUDIO_BLOCK_SAMPLES;
        if (n < (size_t)AUDIO_BLOCK_SAMPLES) n = AUDIO_BLOCK_SAMPLES;
        if (n > max_slice) n = max_slice;
        slice_samples_ = n;
    }

    void setMode(uint8_t m) { mode_ = m; }
    void setRatchet(uint8_t r) { ratchet_ = r; }
    void setBits(uint8_t b)     { bits_     = b & 0x0F; }
    void setDecimate(uint8_t d) { decimate_ = d & 0x0F; }
    void setOffset(uint8_t o)   { offset_   = o & 0x0F; }

    void update() override {
        audio_block_f32_t* in  = receiveReadOnly_f32(0);
        audio_block_f32_t* out = AudioStream_F32::allocate_f32();
        if (!out) {
            if (in) AudioStream_F32::release(in);
            return;
        }

        // Snapshot volatile params once per block to ensure consistency
        // within the sample loop.
        const bool    cur_hold     = hold_;
        const bool    cur_freeze   = freeze_;
        const size_t  cur_slice    = slice_samples_;
        const uint8_t cur_mode     = mode_;
        const uint8_t cur_ratchet  = ratchet_;
        const uint8_t cur_bits     = bits_;
        const uint8_t cur_decimate = decimate_;
        const uint8_t cur_offset   = offset_;
        const size_t  buf_size     = g_buffer.NumSamples;

        // In RATCHET mode the effective loop is the first 1/ratchet of the slice.
        const size_t loop_len = (cur_mode == MODE_RATCHET && cur_ratchet > 1)
            ? std::max(cur_slice / (size_t)cur_ratchet, (size_t)AUDIO_BLOCK_SAMPLES)
            : cur_slice;

        // Detect rising edge of hold: capture the last cur_slice samples as
        // the frozen slice. Do this BEFORE writing so write_ix still points
        // to where the freshest content ends.
        if (cur_hold && !was_held_) {
            size_t wi = g_buffer.GetWriteIx();
            size_t total_back = ((size_t)cur_offset + 1) * cur_slice;
            if (total_back > buf_size) total_back = buf_size;
            slice_start_ = (wi + buf_size - total_back) % buf_size;
            pos_         = 0;
            ping_fwd_    = true;
        }
        was_held_ = cur_hold;

        // Record incoming audio unless freeze is active.
        if (in && !cur_freeze) g_buffer.Write(in->data, AUDIO_BLOCK_SAMPLES);

        // LIVE FX mode: FWD + offset=0 + hold (and not frozen) routes live audio
        // through bit crush / sample decimation directly, ignoring div/slice logic.
        // When frozen, fall through to the stutter path which reads the frozen buffer.
        if (cur_mode == MODE_FWD && cur_offset == 0 && cur_hold && !cur_freeze) {
            const size_t dec_factor = (size_t)cur_decimate + 1;
            if (in) {
                for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                    if (live_dec_counter_ == 0)
                        live_dec_held_ = in->data[i];
                    float raw = live_dec_held_;
                    if (cur_bits > 0)
                        raw = CrushBits(raw, cur_bits);
                    out->data[i] = raw;
                    if (++live_dec_counter_ >= dec_factor)
                        live_dec_counter_ = 0;
                }
            } else {
                std::fill_n(out->data, AUDIO_BLOCK_SAMPLES, 0.0f);
            }
            AudioStream_F32::transmit(out);
            AudioStream_F32::release(out);
            if (in) AudioStream_F32::release(in);
            return;
        }

        if (!cur_hold || !g_buffer.IsReady()) {
            // BYPASS: pass input through unchanged.
            if (in) {
                memcpy(out->data, in->data, AUDIO_BLOCK_SAMPLES * sizeof(float));
            } else {
                std::fill_n(out->data, AUDIO_BLOCK_SAMPLES, 0.0f);
            }
        } else {
            // STUTTER: loop the frozen slice with optional micro-fades.
            // fade_len = 0 when slice is too short to hold non-overlapping fades.
            const size_t fade_len   = (loop_len >= FADE_SAMPLES * 2) ? FADE_SAMPLES : 0;
            const float  fade_scale = (fade_len > 0) ? (1.0f / (float)fade_len) : 0.0f;

            // Hoist mode check outside the per-sample loop.
            // going_fwd tracks current direction; for PING it toggles each
            // full pass through the slice (handled inside the loop below).
            bool going_fwd = (cur_mode != MODE_REV)
                             && (cur_mode != MODE_PING || ping_fwd_);

            // Sample-rate reduction: quantise position to nearest multiple of
            // dec_factor so pitch and loop length are preserved.
            const size_t dec_factor = (size_t)cur_decimate + 1; // 1 = bypass

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                const size_t p = pos_;

                // Apply temporal quantisation (sample-rate reduction).
                const size_t eff_p = (cur_decimate > 0)
                    ? (p / dec_factor) * dec_factor
                    : p;

                // Compute read pointer with single-branch wrap (no modulo).
                // Slice is constrained to <= buf_size/2 so at most one wrap.
                size_t read_ptr;
                if (going_fwd) {
                    read_ptr = slice_start_ + eff_p;
                    if (read_ptr >= buf_size) read_ptr -= buf_size;
                } else {
                    // REV: start from end of slice and walk backwards.
                    read_ptr = slice_start_ + cur_slice - 1 - eff_p;
                    if (read_ptr >= buf_size) read_ptr -= buf_size;
                }

                // Micro-fade at loop boundaries to suppress discontinuity clicks.
                float fade = 1.0f;
                if (fade_len > 0) {
                    if (p < fade_len) {
                        fade = (float)p * fade_scale;
                    } else if (p >= loop_len - fade_len) {
                        fade = (float)(loop_len - p) * fade_scale;
                    }
                }

                // Bit crush: quantise to (16 - cur_bits) effective bits.
                float raw = g_buffer.ReadAt(read_ptr);
                if (cur_bits > 0) {
                    raw = CrushBits(raw, cur_bits);
                }
                // No Clip16 here: headroom stays float until the edge converter.
                out->data[i] = raw * fade;

                // Advance position within loop; wrap and handle ping-pong toggle.
                pos_++;
                if (pos_ >= loop_len) {
                    pos_ = 0;
                    if (cur_mode == MODE_PING) {
                        ping_fwd_ = !ping_fwd_;
                        going_fwd = ping_fwd_;
                    }
                }
            }
        }

        AudioStream_F32::transmit(out);
        AudioStream_F32::release(out);
        if (in) AudioStream_F32::release(in);
    }

private:
    // Same quantisation the int16 version got for free from its storage:
    // clamp to the 16-bit grid, arithmetic-shift away `bits` LSBs, restore.
    static inline float CrushBits(float x, uint8_t bits) {
        int32_t q = (int32_t)(x * 32768.0f);
        if (q > 32767) q = 32767;
        else if (q < -32768) q = -32768;
        q = (q >> bits) << bits;
        return (float)q * (1.0f / 32768.0f);
    }

    GlitchBuffer<float> g_buffer;
    audio_block_f32_t* input_queue_array[1];

    // Written from Controller() (ISR), read from update() (audio interrupt).
    volatile bool    hold_          = false;
    volatile bool    freeze_        = false;
    volatile size_t  slice_samples_ = GLITCH_BUFFER_SAMPLES / 8; // 125ms default
    volatile uint8_t mode_          = MODE_FWD;
    volatile uint8_t ratchet_       = 2; // 1–6, used by MODE_RATCHET
    volatile uint8_t bits_          = 0; // 0=bypass(16-bit) … 15=1-bit
    volatile uint8_t decimate_      = 0; // 0=bypass … 15=16× sample hold
    volatile uint8_t offset_        = 0; // 0–15 slices back on hold-rise

    // Internal state accessed only from update() — not volatile.
    bool   was_held_    = false;
    size_t slice_start_ = 0;
    size_t pos_         = 0;
    size_t live_dec_counter_ = 0; // sample-hold counter for LIVE FX mode
    float  live_dec_held_    = 0.0f; // last held sample for LIVE FX mode
    bool   ping_fwd_    = true;
};
