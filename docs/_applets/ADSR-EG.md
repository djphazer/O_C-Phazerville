---
layout: default
---
# ADSR EG

![ADSR EG screenshot](images/ADSR-EG.png)

*ADSR EG* is a linear envelope generator with two independent channels.

### I/O

|        |                1/3                 |                 2/4                 |
| ------ | :--------------------------------: | :---------------------------------: |
| TRIG   |           Channel A Gate           |           Channel B Gate            |
| CV INs | Env. A Release | Env. B Release |
| OUTs   |             Envelope A             |             Envelope B              |

Note that CV inputs modulate the release stage over a range of about -2.5 volts to about 2.5 volts. There is a small center detent in the middle of the range, at which point no modification will be made.

### UI Parameters
Push encoder to advance cursor; turn to adjust.
* Channel A: Attack duration
* Channel A: Decay duration
* Channel A: Sustain level
* Channel A: Release duration
* Channel B: Attack duration
* Channel B: Decay duration
* Channel B: Sustain level
* Channel B: Release duration

### Credits
Adapted from the original [ADSR EG](https://github.com/Chysn/O_C-HemisphereSuite/wiki/ADSR-EG) by Chysn, with modifications by ghostils.

Future improvements may be derived from the subsequent [ADSREG_PLUS](https://github.com/ghostils/O_C-HemisphereSuite/blob/production/software/o_c_REV/HEM_ADSREG_PLUS.ino) applet by ghostils.
