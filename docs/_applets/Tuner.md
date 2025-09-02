---
layout: default
---
# Tuner

![Screenshot 2024-06-13 15-48-34](https://github.com/djphazer/O_C-Phazerville/assets/109086194/663af09c-d67f-461d-a056-7eba66c3a6c0)

**Tuner** is a chromatic tuner with adjustable A4 setting.

**_Important:_** Tuner can only measure frequencies on the physical TR4 input on Teensy 3.2-based hardware, and TR1 on Teensy 4.0-based hardware. It will remind you which side it needs to be on. ;)
<br>_(pictured above running in a FLIP_180 build, upside-down orientation)_

As of PSv1.8, Tuner support has been added for Teensy 4.x. On Teensy 4.0, it uses TR1 rather than TR4.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Signal input   |   N/A  |
| CV INs | N/A    | N/A    |
| OUTs   |  N/A   |  N/A   |



## UI Parameters
* Set A4 frequency (press to reset to A=440hz)


The A4 frequency has a range of 400-500Hz. The closest note is displayed under the frequency. Under that, it shows how far off from that note in cents the input is. When the note is in tune, it will become highlighted.

If Tuner seems to stop responding, push the encoder button to reset it.

Yeah, that's about it. It's a tuner.

Tuner is available from Hemisphere Suite v1.4.
