---
layout: default
---
# Boots 'n Cats

![Screenshot 2024-06-13 14-21-41](https://github.com/djphazer/O_C-Phazerville/assets/109086194/a73e26c6-7d16-4031-8db8-c55c17d4f201)

*Boots 'n Cats* (BootsNCat) is a simple bass/snare drum synthesizer with stereo audio outputs.

### I/O

|        |          1/3          |          2/4           |
| ------ | :-------------------: | :--------------------: |
| TRIG   |   Bass drum trigger   |   Snare drum trigger   |
| CV INs | Bass drum attenuation | Snare drum attenuation |
| OUTs   |  Bass drum (or mix)   |  Snare drum (or mix)   |

Note that triggering the envelope in reverse does not change the CV modulation controls, meaning that CV 1 controls the effective "attack" segment regardless of direction.

### UI Parameters
* Bass drum tone
* Base drum decay
* Snare drum tone
* Snare drum decay
* Blend

**Tone**
* Each drum has a Tone control that sets the basic sound of the drum. For the bass drum, Tone controls the frequency, with higher values indicating a higher frequency. For the snare drum, the Tone control is more like a low-pass filter, with high values indicating a higher cutoff frequency.

**Decay**
* Each drum has a Decay control that sets the decay of the drum between about 1/10 of a second and 1 second, with higher values indicating longer decay.

**Blend**
* At a Blend level of 0, the bass drum and snare drum are totally isolated on Outputs A/C and B/D, respectively. As the Blend is increased, some of the signal from the opposite channel will be fed into each channel. A Blend level of 63 results in a 50/50 mix on both outputs.

### Credits
Adapted from [BootsNCat](https://github.com/Chysn/O_C-HemisphereSuite/wiki/BootsNCat) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
