---
layout: default
---
# VectorMorph

![Screenshot 2024-06-13 15-53-40](https://github.com/djphazer/O_C-Phazerville/assets/109086194/386a8e5d-a1e4-4d7b-8be0-71c20f30259c)

[Video demo](https://youtu.be/WrENiAQHhq0)

**VectMorph** is a morph controller inspired by the Doepfer A-144, and based on Vector Oscillator waveforms. There is a variety of built-in waveforms from which to choose, or you can create your own with the [Waveform Editor](Waveform-Editor).

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |   N/A  |  N/A   |
| CV INs |  Phase Ch 1 (and Ch 2 when linked)   |  Phase Ch 2   |
| OUTs   |  Ch 1   |  Ch 2   |


### UI Parameters
* Channel 1 Phase Offset
* Channel 1 Waveform selection
* Channel 2 Phase Offset
* Channel 2 Waveform selection

The Vector Morph Controller (VectMorph) is a modulation source for sending phase-offset output to one or more destinations. A typical use case would involve sending overlapping phased triangular waves to control a voltage-controlled mixer.

Each channel can be assigned a Phase Offset, in degrees. This offset is added to the voltage-determined phase to determine the current amplitude of the channel's output.

## Linked Mode

By default, CV 1 controls the phase of both channels. This is Linked Mode. You can use two instances of the Vector Morph Controller, one in each hemisphere, to control up to four things. To use Linked Mode in both hemispheres, mult one hemisphere's CV 1 input to the other hemisphere's CV 1 input (as demonstrated in the video).

If you wish to control the phase of each channel independently, just send some voltage to CV 2. This will disable Linked Mode, and the Link icon will disappear. To return to Linked Mode, stop sending voltage to CV 2, and use the encoder to change the waveform of either channel.

Vector Morph Controller is in Hemisphere Suite starting with v1.6.
