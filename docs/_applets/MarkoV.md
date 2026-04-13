---
layout: default
---
# MarkoV

**MarkoV** is a melodic Markov chain generator. On each clock it chooses the next note from a weighted probability table, producing lines that have a distinct musical character — moving in patterns that feel stylistically coherent — but never exactly repeating.

---

## What is a Markov Chain?

A Markov chain is a system where the *next* state is chosen by weighted random probability based entirely on the *current* state. Each state has its own set of transition weights pointing toward every other state. The chain has no memory beyond the present moment, yet the weights give it strong tendencies.

This puts it in a distinct category from the other generators you may know:

| Generator | How it works | Character |
|---|---|---|
| **Pure random** | No memory — each step is fully independent | Unpredictable, no musical tendency |
| **Euclidean / clock divider** | Deterministic, fixed repeating pattern | Rhythmic, predictable, loops exactly |
| **Turing Machine** | Shift register — a bit sequence that locks or slowly drifts | Loops that evolve gradually |
| **Markov chain** | Weighted probability from the current state | Musical tendency without fixed repetition |

The key quality is *stylistic coherence without a loop*: the chain gravitates toward certain intervals and motion patterns — just like a real musical style — without ever locking into an exact repeating sequence. You get the feeling of a musician working within a vocabulary, rather than a sequencer running a pattern.

---

## I/O

|        | 1 (left) | 2 (right) |
| ------ | :------: | :-------: |
| **TRIG** | Clock — advance chain | Reset — short = replay from seed; long = new seed |
| **CV IN** | Chaos offset | Transpose (V/Oct) |
| **OUT** | Quantized pitch | Trigger (on pitch change) |

**Out A** outputs the current state, quantized through the selected scale channel, with CV 2 added as a V/Oct transpose.

**Out B** fires a trigger only when the *quantized pitch actually changes*. Two adjacent Markov states that happen to fall on the same scale degree will not re-trigger. This makes Out B useful as a gate for a VCA or envelope where you want the envelope to fire only on genuine new notes — the trigger rhythm becomes a natural consequence of scale choice.

**CV 1 (Chaos offset):** Adds to the encoder-set Chaos baseline. When nothing is patched and the source is set to None, it has no effect. If the source is set to a channel with nothing plugged in, the floating ADC input may read high and push Chaos toward 100% — set CV 1 source to **None** when not using it.

**CV 2 (Transpose):** Raw V/Oct offset added after quantization. The trigger comparison is made before transpose is applied, so CV 2 jitter does not cause spurious triggers. Patch a sequencer here to transpose the entire Markov melody between keys in real time.

---

## Controls

Four parameters, navigated by the encoder (rotate to move cursor, press to enter edit, press again to exit).

| Cursor | Parameter | Range | Notes |
|--------|-----------|-------|-------|
| **Matrix** | Tendency profile | S / T / J / G / D | Sets the transition weight table |
| **Scale** | Quantizer channel | Q1–Q4 | Selects which global quantizer to use |
| **Chaos** | Randomisation | 0–100% | Blends weights toward flat/uniform |
| **Seed** | Loop anchor | — | Dice icon; controls the deterministic reset point |

**Scale** uses a dotted underline to indicate that **Aux** (the button below the right encoder) opens the full scale editor for the selected quantizer channel. Rotating the encoder while on Scale steps through Q1–Q4.

**Seed** has no numeric value — the dice icon shows its current state. See Seed & Reset below for full details.

---

## Chaos

Chaos blends the profile's weighted transition table toward a flat, uniform distribution:

- **0%** — pure profile weights; the chain follows its tendency strongly
- **50%** — character is present but motion is more varied
- **100%** — all weights equal; the chain moves at random with no tendency

CV 1 adds to the encoder-set baseline, so you can set a floor with the encoder and sweep upward with voltage. Patching an LFO here creates continuously evolving melodic density.

---

## The Five Profiles (Transition Matrices)

Each profile is an 8×8 weight table. The **row** is the current state (where you are now); the **column** is a possible next state (where you might go). Higher numbers mean more likely. At Chaos=0 these weights are followed closely; at Chaos=100 they are ignored entirely.

The eight states are scale degrees: **0** = root, **1** = 2nd, **2** = 3rd, **3** = 4th, **4** = 5th, **5** = 6th, **6** = 7th, **7** = octave. The actual pitches depend on your chosen quantizer and scale.

### S — Stability (Pentatonic)

Root (0) and fifth (4) are overwhelmingly preferred destinations from any position. The chain continuously gravitates back to these two anchors. The octave (7) is a common secondary arrival. All other steps are rare.

Produces melodies that orbit the root and fifth — characteristic of folk, modal, and drone-adjacent music. Even with moderate Chaos, the tonal centre stays very clear. Best with a pentatonic or similarly open scale.

### T — Tension (Chromatic)

Stepwise motion dominates: from any position the most likely moves are ±1 state, with the current note also common (repetition). Leaps are very rare. The chain produces snake-like lines that creep up or down through the range.

Works well when the quantizer includes close intervals (chromatic, whole-tone). With a sparse scale the steps become larger intervals. Add Chaos to occasionally break out of the current direction of travel.

### J — Jazz Tendencies

The seventh (state 6) exerts a strong gravitational pull from almost every position — many rows weight it very highly. From the seventh, the root is the overwhelmingly likely resolution. This creates the core jazz gesture: tension toward the 7th, release to root, repeat with variation.

Secondary tendencies include the 3rd (state 2) as a common secondary arrival and an implied tritone pull from the 4th toward the 7th. Use a Dorian, Mixolydian, or Lydian dominant scale to put the 7th in the right harmonic position.

### G — Glacial

Very heavy self-loops: staying on the current note is the most probable move (~40–50%). When the chain does move, only adjacent steps (±1) are possible — leaps are essentially eliminated. Root and fifth remain as attractors when movement finally occurs.

Suited to very slow clocks and long sustained notes. The chain drifts minimally through a scale, changing direction only rarely. Adding Chaos is particularly effective here — it breaks the self-loops and introduces movement while still preventing leaps.

### D — Drone

The strongest self-loops of any profile: ~83% probability of staying on the current note. No attractors — the chain has no pull toward root, fifth, or any other degree. The only meaningful escapes are ±1 steps, and even these are rare.

**Out B will stay silent for long stretches at Chaos=0.** The note is locked in place. This profile is intended for use with CV 1 Chaos as the primary performance control: at low Chaos you get a sustained drone; sweeping Chaos upward gradually introduces movement and triggers. The transition from silence to occasional movement to active melody all happens within the Chaos range.

Patch an envelope or LFO into CV 1 to animate the drone into life at musical moments, then let it settle back to stillness.

---

## Seed & Reset

The seed system gives you a deterministic loop anchor. Two values are stored: the **start state** (which scale degree to return to) and an **RNG seed** (the seed for the random number generator). Because resetting the RNG seed replays the same random number sequence, a short-press reset reproduces the *exact same note sequence* every time.

### Short press on Digital 2 — Replay loop
Resets the RNG to the stored seed and returns to the start state. The dice icon briefly inverts (~170ms) to confirm. The chain will now play the identical sequence of notes it played after the last seed was set.

### Long press on Digital 2 (~3 seconds) — New seed
Generates a new start state and a new RNG seed from the current time. From this moment on, short press replays *this* new loop.

### Encoder on Seed cursor — Re-roll
Rotating the encoder immediately rolls a new seed. You do not need to click out — keep rotating to keep rolling until you find something you like. The dice icon shifts up one pixel briefly to confirm each roll.

### Aux on Seed cursor — Re-roll
Same as rotating the encoder on Seed: generates a new seed and jumps to it.

**Performance workflow:** let the chain run freely → hear something you like → short press Digital 2 to lock in that loop → use long press or encoder re-roll to move to a new loop at the next section.

---

## Display

```
                  [Scale]    ← cursor label, right-justified, Edit mode only
[S] [Q1] [42%] [dice]       ← profile / scale / chaos% / seed, y=15
──────────────────────       ← separator
█  ██ █  ██ █  ██  █        ← scrolling bar graph, 8 steps, oldest → newest
──────────────────────       ← baseline
```

The bar graph shows the last 8 states. Bar height represents scale degree — state 0 = minimum height, state 7 = full height. This is a history readout only; it does not predict future steps.

---

## Tips

- **Out B as rhythmic gate:** Dense scales produce more frequent triggers (more unique pitches); sparse pentatonic scales create natural rests. The trigger rhythm is an emergent property of scale plus profile.
- **D profile + Chaos CV:** Use D as a drone with CV 1 controlling how much movement occurs. An envelope triggered by a performance event can sweep Chaos up and back, briefly animating the drone into a melodic phrase.
- **Glacial + slow clock + reverb:** Long note durations with minimal movement. Works well as a background drone that slowly evolves.
- **Jazz profile + Dorian or Mixolydian:** The 7th-degree pull takes on very different character depending on whether the 7th is major or minor. Mixolydian gives a bluesy dominant-7th feel; Dorian gives a cooler, more ambiguous tension.
- **Chaos as a performance arc:** Start at 0% and gradually increase over a long section to move from composed tendency to free randomness, then snap back with a seed reset.
- **Pair with MarkovPerc:** Run both from the same clock. MarkovPerc drives rhythm and accent; MarkoV drives pitch. The profile names (S/T/J) are intentionally parallel — matching moods on both creates coherent ensemble textures.
- **Transpose with a sequencer:** Patch a step sequencer into CV 2 to move MarkoV between keys at section boundaries without changing the quantizer.

---

## Credits

MarkoV by uglifruit.
