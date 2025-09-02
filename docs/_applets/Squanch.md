---
layout: default
---
# Squanch

![Screenshot 2024-06-13 15-40-36](https://github.com/djphazer/O_C-Phazerville/assets/109086194/1a6bca7f-bf52-48b7-8b78-eca85b53700c)

**Squanch** is a pitch-shifting quantizer with a single input and two pitch-shifted outputs, useful for producing harmonic intervals. It can be used as a voltage adder.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock | +1 Octave for Pitch 1 |
| CV INs | Main Signal Input | Transpose/Shift for Pitch 2 |
| OUTs   | Pitch CV 1 | Pitch CV 2 |


### UI Parameters
* Pitch shift for each output, in scale degrees
* Note Wrap range for each output, in scale degrees
* Scale
* Root

### Notes
A clock pulse at Digital 1 causes Squanch to sample the signal at CV 1 and quantize it. A gate at Digital 2 adds one octave to the output at A/C.

CV 1 is the signal to be quantized. CV 2 is a bi-polar shift input that adds (or subtracts, if negative) voltage to or from the output at B/D.

Each channel begins in continuous operation. That is, the incoming CV is quantized at a rate of about 16667 times per second. This might result in undesirable slippage between notes, so clocked operation is available.

To enter clocked operation, send a clock signal to Digital 1. A clock icon will appear next to the selector for the scale to indicate that that quantizer is in clocked mode.

To return to continuous operation, stop sending clock to the quantizer, and then change the scale. As long as no additional clock signals are received, the quantizer will remain in continuous operation.

### Credits
Adapted from [Squanch](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Squanch---Shifting-Quantizer) by Chysn; in Hemisphere Suite starting with v1.5.
