---
layout: default
---
# DuoTET

![DuoTET screenshot](images/DuoTET.png)

_Copied from the source code comments:_

**DuoTET** is a dual/duophonic microtonal quantizer. It is
specifically intended to facilitate fluid exploration
of xenharmonic terrain for composers who may be acclimated
to western notions of pitch and harmony, while remaining
flexible enough to satisfy users who are comfortable with
a wider range of tonalities.

DuoTET generates a scale of N notes by stacking alternating
intervals on top of one another. An offset into the list of
generated notes can be chosen, at which point that note becomes
the root of the scale, the note set is normalized with respect
to that note, and the resulting pitch collection is conditioned
into a collection of pitch classes, which can be thought of as
either a chord or a scale depending on which concept is more
convenient to the composer.

The "Duo" portion of DuoTET refers to the function of its second
quantizer (B). This quantizer can be configured to operate
independent of the first quantizer, but may also be configured to
add the input of the first quantizer to its own, along with a
configurable offset, in order to create duophonic harmonies.

DuoTET is currently unfinished. The trigger inputs are unused and
should be configurable as sample-and-holds for the two quantizers.
Additional work includes providing parameters for wrapping the
lower and upper bounds of the two quantizers in order to provide
control over the voicing and register of the two. Transposition of
the first quantizer should be considered. It would also be
ideal to be able to modulate the parameters of the two quantizers
using other applets.

### I/O

|        |                             1/3                             |                  2/4                  |
| ------ | :---------------------------------------------------------: | :-----------------------------------: |
| TRIG   | Unused   |  Unused
| CV INs |                          Input Pitch A                          |               Input Pitch B (or offset)               |
| OUTs   |                Quantized Pitch A                |     Quantized Pitch B     |


### UI Parameters
* TET: equal subdivisions of the octave
* iA: first interval
* iB: second interval
* len: number of allowed intervals per octave (eg. 5 = pentatonic, 7 = diatonic)
* off: offset of the allowed intervals
* B+A: Toggle to set Pitch B relative to Pitch A (Y), or as an independent second channel (N)
* B+: Interval offset for Pitch B
* B^: Upper bound for Pitch B (in intervals)
* Bv: Lower bound for Pitch B (in intervals)
* A^: Upper bound for Pitch A (in intervals)
* Av: Lower bound for Pitch A (in intervals)

Note: All parameters with "()" may be additionally mapped to any of the CV inputs or internal output channels. Select a parameter, and press AuxButton to map a CV source.

See https://en.xen.wiki/ for details on intervals for any given TET setting

### T3.2 Disclaimer

This applet was developed on T4.1 hardware, and may exceed CPU and I/O limitations of Teensy 3.2 hardware.

## Credits

Authored by Eris Fairbanks.
