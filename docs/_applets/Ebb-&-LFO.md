---
layout: default
---
# Ebb & LFO

![Ebb And LFO screenshot](images/EbbAndLFO.png)

This is a single Tides-like oscillator with two outputs. Both inputs and outputs are configurable. The outputs are visualized on the screen.

It can function like a looping envelope generator or an audio-rate oscillator, with modulatable parameters for morphing the contour. Triggers to input 1/3 may function as tap tempo. "One Shot" mode will output a single waveform cycle and stop, much like the AD envelope mode on Tides (use Reset to retrigger).

### Clocked Mode

When the mode is switched from V/Oct to Clocked, the Frequency control is replaced with a clock divider/multiplier. This mode uses the clever Mutable Instruments pattern predictor algorithm to follow incoming clock trigger patterns and match the groove.

### I/O

|        |                          1/3                          |    2/4     |
| ------ | :---------------------------------------------------: | :--------: |
| TRIG   |                         Clock                         |   Reset/Retrigger    |
| CV INs | Assignable<br>(Freq / Slope / Shape / Fold) | Assignable<br>(Amp / Slope / Shape / Fold) |
| OUTs   |                      Waveform A                       | Waveform B |

### CV Inputs & UI Parameters
These are also the corresponding on-screen parameters.
Each input is assigned to one of:
* Level / **Am**plitude (CV2 only)
* Frequency (**Hz**) (CV1 only) - tracks V/Oct for audio-rate operation
* **Sl**ope - skews the waveform (much like the deprecated [SkewedLFO](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Skewed-LFO) applet)
* **Sh**ape - morphs between various curved segment combos
* **Fo**ld - wavefolder!

Lastly, there is a UI toggle for _One Shot_ mode.

### Outputs
Each output can be one of:
* **Un**ipolar
* **Bi**polar
* Gate **Hi**gh - starts low, goes high at the end of the Attack phase
* Gate **Lo**w - starts high, goes low at the end of the Attack phase

# Credits
The original applet and backend _tideslite_ algorithm were written by **qiemem** (Bryan Head). In Phazerville from v1.6
