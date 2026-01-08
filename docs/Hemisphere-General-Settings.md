---
layout: default
parent: Config
nav_order: 2
---
# General Settings

The first full-screen page in the [config menu](Hemisphere-Config) (after the floating [presets menu](Hemisphere-Presets)) is for General Settings

![Screenshot 2024-06-13 14-11-32](https://github.com/djphazer/O_C-Phazerville/assets/109086194/34eed3aa-3307-4734-90d4-a6e65442d4af)
(TODO: updated screenshot)

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

### Cursor Wrap
As of v1.7, the Legacy cursor mode (press encoder to step to next parameter, rotate to edit) has been removed, and been replaced by a **modal** cursor:
* rotate encoder to move cursor
* push to toggle editing mode
* rotate to edit parameter
* push again to untoggle editing mode

Setting the cursor configuration to **"modal + wrap"** will allow infinite looping scrolling: the cursor will wrap from the last parameter to the first, and vice versa.

Setting the cursor to simply **"modal"** will prevent looped scrolling, terminating at the beginning and end of the parameter list.

_Note: Some applets (eg. Button2) override the default cursor behaviour, usually to enable different functionality when scrolling CW versus CCW._

***

### MIDI-PC Channel
Preset changes can be triggered with MIDI Program Change messages. This setting allows you to filter this to a specific MIDI Channel, or "Omni" for all channels, or "Off" to disable.

### Preset Jump Trigger
_New in v1.12!_
Hiding next to the MIDI PC setting is a Trigger Input mapping that can be used to load the next Preset in sequence. Just like any other [Input Mapping](Hemisphere-Input-Mapping), it can be assigned to any of the physical input jacks, applet outputs A, B, C, etc., or MIDI Maps. It is triggered when the selected input goes high (above a certain threshold, typically 1.5V). If the Clock is running, the preset load action synchronizes to the next Beat.

***

### Auto MIDI Output
(Experimental) When enabled, MIDI messages are sent automatically based on applet outputs. By default, the Left Hemisphere outputs on Channel 1, and the Right Hemisphere on Channel 2 (configurable with the [MIDI Out](MIDI-Out) applet). Outputs A/C are interpreted as Note values, and B/D as gates for NoteOn/NoteOff.

