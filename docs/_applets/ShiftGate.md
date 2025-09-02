---
layout: default
---
# ShiftGate

![Screenshot 2024-06-13 15-36-00](https://github.com/djphazer/O_C-Phazerville/assets/109086194/ccca291c-2e3e-404b-a130-b2d2bdddc3b5)

**ShiftGate** is a dual shift register-based gate/trigger sequencer for creating aleatoric rhythm patterns.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Clock   |  Freeze   |
| CV INs |   Flip bit 0 (Ch A)  |  Flip bit 0 (Ch B)   |
| OUTs   | Gate/Trigger Ch A   |  Gate/Trigger Ch B   |


## UI Parameters
* Length
* Output type

ShiftGate has two channels controlled by a single clock input. Each channel starts with a random 16-bit register*. When a clock is received at Digital 1, the following things happen:

1. The register is shifted to the left. 
1. If Digital 2 is high, the register is frozen; the high bit (based on length) of the previous value is moved back to the beginning (bit 0), and skip to 4.
1. If the CV input for the corresponding channel is low, the high bit (based on length) of the previous value is moved back to the beginning (bit 0). If the CV input is high, the high bit is flipped, and that value is put at the beginning. This is known as an XOR operation.
1. The value of bit 0 is examined. If the channel's Type is set to "Trig," then a trigger will be sent from the channel's output if the value is 1. If the channel's type is set to "Gate," the gate state is high if the value is 1. The gate will then _remain_ high until the next time the register's bit 0 is 0.

ShiftGate is in Hemisphere Suite from v1.5.

_* When ShiftGate's state is saved via SysEx or system save, Output A's register is saved, and Output B's register is randomized._
