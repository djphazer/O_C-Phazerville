---
layout: default
---
# Switch

![Screenshot 2024-06-13 15-43-19](https://github.com/djphazer/O_C-Phazerville/assets/109086194/d2f17800-b343-4bbb-b6ab-44d38d48dce1)

[Video demo](https://youtu.be/juu65pJyXlY)

**Switch** is a two-channel switch with two switching methods: sequential and gated. 

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Flip/flop (Ch A)   |   Gate (Ch B)  |
| CV INs |  Signal 1   |   Signal 2  |
| OUTs   |  Channel A   |   Channel B  |


## UI Parameters
* None

The solid meter on the far left represents the state of output Channel A (noting the active input signal), likewise the solid meter on the far right represents output Channel B. The pair of meters in the middle represent input signals 1 and 2.

The sequential output (Ch A) alternates between Signal 1 and Signal 2, and the gated output (CH B) sends Signal 1 when digital Digital 2 is low, and Signal 2 when high.

**Note**: _The Ornament and Crime's circuitry is not made for precision 1:1 reproduction of voltages. The outputs will not exactly match the inputs, so Switch is not suitable for pitch CV. This is inherent to the O_C's design and does not indicate poor calibration._
