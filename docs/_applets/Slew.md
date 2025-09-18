---
layout: default
---
# Slew

![Screenshot 2024-06-13 15-38-53](https://github.com/djphazer/O_C-Phazerville/assets/109086194/547fe806-1acb-4628-9b65-b408316e7c39)

**Slew** is a simple slew (or lag) processor. Two independent channels share the same settings. Channel 1's output is linear, and Channel 2's output is exponential.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Defeat Ch1    | Defeat Ch2    |
| CV INs | Input Ch1    | Input Ch2    |
| OUTs   | Linear Ch1 output    | Exponential Ch2 output    |


### UI Parameters
* Rise time
* Fall time

When Rise or Fall values are changed, a time (in ms) will briefly appear on the display. This indicates the approximate time that it would take for the linear signal to rise or fall 5 volts. The time between actual voltages will be proportionate to this time, and the exponential signal will take less time.

When a high gate is present at either digital input, the corresponding channel's slew will be defeated. That is, the input will pass through to the output.
