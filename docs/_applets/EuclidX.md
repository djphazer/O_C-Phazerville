---
layout: default
---
# EuclidX

![EuclidX screenshot](images/EuclidX.png)

A deluxe Euclidean pattern generator, two channels, up to 32 steps each. Using UI code from qiemem; named after adegani's version of the applet with the flexible CV Input routing. The extra zero-padding parameter makes it worthy of the *X* ;-)

### I/O

|        |      1/3      |     2/4      |
| ------ | :-----------: | :----------: |
| TRIG   |     Clock     |    Reset     |
| CV INs |  Assignable   |  Assignable  |
| OUTs   | Ch 1 Triggers | Ch 2 Trigger |

### UI Parameters
* Pattern length
* Pattern fill (hits)
* Offset (right shift)
* Padding (empty steps added to the pattern length)
* CV Input assignments

### CV Inputs
The CV inputs will modulate whatever parameter the tiny _1_ or _2_ is sitting next to.

### Outputs
Each channel outputs a standard trigger pulse when clocked. Global Pulse Length can be adjusted in Hemisphere Config.

## Future ambitions
* Expand the pattern generator to 64 bits...
* Some kind of UI tweaks to better visualize patterns longer than 16.

## Credits
This applet is a mashup of code from qiemem, adegani, djphazer, and Chysn, with icons by ph-xyz
