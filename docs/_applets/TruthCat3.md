---
layout: default
---
# TruthCat3

**TruthCat3** is a truth-table rhythm generator based on chained NOR/NAND Boolean logic. It clocks through the rows of a 4-bit binary truth table and evaluates the expression **A op₀ B op₁ C op₂ D** to produce gate outputs. The expression can be parenthesised 5 different ways — the third Catalan number (C₃ = 5) — and each bracketing produces a different rhythm from the same inputs. CV1 selects the NOR/NAND operator mix; CV2 selects the bracketing.

TruthCat3 is the compact sibling to TruthCat4. With only 4 inputs and 5 trees it is easier to understand and predict, and its 16-step cycle maps cleanly to one bar at most clock divisions.

---

## Concept

### The truth table

The applet maintains a 4-bit step counter. On each clock, the counter value is decoded directly into four Boolean inputs:

```
Counter value   A   B   C   D
    0 = 0000    0   0   0   0
    1 = 0001    0   0   0   1
    2 = 0010    0   0   1   0
    3 = 0011    0   0   1   1
    4 = 0100    0   1   0   0
   ...
   15 = 1111    1   1   1   1
```

**A = bit 3, B = bit 2, C = bit 1, D = bit 0.** The full 16-step cycle visits every 4-bit combination exactly once in binary order.

### NOR and NAND

```
NOR:  X NOR Y  = NOT (X OR Y)   — true only when BOTH inputs are false (0 NOR 0 = 1)
NAND: X NAND Y = NOT (X AND Y)  — true unless BOTH inputs are true  (1 NAND 1 = 0)
```

NOR fires on silence; NAND fires on everything except full density.

### The three operator slots

```
A  op₀  B  op₁  C  op₂  D
```

Each slot is independently NOR or NAND, giving **8 operator patterns** (2³). More NANDs generally produces denser rhythms; pure NOR (`OOO`) produces sparser output.

### The 5 parse trees

Without parentheses, `A op B op C op D` is ambiguous. With three binary operators and four operands, there are exactly **5 distinct ways** to fully parenthesise the expression — this is the third Catalan number (C₃ = 5). Each bracketing assigns a different **root operator** and different **sub-expressions**, changing the result even when the inputs and operators are identical.

The 8 operator patterns × 5 parse trees = **40 distinct rhythms** that OUT1 can generate, all deterministic.

---

## A Worked Example

Step 5 (counter = 5 = `0101`): A=0, B=1, C=0, D=1. Operator pattern 0 (`OOO` = all NOR).

**Tree 1 — right-associative:**
`A NOR (B NOR (C NOR D))`
```
C NOR D     = 0 NOR 1 = NOT(0|1) = 0
B NOR 0     = 1 NOR 0 = NOT(1|0) = 0
A NOR 0     = 0 NOR 0 = NOT(0|0) = 1  → OUT1 = HIGH
```

**Tree 3 — balanced:**
`(A NOR B) NOR (C NOR D)`
```
A NOR B     = 0 NOR 1 = 0
C NOR D     = 0 NOR 1 = 0
0 NOR 0     = NOT(0|0) = 1  → OUT1 = HIGH
```

**Tree 5 — left-associative:**
`((A NOR B) NOR C) NOR D`
```
A NOR B     = 0 NOR 1 = 0
0 NOR C     = 0 NOR 0 = 1
1 NOR D     = 1 NOR 1 = NOT(1|1) = 0  → OUT1 = LOW
```

Same step, same operators — trees 1 and 3 agree, tree 5 disagrees. The XOR default OUT2 mode fires at exactly these points of disagreement.

---

## Operator Patterns

The three operator slots correspond to the three positions in the expression:

```
A  op0  B  op1  C  op2  D
```

Displayed as a three-character string where **O** = NOR and **A** = NAND:

| Index | Pattern | NANDs |
|-------|---------|-------|
| 0 | `OOO` | 0 |
| 1 | `AOO` | 1 |
| 2 | `OAO` | 1 |
| 3 | `OOA` | 1 |
| 4 | `AAO` | 2 |
| 5 | `AOA` | 2 |
| 6 | `OAA` | 2 |
| 7 | `AAA` | 3 |

Higher CV on CV1 pushes toward more NAND-heavy patterns. When Ops is set to **CV**, CV1 maps directly across all 8 patterns.

---

## Parse Trees (Bracketings)

The 5 parse trees correspond to the 5 ways of fully parenthesising a chain of four values. The operator indices always refer to the original left-to-right positions (op₀ = A–B, op₁ = B–C, op₂ = C–D).

| Tree | Expression |
|------|-----------|
| 1 | `A op (B op (C op D))` — right-associative spine |
| 2 | `A op ((B op C) op D)` |
| 3 | `(A op B) op (C op D)` — balanced split |
| 4 | `(A op (B op C)) op D` |
| 5 | `((A op B) op C) op D` — left-associative spine |

In trees 1–2, op₀ is the root (determines the final gate). In trees 4–5, op₂ is the root. Tree 3 uses op₁ as the root, making it uniquely symmetric.

The five trees form a clear spectrum: tree 1 evaluates right-to-left, tree 5 evaluates left-to-right, and trees 2–4 are the three intermediate bracketings.

---

## I/O

|        | 1 (left) | 2 (right) |
| ------ | :------: | :-------: |
| **TRIG** | Clock — advance truth-table counter | TR2 — function depends on TR2 Mode setting |
| **CV IN** | Operator pattern select / CV ctrl | Parse tree select / CV ctrl |
| **OUT** | Main gate — Boolean result of current step | Related gate — derived from main result per OUT2 Relation setting |

---

## How the Truth Table Works

On each clock:

1. The counter value supplies four Boolean inputs — **A** = bit 3, **B** = bit 2, **C** = bit 1, **D** = bit 0.
2. The selected operator pattern and parse tree determine the Boolean result.
3. OUT1 fires a trigger when the result is true. OUT2 fires based on the OUT2 Relation mode.
4. The counter increments and wraps at the configured step length.

At the default 16-step length, all 4-bit combinations are visited in order — a complete one-bar deterministic rhythm cycle.

The outputs fire **trigger pulses** (not gates). Two adjacent true steps both fire independently.

---

## Controls

Five parameters, navigated by the encoder (rotate to move cursor, press to enter edit, press again to exit):

| Cursor | Parameter | Range | Notes |
|--------|-----------|-------|-------|
| **Ops** | Operator pattern | 0–7, or CV | NOR/NAND mix across the three slots |
| **Steps** | Sequence length | 3, 4, 5, 6, 7, 8, 10, 12, 15, 16 | Default 16 |
| **Tree** | Parse tree | 1–5 (internal 0–4), or CV | Bracketing of the expression |
| **TR2 Mode** | TR2 jack function | RST / HLD / TrAdv / OpAdv | See TR2 Modes below |
| **OUT2** | OUT2 relation | NXT / NOT / AND / OR / XOR / FLP | See OUT2 Relation below |

---

## TR2 Modes

| Mode | Display | Behaviour |
|------|---------|-----------|
| **Reset** | `RST` | Rising edge on TR2 resets the step counter to 0 |
| **Hold** | `HLD` | While TR2 is high, TR1 clock pulses do not advance the counter |
| **Advance Tree** | `TrAdv` | Rising edge on TR2 increments the Tree setting (wraps 5 → 1) |
| **Advance Ops** | `OpAdv` | Rising edge on TR2 increments the Ops setting (wraps 7 → 0) |

Default: **RST**.

---

## OUT2 Relation

The "adjacent tree" modes compare the current step evaluated with the **next parse tree** (`(tree + 1) mod 5`).

| Mode | Display | OUT2 fires when… |
|------|---------|----------------------|
| **NXT** | `NXT` | The same step evaluated with the next parse tree would be true |
| **NOT** | `NOT` | The current main result is false |
| **AND NXT** | `AND` | Both current and next tree evaluate true for this step |
| **OR NXT** | `OR` | Either current or next tree evaluates true for this step |
| **XOR NXT** | `XOR` | Current and next tree results differ for this step |
| **FLIP OPS** | `FLP` | Same step and tree but all operators inverted (NOR↔NAND) |

Default: **XOR**. With only 5 trees the XOR mode is particularly meaningful — trees are close neighbours and their disagreements cluster at specific rhythmically interesting steps.

---

## Why the Trees Sound Different

**Right-spine (`A op (B op (C op D))`):** op₀ is the root. The result is determined by whether A matches the outcome of the right sub-chain. With all NOR, a single true bit anywhere in B–C–D can cascade upward and flip the root.

**Left-spine (`((A op B) op C) op D`):** op₂ is the root. The entire left chain is reduced first, then D is applied at the top. A true D with all NOR collapses the final result regardless of what A–B–C produced.

**Balanced split (`(A op B) op (C op D)`):** op₁ is root. Both halves must collapse independently — symmetric structure that tends toward lower density at all NOR since two sub-results both need to be false.

Because there are only 5 trees, the differences between adjacent trees are pronounced and easy to hear by ear. Stepping through trees 1→5 with TrAdv mode gives a clear demonstration of how bracketing changes rhythm density.

---

## Display

```
↓NOR       ↑NAND          ← context help (when Ops cursor active)
───────────────
Op: ↓ ↓ ↑               ← 3 arrow icons: ↓=NOR, ↑=NAND
07/16    T:03             ← current step / length | tree (1-indexed)
RST      B:XOR            ← TR2 mode | Output2-label:OUT2-mode
A ▌  ▌▌   ▌  ▌▌          ← OUT1 history (16 steps)
B ▌▌ ▌  ▌▌  ▌ ▌           ← OUT2 history (16 steps)
```

Output letters adapt to hemisphere position: top-left A/B, top-right C/D, bottom-left E/F, bottom-right G/H.

- **Op** — 3 arrow icons (↓ = NOR, ↑ = NAND); shows "CV N" (CV + live pattern number 1–8) when under CV control
- **T** — current parse tree (1-indexed); when under CV control, the live tree number replaces the "T:" label entirely
- **Step/length** — current step (1-indexed) / total steps; length underlined when selected
- **TR2 mode** — active mode abbreviation
- **OUT2** — output letter + colon + mode abbreviation

---

## Tips

- **16 steps as musical default:** One complete 4-bit cycle = one bar at most clock divisions. Every input combination appears exactly once.
- **Tree stepping with TrAdv:** With 5 trees it takes only 5 TR2 pulses to cycle through all bracketings. Patch a 4-bar clock division into TR2 in TrAdv mode to change tree every phrase.
- **Compare with TruthCat4:** TruthCat3 has 40 rhythms vs 224 for TruthCat4. It is more predictable and repeats faster — useful as a hi-hat or trigger source where you want shorter, graspable patterns.
- **XOR accent output:** With 5 trees the XOR output fires where tree N and tree N+1 disagree. Because adjacent trees differ in only one bracketing choice, these disagreements are sparse and rhythmically accented.
- **FLIP OPS for counterpoint:** At `OOO` FLIP gives `AAA` — the densest possible output against the sparsest. At mixed patterns the two outputs interlock.
- **Short steps for faster cycles:** Steps = 4 or 8 visits only the low end of the truth table. Different trees still produce distinct rhythms even from this small window.
- **CV sweep:** Sweeping CV1 and CV2 simultaneously morphs through all 40 rhythms. The smaller space makes it feel more controlled than TruthCat4's 224-rhythm sweep.
- **Pair with TruthCat4:** Clock both from the same source. TruthCat3's 16-step cycle nests cleanly inside TruthCat4's 32-step cycle, creating polyrhythmic interlocking patterns.

---

## Credits

TruthCat3 by Andy Jenkinson / uglifruit, developed with Claude Code (Anthropic).
