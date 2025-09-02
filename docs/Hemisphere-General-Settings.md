---
layout: default
parent: Hemisphere Config
nav_order: 2
---
# General Settings

## Page 1: General Settings

The first full-screen page in the [config menu](Hemisphere-Config) (after the floating [presets menu](Hemisphere-Presets)) is for General Settings

![Screenshot 2024-06-13 14-11-32](https://github.com/djphazer/O_C-Phazerville/assets/109086194/34eed3aa-3307-4734-90d4-a6e65442d4af)

### Trigger Length
This sets the pulse width (in milliseconds, approximate) for applets that generate simple triggers, such as **EuclidX** or **TrigSeq**. The old default was close to 3ms, but some modules may require longer pulses.

***

### Screensaver
Options are:
* [blank]
* Meters
* Scope
* Zips (or "Stars" in Teensy 4.x builds)

***

### Cursor
As of v1.7, the Legacy cursor mode (press encoder to step to next parameter, rotate to edit) has been removed, and been replaced by a **modal** cursor: **rotate encoder to move cursor, push to toggle editing, rotate to edit parameter, push again to untoggle editing**

_Note: Some applets (eg. Button2) override the default cursor behaviour, usually to enable different functionality when scrolling CW versus CCW._

Setting the cursor configuration to **"modal + wrap"** will allow infinite looping scrolling: the cursor will wrap from the last parameter to the first, and vice versa.

Setting the cursor to simply **"modal"** will prevent looped scrolling, terminating at the beginning and end of the parameter list.

***

### Auto MIDI Output
(Experimental) When enabled, MIDI messages are sent automatically based on applet outputs. By default, the Left Hemisphere outputs on Channel 1, and the Right Hemisphere on Channel 2 (configurable with the [MIDI Out](MIDI-Out) applet). Outputs A/C are interpreted as Note values, and B/D as gates for NoteOn/NoteOff.
