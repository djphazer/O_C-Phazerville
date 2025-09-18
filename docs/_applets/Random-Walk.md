---
layout: default
---
# Random Walk

![Screenshot 2024-06-13 15-21-00](https://github.com/djphazer/O_C-Phazerville/assets/109086194/6c3dd101-3c91-4180-9f33-6130dc4763dd)

**RndWalk** is a dual-channel stepped random bipolar CV generator. Range sets the total "distance" available to explore, and Step sets the maximum travel of each random step. The output voltage range may be scaled to full, 1 octave, 1 semitone, or 0.5 semitones.

Output Y may be clocked using either Trigger input 1/3 or 2/4, and subject to a clock division up to `/32`

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  X Clock (also assignable to Y clock)  |   Assignable to Y clock |
| CV INs |   Range  |  Step   |
| OUTs   |  Random Walk X   |  Random walk Y   |

### UI Parameters
 - Range
 - Step
 - Smooth
 - Y Trigger input
 - Y Trigger division
 - CV Range

### Credits
Authored by adegani
