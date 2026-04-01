---
layout: default
---
# EuclidO

![EuclidO screenshot](images/EuclidO.png)

A single-channel Euclidean pattern generator with a circular ring visualization, up to 16 steps. Active steps (fills) are shown as filled circles on the ring, inactive steps as hollow circles. A triangle marker indicates the pattern's rotation point, and a square cursor tracks the current playhead position. The center of the ring displays the currently selected parameter and its value.

### I/O

|        |       1/3        |      2/4       |
| ------ | :--------------: | :------------: |
| TRIG   |      Clock       |     Reset      |
| CV INs |        —         |       —        |
| OUTs   | On-step Triggers | Off-step Triggers |

### UI Parameters
Press the encoder button to cycle through parameters, then turn the encoder to adjust:

* **Fills** — Number of active hits distributed across the pattern (1 to length)
* **Offset** — Rotation of the pattern; wraps around in both directions
* **Length** — Total number of steps in the pattern (1 to 16)

### Outputs
* **Output 1** sends a trigger pulse on each active (filled) step.
* **Output 2** sends a trigger pulse on each inactive (empty) step.

Global Pulse Length can be adjusted in Hemisphere Config.