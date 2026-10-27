#pragma once

#include "AudioBuffer.h"
#include "../dsputils.h"
#include "../dsputils_arm.h"
#include "../src/extern/stmlib_utils_random.h"
#include <Audio.h>

// Thin subclass of ExtAudioBuffer exposing write position and raw buffer
// access needed by AudioEffectMist for absolute-position grain reads.
template <typename T = int16_t>
class MistCircBuffer : public ExtAudioBuffer<T> {
public:
    using ExtAudioBuffer<T>::ExtAudioBuffer;
    size_t GetWriteIx() const { return this->write_ix; }
    T*     RawBuffer()  const { return this->buffer; }
    bool   IsReady()    const { return this->buffer != nullptr; }
};

// Clouds-inspired live granular processor.
//
// Improvements over AudioEffectMist:
//   Texture   — continuous window morph: rect (0) → triangle (0.5) → Hann (1.0)
//               Uses a 256-entry Q15 Hann LUT (512 bytes flash) — no sinf in hot loop.
//   Density   — centred: 0=silence, <0=regular periodic, >0=stochastic cloud
//   Feedback  — fraction of grain output fed back into the record buffer
//
// Beads-inspired additions (Mutable Instruments Beads is MIT-licensed):
//   Reverse   — grains play backward, Beads-style (negated read direction)
//   Character — 4 media-emulation settings à la Beads' quality modes:
//               0=Clean, 1=Tape (soft saturation), 2=Vintage (12-bit + drive),
//               3=Dirty (8-bit + heavy saturation). Applied on the record path,
//               so only the granulated signal is degraded, like Beads'
//               converter clock.
//   Chaos     — Beads-style attenurandomizer: per-grain randomization of grain
//               size (±75%) and texture (±50%).
//
// Hot-loop design:
//   Single grain inner loop (no switch) — texture morph is 6 float ops per sample.
//   Hann via LUT read + one fmul: no transcendental functions in the hot path.
//   Hermite interpolation: 6 fmul + 5 fadd — single-cycle on Cortex-M7 FPU.
//   Feedback held in a float[128] member array (not stack) to avoid stack pressure.
class AudioEffectMist : public AudioStream {
public:
    static const size_t BUFFER_SAMPLES = AUDIO_SAMPLE_RATE; // ~1 sec
    static const int    MAX_GRAINS = 12;

    AudioEffectMist(size_t buf_len = BUFFER_SAMPLES)
        : AudioStream(1, input_queue_array), g_buffer(buf_len) {}

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
    // Beads-inspired: reverse grain playback, media character, per-grain chaos
    void setReverse(bool r)          { reverse_   = r; }
    // character ∈ {0,1,2,3}: 0=Clean, 1=Tape, 2=Vintage, 3=Dirty
    void setCharacter(int c)         { character_ = c; }
    // chaos ∈ [0, 1]: per-grain size/texture randomization amount
    void setChaos(float c)           { chaos_     = c; }

    uint8_t ActiveGrainCount() const {
        uint8_t n = 0;
        for (const auto& g : grains) if (g.active) n++;
        return n;
    }

    void update() override {
        audio_block_t* in  = receiveReadOnly(0);
        audio_block_t* out = allocate();
        if (!out) { if (in) release(in); return; }

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
        const bool   cur_reverse  = reverse_;
        const int    cur_character = character_;
        const float  cur_chaos    = chaos_;
        const size_t buf_size     = g_buffer.NumSamples;

        if (!g_buffer.IsReady()) {
            // No PSRAM — pass through unchanged.
            if (in) {
                memcpy(out->data, in->data, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
            } else {
                memset(out->data, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
            }
            transmit(out, 0);
            release(out);
            if (in) release(in);
            return;
        }

        int16_t* buf = g_buffer.RawBuffer();

        // ── Write incoming audio (+ feedback from previous block) unless frozen ─
        // Character (Beads-style media emulation) is applied here so only the
        // granulated signal is degraded — the dry path stays pristine.
        if (!cur_freeze && in) {
            if (cur_feedback > 0.0f || cur_character > 0) {
                // Mix live input with previous block's feedback, degrade, write.
                int16_t tmp[AUDIO_BLOCK_SAMPLES];
                for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                    int32_t s = (int32_t)in->data[i];
                    if (cur_feedback > 0.0f) s += Clip16(feedback_buf_[i]);
                    if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
                    if (cur_character > 0) s = DegradeCharacter((int16_t)s, cur_character);
                    tmp[i] = (int16_t)s;
                }
                g_buffer.Write(tmp);
            } else {
                g_buffer.Write(in);
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
                               cur_psprd, cur_texture, cur_reverse, cur_chaos,
                               buf_size);
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
        // Hann: read Q15 LUT, convert with one fmul. No sinf in the hot path.
        float accum_buf[AUDIO_BLOCK_SAMPLES] = {};

        for (auto& g : grains) {
            if (!g.active) continue;

            float       t      = (float)g.phase * g.inv_grain_len;
            const float t_step = g.inv_grain_len;

            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                // ── Window ────────────────────────────────────────────────────
                float tri = (t < 0.5f) ? (2.0f * t) : (2.0f - 2.0f * t);
                uint8_t lut_idx = (uint8_t)(t * 255.0f + 0.5f);
                float hann = (float)hann_lut_[lut_idx] * (1.0f / 32767.0f);
                float w = tri + g.tex_lo * (1.0f - tri) + g.tex_hi * (hann - tri);
                t += t_step;

                // ── Hermite interpolated read ──────────────────────────────────
                size_t idx  = (size_t)g.read_ptr;
                float  frac = g.read_ptr - (float)idx;
                size_t im1  = (idx == 0)             ? buf_size - 1       : idx - 1;
                size_t i1   = (idx + 1 >= buf_size)  ? 0                  : idx + 1;
                size_t i2   = (idx + 2 >= buf_size)  ? idx + 2 - buf_size : idx + 2;
                accum_buf[i] += InterpHermite((float)buf[im1], (float)buf[idx],
                                              (float)buf[i1],  (float)buf[i2], frac) * w;

                g.read_ptr += g.pitch;
                if (g.read_ptr >= (float)buf_size) g.read_ptr -= (float)buf_size;
                if (g.read_ptr < 0.0f)             g.read_ptr += (float)buf_size;
                if (++g.phase >= g.grain_len) { g.active = false; break; }
            }
        }

        // ── Pass 3: scale, clip to output; capture feedback for next block ─────
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float scaled = accum_buf[i] * GRAIN_SCALE;
            out->data[i] = Clip16(scaled);
            feedback_buf_[i] = scaled * cur_feedback;
        }

        transmit(out, 0);
        release(out);
        if (in) release(in);
    }

public:
    // 256-entry Q15 Hann window LUT: round(sin²(π×i/255) × 32767), i = 0..255.
    // const static → placed in .rodata (flash) by the linker. 512 bytes, zero SRAM cost.
    // Public so the float32 port (AudioEffectCloudsF32) shares the same table.
    static const int16_t hann_lut_[256];

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

    MistCircBuffer<int16_t> g_buffer;
    audio_block_t* input_queue_array[1];

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
    volatile bool  reverse_  = false;  // Beads-style reverse grain playback
    volatile int   character_ = 0;     // 0=Clean, 1=Tape, 2=Vintage, 3=Dirty
    volatile float chaos_    = 0.0f;   // per-grain size/texture randomization

    // Beads-style media emulation for the record path.
    // 0=Clean (bypass), 1=Tape (soft saturation), 2=Vintage (12-bit + drive),
    // 3=Dirty (8-bit + heavy saturation).
    static int16_t DegradeCharacter(int16_t s, int character) {
        float f = (float)s * (1.0f / 32768.0f);
        switch (character) {
            case 1: // Tape
                f = f / (1.0f + 0.8f * fabsf(f));
                break;
            case 2: // Vintage
                f = f / (1.0f + 1.5f * fabsf(f));
                f = (float)((int)(f * 2047.0f)) * (1.0f / 2047.0f);
                break;
            default: // Dirty (3)
                f = f / (1.0f + 3.0f * fabsf(f));
                f = (float)((int)(f * 127.0f)) * (1.0f / 127.0f);
                break;
        }
        int32_t o = (int32_t)(f * 32767.0f);
        if (o > 32767) o = 32767; else if (o < -32768) o = -32768;
        return (int16_t)o;
    }

    void spawnGrain(float cur_pos, float cur_size, float cur_spray,
                    float cur_pitch, float cur_psprd, float cur_texture,
                    bool cur_reverse, float cur_chaos,
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

        // Chaos (Beads attenurandomizer-style): per-grain size (±75%) and
        // texture (±50%) randomization.
        float eff_size = cur_size;
        float eff_tex  = cur_texture;
        if (cur_chaos > 0.0f) {
            eff_size *= 1.0f + cur_chaos * (stmlib::Random::GetFloat() * 2.0f - 1.0f) * 0.75f;
            if (eff_size < 0.01f) eff_size = 0.01f;
            eff_tex += cur_chaos * (stmlib::Random::GetFloat() * 2.0f - 1.0f) * 0.5f;
            if (eff_tex < 0.0f) eff_tex = 0.0f; else if (eff_tex > 1.0f) eff_tex = 1.0f;
        }

        // Absolute start position: pos=0 → write head (live), pos=1 → oldest.
        float rptr = (float)g_buffer.GetWriteIx() - (float)buf_size * eff_pos;
        if (rptr < 0.0f) rptr += (float)buf_size;

        size_t glen = (size_t)(eff_size * AUDIO_SAMPLE_RATE_EXACT);
        if (glen < (size_t)AUDIO_BLOCK_SAMPLES) glen = AUDIO_BLOCK_SAMPLES;
        if (glen > buf_size / 2)                glen = buf_size / 2;

        // Beads-style reverse: negate the read direction.
        float grain_pitch = cur_pitch * (cur_reverse ? -1.0f : 1.0f);
        if (cur_psprd > 0.0f) {
            float rand_semis = cur_psprd * (stmlib::Random::GetFloat() * 2.0f - 1.0f);
            grain_pitch *= SemitonesToRatio(rand_semis);
        }

        // Precompute texture blend weights (per-grain constant — not per-sample).
        // Region 1 (texture 0→0.5): tex_lo 0→1, tex_hi stays 0. (rect→tri morph)
        // Region 2 (texture 0.5→1): tex_lo stays 1, tex_hi 0→1. (tri→Hann morph)
        float tex_lo, tex_hi;
        if (eff_tex <= 0.5f) {
            tex_lo = eff_tex * 2.0f;
            tex_hi = 0.0f;
        } else {
            tex_lo = 1.0f;
            tex_hi = (eff_tex - 0.5f) * 2.0f;
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

// ── Hann LUT ──────────────────────────────────────────────────────────────────
// round(sin²(π × i/255) × 32767) for i = 0..255. 512 bytes in .rodata (flash).
const int16_t AudioEffectMist::hann_lut_[256] = {
       0,     5,    20,    45,    80,   124,   179,   243,
     317,   401,   495,   598,   711,   833,   965,  1106,
    1257,  1416,  1585,  1763,  1949,  2145,  2349,  2561,
    2782,  3011,  3249,  3494,  3747,  4008,  4276,  4552,
    4834,  5124,  5421,  5724,  6034,  6350,  6672,  7000,
    7334,  7673,  8018,  8367,  8722,  9081,  9444,  9812,
   10184, 10559, 10938, 11321, 11706, 12094, 12485, 12879,
   13274, 13671, 14070, 14470, 14872, 15274, 15677, 16081,
   16484, 16888, 17291, 17694, 18096, 18497, 18897, 19295,
   19691, 20085, 20477, 20867, 21254, 21638, 22019, 22396,
   22770, 23139, 23505, 23866, 24223, 24575, 24922, 25264,
   25601, 25932, 26257, 26576, 26889, 27195, 27495, 27789,
   28075, 28354, 28626, 28891, 29148, 29397, 29638, 29871,
   30096, 30313, 30521, 30721, 30912, 31094, 31267, 31432,
   31587, 31732, 31869, 31996, 32114, 32222, 32320, 32409,
   32488, 32557, 32617, 32666, 32706, 32736, 32756, 32766,
   32766, 32756, 32736, 32706, 32666, 32617, 32557, 32488,
   32409, 32320, 32222, 32114, 31996, 31869, 31732, 31587,
   31432, 31267, 31094, 30912, 30721, 30521, 30313, 30096,
   29871, 29638, 29397, 29148, 28891, 28626, 28354, 28075,
   27789, 27495, 27195, 26889, 26576, 26257, 25932, 25601,
   25264, 24922, 24575, 24223, 23866, 23505, 23139, 22770,
   22396, 22019, 21638, 21254, 20867, 20477, 20085, 19691,
   19295, 18897, 18497, 18096, 17694, 17291, 16888, 16484,
   16081, 15677, 15274, 14872, 14470, 14070, 13671, 13274,
   12879, 12485, 12094, 11706, 11321, 10938, 10559, 10184,
    9812,  9444,  9081,  8722,  8367,  8018,  7673,  7334,
    7000,  6672,  6350,  6034,  5724,  5421,  5124,  4834,
    4552,  4276,  4008,  3747,  3494,  3249,  3011,  2782,
    2561,  2349,  2145,  1949,  1763,  1585,  1416,  1257,
    1106,   965,   833,   711,   598,   495,   401,   317,
     243,   179,   124,    80,    45,    20,     5,     0,
};
