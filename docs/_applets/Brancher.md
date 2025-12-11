---
layout: default
---
# Brancher

![Brancher screenshot](images/Brancher.png)

**Brancher** is a Bernoulli Gate, inspired by Mutable Instruments Branches. It sends incoming gate or clock from the first Trig input to one of two outputs, based on a selected probability. The second Trig input can be used to flip-flop the outputs as on/off gates - one stays high until another trigger flips it.

### I/O

|        |                1/3                 |    2/4     |
| ------ | :--------------------------------: | :--------: |
| TRIG   | Signal (clock or gate) for routing | Flip-Flop Clock  |
| CV INs |  Probability modulation (bipolar)  | No effect  |
| OUTs   |             Output A/C             | Output B/D |

### UI Parameters
* Probability
* Press encoder to override probability-selected output and switch to the other one

### Credits
Adapted from [Brancher](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Brancher) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
