---
layout: default
---
# Dr. LoFi

![Screenshot 2024-06-13 14-42-25](https://github.com/djphazer/O_C-Phazerville/assets/109086194/bb9699a5-1411-4107-9e2c-4c6e0a2df9c8)

(Formerly called LoFi Echo, LoFi Tape)

**Dr. LoFi** is a friend of Dr. Crusher, who built a digital delay line using LoFi Tape.
It still provides sample rate and bit depth reduction for CV or audio signals, along with a feedback loop with configurable delay and depth.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Pause/Freeze | Feedback=100 |
| CV INs | Signal Input  | Rate mod |
| OUTs   | Signal Output | Reverse buffer output |

### UI Parameters
* Delay Time - set to 0 for passthru with digital distortion
* Feedback Amount (%)
* Rate Reduction - 1 is fastest (16.6khz); higher is slower / lower sampling rate
  - some smoothing is applied as rate reduction increases, like a low-pass filter
* Bit Depth Reduction - 0 is full-range (sampled at 12-bit); higher truncates more bits

### Notes
There is one global 8-bit PCM buffer shared by both Hemispheres, which has strange and experimental implications when running the applet on both sides simultaneously.

Out A/C is the main output; Out B/D is the buffer played in reverse (weird but cool)

Note that some "crushing" will always be applied, no matter what. This is a digital module processing signals at 16.6khz, you're going to get aliasing and noize!

If you're not getting any signal at lower bit resolutions, increase the input amplitude.

### Credits
Adapted from [LoFi Tape](https://github.com/Chysn/O_C-HemisphereSuite/wiki/LoFi-Tape) by Chysn. Reworked by djphazer, based on a digital delay line idea by armandvedel.
