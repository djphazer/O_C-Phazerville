---
layout: default
---
# CVSeq

![CVSeq screenshot](images/CVSeq.png)

**CVSeq** is a clocked dual-channel CV/note sequencer with a shared pool of CV values. Each channel has its own list of steps; each step picks a value from the pool and lasts a configurable number of clocks (including a half-clock option for ratchets/swing). Output 1 plays channel 1 (quantized, with optional CV1/CV2 transpose); Output 2 can play channel 2 or mirror channel 1 at various octave offsets. Both channels can have independent loop regions, toggleable on the fly.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock (timebase) | Reset |
| CV INs | CV1 (transpose / pitch base for channel 1) | Per `In2` mode (CV2 input or CV1 transpose) |
| OUTs   | Channel 1 CV (quantized) | Per `Out2` mode |

### UI Controls
CVSeq has four screens that the cursor traverses in order:

- **Main screen**: In2 mode, Out2 mode, Quantizer select, Transpose mode, Reset, Random, Loop1, Loop2, Loop swap.
- **Values screen**: select a CV value index from the shared pool, edit it as voltage or as note, plus Zero/Copy/Paste shortcuts.
- **Channel screen**: pick the channel, set the number of steps, pick a step, edit its value index, mark loop start/end, set clocks-per-step.
- **General screen**: copy CH1→CH2 and CH2→CH1.

A progress bar at the top of every screen shows channel 1 and channel 2 playback position, plus an independent bar-position counter (advances every half-clock, with tick marks at quarters of the bar).

## In2 (CV2 input) modes
`In2` defines what CV2 is used for:

| Mode  | Description                                                                          |
|-------|-------------------------------------------------------------------------------------|
| CV2   | CV2 is the pitch base for **channel 2** (added before the channel 2 sequence value, then quantized). |
| Trnsp | CV2 transposes **channel 1** (read in semitones, applied before quantization). In `Deg` mode the transposed value is snapped to a scale note; in `Semi` mode the transpose stays chromatic. |

## Out2 modes
Output 2 can play channel 2 or mirror channel 1:

| Out2 Mode | Description |
|-----------|-------------|
| CV2       | Channel 2 sequencer output (quantized, using CV2 as pitch base when `In2` = `CV2`) |
| CV1-2     | Channel 1 output, two octaves down |
| CV1-1     | Channel 1 output, one octave down |
| CV1       | Channel 1 output (same as Out1) |
| CV1+1     | Channel 1 output, one octave up |
| CV1+2     | Channel 1 output, two octaves up |
| Gate      | Short gate at each new channel 1 step |
| CV1in     | CV1 input passed through (unquantized, not transposed) |

## Quantizer & transpose
Both channels share one quantizer (`Q1`–`Q8`). On the main screen:

- The quantizer field shows the current scale and root note. Pressing **AUX** while the cursor is on `Q-engine` opens the global quantizer editor for the selected channel.
- The transpose-mode toggle (`Deg` / `Semi`) controls how the CV1-transpose (and the manual root note when no scale is selected) is interpreted:
  - `Deg`: transpose is added **before** quantization, snapping to a scale degree.
  - `Semi`: transpose is added **after** quantization, staying chromatic.
- Sub-semitone `+50¢` residue from a CV value (odd values) is always applied chromatically, after quantization.

## Values screen
The shared pool holds **32 CV values**, indexed 1–32. Each step on either channel points at one of these values. Editing a value changes every step that references it.

### Value index
Selects which CV value (1–32) to edit. Pressing **AUX** while on the CV or Note row toggles a quick mode that lets the encoder scrub the index without leaving the value/note field.

### CV
The value as voltage. Resolution is ~0.08 V (±10 V range), shown with two decimals.

### Note
The same value as a chromatic note name + octave. Octave is signed (e.g. `B-1`). Odd values append `+50c` to indicate a quarter-tone between two semitones — useful as a chromatic detune that survives quantization.

### Zero / Cpy / Past
- `0`: reset the current value to 0 V.
- `Cpy`: copy the current value to the clipboard.
- `Past`: paste the clipboard onto the current value.

## Channel screen
Per-channel step editor.

### Channel (CH)
Selects the channel (1 or 2) being edited. Pressing **AUX** here toggles **channel-follow**: when on, the step cursor automatically tracks the live playback position of the selected channel (the channel label is highlighted to show follow is active).

### Num steps (length icon)
Number of active steps in the channel sequence (1–64). Steps beyond this count are kept in memory but ignored during playback.

### Step (#)
Selects which step you’re editing. Pressing **AUX** on any step-related field toggles a quick mode that lets the encoder scrub the step index without leaving the field.

### Value (val #)
Index into the shared CV-values pool (1–32) that this step plays.

### Loop start / Loop end
- `>` next to the step number marks it as the channel’s **loop start**.
- `<` next to the step number marks it as the channel’s **loop end**.
- Press the encoder while the cursor is on either field to set the loop start/end to the currently viewed step.

### Clocks
How many clocks this step lasts (1–31), with a special `1/2` value for half-clock ratchets/swing:

- A `1/2` step contributes one half-clock event (advances on either the next whole clock or the next half-clock boundary, whichever comes first).
- Pairs of consecutive `1/2` steps fit cleanly inside a single clock.
- A trailing odd half-clock run (e.g. a single `1/2` step followed by a whole-clock step) is automatically extended to a full clock so the next step always starts on a clock boundary. The bottom row shows `/` after the start time when a step actually starts on a half-clock boundary.

### Total length / step start
The bottom row shows the total length of the active sequence in whole clocks, followed by when the current step starts (in clocks, relative to the start of the sequence). A trailing `/` means the step starts on a half-clock boundary. If the viewed step is past `num_steps`, the start position shows `-` (the step won’t play).

## General screen
Sequence-level utilities:

- `CH1 > CH2`: copy channel 1 (steps, length, loop) onto channel 2.
- `CH2 > CH1`: copy channel 2 onto channel 1.

## Loops
Each channel has an independent loop region defined by **Loop start** and **Loop end** (set on the channel screen). On the main screen, three controls toggle loops live:

- `ch1` / `ch2`: toggle loop for that channel.
- `Swp`: toggle both loops at once (useful for swapping which channel is looping).

Loop engagement is **clock-locked** and uses three internal sequencers per channel to make engage/disengage musical:

- **Live**: what you actually hear.
- **Shadow**: keeps running as if the loop were off, so disengaging the loop resumes where the sequence “would have been”.
- **Phantom**: always plays the loop region (anchored at the loop start at sequence reset). When you engage the loop from outside the loop region, playback snaps to the phantom’s phase-locked position so the loop enters in time. If you engage while already inside the loop region, the live sequencer stays put and just retargets under loop semantics.

Toggling a loop while a jump is already pending (before the next clock) cancels the pending jump.

## Random
Selecting `Random` on the main page opens a scrollable, checklist-style randomizer:

- `CH1` / `CH2`: per-channel checkboxes — only the checked channels are affected by `Steps` (and `Init` / `Zero` always reset both).
- `Values`: randomize the entire CV-values pool within ±18 semitones (whole-semitone increments, 32 values).
- `Transpose`: randomly transpose ~half of the CV values up or down by up to 6 semitones.
- `Steps`: regenerate the step list for each enabled channel. Picks a musically reasonable target length (8/16/32 clocks for CH1; CH2 gets the same, half, or a quarter), then fills with random clock counts (mostly 4-clock, some 2-clock) and random value indices. Loops are cleared.
- `Zero`: clear all CV values to 0, reset both channels to a single empty step, clear loops.
- `Init`: load the default init sequence (root / 4th / 5th–driving techno pattern, CH1 = 3 steps, CH2 = 2 steps).
- `Back`: exit the menu without changes.

## Reset
Resetting (digital 2 trigger or pressing **Reset** on the main page) is **latched**: the current step keeps playing until the next clock, then both channels jump back to step 0. This keeps Reset musical when fired off-grid.

## Credits
Authored by Daniel Gorgan
