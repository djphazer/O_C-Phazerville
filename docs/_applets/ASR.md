---
layout: default
---
# "A"SR

![Screenshot 2024-06-13 14-18-20](https://github.com/djphazer/O_C-Phazerville/assets/109086194/5350b4c4-f263-4374-944b-25beb01e46c8)

**"A"SR** is an "Analog" Shift Register simulation with a 256-step ring buffer.

When selected in one hemisphere (Unlinked Mode), it has two indexed outputs. When selected in both hemispheres (Linked Mode), the same buffer is used across all four outputs and a link icon will appear in the upper right corner of the screen.

### I/O (Unlinked Mode)

|        |             1/3             |                           2/4                           |
| ------ | :-------------------------: | :-----------------------------------------------------: |
| TRIG   |   Clock (Advance buffer)    |               Gate freeze writing buffer                |
| CV INs |    Signal to be sampled     | Index (n) modulation (bipolar between -32 and 32 steps) |
| OUTs   | Most recent value in buffer |                Buffer value n steps back                |

### I/O (Linked Mode)
When linked, the scale may be different for each hemisphere, but the base index will be the same (and may be modulated independently).

|                |              1              |         2               | 3                       | 4                       |
| -------------- |:---------------------------:|:-----------------------:|:-----------------------:|:-----------------------:|
| Trig (Digital) | Clock (Advance buffer)      | No effect               | No effect               | No effect               |
| CV In          | Signal to be sampled        | Index (n) modulation    | No effect               | Index (m) modulation   |
| Output         | n steps back in buffer      | 2n steps back in buffer | 3m steps back in buffer | 4m steps back in buffer |

Note: At an index of 0, all outputs will be the same. When the index value is modulated by CV, a small CV indicator displays below the value.

### UI Parameters
* Index
* Edit Quantizer settings, per output

### Credits
Adapted from ["A"SR](https://github.com/Chysn/O_C-HemisphereSuite/wiki/%22A%22SR) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
