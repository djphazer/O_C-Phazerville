---
layout: default
---
# SwitchSeq

![Screenshot 2024-06-13 15-44-08](https://github.com/djphazer/O_C-Phazerville/assets/109086194/3d4d16b0-e5dd-4e19-93da-a0e34b8129ea)

**SwitchSeq** is a flexible player for [Seq32](Seq32) patterns, providing various ways to switch between four sequences running in parallel. The global quantizers Q1-Q4 are used for outputs A-D, respectively. (Use [Hemisphere Config](Hemisphere-Config) to adjust Quantizer Scale settings)

## I/O

|        |    1/3     |    2/4     |
| ------ | :--------: | :--------: |
| TRIG   | Clock      | Reset      |
| CV INs | Octave / Seq select / Unquantized Input<br>(depends on Mode) | (depends on Mode) |
| OUTs   | Ch1 Pitch CV | Ch2 Pitch CV |


## Usage
4 sequences are played in parallel. The patterns themselves can be edited with [Seq32](Seq32), and can have various lengths. Currently, _Accent_ and _Mute_ have no effect in SwitchSeq.

The CV inputs can manipulate the sequences in a variety of ways.

There are two channels, with a shared Clock and Reset. Each channel has a few different Modes, controllable with the encoder.

### Modes:
* OCT: This mode is active when the sequence arrow is solid. The encoder will select the active sequence, with CV controlling the octave (by 1 or 2 octaves).
* PCK: The CV input will pick which sequence is playing. Sampled after each clock.
* RNG: A sequence is chosen at random when clocked. CV is ignored.
* QNT: The internal sequences are ignored and the CV input will be quantized using the global Quantizer settings.

## Credits
Authored by Nick Beirne
