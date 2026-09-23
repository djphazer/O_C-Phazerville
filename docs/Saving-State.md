---
title: Saving State
nav_order: 5
---

# State Data Storage

There are generally three types of data stored in the module:
1. Calibration Data
2. Global Settings
3. [Presets](Hemisphere-Presets)

Calibration Data is kept in a reserved part of EEPROM where it's rarely modified.

Global Settings include things like the default App to run on startup, custom user scales, and custom vector waveforms. It also includes app data for all legacy Apps that don't otherwise use Preset files.

Presets include all Applet settings in Hemisphere/Quadrants, I/O mappings, Q-engines, etc. - everything you really care about.

On newer Teensy 4.x modules, Global Settings and Presets are stored in portable binary files, either in a partition of flash called LittleFS, or on a microSD card.

For older Teensy 3.2 modules, all of it is crammed into the 2KB EEPROM space in flash.

## How To Save

To manually save Global Settings to EEPROM: _Long-press RIGHT encoder to escape to main menu, long-press RIGHT again to save_

This does not necessarily store the current [Preset](Hemisphere-Presets), unless you've enabled [Auto-Save](Hemisphere-Presets#auto-save).

To Save/Load presets or toggle Auto-Saving in Hemisphere/Quadrants, long-press the DOWN (or B) button to open the config menu. If necessary, scroll all the way to the LEFT to the floating preset menu. In Quadrants, the A + X key combo is a great shortcut for Load, with Save accessible by rotating L-Enc in the menu.

## Teensy 4.0 & 4.1

Several Apps have been updated to store settings in binary files on T4 hardware instead of using emulated EEPROM. This includes: **Hemispheres**/**Quadrants**, **Scenery**, **Calibr8or**, **Captain MIDI**.

Settings are typically saved when you store a Preset, which happens automatically in some cases. Global settings like custom Scales and Vector Waveforms are stored in a separate config file, only when you invoke it with a R-Enc-Long-Press on the Main Menu.

Newer Teensy's have enough space in flash storage to accomodate a filesystem (aka LittleFS or LFS). Teensy 4.1 also has a microSD card slot; you could theoretically add one to a Teensy 4.0 as well. This allows significantly more storage, and will also makes it easier to backup/restore/transfer settings by simply copying files. (Files on the internal LFS storage will become more accessible when USB MTP Disk support is stable.)

The binary file format used by Phazerville Apps (PhzConfig) has a small header with a signature & checksum, followed by a simple KEY-VALUE hashmap using 16-bit KEYs and 64-bit VALUEs. External editors are a possibility.
