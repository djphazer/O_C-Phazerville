---
layout: default
---
# Glitch (mono)

TODO: screenshot

A live-input stutter, reverse, and lo-fi degradation effect. Audio is continuously recorded
into a one-second circular buffer. Gating **ON** freezes the playhead at a clock-sized slice
and loops it — creating stutters, reverse loops, ping-pong bounces, or tightly subdivided
ratchets. **FRZ** stops the buffer recording so the same captured content repeats
indefinitely. **Bit**, **Smp**, and **Off** apply lo-fi processing to the looped signal.
Wet/dry mix blends the result against the live input.

In `FWD` mode with `Off = 0`, gating **ON** routes live audio directly through Bit crush
and Smp decimation without any slicing — making it a straightforward gated lo-fi insert.

### Signal flow
```
input ──► [CircularBuffer — records unless FRZ is high]
       ├──► AudioEffectGlitch (wet path)  ─┐
       └──► dry path                        ├──► AudioMixer ──► output
                                           ─┘
```

The display scrolls — six rows are visible at a time. Scroll arrows appear at the top-right
or bottom-right when more rows exist above or below the current view.

---

### Parameters

#### CLK — Clock source / DIV — Slice length

**CLK** selects the clock source used to measure beat period. Set to **CLK1** or **CLK4**
to follow the internal Phazerville master clock. Set to a digital input (e.g. **TR1**) to
measure the period between rising edges on that jack instead.

**DIV** sets the slice length as a fraction of one beat:

| DIV  | Slice length |
|------|-------------|
| 1/2  | Half a beat |
| 1/3  | One third of a beat |
| 1/4  | One quarter (one beat at 4/4) |
| 1/6  | One sixth |
| 1/8  | Eighth note |
| 1/16 | Sixteenth note (default) |
| 1/32 | Thirty-second note |
| 1/64 | Sixty-fourth note — very short stabs |

**Example:** CLK1 at 120 BPM = 0.5 s/beat. DIV 1/16 → slice ≈ 31 ms.

> Any periodic signal patched to CLK — a divided clock, an LFO, a gate sequencer —
> sets the reference tempo via its rising edges.

---

#### ON — Hold gate source

**ON** is the gate input that activates the glitch. While high, the playhead locks to a
clock-sized slice of the buffer and loops it. When it drops, the effect returns immediately
to live audio — the buffer never stopped recording, so there is no gap.

**AuxButton** latches and unlatches manual hold without a patched cable. The `ON:` label
inverts while the latch is active.

> Short trigger pulses produce brief one-shot stutters. Long gates hold the loop for as
> long as the gate is high — try an envelope follower, slow LFO gate, or footswitch.

---

#### FRZ — Freeze source

**FRZ** stops the circular buffer from recording while high. New audio is not written, so
the buffer content is preserved exactly. Any looping triggered by **ON** will repeat the
same frozen material for as long as **FRZ** remains high.

Releasing **FRZ** resumes recording immediately. Releasing **ON** while **FRZ** is still
high returns to dry audio but the buffer remains frozen.

> Use FRZ to lock a musical phrase in the buffer, then gate ON repeatedly to stutter
> different slices of that frozen phrase without it ever being overwritten.

---

#### Mod — Playback mode / Mod CV

Selects how the frozen slice is looped while **ON** is high:

| Mode | Display | Behaviour |
|------|---------|-----------|
| Forward   | `FWD` | Loops start → end continuously |
| Reverse   | `REV` | Loops end → start continuously |
| Ping-pong | `PNG` | Alternates forward and reverse on each pass |
| Ratchet   | `RAT` | Subdivides the slice — see **Rch** below |

**Special case — FWD with Off = 0:** instead of looping a buffer slice, live input audio
is routed directly through Bit crush and Smp decimation while ON is high. Div is ignored.
This turns the applet into a gated lo-fi insert effect.

**Mod CV** offsets the active mode by a CV voltage, sweeping across all four modes at full
swing. Useful for automated or random mode switching mid-performance.

---

#### Rch — Ratchet count / Rch CV

Active when **Mod** is set to `RAT`. Subdivides the frozen slice into *N* equal sub-loops,
repeating the capture window *N* times at a proportionally higher rate.

| Rch | Effect |
|-----|--------|
| 1 | No subdivision — same as FWD |
| 2 | Two repeats of the first half |
| 3 | Three repeats of the first third |
| 4 | Four repeats — classic drum-machine ratchet |
| 6 | Six rapid repeats — tremolo-like flutter |

**Rch CV** sweeps the count around the base value. A random or stepped CV source here
gives unpredictable subdivision changes each time ON is gated.

---

#### Bit — Bit crush (0–15)

Reduces the bit depth of the looped (or live-insert) signal. `0` = bypass, full 16-bit
quality (default). Each increment masks off one additional low bit:

- `8` ≈ 8-bit lo-fi
- `15` = 1-bit comparator — hard square wave at full amplitude

> At maximum, all samples collapse to ±32768 regardless of input level, which produces a
> significant increase in perceived loudness. This is expected comparator behaviour.

**Bit CV** offsets the value; positive CV pushes toward 15, negative toward 0.

---

#### Smp — Sample-rate decimation (0–15)

Repeats each read position in the loop *N+1* times before advancing, creating staircase
aliasing without changing pitch or loop length. `0` = bypass (default). `15` = 16× hold —
very coarse stepping.

- `4` ≈ 5× hold — audible aliasing texture
- `15` = extreme lo-fi staircase, reminiscent of early samplers

**Smp CV** offsets the value; positive CV pushes toward 15, negative toward 0.

---

#### Off — Slice offset (0–15)

On each rising edge of **ON**, instead of capturing the most recent slice, reaches back
*(Off + 1) × slice length* into the buffer. `0` = most recent material (default). `15` =
up to 15 slices back — potentially up to one second ago depending on DIV.

- `0` — captures the last slice (normal behaviour)
- `4` — reaches back 4 slice-lengths; useful when the most recent material has a transient
  you want to avoid
- `15` — maximum reach-back; material can be up to ~1 second old (with PSRAM)

> Off = 0 combined with Mod = FWD activates the live lo-fi insert path — Off > 0 disables
> this and always uses the buffer slice path.

**Off CV** offsets the value; positive CV pushes toward 15, negative toward 0.

---

#### Mix — Wet/dry balance / Mix CV

Blends the processed signal (wet) with the unprocessed live input (dry) using equal-power
crossfading. `0%` = fully dry (bypass), `100%` = fully wet (default).

**Mix CV** adds to the base value; negative attenuation pulls toward dry even when Mix is
at 100%.

---

### Suggested patches

**Classic beat-repeat (TR-style)**
- Set CLK = TR1, DIV = 1/16. Patch a 1/4-note clock to TR1.
- Patch a momentary gate to ON. Tap on beats 3 or 4 to freeze and repeat the last sixteenth.

**Reverse ambient shimmer**
- Set Mod = `REV`, DIV = 1/2, Mix = 60%.
- Patch a slow LFO gate (high ~1 beat, low ~3 beats) to ON.
- The backwards slice rises underneath the forward dry signal.

**Ping-pong flutter**
- Set Mod = `PNG`, DIV = 1/8, Mix = 80%.
- Gate ON for 2–4 beats. The slice bounces forward and back — between a flanger and a chorus.

**Drum ratchet / roll**
- Set Mod = `RAT`, DIV = 1/4, Rch = 4.
- Trigger ON from a sequencer gate on snare hits.
- The snare transient fires four times within the 1/4-note window.
- Patch S&H random CV to Rch CV to randomise the subdivision per hit.

**Gated lo-fi insert**
- Set Mod = `FWD`, Off = 0, Bit = 8, Smp = 4, Mix = 100%.
- Gate ON from a clock or footswitch.
- While ON is high, live audio passes through 8-bit crush and 5× decimation.
  Releasing ON returns to clean dry audio instantly.
- Patch random CV to Bit CV or Smp CV for unpredictable degradation per hit.

**Buffer freeze loop**
- Set FRZ to a gate source (footswitch or sequencer). Gate ON separately.
- While FRZ is high the buffer is locked; ON loops a slice of the frozen material.
- Releasing FRZ lets new audio in; releasing ON returns to dry.

**Time-shifted freeze**
- Set Off = 4, Mod = `REV`, Mix = 80%.
- Enable manual hold (AuxButton).
- The slice plays backwards from a point 4 slice-lengths behind the live position.

**CV-morphing glitch**
- Set Mod = `FWD`. Patch a slow random voltage to Mod CV and Rch CV simultaneously.
- Enable manual hold (AuxButton). As CV drifts the mode and subdivision shift continuously.

---

### Notes

- A 1-second PSRAM buffer is used when external PSRAM is present. Without PSRAM the buffer
  shrinks to ~62 ms and the display shows `No PSRAM`. Operation is limited to very short slices.
- A 64-sample (~1.3 ms) micro-fade is applied at slice loop boundaries to suppress clicks.
  For slices shorter than 128 samples the fade is skipped.
- The buffer records continuously regardless of hold state (unless FRZ is high), so releasing
  ON always returns to current live audio with no stale gap.
- Bit = 15 (1-bit comparator mode) produces a significant loudness increase — all samples
  are driven to full amplitude regardless of input level. This is by design.

---

### Credits

Authored by Andy Jenkinson 'uglifruit' - using ClaudeCode.
DSP core (`AudioEffectGlitch`) and applet wrapper (`GlitchApplet`) released under the
MIT License.

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
