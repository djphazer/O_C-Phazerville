---
layout: default
parent: Hemisphere Config
nav_order: 1
---
# Hemisphere Presets

## Floating menu: Presets

The first three options in the [config menu](Hemisphere-Config) are **Load**, **Save**, and **(auto)**

Rotate either encoder to select, push to enter. Press UP or DOWN buttons to cancel. Both applets (plus Clock Setup) with their state are recalled, along with the rest of the Config options on this page.

Presets are saved with the name of the applets in each hemisphere. Note that [default builds](https://github.com/djphazer/O_C-Phazerville/releases) have 4 preset slots, but you can enable 8 or 16 with a [custom build](https://github.com/djphazer/O_C-Phazerville/discussions/38)

![Screenshot 2024-06-13 14-10-47](https://github.com/djphazer/O_C-Phazerville/assets/109086194/c1413c95-627c-40d2-88ff-79b00829f31b)
![Screenshot 2024-06-13 14-13-35](https://github.com/djphazer/O_C-Phazerville/assets/109086194/9343eb3d-77d8-41fa-ba64-b616aa35d544)

***

### Load Presets via MIDI PC

Sending MIDI Program Change messages to Channel 1 will load the corresponding preset
* Value 0: Preset A
* Value 1: Preset B
* Value 2: Preset C
* Value 3: Preset D
* ...Etcetera (up to however many presets are enabled in your build: 4, 8, or 16)

***

### Auto-save

(New in v1.6.999)

If enabled, settings are automatically stored in the last loaded Preset when the screensaver is invoked, or when loading the main App menu via Right Encoder Long-press. (You can set the screensaver timeout as low as 1 minute in Calibration)

![Screenshot 2024-06-13 14-11-00](https://github.com/djphazer/O_C-Phazerville/assets/109086194/a80339d8-373c-41e2-bcd6-7f4782c05262)

When storing a Preset, settings are immediately written to EEPROM - no need to manually do an [EEPROM Save](Saving-State), unless you need to save changes to global patterns, etc. Combined with Auto-save, this can result in frequent EEPROM writes... (only 100,000 write cycles are guaranteed, so use at your own risk!)
