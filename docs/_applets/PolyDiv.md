---
layout: default
---
# PolyDiv

![Screenshot 2024-06-13 15-18-07](https://github.com/djphazer/O_C-Phazerville/assets/109086194/121479a0-62cd-41a7-ad41-293f4cf21fb6)

**PolyDiv** is a set of 4 clock dividers running in parallel, driven by a single Clock and Reset. A combination of them can be routed to each output with a 2x4 matrix.

The CV inputs can be used to toggle **XOR** mode per channel - a positive voltage switches from the default logical **OR** operation to **XOR**. This means two simultaneous pulses from the selected clock dividers will cancel each other out, leading to more curious polyrhythmic patterns.

[See it in action!](https://www.youtube.com/watch?v=J1OH-oomvMA&t=305s)

### I/O

|        |                         1/3                         |              2/4              |
| ------ | :-------------------------------------------------: | :---------------------------: |
| TRIG   |                        Clock                        |             Reset             |
| CV INs | Ch 1 mode:<br>XOR (positive)<br>OR (zero) | Ch 2 mode:<br>XOR<br>OR |
| OUTs   |                Ch 1 Trigger sequence                |     Ch 2 Trigger sequence     |

### UI Parameters
* Division value for each of the four clock dividers
* Checkboxes to route pulses to outputs

### Credits
Authored by djphazer, loosely based on the Moog Subharmonicon sequencer
