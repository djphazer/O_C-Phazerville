---
layout: default
---
# Dynamics (mono/stereo)

TODO: screenshot

A single chain of standard dynamics processors with minimal parameters and sensible defaults. The signal flow is:
```
input -> Gate -> Compressor -> Gain -> Limiter -> output
```
_You can have up to +30dB of gain - as a little treat!_

### Description
First, the Gate will mute incoming signals until they exceed the Gate Threshold level. It opens and closes pretty fast.

Next, the Compressor kicks in when signals exceed its Threshold, and reduces level with somewhat slower attack and release, and a soft knee of 6dB.

Then you get to boost everything with the make-up gain, if you want; "auto" makes some kind of guess for you.

Finally, the Limiter tries its hardest to clamp the post-Gain signal with the shortest possible attack/release. Transients that sneak through the Limiter may be hard-clipped... ;)

You can bypass any of the stages by setting parameters to their min or max, depending.

### Parameters
* Gate Threshold
* Compressor Threshold
* Make-up Gain
* Limiter Threshold

### Credits
Authored by djphazer. Using a "stolen" Teensy library, Dynamics Processor (Gate, Compressor & Limiter) - Copyright (c) 2017, Marc Paquette.
