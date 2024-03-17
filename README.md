For information on the Phazerville Suite, please see the [original repository](https://github.com/djphazer/O_C-Phazerville)
Shoutout to djphazer of course, and everyone who's contributed to the firmware!

This is a copy of Phazerville, with an added applet in Hemispheres mode called **EnsOscKey**.
This applet was designed for use with the 4ms Ensemble Oscillator. The goal is to change the Ensemble Oscillator's **Scale** value to the appropriate chord shape, given the root, scale, and current note.

Check out a [video demonstration](https://youtu.be/lqLH0176VFw?si=NIXoEPxWpawai7iP)

The applet is essentially a repurposed DualQuant, another applet from Hemispheres. Some functionality and behavior is retained.

## How to use
Set your root and scale at the top. On the line underneath the top options, you'll see the resulting chord printed. Below that are 4 fields, labeled: `I:`, `i:`, `o:` and `x:`. The values next to them represent the position on the Ensemble Oscillator's **Scale** knob (1-10) that the output voltage should point to when the function results in that chord type.
`I` = the value to output when the resulting chord is Major, `i` = minor, `o` = diminished, and `x` is the value to output if the note is not in the scale.

#### Channel 1
- **CV1**: Send a quantized sequence into the CV input of channel 1. In order for things to work right, make sure that the root and scale of EnsOscKey matches that of the quantized input sequence.
- **TR1**: TR1 functions the same way it does in DualQuant. It allows you to clock your quantization. If using a clock in TR1, the output voltage will not change until the next trigger is received.

#### Channel 2
Channel 2 isn't needed for the primary functions of this applet. These are some additional points of optional control.
- **TR2**: If the cursor is not already in Edit Mode (i.e. not currently highlighting any value), a trigger into TR2 will activate Edit Mode, with the `I` field selected. Each subsequent trigger will move the cursor to the next voltage output setting (i.e. one of the bottom 4 fields). It will continue looping around the 4 voltage output settings.
- **CV2**: CV into CV2 will change the value of whichever voltage output setting field is highlighted.
  


