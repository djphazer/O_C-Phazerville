#pragma once

#include <Audio.h>
#include "../dsputils.h"

// AudioEffectGrit — 4-mode distortion/lo-fi processor.
//
// Modes:
//   CLIP  — hard clipping at an adjustable threshold
//   SAT   — soft saturation via x/(1+|x|*k) rational approximation
//   CRUSH — bit-depth reduction (0–15 bits discarded)
//   DECI  — sample-rate decimation (sample-hold 1–16×)
//
// Post-distortion: single-pole IIR lowpass (Tone) on the wet signal.
// Wet/dry mix applied after tone filtering.
//
// All DSP parameters are volatile (set from Controller() ISR, read by
// audio interrupt update()).

class AudioEffectGrit : public AudioStream {
public:
    static const uint8_t MODE_CLIP  = 0;
    static const uint8_t MODE_SAT   = 1;
    static const uint8_t MODE_CRUSH = 2;
    static const uint8_t MODE_DECI  = 3;

    AudioEffectGrit() : AudioStream(1, input_queue_array_) {}

    void Acquire() {
        tone_z1_    = 0.0f;
        deci_hold_  = 0;
        deci_count_ = 0;
    }
    void Release() {
        tone_z1_    = 0.0f;
        deci_hold_  = 0;
        deci_count_ = 0;
    }

    // Called from Controller() (~150 Hz) — precompute derived values
    void setMode(uint8_t m)   { mode_  = m & 0x03; }

    // drive: 1.0 = unity, up to ~10.0
    void setDrive(float d)    { drive_ = d; }

    // amt: 0.0–1.0
    void setAmt(float a)      { amt_ = a; }

    // tone_coeff: precomputed 1-pole LP coefficient (computed by applet)
    void setToneCoeff(float c) { tone_coeff_ = c; }

    // wet/dry gains (precomputed equal-power)
    void setMix(float wet, float dry) { mix_wet_ = wet; mix_dry_ = dry; }

    void update() override {
        audio_block_t* in = receiveReadOnly(0);
        if (!in) return;

        audio_block_t* out = allocate();
        if (!out) { release(in); return; }

        const uint8_t  mode      = mode_;
        const float    drive     = drive_;
        const float    amt       = amt_;
        const float    tone_c    = tone_coeff_;
        const float    wet       = mix_wet_;
        const float    dry_g     = mix_dry_;

        // Precompute mode-specific constants from amt
        // CLIP: threshold in normalised float (amt=1.0 → no clip, amt=0.0 → threshold≈0)
        const float clip_thresh = 0.05f + amt * 0.95f;  // 0.05–1.0
        // SAT:  k scales saturation knee; higher k = harder knee
        const float sat_k       = (1.0f - amt) * 0.01f + amt * 30.0f; // 0.01–30
        // CRUSH: bits to discard (0 = clean, 15 = 1-bit)
        const int   crush_bits  = (int)(amt * 15.0f + 0.5f);
        // DECI:  hold period (1 = clean, 16 = heavy)
        const int   deci_hold_target = 1 + (int)(amt * 15.0f + 0.5f);

        float tone_z1   = tone_z1_;
        int16_t dh      = deci_hold_;
        int     dc      = deci_count_;

        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float raw = in->data[i] * (1.0f / 32768.0f);
            float x   = raw * drive;
            float w;

            switch (mode) {
                case MODE_CLIP:
                    w = x;
                    if      (w >  clip_thresh) w =  clip_thresh;
                    else if (w < -clip_thresh) w = -clip_thresh;
                    break;

                case MODE_SAT:
                    w = x / (1.0f + fabsf(x) * sat_k);
                    break;

                case MODE_CRUSH: {
                    int16_t q = (int16_t)(x * 32767.0f);
                    if (crush_bits > 0) { q = (q >> crush_bits) << crush_bits; }
                    w = q * (1.0f / 32768.0f);
                    break;
                }

                case MODE_DECI:
                default:
                    if (++dc >= deci_hold_target) {
                        dh = in->data[i];
                        dc = 0;
                    }
                    w = dh * (1.0f / 32768.0f);
                    break;
            }

            // Tone: single-pole IIR lowpass on wet signal
            tone_z1 += tone_c * (w - tone_z1);
            w = tone_z1;

            // Wet/dry mix
            float y = w * wet + raw * dry_g;
            out->data[i] = Clip16(y * 32767.0f);
        }

        tone_z1_    = tone_z1;
        deci_hold_  = dh;
        deci_count_ = dc;

        transmit(out);
        release(out);
        release(in);
    }

private:
    audio_block_t* input_queue_array_[1];

    volatile uint8_t mode_       = MODE_CLIP;
    volatile float   drive_      = 1.0f;
    volatile float   amt_        = 0.5f;
    volatile float   tone_coeff_ = 1.0f;   // 1.0 = fully open (no LP filtering)
    volatile float   mix_wet_    = 1.0f;
    volatile float   mix_dry_    = 0.0f;

    float   tone_z1_    = 0.0f;
    int16_t deci_hold_  = 0;
    int     deci_count_ = 0;
};
