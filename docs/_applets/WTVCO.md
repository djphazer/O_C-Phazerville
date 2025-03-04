---
layout: default
---
# WTVCO

![WTVCO Screenshot](images/WTVCO.png)

An 8-bit wavetable voltage-controlled oscillator. The CV inputs are assignable. Digital inputs 1 & 2 shift the pitch range down and up by octave steps.

The visualizer shows the selected waveform. A, B, and C are the source waveforms and ~ is the output wave. Use the encoder to choose new source waveforms, or navigate to the parameter adjustment and mapping pages by pressing with ~ selected.

As the core feature of the applet, CV modulation of the **Blend** parameter will change the shape of the output waveform. When Blend CV = 0V, the output will resemble waveform A, at +2.5V it will resemble waveform B, and at +5V, waveform C. Any CV in between will produce a proportional blend of the corresponding pair of source waves. Voltages beyond +5 and below 0 will result in "inverted-interpolation-overflow-wavefolding," which is rad. Try it!

### I/O

|        | 1/3                   | 2/4                      |
| ------ | :-------------------: | :----------------------: |
| TRIG   | Oct-Shift Down        | Oct-Shift Up             |
| CV INs | Assignable (Pitch)    | Assignable (Blend)       |
| OUTs   | Blended Waveform      | Reverse Blended Waveform |

## Parameters:
* Pitch - controls output frequency. Encoder adjusts base pitch in semitone increments. CV input modulates pitch offset using V/Oct standard. CV1 is set to Pitch offset by default. Responds well to audio-rate FM. Full frequency range is from 8000ish to 0.02 Hz. Works great as a funky LFO (can wobble with a period as slow as 62 seconds). Toward the high end of the frequency range there are some interesting artifacts and aliasing effects.
* Blend - morphs output waveform proportionally between a pair of selected source waveforms (A/B or B/C). Blend can be adjusted by encoder, or CV input modulation. CV2 is set to Blend by default. The Output Visualizer displays blended wave shape. A, B, and C visualizers show the respective source waves.
* Volume - regular ordinary volume control. 0-100%. Can be modulated using CV.
* SqDuty - controls the width of the pulse wave. 127 = 50% (square).
* SR.Div - sample rate division. This parameter tells the phase accumulator how many controller cycles to wait before updating. In other words, it decreases the audio sample rate, which in turn decreases the pitch in "sub-harmonic" intervals, but also produces some very nice downsample distortion at higher values. Sequence this parameter for interesting results!

## Outputs
* Output 1 - Blended Waveform: Does everything a normal VCO does, and looks cool doing it.
* Output 2 - Reverse Waveform: A backwards version of Output 1's waveform. Quite useful when used as an LFO. Independent pitch control of Output 2 is on the to-do list :).

## Aux Button Functions
* Noise Freeze - while the Noise wave is displayed in the waveform selection menu, press the Aux Button to toggle between "realtime" and "frozen" noise buffer.
* Random-Step Re-Roll - while the RandStp wave is displayed in the waveform selection menu, press the Aux Buttom to instantly re-randomize the step heights. The steps are randomized each time the waveform is re-selected, but this shortcut prevents extra encoder movements.

### Credits
Authored by beau.seidon, with lots of good advice from qiemem and djphazer.

Inspired by Professor Bruce Land of Cornell University, and particularly Lab 3 (Audio Synthesis) from his "AVR microcontroller lectures 2012" YouTube series.
