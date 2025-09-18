---
layout: default
---
# DivSeq

![DivSeq Screenshot](images/DivSeq.png)

**DivSeq** is a dual sequential clock divider with a single input clock. Each of the two channels is composed of a sequence of up to 5 clock dividers. Under normal operation, a step "n" triggers 1 clock pulse, skips n-1 pulses, and then steps to the next clock divider. Multipliers fire _n_ triggers within 1 clock pulse and then advance.

Via the CV inputs, each channel may also be in inverted mode (skip 1, trigger n-1 pulses, step) with positive voltage, or cross-channel XOR mode with negative voltage. There is a virtual detent around 0v for normal mode.

Performance mutes may be manually toggled for each divider step. Each step may be set to a divider of 0 (off) up to 63.

[See it in action!](https://youtu.be/J1OH-oomvMA?si=Z97wJ3HXe0HocaBa&t=357)


### I/O

|        |                         1/3                         |              2/4              |
| ------ | :-------------------------------------------------: | :---------------------------: |
| TRIG   |                        Clock                        |             Reset             |
| CV INs | Ch 1 mode:<br>inverted (positive)<br>XOR (negative) | Ch 2 mode:<br>inverted<br>XOR |
| OUTs   |                Ch 1 Trigger sequence                |     Ch 2 Trigger sequence     |


### UI Parameters
* Ch 1 divider value steps 1-5
* Ch 2 divider value steps 1-5
* Ch 1 performance mutes steps 1-5
* Ch 2 performance mutes steps 1-5

With a step highlighted for editing, the AuxButton action will also toggle mute.

### Tips
The total length of the trigger pattern is the sum of all 5 steps (with any multipliers occupying just 1 step). For musical applications in standard 4/4 time signatures, you can get a regular repeating phrase as long as the steps add up to a power of two - 16, 32, or 64 work well. Similar results for 3/4 or 6/8 time if the steps add up to 12, 24, 48, etc.

A pattern of (28, x4, 2, 1) puts a little fill at the end of a 32-step phrase.

Use a single step set to "4" for a standard 4/4 kick. Send a positive gate into the CV input to invert the same pattern for a techno bassline.

Mute the first step to skip that many Clocks at the beginning after Reset - useful for offsetting a snare pattern.
