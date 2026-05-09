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

- **Main screen**: In2 mode, Out2 mode, Quantizer select, Transpose mode, Reset, Random, Loop1, Loop2, Loop flip.
- **Values screen**: select a CV value index from the shared pool, edit it as voltage or as note, plus Zero/Copy/Paste shortcuts.
- **Channel screen**: pick the channel, set the number of steps, pick a step, edit its value index, mark loop endpoints, set clocks-per-step.
- **General screen**: copy CH1→CH2 and CH2→CH1.

A header on every screen shows channel 1 and channel 2 playback positions, plus a 4-clock bar counter (one tick per half-clock, dotted markers at each whole clock). Channel bars are scaled to `Num steps`, so if a loop pulls playback past that length the bar pins to full until playback returns inside it.

## In2 (CV2 input) modes
`In2` defines what CV2 is used for:

| Mode  | Description                                                                          |
|-------|-------------------------------------------------------------------------------------|
| CV2   | CV2 is the pitch base for **channel 2** (added before the channel 2 sequence value, then quantized). |
| Trnsp | CV2 transposes **channel 1** (read in semitones). See *Quantizer & transpose* for how `Deg`/`Root` controls when the transpose is applied. |

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
Both channels share one quantizer (`Q1`–`Q8`), with an additional `OFF` setting one click below `Q1` that bypasses quantization entirely (CV1/CV2 inputs, sequence values, and transpose are all summed and output chromatically). On the main screen:

- The quantizer field shows the current scale and root note when a quantizer is selected, or `OFF` when bypassed. Pressing **AUX** while the cursor is on `Q-engine` opens the global quantizer editor for the underlying channel (works even in `OFF` mode, so you can prep a quantizer before switching back).
- The transpose-mode toggle (`Deg` / `Root`) controls how the CV1-transpose (and the manual root note when no scale is selected) is interpreted:
  - `Deg`: transpose is added **before** quantization, snapping to a scale degree.
  - `Root`: transpose is added **after** quantization, staying chromatic (acts like a manual root-note offset, matching TB3PO's terminology).
- In `OFF` mode the `Deg`/`Root` toggle has no audible effect — both add the transpose chromatically.
- Sub-semitone `+50¢` residue from a CV value (odd values) is always applied chromatically, after quantization.

## Values screen
The shared pool holds **32 CV values**, indexed 1–32. Each step on either channel points at one of these values. Editing a value changes every step that references it.

### Value index
Selects which CV value (1–32) to edit. Pressing **AUX** while on the CV or Note row toggles a quick mode that lets the encoder scrub the index without leaving the value/note field.

### CV
The value as voltage. Each increment is half a semitone (50¢, ~0.042 V), giving a ±63.5-semitone (~±5.29 V) range, shown with two decimals.

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
Selects which step you’re editing. Pressing **AUX** on Num steps, Step, Value, Loop, or Clocks toggles a quick mode that lets the encoder scrub the step index without leaving the current field.

### Value (val #)
Index into the shared CV-values pool (1–32) that this step plays.

### Loop
The loop region is bounded by two markers, drawn as `>` on the lower step and `<` on the higher. Press the encoder to snap the *closer* marker onto the current step — extends the region when you're outside it, contracts the nearer side when inside, collapses to a single step when on a marker. Markers can land anywhere from step 1 to 64 regardless of **Num steps**; steps past the active length only play when the loop pulls them in.

### Clocks
How long this step lasts. 32 distinct length values: a half-clock `1/2` for ratchets/swing, then 1–31 whole clocks. A `1/2` step advances on the next half- or whole-clock boundary, whichever comes first. Pairs of consecutive `1/2` steps fit cleanly inside a single clock; a trailing odd `1/2` run is auto-extended to a full clock so the next step always starts on a clock boundary.

### Total length / step start
Bottom row: total length of the active sequence in whole clocks, then when the current step starts (clocks from sequence start). A trailing `/` means the step lands on a half-clock boundary (e.g. `5/` is five-and-a-half clocks in). If the step is past `num_steps`, the start position shows `-`.

## General screen
Sequence-level utilities:

- `CH1 > CH2`: copy channel 1 (steps, length, loop) onto channel 2.
- `CH2 > CH1`: copy channel 2 onto channel 1.

## Loops
Each channel has an independent loop region (set on the channel screen). On the main screen:

- `ch1` / `ch2`: toggle loop for that channel.
- `Flp`: toggle both loops at once (useful for swapping which channel is looping).

### Engagement
Loop changes are **clock-locked** — toggle/engage/disengage takes effect on the next whole clock so the swap stays musical:

- Engaging from outside the region: snaps to the position the loop *would* be playing if it had been running since reset (phase-locked re-entry).
- Engaging from inside the region: leaves the playhead where it is.
- Disengaging: continues from where the un-looped sequence would have been — the loop is treated as a detour, not a reset.
- Toggling again before the next clock cancels the pending change.
- Editing the loop endpoints re-anchors the phase reference, so the next engage stays in time.
- A reset bypasses all of this: it applies the current loop-enabled state immediately and clears any pending toggle.

## Random
Selecting `Random` on the main page opens a scrollable, checklist-style randomizer:

- `CH1` / `CH2`: per-channel checkboxes — only the checked channels are affected by `Steps` (and `Init` / `Zero` always reset both).
- `Values`: randomize the entire CV-values pool within ±18 semitones (whole semitones).
- `Transpose`: randomly transpose ~half of the CV values up or down by up to 6 semitones.
- `Steps`: regenerate the step list for each enabled channel with random clocks-per-step (mostly 4, some 2) and random value indices. CH1 targets 16 clocks (occasionally 8 or 32); CH2 is the same, half, or a quarter. Loops are cleared.
- `Zero`: clear all CV values to 0, reset both channels to a single empty step, clear loops.
- `Init`: load the default init sequence (root / 4th / 5th–driving techno pattern, CH1 = 3 steps, CH2 = 2 steps), and refill the CV pool with that interval set (the first nine slots get specific intervals, the rest are filled with random picks from the same set).
- `Back`: exit the menu without changes.

## Reset
Resetting (digital 2 trigger or pressing **Reset** on the main page) is **latched**: the current step keeps playing until the next *whole* clock, then both channels jump back to step 0. This keeps Reset musical when fired off-grid.

## Persistence

> ⚠️ **Step data is not saved across power cycles.** Saved presets store only configuration (In2/Out2 modes, quantizer selection including `OFF`, transpose mode, random checkbox state). The CV pool, step list, sequence lengths, and loop points all reset to the default Init pattern on power-up or preset reload.

Editing during a session is fine — just don’t expect a custom sequence to survive the unit being powered off.

## Credits
Authored by Daniel Gorgan
