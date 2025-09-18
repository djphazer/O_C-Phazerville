---
layout: default
---
# Delay (mono/stereo)

![Delay screenshot](images/Delay.png)

A very flexible delay effect with up to 8 taps and almost 12 seconds of delay time.
Delay time can be set directly in milliseconds, by a clock, or in Hz for karplus-strong/comb filtering like usage.
Time modulation can be configured to either result in pitch shifts or no pitch shifts.
It can be used as a mono or stereo applet.
In stereo mode, supports either normal feedback or ping-pong feedback.

### Parameters
* Delay Time: Controls the delay time of the taps. Changing the units of the delay time changes how delay time is controlled:
    * ms: Set delay time directly in ms. CV will control delay time linearly.
    * clocked: Set delay time as a multiplication or division of a clock source. If the internal clock is selected, will use 16th notes as base clock rate. CV will control clock ratio, with a semitone of CV incrementing the ratio by 1.
    * Hz: Set delay time in Hz (ie delay time in seconds will be 1/Hz). This is very useful for creating Karplus-String or comb filtering effects, or other effects that use very short delay times, such as flanging. CV will use v/oct.
* FB: Feedback amount. When in stereo, negative feedback will ping-pong (ie the left channel output will be fed back to the right channel, and vice versa). CVable.
* Wet: Mix of dry and wet signal. CVable.
* Taps: Number of delay taps, from 1 through 8. The taps will evenly divide the total delay time.
* Modulation method: Controls how the delay responds to modulation.
    * Crossfade: When delay time changes, taps will rapidly crossfade to the new delay time, resulting in no pitch shift.
    * Stretch: When delay time changes, taps travel smoothly across the audio buffer to the new delay time, resulting in pitch shifts.

### Suggested patches

* Beat-repeaty clocked delay: TODO
* Karplus-Strong: TODO
* Flanger/Chorus: TODO

### Credits

Authored by qiemem (Bryan Head).
