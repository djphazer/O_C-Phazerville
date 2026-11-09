---
layout: default
---

# Burst

![Burst screenshot](images/Burst.png)

**Burst** is a burst generator based loosely on the Ladik S-075.

### I/O

|      |        1/3       |                      2/4                      |
| ---- | :--------------: | :-------------------------------------------: |
| TRIG | Clock (optional) | Start burst sequence |
| CV INs | Steps | Spacing |
| OUTs | Burst triggers | Gate (high while the burst is active) |

### UI Parameters

- **Passthru:** Selects whether clock pulses at Trig 1 or external trigger pulses at Trig 2 are combined with the burst sequence at Output 1, or neither.
- **Skip percentage:** Sets the probability that a trigger will be skipped.
- **Steps (1–12):** Sets the number of triggers in the Burst.
- **Spacing (8–500 ms):** Sets the time between triggers in unclocked mode. In clocked mode, timing follows the incoming clock and can exceed the manual 500 ms range.
- **Acceleration/deceleration:** Changes the spacing between successive steps. Positive values bring later steps closer together; negative values spread them farther apart.
- **Clock division/multiplication:** In clocked mode, adjusts the timing relationship to the incoming clock.

### Notes

**CV1 — Steps**

CV1 modulates the Steps setting. Negative voltage decreases the value, while positive voltage increases it. The CV indicator appears when the modulated value differs from the panel setting.

**CV2 — Spacing and clock division/multiplication**

In unclocked mode, CV2 modulates the spacing between steps. Positive voltage increases the spacing, while negative voltage decreases it. The CV indicator appears when the modulated spacing differs from the panel setting.

In clocked mode, CV2 modulates the effective clock division or multiplication setting relative to the panel setting.

**Status indicators**

- **Steps remaining:** The counter at the bottom of the screen shows the number of steps remaining in the current sequence.
- **Zap icon:** Indicates that the burst is active.
- **Skip indicator:** An asterisk briefly appears when a trigger is skipped because of the Skip percentage setting.

If a new trigger arrives at Trig 2 before the current sequence finishes, a new sequence starts.

### Credits

Adapted from [Burst](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Burst) © 2018–2022, Jason Justian and Beige Maze Laboratories.
