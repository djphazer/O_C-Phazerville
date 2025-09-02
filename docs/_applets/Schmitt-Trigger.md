---
layout: default
---
# Schmitt Trigger

![Screenshot 2024-06-13 15-23-33](https://github.com/djphazer/O_C-Phazerville/assets/109086194/1f7b5b58-7818-4b2c-9879-b5ff620ca812)

This applet is a dual Schmitt Trigger with a programmable threshold range.

A Schmitt Trigger is a type of comparator that provides hysteresis in a modular patch. Each Schmitt Trigger's output goes high when its input crosses the High threshold (default of 2.6V), and stays high until the input goes back below the Low threshold (default of 2.1V).

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  N/A   |   N/A  |
| CV INs |  Signal 1   |  Signal 2   |
| OUTs   |  Gate A (if Signal 1 between high and low threshold)  |  Gate B (if Signal 2 between high and low threshold)  |


### UI Parameters
* Low threshold
* High threshold



Schmitt Trigger is in Hemisphere Suite from v1.4.
