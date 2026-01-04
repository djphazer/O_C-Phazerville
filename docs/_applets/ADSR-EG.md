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

_Caveat:_ ADSR envelopes require a proper Gate signal to activate and hold high the sustain stage. As such, internal clock triggers and trigger input multiplication/division will not affect it. However, you can potentially assign the output signal from another applet. If you need an envelope that responds to simple rising edge triggers - either physical or virtual - try [AD-EG](AD-EG) instead.

The CV inputs modulate the release stage over a range of about -3 volts to about 3 volts. There is a small center detent in the middle of the range, at which point no modification will be made.

### UI Parameters
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
