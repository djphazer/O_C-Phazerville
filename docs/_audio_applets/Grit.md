---
layout: default
---
# Grit — Distortion / Lo-Fi Processor

(TODO: screenshot)

## License

MIT License  
(c) 2026 Andy Jenkinson (uglifruit) — with ClaudeCode.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## Overview

Grit is a four-mode distortion and lo-fi processor. It covers the range from smooth amp-style saturation to hard digital clipping, bit crushing, and sample-rate degradation — all in a single applet slot.

Available as a **MONO** applet.

---

## Signal Flow

```
Audio In ──► AudioEffectGrit (distortion + tone) ──► Audio Out
```

Wet/dry mixing is handled internally. The dry signal is the unprocessed input; the wet signal is the distorted output after tone filtering.

---

## Parameters

| Param | Range | CV | Description |
|-------|-------|----|-------------|
| **Mode** | CLIP / SAT / CRUS / DECI | — | Distortion algorithm (see below) |
| **Drv** | 0–100% | drv_cv | Input gain before distortion. 0% = ×1 (unity), 100% = ×10. |
| **Thr/Kne/Bit/Dec** | 0–100% | amt_cv | Effect depth — meaning depends on Mode (see below). Label changes with Mode. |
| **Ton** | 0–100% | tone_cv | Post-distortion lowpass. 0% ≈ 800 Hz (dark), 100% = fully open. |
| **Mix** | 0–100% | mix_cv | Wet/dry balance. 0% = fully dry, 100% = fully wet. |

---

## Modes

### CLIP — Hard Clipping

Clips the signal at an adjustable threshold. **Amt (Thr)** controls the threshold level:

- **100%** — threshold at full scale; no audible clipping (clean pass-through at low drive)
- **50%** — clips loud transients and peaks; adds harmonic distortion
- **0%** — threshold near zero; severe hard limiting

Classic transistor/diode-style distortion at high drive. Use Tone to roll off the harsh high-frequency content that hard clipping adds.

### SAT — Soft Saturation

Smooth amplitude-limiting via a rational approximation (`x / (1 + |x| × k)`). **Amt (Kne)** controls the saturation knee:

- **0%** — nearly linear; very slight softening at peaks
- **50%** — moderate saturation; warm, tape-like compression
- **100%** — hard saturation; similar to clipping but with a softer transition

Unlike CLIP, SAT never introduces a hard discontinuity. The result is smooth even at high drive settings.

### CRUS — Bit Crushing

Reduces the bit depth of the audio signal. **Amt (Bit)** controls how many bits are discarded:

- **0%** — 16-bit (full resolution); clean
- **50%** — ~8-bit; noticeable quantisation noise and steps
- **100%** — 1-bit; extreme digital grit / square-wave distortion

Low bit depths produce characteristic quantisation noise that sits above the signal. Combine with Tone to tame the high-frequency noise content.

### DECI — Decimation (Sample Hold)

Reduces the effective sample rate by holding each sample for multiple output samples. **Amt (Dec)** controls the hold factor:

- **0%** — 1× hold (full sample rate); clean
- **50%** — ~8× hold; ZX Spectrum / 8-bit computer character
- **100%** — 16× hold; heavy stairstepping, very low perceived sample rate

Decimation produces a characteristic "staircase" distortion that is different in character from bit crushing — it degrades the time resolution of the signal rather than its amplitude resolution.

---

## Controls

| Control | Action |
|---------|--------|
| Encoder (browse) | Navigate cursor through parameters |
| Encoder (edit) | Adjust parameter value; on Mode — cycle CLIP / SAT / CRUS / DECI |
| Button | Enter/exit edit mode; assign CV input when cursor is on a CV slot |
| **Aux button** | Cycle mode (CLIP → SAT → CRUS → DECI → CLIP) |

---

## Tone

The Tone parameter applies a single-pole lowpass filter to the wet (distorted) signal only. This is useful for:

- Taming the harsh upper harmonics added by CLIP and CRUS
- Creating a "muffled" or "lo-fi cassette" character when combined with SAT
- Blending in a softened, de-emphasised distortion alongside the dry signal with Mix

Tone does not affect the dry signal path.

---

## CV / Gate Inputs

Drv, Amt, Tone, and Mix each have an assignable CV input slot. Mode has no CV. Assign inputs using the in-applet CV input editor (press button when cursor is on a CV label).

---

## Suggested Patches

**Warm overdrive**
- Mode = SAT, Drv = 40%, Amt = 60%, Tone = 70%, Mix = 100%
- Drive into saturation, roll off harsh highs with Tone.

**Fuzz/hard clip**
- Mode = CLIP, Drv = 80%, Amt = 30%, Tone = 50%, Mix = 100%
- Extreme drive into a low threshold → classic fuzz character.

**8-bit digital crunch**
- Mode = CRUS, Drv = 0%, Amt = 50%, Tone = 60%, Mix = 70%
- Mid bit depth; Tone softens quantisation noise; Mix blends back some clean signal.

**Lo-fi sample degradation**
- Mode = DECI, Drv = 0%, Amt = 40%, Tone = 80%, Mix = 100%
- Moderate decimation; sounds like a low-quality digital recording.

**Parallel grit blend**
- Any mode, Mix = 30–50%
- Mixes a small amount of distorted signal back with the clean source — adds density and presence without overwhelming the original.

---

## Technical Notes

- All DSP runs in the audio interrupt (update()). No heap allocation, no buffers — only a few float state variables per instance.
- The `x / (1 + |x| × k)` saturation approximation is a Padé-type rational function. It is continuous and differentiable (no discontinuity), so it adds only even and odd harmonics smoothly rather than the harsh odd-harmonic content of hard clipping.
- Tone coefficient is computed at control rate (~150 Hz) using `expf()` — not in the audio hot loop.
- RAM1 impact: negligible.

---

## Files

| File | Purpose |
|------|---------|
| `audio_applets/GritApplet.h` | UI and control layer (HemisphereAudioApplet) |
| `Audio/AudioEffectGrit.h` | DSP engine (AudioStream subclass) |
