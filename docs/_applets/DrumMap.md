---
layout: default
---
# DrumMap

![DrumMap Screenshot](images/DrumMap.png)

**DrumMap** is a 2 channel port of the popular rhythm sequencer [Grids](https://mutable-instruments.net/modules/grids/), from Mutable Instruments. You are able to select any of the 3 parts to assign to each channel. In addition, the 2nd channel can be used as the accent channel of the 1st channel. Traverse a map of rhythms by X and Y coordinates!

DrumMap expects a 32nd note clock (8ppqn) to operate normally, but can use other clock speeds to advance the sequencer at slower or faster rates.

If you are unfamiliar with the premise of Grids, [watch this video for an excellent explanation](https://www.youtube.com/watch?v=l5yN0N6aTws).

Thanks to Émilie Gillet for making the code for Mutable Instruments modules open source!

[See it in action!](https://youtu.be/yLr3vQJm3wM)

### I/O

|        |               1/3                |                  2/4                   |
| ------ | :------------------------------: | :------------------------------------: |
| TRIG   |              Clock               |                 Reset                  |
| CV INs | Assignable (Fill A, X position,) | Assignable (Fill B, Y Position, Chaos) |
| OUTs   |      Ch 1 Trigger sequence       |         Ch 2 Trigger sequence          |

Note that alternate rhythm patterns are available with the build flag "DRUMMAP_GRIDS2"

### UI Parameters
* Ch 1 Drum (Kick, snare, high hat)
* Ch 2 Drum (Kick, snare, high hat, accent)
* Ch 1 Fill
* Ch 2 Fill
* X position
* Y postion
* Chaos
* CV assignment (X/Y, Fill A/B, Fill A/Chaos)

In order to make parameter changes more performable using the encoder, knob acceleration has been implemented for the "Fill", "X", "Y" and "Chaos" parameters. This means if you turn the knob quickly it will traverse the whole range, but if you turn it slowly it will increment by a single value.

Additionally, DrumMap will auto-reset after ~2 seconds of no clock signal.

### Assignable CV Inputs
You can change the CV Input assignment from “Part A Fill and Part B Fill”, “X and Y coordinates” and “Part A Fill and Chaos” (the last is useful when you’re using accent on channel 2, since it has no fill). CV is added to the parameter value set via the encoder, and voltages from -3 to +5 are supported. This means when the parameter is set in the middle, you should be able to use a +/-5 volt CV signal to reach both ends of the parameter.

### Credits
Adapted from [DrumMap](https://github.com/benirose/O_C-BenisphereSuite/wiki/DrumMap) by benirose. Alternate "Grids2" patterns collected by [KittenVillage](https://github.com/KittenVillage).
