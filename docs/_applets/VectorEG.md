---
layout: default
---
# Vector EG

![Screenshot 2024-06-13 15-50-29](https://github.com/djphazer/O_C-Phazerville/assets/109086194/01083c00-7c85-415a-9985-1e885b516b88)

VectorEG is a dual AR envelope generator based on Vector Oscillator waveforms. There is a variety of built-in waveforms from which to choose, or you can create your own with the [Waveform Editor](Waveform-Editor).

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Gate Ch1    | Gate Ch2    |
| CV INs |  N/A   | N/A    |
| OUTs   | Ch1 EG    | Ch2 EG    |


Controls
* Channel 1 EG Frequency
* Channel 1 waveform selection
* Channel 2 EG Frequency
* Channel 2 waveform selection


The EG runs freely until it gets to the second-to-last segment, and sustains at that point. When released, it proceeds to the level of the final segment at the speed specified by the final segment.

Unlike a traditional envelope generator, the speed of the envelope is determined with a single frequency control. The higher the frequency, the faster the envelope will run.

The outputs are uni-polar, and with positive offset, so that a level of -128 in the Waveform Editor corresponds to 0V, and a level of 127 is 5V.

VectorEG is in Hemisphere Suite starting with v1.6.
