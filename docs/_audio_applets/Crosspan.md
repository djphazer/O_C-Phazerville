---
layout: default
---
# Crosspan (stereo)

![Crosspan screenshot](images/Crosspan.png)

A simple crosspanner.
Crossfades left input to right input for the left output and right input to left input for the right output.
This results in the left input panning from left to right while the right input pans from right to left.

### Parameters
* Crosspan: Represented as crossfade bar. The line on the bar indicates where the left channel will end up in the output, while the right channel will be the reverse. CVable.
* Crossfade curve: The equation to use for mixing the left and right channels.
    * Equal pow: Preserves the total power. Use when the left and right channels are unrelated to each other.
    * Equal amp: Preserves the total amplitude. Use when the left and right channels are likely to correlate, such as when mixing two oscillators that are tuned in a perfect harmonic ratio.

### Credits
Authored by qiemem (Bryan Head).
