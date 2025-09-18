---
layout: default
---
# Clock Divider

![Screenshot 2024-06-13 14-34-19](https://github.com/djphazer/O_C-Phazerville/assets/109086194/1a4bd41e-1c36-473b-8c4e-75fe55a00c0b)

Complex clock divisions / multiplications made easy! **ClockDiv** is a dual, complex clock divider and multiplier, using a single clock source. Each stage can divide from 1:1 to 1:32 (for a maximum of 1:1024), or multiply from 1:1 to 32:1 (for a theoretical maximum of 1024:1 — although in practice you're unlikely to exceed 180:1 without strange behaviour). Voltage control is available for the first stage of each channel (displaying a CV icon when engaged).

### I/O

|        |                     1/3                     |                     2/4                     |
| ------ | :-----------------------------------------: | :-----------------------------------------: |
| TRIG   |                 Clock input                 |                    Reset                    |
| CV INs | Ch 1 div/mult factor 1 modulation (bipolar) | Ch 2 div/mult factor 1 modulation (bipolar) |
| OUTs   |            Ch 1 div/mult result             |            Ch 2 div/mult result             |


### UI Parameters
* Ch 1 div/mult factor 1
* Ch 1 div/mult factor 2
* Ch 2 div/mult factor 1
* Ch 2 div/mult factor 2

### Notes
Division works by counting clocks and sending a trigger on the _n_th clock. When the encoder is turned, the counter is reset.

Multiplication works by counting interrupt cycles (c) between the last two clock inputs, and sending a trigger every c/n cycles.

CV inputs are bipolar, with a range of about -2.5 volts to about 2.5 volts. Positive values set clock division, and negative values set clock multiplication. There's a small center detent CV range, over which range control is only set from the panel. This is done because O_C can't sense whether a jack is patched.

### Credits
Adapted from [Clock-Divider-Multiplier](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Clock-Divider-Multiplier) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
