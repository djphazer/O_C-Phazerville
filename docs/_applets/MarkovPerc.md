---
layout: default
---
# MarkovPerc

A first-order **Markov chain** rhythmic generator — the percussive sibling to MarkoV. Instead of scale degrees, the states are **hit types**: rests, plain hits, accented hits, flams, and ratchets. On each clock, the chain chooses the next hit type based on weighted probabilities from the current state, creating a drummer that develops evolving rhythmic patterns with internal memory and stylistic consistency.

---

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock — advance to next hit state | Reset — short press returns to seed state; long press sets a new random seed |
| CV INs | Chaos offset — adds to the encoder-set Chaos baseline | Density — biases toward hits vs. rests (positive voltage = more hits) |
| OUTs   | Trigger — fires sub-triggers for ratchets and flams within the beat | Accent CV — 0–5V representing the accent intensity of each individual hit |

_Output A fires multiple pulses per clock for ratchets (2–4 evenly spaced) and flams (a soft grace note followed by the main hit). Output B tracks the accent of the most recent sub-trigger and holds until the trigger length expires._

---

### UI Parameters

Turn the encoder to move between parameters. Click to enter edit mode, click again to lock.

**Aux button** immediately resets to the seed state — useful in performance to return to a known rhythmic feel without interrupting the clock.

| Display | Parameter | Range | Notes |
|---------|-----------|-------|-------|
| **S / T / J** | Style (Tendency Profile) | S, T, J | See profiles below |
| **0–100%** | Chaos | 0–100% | Baseline chaos; CV In 1 adds on top |

The top line of the display shows the name of the currently-selected parameter.

---

### Tendency Profiles

Each profile is an 8×8 transition matrix defining how likely each hit type is to follow any other. Weights range from 1 to 18, giving clear stylistic character at 0% chaos.

**S — Steady (Rock/Pop)**
Gravitates strongly toward plain hits and accented hits. Rests are brief — the machine quickly rebounds to playing. Flams appear as occasional ornaments. Ratchets are rare fills after dense passages. Produces a reliable, driving groove.

**T — Syncopated (Funk/Latin)**
Rests are structurally meaningful pauses, not gaps to escape from. Flams are very common — from almost any state, a flam is a likely next event. Ratchet-2 serves as a common syncopation device. Higher overall energy than Steady, but with deliberate space built in.

**J — Jazz/Free**
Favours complexity: accented flams are the signature gesture, ratchets are common fills, and extended rests are followed by dense bursts of activity. The chain can self-reinforce into cascading ratchets (Ratchet-3 → Ratchet-4 → Ratchet-4…) before collapsing to a rest. Most unpredictable of the three profiles.

---

### Hit Types

| Display | State | Description |
|---------|-------|-------------|
| `Rst` | REST | Silence — no trigger fired |
| `Hit` | HIT | Single trigger, moderate accent (2V on Out B) |
| `AcH` | ACC_HIT | Single trigger, full accent (5V on Out B) |
| `Flm` | FLAM | Grace note (soft, immediate) + main hit (medium, slight delay) |
| `AcF` | ACC_FLAM | Grace note (soft) + accented main hit (full accent) |
| `Rc2` | RATCHET_2 | 2 triggers evenly spaced across the clock period |
| `Rc3` | RATCHET_3 | 3 triggers evenly spaced |
| `Rc4` | RATCHET_4 | 4 triggers evenly spaced |

For ratchets, Output A fires at the start of the beat with a hard accent (4V), followed by softer repeats. For flams, the grace note fires immediately with a 1V accent; the main hit arrives ~1/8 of the clock period later.

---

### Chaos

At **0% chaos** the profile weights dominate — hit sequences are strongly shaped by the chosen style.

At **100% chaos** all transition weights are equalised — any hit type is equally likely from any state, regardless of the current hit or the style. The profile has no effect.

**CV In 1** adds to the encoder-set Chaos baseline, so you can set a floor with the encoder and modulate upward with voltage. Patching an LFO here creates evolving rhythmic density over time.

---

### Density (CV In 2)

**CV In 2** biases the chain toward hits or rests without changing the style:

- **0V** — neutral, profile determines balance naturally
- **Positive voltage** — rest state becomes less likely, hit states more likely; pushes toward continuous playing
- **Negative voltage** — rest state more likely, hits less likely; creates sparser, more spacious patterns

Use Density to dynamically control how busy the pattern is in response to another part of your patch — for example, patching a slowly rising envelope during a build-up section.

---

### Reset & Seed

**Short press** on Digital In 2 (or as a trigger): returns to the **seed state** — the same starting hit type every time. Use this to return to a known rhythmic feel at a section boundary.

**Long press** (hold Digital In 2 for ~1 second): picks a **new random seed** (never lands on REST) and jumps to it. That state becomes the new short-press reset point. The seed is saved with presets.

**Aux button**: instantly jumps to the current seed state without waiting for a clock edge — useful for quick resets mid-performance.

---

### Display

The lower portion of the screen shows a scrolling history of the last 8 hit states, oldest on the left, newest on the right. Each state has a distinct visual glyph:

- **Rest**: tiny dot at the baseline
- **Hit**: single thin bar, full height
- **Acc Hit**: wide bar, full height
- **Flam**: short grace note bar + taller main hit bar
- **Acc Flam**: short grace note + tall wide main hit bar
- **Ratchet-2**: two medium bars
- **Ratchet-3**: three shorter bars
- **Ratchet-4**: four short closely-spaced bars

---

### What is a Markov Chain?

A Markov chain is a mathematical model where the probability of the next event depends only on the current state — not on the history of how you got there. In MarkovPerc, each **state** is a hit type, and the **transition matrix** defines how likely you are to move to any other hit type from your current position.

The musical insight is that drumming styles have characteristic patterns of what follows what:
- Steady rock patterns tend to stay in motion with occasional ornaments
- Funk patterns use space deliberately and reach for flams
- Jazz patterns build tension through complexity and release through rest

By encoding these tendencies as probability weights, MarkovPerc generates rhythms that feel stylistically consistent without being deterministic or repetitive.

---

### Tips

- **Pair with a VCA or envelope**: Output A's trigger, combined with Output B's accent voltage, can drive a velocity-sensitive VCA or envelope. The accent CV naturally shapes the dynamic contour of each hit.
- **Use with a kick drum module**: The structured rest/hit balance of the S profile produces reliable kick patterns that groove without becoming mechanical. Patch a second trigger-to-audio module into a different hemisphere channel for snare.
- **Density sweep for builds**: Patch a slow rising CV into CV In 2 to gradually push from sparse (rests) to dense (continuous hits) over a phrase or section.
- **J profile for live improvisation**: The Jazz profile's tendency toward self-reinforcing ratchets creates natural peak moments. Use the Aux button to reset back to a simpler seed when the complexity gets too high.
- **Pair MarkovPerc with MarkoV**: Clock both from the same source. MarkovPerc drives a percussion voice while MarkoV generates a melody. The S/T/J profile names are intentionally parallel — matching the "mood" of both creates coherent ensemble textures.
- **Long-press seed to change groove**: Mid-performance, hold Digital In 2 to pick a new random seed and pivot to a different rhythmic starting point.
