---
layout: default
---
# Clock Skipper

![Screenshot 2024-06-13 14-35-42](https://github.com/djphazer/O_C-Phazerville/assets/109086194/5bb55b47-c946-4311-a33a-0314548b7fa7)

Clock Skipper is a dual probability skipper, with independent inputs.

When an incoming clock is received at the digital input, that clock may or may not be sent to the corresponding output, depending on the probability. A probability of 100% means that the gate will always be passed along. Probability can be modified with voltage to the CV inputs. Negative voltage decreases probability, and positive voltage increases probability.

### I/O

|        |                  1/3                  |                  2/4                  |
| ------ | :-----------------------------------: | :-----------------------------------: |
| TRIG   |            Channel 1 Clock            |            Channel 2 Clock            |
| CV INs | Ch 1 Probability modulation (bipolar) | Ch 2 Probability modulation (bipolar) |
| OUTs   |              Ch 1 Clock               |              Ch 2 Clock               |


### UI Parameters
* Ch 1 Probability
* Ch 2 Probability


Adapted from [Clock Skipper](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Clock-Skipper) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
