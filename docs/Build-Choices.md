---
title: Build Choices
nav_order: 3
---

# Build Choices

_Pay attention!_ There are several different hardware variants, each requiring a different firmware binary, indicated in the filename:
* **T32** - for standard O_C hardware with Teensy 3.2
* **T32+VOR** - for **Plum Audio** 1uO_C / OCP / OCP X hardware with Variable Output Range (VOR)
* **T40** - for standard O_C hardware with Teensy 4.0 (or 4.1, uncommon)
* **NLM+cardOC** - for the Northern Light Modular Easel card module with Teensy 4.0
* **T41** - for all new O_CT4.1 aka **ORN8** hardware with 8-channel I/O

[Release builds](https://github.com/djphazer/O_C-Phazerville/releases) typically include **Hemispheres** + all the Applets, and a variety of other Apps (check the release notes for details)

Roll your own! Generate a [custom O_C firmware](https://github.com/djphazer/O_C-BenisphereSuite/discussions/38) for Teensy 3.2 with your choice of Apps! (Read and leave a comment on the Discussion, and a robot will build it for you.)

Teensy 4.0 releases for O_C hardware include almost all of the available Apps.

Teensy 4.1 releases for ORN8 hardware currently exclude most legacy Apps to prioritize [**Quadrants**](Quadrants) and the new Audio DSP subsystem. All legacy Apps can still run, but you'll have to modify the build config flags and compile it yourself. (Check the README for tips)

### VOR

Files with "**+VOR**" are only for **Plum Audio / 4ROBOTS** hardware variants equipped with a _Variable Output Range_ circuit - O&C Plus, 1uO_c, & OCP X. These modules can also use the standard T32 firmware if VOR is disabled via the jumper (set to 'NV').

Other standard 8HP uO_C, After Later uO_C 1U, or full-size 14HP O_C modules should be installed _without_ VOR - your outputs will behave strangely otherwise!

### Flipped Operation

As of v1.8.3, flipping the screen/controls is a calibration option. No need for a special "_flipped" build!

In [Setup/About](Setup-About), you'll see two arrows in the title bar. Up/Down indicates screen flip; Left/Right indicates controls & IO reversal. If you need to change either, dual-press the UP+DOWN buttons to cycle through. Return to main menu and it will save; power cycle to take effect.
