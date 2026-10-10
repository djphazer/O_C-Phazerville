#pragma once

// Float32 port of AudioEffectClouds (see AudioEffectClouds.h): same grain
// scheduler (centred density: 0=silence, neg=periodic, pos=stochastic), same
// rect→tri→Hann window morph from the shared Q15 Hann LUT (identical window
// values — one table in flash serves both ports), same Hermite-interpolated
// grain reads. The record buffer stores float32, so grain playback and the
// feedback path never truncate to int16; Hermite taps read floats directly
// (no per-tap int→float conversion). The feedback write into the buffer keeps
// the original's double saturation — feedback term clamped to full scale,
// then the input+feedback sum clamped again — so the feedback loop stays
// bounded exactly like the int16 version's rail clipping (±1.0 here being the
// float image of the int16 rails). Grain output leaves unclipped: headroom
// stays float until the applet's edge converter.
//
// Buffer memory follows the int16 scheme (ExtAudioBuffer/extmem_calloc: PSRAM
// when present): same sample counts, 4 bytes/sample instead of 2.

#include "AudioBuffer.h"
#include "AudioEffectClouds.h" // CloudsCircBuffer<T> template + shared Hann LUT
#include "../dsputils.h"
#include "../src/extern/stmlib_utils_random.h"
#include "../extern/f32/AudioStream_F32.h"
#include <Audio.h>

class AudioEffectCloudsF32 : public AudioStream_F32 {
public:
    static const size_t CLOUDS_BUFFER_SAMPLES = AUDIO_SAMPLE_RATE; // ~1 sec
    static const int    MAX_GRAINS = 12;

    AudioEffectCloudsF32(size_t buf_len = CLOUDS_BUFFER_SAMPLES)
        : AudioStream_F32(1, input_queue_array), g_buffer(buf_len) {}

    void Acquire() { g_buffer.Acquire(); }
    void Release() { g_buffer.Release(); }
    bool IsReady() const { return g_buffer.IsReady(); }

    // Setters — called from Controller() at ISR rate (~16.6 kHz).
    // update() snapshots them once per block.
    void setPosition(float p)        { pos_      = p; }
    // density ∈ [−1, +1]: 0=silence, neg=regular periodic, pos=stochastic
    void setDensity(float d)         { density_  = d; }
    void setSize(float secs)         { size_     = secs; }
    void setSpray(float s)           { spray_    = s; }
    void setPitch(float r)           { pitch_    = r; }
    void setPitchSpread(float semis) { psprd_    = semis; }
    // texture ∈ [0, 1]: 0=rect, 0.5=triangle, 1.0=Hann
    void setTexture(float t)         { texture_  = t; }
    // feedback ∈ [0, 1]: fraction of grain cloud fed back into record buffer
    void setFeedback(float f)        { feedback_ = f; }
    void setFreeze(bool f)           { freeze_   = f; }

    uint8_t ActiveGrainCount() const {
        uint8_t n = 0;
        for (const auto& g : grains) if (g.active) n++;
        return n;
    }

    void update() override {
        audio_block_f32_t* in  = receiveReadOnly_f32(0);
        audio_block_f32_t* out = AudioStream_F32::allocate_f32();
        if (!out) {
            if (in) AudioStream_F32::release(in);
            return;
        }

        // Snapshot volatile params once for this block.
        const float  cur_pos      = pos_;
        const float  cur_density  = density_;
        const float  cur_size     = size_;
        const float  cur_spray    = spray_;
        const float  cur_pitch    = pitch_;
        const float  cur_psprd    = psprd_;
        const float  cur_texture  = texture_;
        const float  cur_feedback = feedback_;
        const bool   cur_freeze   = freeze_;
        const size_t buf_size     = g_buffer.NumSamples;

        if (!g_buffer.IsReady()) {
            // No PSRAM — pass through unchanged.
            if (in) {
                memcpy(out->data, in->data, AUDIO_BLOCK_SAMPLES * sizeof(float));
            } else {
                std::fill_n(out->data, AUDIO_BLOCK_SAMPLES, 0.0f);
            }
            AudioStream_F32::transmit(out);
            AudioStream_F32::release(out);
            if (in) AudioStream_F32::release(in);
            return;
        }

        float* buf = g_buffer.RawBuffer();

        // ── Write incoming audio (+ feedback from previous block) unless frozen ─
        if (!cur_freeze) {
            if (cur_feedback > 0.0f && in) {
                // Mix live input with previous block's feedback, then write.
                // Same double saturation as the int16 version (which clipped
                // the feedback term to the int16 rails, then clipped the sum
                // again on storage): keeps the feedback loop bounded.
                float tmp[AUDIO_BLOCK_SAMPLES];
                for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                    float fb = feedback_buf_[i];
                    if (fb > 1.0f)  fb = 1.0f;
                    if (fb < -1.0f) fb = -1.0f;
                    float s = in->data[i] + fb;
                    if (s > 1.0f)  s = 1.0f;
                    if (s < -1.0f) s = -1.0f;
                    tmp[i] = s;
                }
                g_buffer.Write(tmp);
            } else if (in) {
                g_buffer.Write(in->data, AUDIO_BLOCK_SAMPLES);
            }
        }

        // ── Pass 1: grain scheduling ───────────────────────────────────────────
        // density=0 → no grains. neg → periodic. pos → stochastic (random advance).
        if (cur_density != 0.0f) {
            const float abs_density = cur_density < 0.0f ? -cur_density : cur_density;
            const bool  stochastic  = cur_density > 0.0f;

            // Cap density so avg concurrent grains stays < MAX_GRAINS.
            const float capped = (abs_density * cur_size < (float)(MAX_GRAINS - 1))
                ? abs_density : (float)(MAX_GRAINS - 1) / cur_size;

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                float advance = capped / AUDIO_SAMPLE_RATE_EXACT;
                if (stochastic) {
                    // Randomise advance in [0.5×, 1.5×] for a stochastic cloud.
                    advance *= 0.5f + stmlib::Random::GetFloat();
                }
                spawn_phase_ += advance;
                if (spawn_phase_ >= 1.0f) {
                    spawn_phase_ -= 1.0f;
                    spawnGrain(cur_pos, cur_size, cur_spray, cur_pitch,
                               cur_psprd, cur_texture, buf_size);
                }
            }
        }

        // ── Pass 2: grain processing (grain-outer, sample-inner) ───────────────
        //
        // Window morph — no switch, no sinf:
        //   w = tri + tex_lo*(1−tri) + tex_hi*(hann−tri)
        //   At tex_lo=0, tex_hi=0: pure triangle.
        //   At tex_lo=1, tex_hi=0: pure rect (=1).
        //   At tex_lo=1, tex_hi=1: pure Hann.
        //   (tex_lo, tex_hi precomputed at spawn — 0 per-sample cost.)
        //
        // Hann: read shared Q15 LUT, convert with one fmul — same values as
        // the int16 port, no sinf in the hot path.
        float accum_buf[AUDIO_BLOCK_SAMPLES] = {};

        for (auto& g : grains) {
            if (!g.active) continue;

            float       t      = (float)g.phase * g.inv_grain_len;
            const float t_step = g.inv_grain_len;

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                // ── Window ────────────────────────────────────────────────────
                float tri = (t < 0.5f) ? (2.0f * t) : (2.0f - 2.0f * t);
                uint8_t lut_idx = (uint8_t)(t * 255.0f + 0.5f);
                float hann = (float)AudioEffectClouds::hann_lut_[lut_idx] * (1.0f / 32767.0f);
                float w = tri + g.tex_lo * (1.0f - tri) + g.tex_hi * (hann - tri);
                t += t_step;

                // ── Hermite interpolated read (float taps — no conversion) ─────
                size_t idx  = (size_t)g.read_ptr;
                float  frac = g.read_ptr - (float)idx;
                size_t im1  = (idx == 0)             ? buf_size - 1       : idx - 1;
                size_t i1   = (idx + 1 >= buf_size)  ? 0                  : idx + 1;
                size_t i2   = (idx + 2 >= buf_size)  ? idx + 2 - buf_size : idx + 2;
                accum_buf[i] += InterpHermite(buf[im1], buf[idx],
                                              buf[i1],  buf[i2], frac) * w;

                g.read_ptr += g.pitch;
                if (g.read_ptr >= (float)buf_size) g.read_ptr -= (float)buf_size;
                if (g.read_ptr < 0.0f)             g.read_ptr += (float)buf_size;
                if (++g.phase >= g.grain_len) { g.active = false; break; }
            }
        }

        // ── Pass 3: scale to output; capture feedback for next block ───────────
        // No clip here: headroom stays float until the applet's edge converter.
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float scaled = accum_buf[i] * GRAIN_SCALE;
            out->data[i] = scaled;
            feedback_buf_[i] = scaled * cur_feedback;
        }

        AudioStream_F32::transmit(out);
        AudioStream_F32::release(out);
        if (in) AudioStream_F32::release(in);
    }

private:
    // Equal-power normalisation for up to 16 grains. 1/sqrt(16) = 0.25.
    static constexpr float GRAIN_SCALE = 0.25f;

    struct Grain {
        bool   active        = false;
        float  read_ptr      = 0.0f;
        float  pitch         = 1.0f;
        size_t grain_len     = 0;
        size_t phase         = 0;
        float  inv_grain_len = 0.0f;
        float  tex_lo        = 0.0f; // blend weight: rect→tri  (precomputed at spawn)
        float  tex_hi        = 0.0f; // blend weight: tri→Hann  (precomputed at spawn)
    } grains[MAX_GRAINS];

    CloudsCircBuffer<float> g_buffer;
    audio_block_f32_t* input_queue_array[1];

    // Feedback accumulator — member array (not stack) to avoid stack pressure.
    float feedback_buf_[AUDIO_BLOCK_SAMPLES] = {};

    // Grain scheduling state (audio ISR only — not volatile).
    float spawn_phase_ = 0.0f;

    // Volatile params: written from Controller() ISR, read from audio interrupt.
    volatile float pos_      = 0.5f;
    volatile float density_  = 0.0f;  // −1..+1 (0=silence)
    volatile float size_     = 0.1f;  // seconds
    volatile float spray_    = 0.0f;
    volatile float pitch_    = 1.0f;  // ratio
    volatile float psprd_    = 0.0f;  // semitones spread
    volatile float texture_  = 0.5f;  // 0=rect, 0.5=tri, 1.0=Hann
    volatile float feedback_ = 0.0f;  // 0..1
    volatile bool  freeze_   = false;

    void spawnGrain(float cur_pos, float cur_size, float cur_spray,
                    float cur_pitch, float cur_psprd, float cur_texture,
                    size_t buf_size) {
        Grain* g = nullptr;
        for (auto& gr : grains) {
            if (!gr.active) { g = &gr; break; }
        }
        if (!g) return; // all slots busy

        // Position scatter — same as Mist.
        float max_spray = cur_pos < 1.0f - cur_pos ? cur_pos : 1.0f - cur_pos;
        float eff_spray = cur_spray < max_spray ? cur_spray : max_spray;
        float scatter   = eff_spray * (stmlib::Random::GetFloat() * 2.0f - 1.0f);
        float eff_pos   = cur_pos + scatter;
        if (eff_pos < 0.0f) eff_pos = 0.0f;
        if (eff_pos > 1.0f) eff_pos = 1.0f;

        // Absolute start position: pos=0 → write head (live), pos=1 → oldest.
        float rptr = (float)g_buffer.GetWriteIx() - (float)buf_size * eff_pos;
        if (rptr < 0.0f) rptr += (float)buf_size;

        size_t glen = (size_t)(cur_size * AUDIO_SAMPLE_RATE_EXACT);
        if (glen < (size_t)AUDIO_BLOCK_SAMPLES) glen = AUDIO_BLOCK_SAMPLES;
        if (glen > buf_size / 2)                glen = buf_size / 2;

        float grain_pitch = cur_pitch;
        if (cur_psprd > 0.0f) {
            float rand_semis = cur_psprd * (stmlib::Random::GetFloat() * 2.0f - 1.0f);
            grain_pitch *= SemitonesToRatio(rand_semis);
        }

        // Precompute texture blend weights (per-grain constant — not per-sample).
        // Region 1 (texture 0→0.5): tex_lo 0→1, tex_hi stays 0. (rect→tri morph)
        // Region 2 (texture 0.5→1): tex_lo stays 1, tex_hi 0→1. (tri→Hann morph)
        float tex_lo, tex_hi;
        if (cur_texture <= 0.5f) {
            tex_lo = cur_texture * 2.0f;
            tex_hi = 0.0f;
        } else {
            tex_lo = 1.0f;
            tex_hi = (cur_texture - 0.5f) * 2.0f;
        }

        g->read_ptr      = rptr;
        g->pitch         = grain_pitch;
        g->grain_len     = glen;
        g->phase         = 0;
        g->inv_grain_len = 1.0f / (float)(glen - 1);
        g->tex_lo        = tex_lo;
        g->tex_hi        = tex_hi;
        g->active        = true;
    }
};
