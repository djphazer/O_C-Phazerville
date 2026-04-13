---
layout: default
---
# Mist — Live Granular Audio Processor

(TODO: screenshot)

## License

MIT License  
(c) 2026 Andy Jenkinson (uglifruit) — with ClaudeCode.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## Overview

Mist is a live granular audio processor for the Phazerville/Hemisphere audio applet framework. It continuously records incoming audio into a 1-second PSRAM circular buffer and plays it back as a cloud of overlapping grains. The result ranges from subtle shimmer and smear to dense, pitch-shifted clouds and frozen textural loops.

Available as both MONO and STEREO instantiations.

---

## Signal Flow

```
Audio In ──► AudioEffectMist (grain cloud) ──► AudioMixer<2> ──► Audio Out
Audio In ──────────────────────────────────► (dry channel) ──►
```

The wet (granular) and dry (live) signals are blended by the **Mix** parameter.

---

## Parameters

### Page 1

| Param | Range | Description |
|-------|-------|-------------|
| **Pos** | 0–100% | Read position in the buffer. 0% = current (live), 100% = 1 second ago. |
| **Den** | 1–50 Hz | Grain spawn rate. Higher values produce a denser, smoother cloud. |
| **Sz** | 10–500 ms | Grain duration. Short = glitchy fragments; long = smooth smear. |
| **Spr** | 0–100% | Position scatter. Randomises each grain's start point around Pos. |

### Page 2

| Param | Range | Description |
|-------|-------|-------------|
| **Pt** | −12 to +12 st | Pitch shift in semitones. Unity = 0. |
| **PSp** | 0–100% | Pitch spread. Randomises each grain's pitch by up to ±1 octave (quadratic curve — most resolution at low values). |
| **Frz** | gate / latch | Freeze. Stops the write pointer so grains replay a fixed snapshot of the buffer. |
| **Mix** | 0–100% | Wet/dry blend (equal-power crossfade). 0% = dry only, 100% = grains only. |
| **Shp** | Sine/Tri/R-Up/R-Dn | Grain envelope shape. Sine (Hann) = smoothest; ramps = edgier attacks/releases. |

All parameters except Frz and Shp accept CV modulation via assignable CV inputs.

---

## Controls

| Control | Action |
|---------|--------|
| Encoder (browse) | Navigate cursor through parameters |
| Encoder (edit) | Adjust parameter value |
| Button | Enter/exit edit mode; assign CV input when cursor is on a CV slot |
| **Aux button** | Latch/unlatch manual freeze — for live performance without a gate cable |

---

## CV / Gate Inputs

Each parameter (except Shape) has an assignable CV input slot. Freeze also accepts a gate/digital input. Inputs are assigned using the in-applet CV input editor (press button when cursor is on a CV label).

---

## Freeze

Freeze halts the write pointer. Grains continue to spawn and play from the frozen buffer, creating a looping granular texture from the captured moment. Two ways to freeze:

- **Gate input** on the Frz parameter (hardware gate cable)
- **Aux button** latch (no cable needed — great for live use)

When latched, the Frz row label is inverted on screen.

---

## Activity Display

A row of pixels at the top of the display (y=7) shows active grain count in real time — one pixel per active grain, up to 12 maximum.

---

## Technical Notes

- Buffer: 44100 samples (~1 second) stored in PSRAM (EXTMEM). Falls back to half-size in OCRAM if no PSRAM is present. Displays "No PSRAM" if unavailable.
- Grains: up to 12 simultaneous grains per channel. Output scaled by 0.25 for headroom.
- Interpolation: Hermite cubic interpolation on grain read pointer for smooth pitch shifting.
- Grain envelope: Hann (sin²), Triangle, Ramp-Up, or Ramp-Down — selected by Shape.
- Pitch spread uses a per-grain random semitone offset applied at spawn time, mapped via a quadratic curve for fine control at low values.

---

## Files

| File | Purpose |
|------|---------|
| `audio_applets/MistApplet.h` | UI and control layer (HemisphereAudioApplet) |
| `Audio/AudioEffectMist.h` | DSP engine (AudioStream subclass) |
