---
layout: default
---
# TruthCat4

**TruthCat4** is a truth-table rhythm generator based on chained NOR/NAND Boolean logic. It clocks through the rows of a 5-bit binary truth table and evaluates the expression **A op₀ B op₁ C op₂ D op₃ E** to produce gate outputs. What makes it unusual is that the same expression can be parenthesised 14 different ways (the Catalan number C₄ = 14), and each bracketing produces a different rhythm from the same inputs. CV1 selects the NOR/NAND operator mix; CV2 selects the bracketing.

---

## Concept

### The truth table

The applet maintains a 5-bit step counter. On each clock, the counter value is decoded directly into five Boolean inputs:

```
Counter value   A   B   C   D   E
    0 = 00000   0   0   0   0   0
    1 = 00001   0   0   0   0   1
    2 = 00010   0   0   0   1   0
    3 = 00011   0   0   0   1   1
   ...
   31 = 11111   1   1   1   1   1
```

**A = bit 4, B = bit 3, C = bit 2, D = bit 1, E = bit 0.** The full 32-step cycle visits every combination of five bits exactly once, in binary order.

### NOR and NAND

The two operators are:

```
NOR:  X NOR Y  = NOT (X OR Y)   — true only when BOTH inputs are false (0 NOR 0 = 1)
NAND: X NAND Y = NOT (X AND Y)  — true unless BOTH inputs are true  (1 NAND 1 = 0)
```

NOR fires on silence; NAND fires on everything except full density. Both are universal gates — any Boolean function can be built from either one alone.

### The four operator slots

The expression has four operator positions:

```
A  op₀  B  op₁  C  op₂  D  op₃  E
```

Each slot is independently NOR or NAND, giving **16 operator patterns**. These are ordered by how many NAND operators they contain (0 through 4). More NANDs generally produces denser rhythms because NAND fires more often; pure NOR (`OOOO`) produces sparser output.

### The 14 parse trees

Without parentheses, `A op B op C op D op E` is ambiguous. With four binary operators and five operands, there are exactly **14 distinct ways** to fully parenthesise the expression — this is the fourth Catalan number (C₄ = 14). Each bracketing assigns a different **root operator** and different **sub-expressions**, which changes the result even when the inputs and operators are identical.

This means from a single truth-table row with a single operator pattern you can extract **14 different Boolean results** — one per parse tree.

The 16 operator patterns × 14 parse trees = **224 distinct rhythms** that OUT1 can generate, all from the same deterministic counter sequence.

---

## A Worked Example

Let's trace step 6 (counter = 6 = `00110`, so A=0, B=0, C=1, D=1, E=0) with operator pattern 0 (`OOOO` = all NOR) through two different parse trees.

**Tree 1 — right-associative:**
`A NOR (B NOR (C NOR (D NOR E)))`
```
D NOR E     = 1 NOR 0 = NOT(1 OR 0)  = NOT(1)  = 0
C NOR 0     = 1 NOR 0 = NOT(1 OR 0)  = NOT(1)  = 0
B NOR 0     = 0 NOR 0 = NOT(0 OR 0)  = NOT(0)  = 1
A NOR 1     = 0 NOR 1 = NOT(0 OR 1)  = NOT(1)  = 0  → OUT1 = LOW
```

**Tree 6 — balanced split:**
`(A NOR B) NOR ((C NOR D) NOR E)`
```
A NOR B     = 0 NOR 0 = 1
C NOR D     = 1 NOR 1 = NOT(1 OR 1)  = NOT(1)  = 0
0 NOR E     = 0 NOR 0 = 1
1 NOR 1     = NOT(1 OR 1) = NOT(1)   = 0  → OUT1 = LOW
```

**Tree 9 — left-spine:**
`(((A NOR B) NOR C) NOR D) NOR E`
```
A NOR B     = 0 NOR 0 = 1
1 NOR C     = 1 NOR 1 = 0
0 NOR D     = 0 NOR 1 = 0
0 NOR E     = 0 NOR 0 = 1  → OUT1 = HIGH
```

Same step, same operators, three different results — solely from the choice of bracketing.

---

## Operator Patterns

The four operator slots correspond to the four positions in the expression:

```
A  op0  B  op1  C  op2  D  op3  E
```

Each slot is independently NOR or NAND. Displayed as a four-character string where **O** = NOR and **A** = NAND (avoiding N which is ambiguous). For example:

- `OOOO` = all NOR — sparsest; fires only when all relevant inputs are false
- `AAAA` = all NAND — densest; fires unless all relevant inputs are true
- `AOOA` = NAND at the outer positions, NOR in the middle

The 16 patterns are ordered by increasing NAND count:

| Index | Pattern | NANDs |
|-------|---------|-------|
| 0 | `OOOO` | 0 |
| 1 | `AOOO` | 1 |
| 2 | `OAOO` | 1 |
| 3 | `OOAO` | 1 |
| 4 | `OOOA` | 1 |
| 5 | `AAOO` | 2 |
| 6 | `AOAO` | 2 |
| 7 | `AOOA` | 2 |
| 8 | `OAAO` | 2 |
| 9 | `OAOA` | 2 |
| 10 | `OOAA` | 2 |
| 11 | `AAAO` | 3 |
| 12 | `AAOA` | 3 |
| 13 | `AOAA` | 3 |
| 14 | `OAAA` | 3 |
| 15 | `AAAA` | 4 |

Higher CV on CV1 pushes toward more NAND-heavy operator patterns. When Ops is set to **CV**, CV1 maps directly across all 16 patterns.

---

## Parse Trees (Bracketings)

The 14 parse trees correspond to the 14 ways of fully parenthesising a chain of five values. The operator indices always refer to the original left-to-right positions in the chain (op₀ = between A and B, op₁ = between B and C, etc.) regardless of how the expression is grouped.

| Tree | Expression |
|------|-----------|
| 1 | `A op (B op (C op (D op E)))` — right-associative spine |
| 2 | `A op (B op ((C op D) op E))` |
| 3 | `A op ((B op C) op (D op E))` |
| 4 | `A op ((B op (C op D)) op E)` |
| 5 | `A op (((B op C) op D) op E)` |
| 6 | `(A op B) op (C op (D op E))` |
| 7 | `(A op B) op ((C op D) op E)` — balanced binary split |
| 8 | `(A op (B op C)) op (D op E)` |
| 9 | `((A op B) op C) op (D op E)` |
| 10 | `(A op (B op (C op D))) op E` |
| 11 | `(A op ((B op C) op D)) op E` |
| 12 | `((A op B) op (C op D)) op E` |
| 13 | `((A op (B op C)) op D) op E` |
| 14 | `(((A op B) op C) op D) op E` — left-associative spine |

Trees 1 and 14 are the two extremes: fully right-associative (A is evaluated last) and fully left-associative (E is evaluated last). Trees 7 and 12 are the most balanced, splitting the five operands into roughly equal sub-groups.

The positional importance of each operator slot shifts dramatically between trees. In trees 1–5, op₀ is the root and determines the final output. In trees 10–14, op₃ is the root. In trees 6–7, op₁ is the root. This means changing a single operator slot from NOR to NAND has a completely different effect depending on which tree is active — it might change nearly every step result, or barely affect anything at all.

---

## I/O

|        | 1 (left) | 2 (right) |
| ------ | :------: | :-------: |
| **TRIG** | Clock — advance truth-table counter | TR2 — function depends on TR2 Mode setting |
| **CV IN** | Operator pattern select / CV ctrl | Parse tree select / CV ctrl |
| **OUT** | Main gate — Boolean result of current step | Related gate — derived from main result per OUT2 Relation setting |

---

## How the Truth Table Works

The applet maintains an internal step counter. On each clock:

1. The counter value supplies the five Boolean inputs — **A** = bit 4, **B** = bit 3, **C** = bit 2, **D** = bit 1, **E** = bit 0.
2. The selected NOR/NAND operator pattern and parse tree determine the Boolean result.
3. OUT1 fires a trigger when the result is true. OUT2 fires a trigger based on the OUT2 Relation mode.
4. The counter increments and wraps at the configured step length.

At the default 32-step length, all binary combinations from `00000` to `11111` are visited in order, giving a complete deterministic rhythm cycle.

For shorter lengths the counter wraps before reaching `11111`, so only a subset of the truth-table rows are used — this produces shorter rhythmic cycles with different densities.

Note: the outputs fire **trigger pulses**, not gates. If two adjacent steps both evaluate true, both fire independently — you get two separate triggers rather than one long held gate.

---

## Controls

Five parameters, navigated by the encoder (rotate to move cursor, press to enter edit, press again to exit). The cursor order matches the visual layout, top to bottom, left to right:

| Cursor | Parameter | Range | Notes |
|--------|-----------|-------|-------|
| **Ops** | Operator pattern | 0–15, or CV | NOR/NAND mix across the four slots |
| **Steps** | Sequence length | 3, 4, 5, 6, 7, 8, 10, 12, 15, 16, 20, 24, 32 | Default 32 |
| **Tree** | Parse tree | 1–14 (internal 0–13), or CV | Bracketing of the expression |
| **TR2 Mode** | TR2 jack function | RST / HLD / TrAdv / OpAdv | See TR2 Modes below |
| **OUT2** | OUT2 relation | NXT / NOT / AND / OR / XOR / FLP | See OUT2 Relation below |

---

## TR2 Modes

| Mode | Display | Behaviour |
|------|---------|-----------|
| **Reset** | `RST` | Rising edge on TR2 resets the step counter to 0 |
| **Hold** | `HLD` | While TR2 is high, TR1 clock pulses do not advance the counter; outputs hold their current values |
| **Advance Tree** | `TrAdv` | Rising edge on TR2 increments the Tree setting (wraps 14 → 1) |
| **Advance Ops** | `OpAdv` | Rising edge on TR2 increments the Ops setting (wraps 15 → 0) |

Default: **RST**.

In TrAdv and OpAdv modes, TR2 modifies the base manual setting. If the setting is on CV, advancing wraps through the fixed pattern values only — the CV ctrl slot is not visited by advance.

---

## OUT2 Relation

OUT2 generates a second gate based on the main result and the selected relation. The "adjacent tree" modes compare the current step with the same step evaluated using the **next parse tree** (`(tree + 1) mod 14`).

| Mode | Display | OUT2 fires when… |
|------|---------|----------------------|
| **NXT** | `NXT` | The same step evaluated with the *next* parse tree would be true |
| **NOT** | `NOT` | The current main result is false |
| **AND NXT** | `AND` | Both current tree and next tree evaluate true for this step |
| **OR NXT** | `OR` | Either current or next tree evaluates true for this step |
| **XOR NXT** | `XOR` | Current and next tree results differ for this step |
| **FLIP OPS** | `FLP` | Same step and tree but all operators inverted (NOR↔NAND) |

Default: **XOR**. The XOR mode fires whenever the current and next-tree interpretations of the same step disagree — this is the "ambiguous" reading. It produces a sparser, accented pattern that fires at the points where adjacent parse trees diverge most.

FLIP OPS gives the complementary reading under inverted logic. For mixed operator patterns it creates a contrapuntal rhythm that tends toward the opposite density to OUT1.

---

## Why the Trees Sound Different

The relationship between parse trees and rhythm density is non-obvious but musically rich. A few patterns worth knowing:

**Right-spine vs. left-spine at all NOR:** Tree 1 (right-associative) evaluates A last. The root operator (op₀) compares A against a possibly-complex sub-expression. If that sub-expression is true, the root NOR fires false. Tree 14 (left-associative) evaluates the opposite way — E is evaluated last, and op₃ determines the final gate. Because NOR fires only when both sides are false, the "shape" of sub-tree trueness cascades differently through each structure.

**Balanced trees tend toward low density at all NOR:** Trees 7 and 12 split the inputs symmetrically. With all NOR, each sub-expression must be entirely false for its parent to be true. Symmetric splits mean two independent sub-trees must both evaluate false simultaneously — which happens less often than a single chain collapsing.

**Single-NAND patterns create asymmetric rhythms:** With op₀ = NAND and the rest NOR (pattern 1), tree 1 has op₀ as its root — so the final gate is a NAND. This fires unless the right sub-tree is true AND A is true. Trees 10–14 have op₃ as root (NOR in this pattern), so the NAND at op₀ is buried deep in the expression and only occasionally reaches the output.

---

## Display

The display in context (top-right quadrant example):

```
↓NOR       ↑NAND          ← context help (when Ops cursor active)
───────────────
Op: ↓ ↓ ↑ ↓               ← 4 arrow icons: ↓=NOR, ↑=NAND
07/32    T:09              ← current step / length | tree (1-indexed)
RST      D:XOR             ← TR2 mode | Output2-label:OUT2-mode
C ▌  ▌▌   ▌  ▌▌           ← OUT1 history (16 steps, output letter = C/E/G)
D ▌▌ ▌  ▌▌  ▌ ▌            ← OUT2 history (16 steps, output letter = D/F/H)
```

The output letters on the history rows adapt to hemisphere position: top-left uses A/B, top-right uses C/D, bottom-left uses E/F, bottom-right uses G/H.

- **Op** — 4 arrow icons (↓ = NOR, ↑ = NAND); shows "CV N" (CV + live pattern number 1–16) when under CV control
- **T** — current parse tree number (1-indexed display); when under CV control, the live tree number replaces the "T:" label entirely
- **St** — current step (1-indexed) / total steps
- **RST / HLD / TrAdv / OpAdv** — active TR2 mode
- **O2** — active OUT2 relation
- **Upper history row** — scrolling 16-step gate history for OUT1 (filled bar = trigger fired)
- **Lower history row** — scrolling 16-step gate history for OUT2

The cursor underline moves between the five editable parameters. When the Ops, TR2, or OUT2 cursor is active, the applet name is replaced with context help text.

---

## Tips

- **All NOR, tree 1 at 32 steps:** This is the simplest case — a right-associative NOR chain. Use it as a reference to learn how the truth table maps to rhythm before exploring other settings. Step 0 (all false) gives NOR(F, NOR(F, NOR(F, NOR(F, F)))) = NOR(F,T) = F — you can trace the whole cycle by hand.
- **Steps = 16 as a musical default:** 16 gives a clean one-bar cycle at most clock divisions and visits the lower half of the truth table — enough variation without the full 32 steps.
- **CV1 and CV2 sweep:** Sweeping both CVs simultaneously morphs through the full 224-rhythm space continuously. At low CV, NOR-heavy short-cycle patterns; at high CV, NAND-heavy and differently-bracketed interpretations.
- **TrAdv + clock divider:** Patch a divided version of your master clock into TR2 in TrAdv mode to automatically cycle through parse trees at a musical rate — e.g. change tree every 4 bars.
- **XOR for accent:** The default OUT2 mode fires whenever the current and next-tree readings of the same step disagree. This tends to produce a sparser, accented pattern useful as a hi-hat or rimshot layered over the main gate.
- **FLIP OPS for counterpoint:** OUT2 in FLIP OPS mode gives you two rhythms that are logically opposed. These tend to interlock without completely cancelling — NOR-heavy patterns produce sparser rhythms, NAND-heavy produce denser ones, and FLIP makes OUT2 the reverse of OUT1's operator logic.
- **Hold mode for phrase locks:** TR2 in HLD mode lets you freeze the counter mid-phrase while keeping a gate high, then release to continue. Useful for stutter effects or to hold a specific row of the truth table while other modulation changes.
- **Short step lengths for dense rhythms:** Steps = 3 or 4 gives very rapid cycling through a small set of truth-table rows. Different trees produce wildly different rhythms from these short windows because only a handful of input combinations are visited.
- **Matching parse trees to operator density:** Tree 7 (balanced split) with high NAND density creates a characteristic "double trigger" rhythm because both halves of the split can independently evaluate true. Tree 1 with high NOR density creates long gaps broken by single triggers — the right-spine structure means any true value anywhere in the chain collapses the NOR cascade.
- **Predictable reset points:** Because TruthCat4 is fully deterministic, a TR2 reset always returns to the same rhythm. Use RST mode to sync to other sequencers or to drop into a known groove at a section boundary.

---

## Credits

TruthCat4 by Andy Jenkinson / uglifruit, developed with Claude Code (Anthropic).
