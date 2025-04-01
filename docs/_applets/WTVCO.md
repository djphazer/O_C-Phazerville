---
layout: default
---
# WTVCO

![WTVCO Screenshot](images/WTVCO.png)

An 8-bit wavetable voltage-controlled oscillator. The CV inputs are assignable. Digital inputs 1 & 2 shift the pitch range down and up by octave steps.

The visualizer shows the selected waveform. A, B, and C are the source waveforms and ~ is the output wave. Use the encoder to choose new source waves, or navigate to the parameter adjustment and mapping pages by pressing with ~ selected.

As the core feature of the applet, CV modulation of the **Blend** parameter will change the shape of the output waveform. When Blend CV = 0V, the output will resemble waveform A, at +2.5V it will resemble waveform B, and at +5V, waveform C. Any CV in between will produce a proportional interpolation of the corresponding pair of source waves. Voltages beyond +5 and below 0 will result in "inverted-interpolation-overflow-wavefolding," which is rad. Try it!

### I/O:

|        | 1/3                     | 2/4                                  |
| ------ | :---------------------: | :----------------------------------: |
| TRIG   | Oct-Shift Down          | Oct-Shift Up                         |
| CV INs | Assignable (Pitch)      | Assignable (Blend)                   |
| OUTs   | Interpolated Waveform 1 | Interpolated Waveform 2 (Reversable) |

## Parameters:
* **Pitch** - controls output frequency.
  - Encoder adjusts base pitch in semitone increments. Pitch for _both outputs_ are shown on Params menu.
  - CV input modulates pitch offset using V/Oct standard (CV1 controls Output1, CV2 controls Output2). _CV1 is set to Pitch offset by default._
  - Full frequency range is from 8000ish to 0.0101 Hz. Responds well to audio-rate FM.
  - Works great as a funky LFO (can wobble with a period as slow 99 seconds, before CV offset).
  - Toward the high end of the frequency range there are some interesting artifacts and aliasing effects.
* **Blend** - morphs output waveform proportionally between a pair of selected source waveforms (A/B or B/C).
  - Blend can be adjusted by encoder, or CV input modulation. _CV2 is set to Blend by default._
  - The Output Visualizer displays blended wave shape. A, B, and C visualizers show the respective source waves.
* **Osc2 Reverse** -  waveform of output 2 is reversable on the Params menu.

* **Volume** - regular ordinary volume attenuation, [0-100%].
  - Controls volume of both outputs.
* **SqDuty** - controls the width of the pulse wave.
  - 127 = 50% (square).
* **SR.Div** - sample rate division.
  - Tells the phase accumulator how many controller cycles to wait before updating.
  - Decreases the pitch in "sub-harmonic" intervals.
  - Also produces some very nice downsample distortion at higher values.
  - Sequence this parameter for interesting results!

## Outputs:
* **Output 1** - Osc1 Interpolated Waveform: Does everything a normal VCO does, and looks cool doing it.
* **Output 2** - Osc2 Interpolated Waveform: A second voice with the same shape as Output 1's waveform,
  - Independent Pitch control, assignable to CV2 input.
  - Output can be toggled between Forward and Reverse mode, quite useful when used as an LFO.

## Aux Button Functions:
* **Noise Freeze** - while the Noise wave is displayed in the waveform selection menu, toggles between "realtime" and "frozen" noise buffer.
* **Random-Step Re-Roll** - while the RandStp wave is displayed in the waveform selection menu, instantly re-randomizes the step heights.
  - The steps are randomized each time the waveform is re-selected, but this shortcut prevents extra encoder movements.

### Credits:
Authored by beau.seidon, with lots of good advice from qiemem and djphazer.

Inspired by Professor Bruce Land of Cornell University, and particularly Lab 3 (Audio Synthesis) from his "AVR microcontroller lectures 2012" YouTube series.
