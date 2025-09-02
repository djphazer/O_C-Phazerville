---
layout: default
---
# Trending

![Screenshot 2024-06-13 15-46-34](https://github.com/djphazer/O_C-Phazerville/assets/109086194/b7dd6304-b608-40fc-97ff-4ee642cc1b21)

**Trending** is a dual slope detector with assignable outputs.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  N/A   |  N/A   |
| CV INs |  Signal 1   |  Signal 2   |
| OUTs   |  Gate / trigger Ch A (corresponding to Signal 1)  |  Gate / trigger Ch B (corresponding to Signal 1)   |


### UI Parameters
* Channel A mode
* Channel B mode
* Sensitivity

## Output modes:
* Rising: The assigned output is a gate, which is high when the signal is rising
* Falling: The assigned output is a gate, which is high when the signal is falling
* Steady: The assigned output is a gate, which is high when the signal is steady
* Moving: The assigned output is a gate, which is high when the signal is rising or falling
* ChgState: The assigned output emits a trigger when the signal changes from one state to another (e.g., rising to steady, steady to falling, falling to rising, etc.)
* ChgValue: The assigned output emits a trigger when the signal changes its value by more than 1/4 semitone (or about .02V)

## Sensitivity

The sensitivity control can be used to fine-tune the response of the slope detector. At lower settings, the detector will respond more slowly, but will be more consistent. At higher settings, the detector will respond faster, but may change direction more erratically.
