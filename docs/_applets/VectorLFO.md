---
layout: default
---
# Vector LFO

![Screenshot 2024-06-13 15-51-26](https://github.com/djphazer/O_C-Phazerville/assets/109086194/8def472b-f8ad-49aa-8f8b-0ffb61a42096)

**Vector LFO** is a dual low-frequency oscillator based on Vector Oscillator waveforms. There is a variety of built-in waveforms from which to choose, or you can create your own with the [Waveform Editor](Waveform-Editor).

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Sync Clock Ch1    | Sync Clock Ch2    |
| CV INs | Freq or Shape Ch1    | Mix Ch1 into Ch2, or Shape Ch2    |
| OUTs   | LFO Ch1    | LFO Ch2 + Ch1 mix    |


### UI Parameters
* LFO 1 frequency
* LFO 1 waveform select
* LFO 2 frequency
* LFO 2 waveform select

The frequency range is from _insanely slow_ (~71 hours) up to Nyquist (~8kHz), and CV modulation tracks V/Oct, so it can be used as an audio-rate oscillator.

With either wave shape selected for editing, pressing the AuxButton assigns CV modulation to Shape - instead of controlling Frequency and Mix, the CV inputs can select different waveforms with semitone increments. Pressing AuxButton on the Frequency parameter reassigns CV modulation to Freq & Mix.

### Outputs

The first output is the output of only LFO 1. LFO 1 can be synchronized with an external clock at Digital 1, and its frequency can be modulated with CV 1.

The second output is a mixed output. By default, it is 100% LFO 2. Increasing voltage to CV 2 mixes in the LFO 1 signal up to a max of 50%/50% of each.

### Manual Oscillator Reset

When a new waveshape is selected from the panel, both LFO 1 and LFO 2 will be reset to the beginning of their respective cycles.

### Credits
VectorLFO was originally added in Hemisphere Suite v1.6, authored by Jason Justian. The Vector Oscillator engine has since been reworked extensively by qiemem (Bryan Head), and the LFO applet modified by qiemem & djphazer.
