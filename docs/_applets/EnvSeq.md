---
layout: default
---
# EnvSeq

![EnvSeq screenshot](images/EnvSeq.png)

**EnvSeq** is a clocked envelope sequencer. Each step outputs a configurable curve (ramp/exp/triangle/flat/VOSC), with per-step level (Scale), offset, probability (chance to play), duration in clocks, optional retriggers (repeating the curve within the step), and per-step length (time-warp of the curve inside the step).

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock | Reset (or Trigger when `TR2` is enabled) |
| CV INs | Mod 1 (per `Mod1` mode) | Mod 2 (per `Mod2` mode) |
| OUTs   | Envelope CV | Assignable (`Out2` mode) |

### UI Controls
* Select `Mod1` mode (how CV1 is applied)
* Select `Mod2` mode (how CV2 is applied)
* Select `Out2` mode (what output 2 does)
* Randomize step parameters
  - Select what parameters to randomize
* Toggle `TR2` trigger mode (use TR2 as a step trigger instead of reset)
* Set number of steps (1–32)
* Manual Reset (restarts on next clock)
* Init (restore default step settings)
* Edit per-step parameters (Step view pages)
  - Step edit selector
  - Curve
  - Offset
  - Scale (level)
  - VOSC waveform, offset, invert/revert, and option
  - Probability (0-100%)
  - Number of triggers (1-8)
  - Number of clocks the step lasts (1-8)
  - Length of curve inside the step (1-200%)
  - Mark step as modulated by CV2 (when CV2 is `ModMark`)

## Mod1 (CV1) modes
`Mod1` defines how CV1 is sampled/applied:

| Mode   | Description                                                               |
|--------|--------------------------------------------------------------------------|
| Mod    | CV1 is applied continuously to every step’s CV output.                   |
| H step | Sample & hold CV1 at each step start. The held value is used for the whole step. |
| H seq  | Sample & hold CV1 at the start of the sequence. The held value is used until the next sequence restart. |

## Mod2 (CV2) modes

| Mode    | Description                                                                                 |
|---------|--------------------------------------------------------------------------------------------|
| Length  | CV2 modulates the **Length** parameter (time-warp of the curve within the step, bipolar, 1-200%)           |
| RetrgFd | CV2 modulates the **Retrigger Fade** percent (bipolar, 0–100%). |
| Mod     | CV2 is added to the output CV for all steps.                                               |
| ModMark | CV2 is added to the output CV only on steps marked with `Mod2` in the step editor.         |
| StepSel | CV2 selects the **next played step** (unipolar **0–5V** mapped across the active `num_steps`). The selection is **sampled on the clock** when advancing steps. Step **Probability is ignored** in this mode, but multi-clock steps (`Clk`) are still honored (the current step holds until its clocks are done). |

## Out2 modes
Output 2 can mirror or complement the main envelope output:

| Out2 Mode | Description |
|-----------|-------------|
| Cpy       | Copy of Out1 (same envelope CV) |
| CpyInvO   | Inverted copy of Out1, mirrored around the step’s Offset |
| CpyInv    | Inverted copy of Out1, mirrored around 0V |
| CpyInv0   | Inverted copy of Out1, mirrored around 0V (explicit 0V center) |
| CpyAbv0   | Absolute-value copy of Out1 (always above 0V) |
| HStart    | Holds the step’s start CV value until the next step begins |
| Step      | Short gate at each new (played) step |
| StepTrg   | Short gate at each new (played) step and retriggers within a step |
| Seq       | Short gate at the start of the sequence (on wraparound) |

Notes:
- Gates are only generated for steps whose Curve is not `None`.
- Gate pulse length is fixed (short trigger-style), not proportional to the incoming clock.

## Step editor
After the main page items, the cursor enters per-step editing. The **viewed step** (the one you edit) is independent from the **currently playing step** shown on the main page.

### Page 1 (shape/level)

#### Step
Select which step number you’re editing.

#### Curve
Shape for the step:
- `None`: silent step (outputs 0V and generates no gates)
- `Zero`: outputs 0V but still counts as an active step (gates can be generated)
- `Flat`: constant level (Scale)
- `RampUp`: 0 → Scale
- `RampDown`: Scale → 0
- `ExpUp`: exponential-like 0 → Scale
- `ExpDown`: exponential-like Scale → 0
- `LogUp`: logarithmic-like 0 → Scale
- `LogDown`: logarithmic-like Scale → 0
- `Triangle`: 0 → Scale → 0
- `VOSC`: vector-oscillator waveform driven by step progress

#### Off (Offset)
DC offset added to the step output (and used as the center point for `CpyInvO`).

#### Scl (Scale)
Amplitude of the step’s curve (how “high” the envelope goes).

### Page 2 (VOSC options)

These apply when the Curve is set to `VOSC`.

#### Waveform
Selects the waveform to use.

#### Offset (%)
Shifts the waveform relative to the step offset (0–100% of the step scale).

#### Revert
Runs the waveform phase backwards.

#### Invert
Flips the waveform vertically.

#### Option
Post-process the waveform:
- `None`
- `FoldUp`
- `FoldDwn`
- `ZeroUp`
- `ZeroDwn`

### Page 3 (time/trigger/prob/mod-mark)

#### Trg (Triggers)
Sets how many times (1-8) the curve restarts within the step.

#### Clk (Clocks)
How many incoming clocks (1-8) this step lasts.

#### Len (Length)
Time-warp of the curve progression within the step (1–200%).
- <100%: curve reaches the end early and then holds its final value for the remainder of the step.
- >100%: curve progresses more slowly (may not reach the end before the step completes).
- If `Mod2` mode is `Length`, CV2 scales this per-step value.

#### Probability
Chance (0–100%) that this step will be played when the sequence reaches it.

#### Retrigger Fade
Sets the target level (0–100%) for the **last** retrigger within the step. Earlier retriggers are scaled down proportionally between full level and this target.

#### Mod2 (mark)
Marks this step as eligible for Mod2 when `Mod2` mode is `ModMark`.

## Random
Selecting `Random` on the main page opens a checklist-style randomizer for step parameters:

- `Ofs`: randomize step Offsets
- `Scl`: randomize step Scales
- `Crv`: randomize step Curves
- `Osc`: allow randomizing VOSC waveforms when Curve is `VOSC`
- `Len`: randomize step Lengths
- `Trg`: randomize step Triggers (occasionally)
- `Clk`: randomize step Clocks (occasionally)
- `Mod`: randomize step Mod2 marks

Select `RND` to apply randomization and return to the main view.

## Credits
Authored by Daniel Gorgan
