---
title: Saving State
nav_order: 5
---

# Saving State

Your App / applet state will not be remembered between power cycles unless you:

* (A) Manually save to EEPROM _(Long-press RIGHT encoder to escape to main menu, long-press RIGHT again to save)_
* (B) Store the current state of Hemisphere to a [preset](Hemisphere-Presets)
* (C) Turn on [Auto Save](Hemisphere-Presets#auto-save)

To Save/Load presets or toggle Auto Saving in Hemisphere, long-press the DOWN button to open the config menu, and (if necessary) rotate the LEFT encoder to paginate to the floating preset menu.

When storing a Preset, it immediately triggers an EEPROM Save (with a potential 2ms interruption, fyi) so there is no need to also long-press-save on the main menu.

## EEPROM Save

All of the Apps included in your firmware build have Config Settings that can change at runtime. You can make them persist after a power cycle with the built-in Ornament and Crime storage system:
* Return to the main menu by long-pressing the right encoder.
* Then, long-press the right encoder again.

This will save the state of all applications in the module, as well as the active App and various Global Settings (user scales, waveforms, sequence patterns, turing machines, etc).
