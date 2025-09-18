---
layout: default
---
# Voltage

![Screenshot 2024-06-13 15-54-04](https://github.com/djphazer/O_C-Phazerville/assets/109086194/1ac30678-a0f4-495f-a95c-e479a9fcdd88)

**Voltage** is a dual gate-activated fixed-voltage emitter.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Gate Ch1 | Gate Ch2 |
| CV INs |   N/A  |  N/A   |
| OUTs   | Voltage Ch1 | Voltage Ch2 |

### UI Parameters
* Ch1 Voltage
* Ch1 Gate behaviour
* Ch2 Voltage
* Ch2 Gate behaviour

### Notes
The voltage per channel is selectable in 1 semitone (approx. .08V) increments. The available range is the min/max your hardware supports. On VOR-capable units, this depends on the current Vbias setting (default -5V to +5V).

For output, there are two gate states available:

* **G-On**: When the gate of the corresponding digital input is high, the output is the specified voltage. Otherwise, it's 0V.
* **G-Off**: When the gate of the corresponding digital input is low, the output is the specified voltage. Otherwise, it's 0V.

The indicator over the output name (A,B,C,D) will display when the corresponding output is emitting non-zero voltage.

### Credits
Adapted from [Voltage](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Voltage) by Chysn; available in Hemisphere Suite from v1.4.
