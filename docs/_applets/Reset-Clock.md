---
layout: default
---
# Reset Clock

![Screenshot 2024-06-13 15-20-41](https://github.com/djphazer/O_C-Phazerville/assets/109086194/283433ed-813c-47ce-a7fe-95620982682f)

**ResetClk** is a tool to assist with sequencers that can only advance forward (like the DFAM). It keeps track of the current step position and emulates a "reset" by quickly sending trigger pulses to loop back to the desired step. The first CV input modulates the position offset, and the applet keeps the actual sequencer position synchronized.

[See it in action!](https://youtu.be/i1xU6-oPwfA)

### I/O

|        |                1/3                |                2/4                 |
| ------ | :-------------------------------: | :--------------------------------: |
| TRIG   |         Clock                     |         Reset         |
| CV INs | Offset (actively corrects position)  | N/A   |
| OUTs   |     Sequence Advance Trigger      |             Final Trigger             |

* The first output is the primary one to use for advancing another sequencer.
* The second output is a trigger that waits to fire until the rapid bursts are finished and the step position has settled. Use this one for triggering envelopes.

### UI Parameters
* Length
* Offset - Reset jumps back to this position; changing with encoder actively corrects current position
* Spacing between pulses
* Current position - to calibrate display with actual sequencer position

### Credits
Copied/Adapted from [Peter Kyme](https://github.com/pkyme/O_C-HemisphereSuite/tree/reset-additions)
