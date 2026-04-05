# FMDrum — FM Synthesis Drum Voice

MIT License  
(c) 2026 Andy Jenkinson (uglifruit) — with ClaudeCode.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## Overview

FMDrum is a single-voice FM synthesis drum synthesiser. A sine-wave modulator drives a phase-modulated sine carrier, both shaped by exponential decay envelopes. A separate white noise generator — high-pass filtered and independently enveloped — adds transient snap and texture. Audio passthrough lets you blend in a live signal alongside the drum voice.

Six built-in presets cover the classic electronic drum palette; a random preset is also available for unexpected starting points.

MONO only.

---

## Signal Flow

```
Modulator (sine) ──► mod_vca (FM index env) ──► Carrier PM input
                                                  Carrier (sine) ──► amp_vca (amp env) ──► Mixer ch0
White Noise ──► HPF (~1kHz) ──► noise_vca (noise env) ──────────────────────────────────► Mixer ch1
Audio In ───────────────────────────────────────────────────────────────────────────────► Mixer ch2
                                                                                           │
                                                                                           ▼
                                                                                       Audio Out
```

The three mixer channels are summed to the output. Noise and passthrough levels are set by the **Noi** and **Mix** parameters. Carrier amplitude is always 1.0; loudness is shaped entirely by the amplitude envelope.

---

## Parameters

All parameters are accessible via a scrollable single-column list. 6 rows are visible at a time; scroll up/down arrows appear when more rows are available above or below.

| Param | Range | Description |
|-------|-------|-------------|
| **TRG** | gate input | Trigger input. Each rising edge fires a new hit — resets all three envelopes to full and resets modulator phase for a consistent transient. |
| **Type** | Kick / Snare / HiHat / O.Hat / Tom / Clap / Rnd | Preset selector. Loads a set of parameters tuned for that drum type. **Rnd** randomises all values. |
| **Pit** | 10–2000 Hz | Base carrier frequency (pitch of the drum). |
| **Dec** | 10–2000 ms | Amplitude envelope decay time. |
| **Swp** | 0–100% | Pitch sweep depth. At 100%, the carrier starts at 2× **Pit** and decays to **Pit** over the amplitude envelope — gives the characteristic kick "thud". |
| **Rto** | 0.1–10.0× | FM ratio (modulator frequency ÷ carrier frequency). Integer ratios give harmonic timbres; non-integer ratios give metallic/inharmonic sounds. |
| **FMi** | 0–100% | FM index — depth of the modulation at envelope peak. Higher values = more aggressive distortion at the attack. |
| **FMd** | 10–1000 ms | FM index envelope decay time. Set shorter than **Dec** for a punch that settles to a clean tone; set longer for sustained FM texture. |
| **Noi** | 0–100% | Noise level. Scales the high-pass filtered noise into the output mixer. |
| **Ndc** | 5–1000 ms | Noise envelope decay time. Short = click/snap; long = washy noise tail. |
| **Mix** | 0–100% | Audio passthrough level. Blends the audio input directly into the output alongside the synthesised drum. |

All parameters except TRG and Type accept CV modulation via assignable CV inputs (displayed inline to the right of each value).

---

## Controls

| Control | Action |
|---------|--------|
| Encoder (browse) | Navigate cursor through the parameter list; display scrolls to follow |
| Encoder (edit) | Adjust parameter value |
| Button | Enter/exit edit mode; assign CV input when cursor is on a CV slot |
| **Aux button** | Cycle to the next preset (wraps through all 6 named presets then Rnd) |

---

## Presets

| Name | Pit | Dec | Swp | Rto | FMi | FMd | Noi | Ndc |
|------|-----|-----|-----|-----|-----|-----|-----|-----|
| Kick  |  60 Hz | 500 ms | 80% | 1.0× | 90% |  200 ms |  5% |  300 ms |
| Snare | 200 Hz | 200 ms | 30% | 1.5× | 60% |  100 ms | 70% | 1200 ms |
| HiHat | 800 Hz |  40 ms |  0% | 1.0× | 20% |   30 ms |100% |  400 ms |
| O.Hat | 800 Hz | 200 ms |  0% | 1.0× | 20% |  100 ms |100% | 1200 ms |
| Tom   | 120 Hz | 350 ms | 60% | 1.2× | 70% |  150 ms | 15% |  600 ms |
| Clap  | 300 Hz |  80 ms |  5% | 0.8× | 40% |   50 ms | 90% |  800 ms |

All presets default to Mix=0 (no audio passthrough).

---

## Technical Notes

- Synthesis method: phase modulation (PM). The modulator output is scaled by the FM index envelope and fed into the carrier's PM input (±1800° / 5π radians maximum depth).
- Envelopes: exponential decay computed per audio block as `coeff = exp(−128 / (decay_ms × 0.001 × sample_rate))`. No attack stage — envelopes snap to 1.0 on trigger.
- Pitch sweep: `carrier_hz = Pit + (Swp% × Pit × amp_env)` — sweep depth is proportional to pitch, so the sweep interval scales naturally with frequency.
- Modulator phase is reset to 0 on every trigger for a consistent initial transient shape.
- Noise filtering: fixed ~1 kHz high-pass (SVF, Q=0.707) removes low-frequency rumble before the noise envelope VCA.
- InterpolatingStream (linear) is used for all three envelope-to-audio connections to smooth per-block steps at audio rate.

---

## Files

| File | Purpose |
|------|---------|
| `audio_applets/FMDrumApplet.h` | Complete applet — DSP signal graph, envelopes, UI, and presets |
