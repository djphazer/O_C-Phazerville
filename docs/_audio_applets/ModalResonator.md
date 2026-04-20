---
layout: default
---
# ModalResonator (mono / stereo)

A modal synthesis resonator bank inspired by Mutable Instruments Rings and Elements. Takes any
incoming audio as an **exciter** — a trigger burst, a noise pulse, a percussive transient, or
external audio — and uses it to "strike" a bank of 12 parallel resonant bandpass filters tuned
to a modal spectrum. The filters ring at their natural frequencies and decay over time, producing
metallic chimes, struck bars, tuned bowls, plucked bodies, and glassy sustain tones.

Because the resonator is a processor, not a source, it preserves full V/Oct tracking and can be
driven by any signal in the chain above it.

### Signal flow

```
input ──► AudioEffectModalResonator (wet path) ─┐
input ──────────────────────────────────── (dry) ┴──► AudioMixer ──► output
```

---

### Parameters

#### Freq — Fundamental frequency

The pitch of the lowest resonator (mode 0). All other modes are tuned relative to this
frequency according to the **Structure** parameter.

- **Note name cursor**: Adjusts pitch in semitone steps. Displays the closest chromatic note name.
- **Hz cursor**: Fine-tunes pitch in ~3-cent steps.
- **Freq CV**: V/Oct pitch CV input. Pitch CV is sampled continuously — no trigger required.

> At Structure = 0 (harmonic), Freq sets the true fundamental of a complete harmonic series.
> At Structure = 100 (inharmonic), Freq sets the lowest partial; the others are spread above it
> in an inharmonic pattern and the perceived "pitch" may feel higher.

---

#### Str — Structure (0–100)

Controls the **spectral stiffness** of the modal bank — how partials are spaced relative
to the fundamental. The model uses an additive stretch algorithm (inspired by Mutable
Instruments Rings) rather than a simple harmonic series.

| Value | Behaviour |
|-------|-----------|
| 0 | Negative stiffness — partials compressed **below** the fundamental, descending cluster |
| ~3 | Stiffness ≈ 0 — true harmonic series: f, 2f, 3f, 4f … → string, organ |
| 25 | Slightly stretched overtones → acoustic piano character |
| 50 | Moderately stretched → marimba, wooden bars |
| 75 | Strongly inharmonic → steel tongue drum, bell plates |
| 100 | Maximally stretched → bell, bowl, gong |

**Low Structure (0–10):** At STR=0 the stiffness is slightly negative, meaning upper
partials are tuned *below* the fundamental frequency in a compressed descending cluster.
This produces dense, dark, gamelan-like tones rather than a bright harmonic series.
Because the partials shift from below the fundamental to above it as Structure increases,
sweeping from 0 upward creates a noticeable **perceived pitch shift** — this is expected
behaviour, not a tuning error. For a conventional harmonic series, use STR≈3.

**Str CV** offsets the value; full positive swing pushes toward maximally stretched.

> Sweeping Structure with a slow CV while the resonator is ringing produces a continuous
> timbral morph — the spectrum reorganises around each new set of partials.

---

#### Brt — Brightness (0–100)

Controls the **amplitude taper** across the mode bank — how much energy is present in the
upper modes compared to the fundamental.

| Value | Behaviour |
|-------|-----------|
| 0 | Only mode 0 sounds; all upper modes are suppressed → deep, fundamental-only tone |
| 50 | Moderate rolloff; the first few partials are audible |
| 100 | All 12 modes have equal weight → full spectral richness, bright and present |

**Brt CV** offsets the value. Modulating Brightness while a note decays creates a timbral
evolution as the sound rings out.

> High brightness with long decay (Dmp near 100) produces very dense, shimmering sustain.
> Low brightness with long decay produces a pure, sine-like tone.

---

#### Dmp — Damping (0–100)

Controls the **decay time** of all resonators — the rate at which the ringing fades to silence.

| Value | Approximate RT60 | Character |
|-------|-----------------|-----------|
| 0 | ~10 ms | Metallic click, percussive tap |
| 25 | ~100 ms | Short clink, cowbell-like |
| 50 | ~300 ms | Medium chime |
| 75 | ~700 ms | Bell, long shimmer |
| 100 | ~2 s | Near-infinite sustain, glass harmonica |

Higher modes decay slightly faster than lower modes regardless of Dmp, which is consistent with
the natural behaviour of physical resonant structures.

**Dmp CV** offsets the value; sweeping from 0 to full swing covers click to sustain.

> Damping can be CV-automated for per-note variation: short decays on percussive triggers,
> long decays on held notes.

---

#### Pos — Position (0–100)

Specifies the **excitation point** on the virtual resonant structure, creating a comb-filtering
effect by selectively suppressing certain harmonics. This is the same mechanism used in
Mutable Instruments Rings.

| Value | Behaviour |
|-------|-----------|
| 0 | No notching; all modes receive equal excitation energy |
| 25 (default) | Mild comb; upper modes slightly attenuated |
| 50 | Strong comb at halfway — even-numbered harmonics are suppressed (odd-harmonic spectrum) |
| 75 | Comb at three-quarters — a different set of partials notched |
| 100 | Approaches no-notching again (full period) |

**Pos CV** sweeps the excitation point continuously. Modulating Position rhythmically with an
LFO produces a timbral tremolo effect where alternating sets of partials rise and fall.

> Position interacts strongly with Structure. At Str = 0, Position produces recognisable
> open/muted-string timbres. At Str = 100, the notch pattern affects a dense inharmonic
> spectrum and produces more unpredictable tonal colouring.

---

#### Mix — Wet/dry balance (0–100%)

Blends the resonated signal (wet) with the unprocessed input (dry) using equal-power
crossfading.

| Value | Behaviour |
|-------|-----------|
| 0% | Fully dry — input passes through unaffected |
| 50% | Equal blend — both the exciter and the resonance are audible |
| 100% (default) | Fully wet — only the resonated output is heard |

**Mix CV** offsets the value. Setting Mix to 50% and automating with a slow LFO creates a
tremolo effect between the raw exciter and the resonant tail.

---

### VU meter

A small bar across the top of the display (y = 7) shows the peak level of the incoming
exciter signal. This is useful for confirming that the resonator is being struck:

- No bar → no excitation signal; the resonator may ring briefly from a previous hit but
  will not be continuously driven.
- Full bar → strong excitation; the resonator is being driven hard and may not fully decay
  between strikes.

---

### AuxButton — Manual strike

Pressing the **Aux** button fires a white-noise burst directly into the resonator at full
amplitude. This is equivalent to a full-velocity percussive strike and is useful for auditioning
the sound without a patched exciter, or for manually triggering a transient during performance.

The noise burst is one audio block (~2.7 ms) long — short enough to be a clean impulse at
most decay settings.

---

### Suggested patches

**Tuned metallic percussion**
- Set Structure = 70–80 (bell/bar), Damping = 40–60, Brightness = 80.
- Drive with a short trigger from a clock divider or a drum sequencer gate.
- V/Oct from a sequencer tracks melodic lines across the bar spectrum.

**Glass harmonica / singing bowl**
- Set Structure = 0 (harmonic), Damping = 90, Brightness = 50, Position = 50.
- Drive with a slow, low-amplitude sine or triangle LFO (continuous light excitation).
- The resonator sustains indefinitely and the position comb creates an airy, filtered tone.

**Plucked body tone (Rings-style)**
- Set Structure = 10–20 (near-harmonic), Damping = 55, Brightness = 65, Position = 25.
- Trigger with a V/Oct sequencer gate. Mix = 100%.
- Varies the pitch quickly for chord arpeggios.

**Chime cluster**
- Place two ModalResonator applets on L and R channels.
- Tune them slightly apart (1–3 semitones). Set both to Structure = 60, Damping = 75.
- Drive with the same trigger source. The two inharmonic spectra beat against each other
  producing a thick, shimmering cloud.

**Timbral sweep**
- Lock a note with AuxButton or a held gate.
- Slowly sweep Structure from 0 to 100 with a slow envelope or LFO on Str CV.
- The spectrum morphs continuously from a pure string tone to a metallic bell.

**Percussive body resonance insert**
- Set Mix = 40–60%, Damping = 15–25 (short decay), Structure = 50.
- Route a snare or tom through the applet.
- The resonator adds a metallic body-ring sympathetically tuned to each hit.

---

### Technical notes

- The resonator bank consists of **12 parallel two-pole bandpass filters** (classic two-pole
  resonator, Direct Form I). Each mode has its own frequency, pole radius, and gain weight.
- All trigonometric coefficient computation (`powf`, `cosf`, `sinf`) runs at control rate
  (~150 Hz) in `Controller()`. The audio interrupt loop (`update()`) contains only
  multiply-accumulate operations — no trig in the hot path.
- A **single-pole DC blocker** (pole at z = 0.995) is applied to the summed output.
- Mode frequencies are clamped below 20 kHz to prevent aliasing near Nyquist.
- The applet is available in both **mono** (processor chain, left or right) and **stereo**
  (full-width stereo processor chain) configurations.
- Memory footprint: approximately 400 bytes per active channel instance — no heap allocation.
- **Extreme parameter combinations:** High Str (80–100) combined with high Dmp (80–100)
  and very low Brt (0–15) pushes upper filter modes near the Nyquist frequency with minimal
  damping. This is at the stability boundary of the resonator filter and produces harsh,
  noisy transients rather than clean tones. These settings are usable for deliberate
  textural noise effects but are not intended as musical resonator sounds.

---

### Credits

DSP architecture inspired by [Mutable Instruments Rings](https://mutable-instruments.net/modules/rings/)
(Émilie Gillet). `ModalResonatorApplet` is an independent reimplementation and does not
use any Mutable Instruments source code.

Authored by Andy Jenkinson 'uglifruit' — using ClaudeCode.
DSP core (`AudioEffectModalResonator`) and applet wrapper (`ModalResonatorApplet`) released
under the MIT License.

```
MIT License
Copyright (c) 2026 Andy Jenkinson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
