---
layout: default
---
# Bug Crack

![BugCrack screenshot](images/BugCrack.png)

**Bug Crack** is a hi-fi replacement for [BootsNCat](BootsNCat). It has a nice and boomy kick, as well as a versatile snare/percussion channel. The snare is inspired by the FM drum voice in [Peaks](https://mutable-instruments.net/modules/peaks/), by Mutable Instruments, and can blend between a sine wave oscillator and noise source.

[Hear it in action](https://youtu.be/ToZaxJ_7NpM)

### I/O

|        |                1/3                |                2/4                 |
| ------ | :-------------------------------: | :--------------------------------: |
| TRIG   |         Kick drum trigger         |         Snare drum trigger         |
| CV INs | Kick drum modulation (assignable) | Snare drum modulation (assignable) |
| OUTs   |             Kick drum             | Snare drum OR Mixed output         |


### UI Parameters
* Kick tone (frequency)
* Kick decay
* Kick punch
* Kick drop
* CV1 (Kick) assignment (attenuation, tone, decay, FM, drop)
* Snare tone (frequency)
* Snare decay
* Snare snap
* Snare blend
* CV2 (Snare) assignment (attenuation, tone, decay, FM, blend)
* MixOut - kick+snare mix on output 2/4

**Frequency (ton) - Kick and Snare**
The base frequency of the drum. Kick has a range of 30-60Hz, snare has a range of 100-700Hz.

**Decay (dec) - Kick and Snare**
The time in takes for the sounds to fade out. On the snare, this parameter also affects the Snap time.

**Punch (FM) - Kick**
Punch causes a quick pitch drop at the beginning of the sound that's common in kick drums. Punch controls how high up the pitch drop starts from.

**Drop (dro) - Kick**
Drop controls the time it takes to get from the Punch value to the Frequency value.

**Snap (FM) - Snare**
Snap is equivalent to the Punch parameter. It causes a pitch drop at the beginning of the sound, at the higher frequencies of the snare range it can create a nice snapping sound.

**Blend (bln) - Snare**
Blend between the oscillator and noise.

**CV Assignment**
In addition to all of the parameters above, the CV inputs can also be assigned to attenuation (atn). A positive voltage increases the attenuation, meaning it gets more quiet. This is opposite to what a "volume" parameter would do, but is required due to constraints of the o_C.

### Credits
Written by benirose & herrkami, with mods by djphazer.
Adapted from [BugCrack](https://github.com/benirose/O_C-BenisphereSuite/wiki/Bug-Crack)
