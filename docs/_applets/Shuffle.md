---
layout: default
---
# Shuffle

![Screenshot 2024-06-13 15-38-15](https://github.com/djphazer/O_C-Phazerville/assets/109086194/43bdec61-eab5-4d57-a44f-0b32a89f23e7)

**Shuffle** is a two-step clock offset. Each step can be delayed by between 0% and 99% of the incoming clock tempo.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock    | Reset    |
| CV INs | % Delay for 1st step (bipolar) | % Delay for 2nd step (bipolar) |
| OUTs   | Shuffled Clock | Triplet Clock |


### UI Parameters
* Percentage delay of each step

Note that reset does not, itself, trigger a clock. It just brings the step back to the beginning so that another clock can trigger the first step.
