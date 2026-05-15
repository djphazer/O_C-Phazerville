---
title: Home
nav_order: 1
---

# Phazerville Suite

## Key Features:
* Support for next-gen [Teensy 4.1-based "O.R.N.8" hardware](https://github.com/PaulStoffregen/O_C_T41) using [**Quadrants**](Quadrants) App
    * includes Audio Applet subsystem for DSP effects, synths, and samplers.
* Support for Teensy 4.0 on original O_C hardware
* Multiple Apps can fit into the original Teensy 3.2 ([choose your own App selection](https://github.com/djphazer/O_C-Phazerville/discussions/38))
* Many new and improved [Hemisphere Applets](App-and-Applet-Index#hemisphere-applets)
* New full screen apps: [Scenery](Scenery) and [Calibr8or](Calibr8or)
* New UI features, codebase optimizations, and QoL improvements
    * [Presets](Hemisphere-Presets) — with auto-save and loading via MIDI Program Changes
    * [Trigger and CV Input re-mapping](Hemisphere-Input-Mapping) (including internal routing)
    * [Internal Clock improvements](Clock-Setup): sync to external CV or MIDI clock, swing, per-channel multiplication / division
    * [Multiple Quantizer Engines](Hemisphere-Quantizer-Setup) (with pop-up editor and performance transposition)
    * [Expanded MIDI Input](MIDI-Input) options for MIDI-to-CV, including polyphony

![Various O_C modules running the firmware](images/three_criminals.jpg)

#### _Note from the Mayor:_
> Thanks for checking out my firmware! I've made efforts to hoard all the notable [Apps and Applets](App-and-Applet-Index) in one repo! All the full-screen Apps from [Hemisphere Suite](https://github.com/Chysn/O_C-HemisphereSuite/wiki) are here, plus all of the [stock O&C firmware](https://ornament-and-cri.me/user-manual-v1_3/) Apps, and a few new ones, albeit in limited combinations depending on which hardware variant you have... see the [Build Choices](Build-Choices) page for details.
> 
> Want to roll your own mix of Apps? You can request a **Custom Build** for Teensy 3.2 with a simple bot command on [this discussion post](https://github.com/djphazer/O_C-Phazerville/discussions/38).
> 
> Read a bit about my [Development Philosophy](Development-Philosophy) to understand my motives behind this project.
> 
> &mdash; _[DJ Phazer](https://ko-fi.com/djphazer)_

***

## Quick Links

You can find documentation for (almost) every single O_C function on this page, or on the sidebar. Use the Search bar at the top for quick lookups.

↯ [Download a **Release**](https://github.com/djphazer/O_C-Phazerville/releases)

ᛃ [Request a **Custom Build**](https://github.com/djphazer/O_C-Phazerville/discussions/38).

⧉ **[Index of all Apps and Applets](App-and-Applet-Index)**

#### Installation
* [Firmware update procedure](Installation)
* [Calibration info](Setup-About#calibration-routine)

***

## New to Ornament and Crime?

**_Ornament and Crime_** is a Polymorphic CV Generator — a swiss army knife of modular control voltage. It can perform as an _excellent_ sequencer, envelope generator, quantizer, MIDI-to-CV and/or CV-to-MIDI interface, and [much more](App-and-Applet-Index#apps-and-applets-by-function) via its many Apps and Applets.

**_o_C_** is a **collaborative open-source project** originated by [Patrick Dowling, mxmxmx, and Tim Churches](https://ornament-and-cri.me/), and extended by many contributors (special thanks to [Chysn](https://github.com/Chysn/O_C-HemisphereSuite) for the original Hemisphere Suite). The **_Phazerville_** firmware optimizes the code base so that more apps can fit on the original hardware (which come in many flavours, for Eurorack and otherwise — _See [Build Choices](Build-Choices)_), and paves the way for the next generation of hardware on the Teensy 4.1 platform.

Each of the [full screen apps](App-and-Applet-Index#full-screen-apps) takes advantage of all inputs and outputs in their own way, which is usually configurable. _**Hemisphere**_ splits the screen into two halves: each side available to load any one of [a long list of applets](App-and-Applet-Index). On o_C hardware with inputs and outputs arranged in 3 rows of 4 columns (i.e. most 8hp units), the I/O corresponding to an applet should be in line with that half of the display. If you're coming from any of the other Hemisphere forks, note that many of the applets have been upgraded for additional flexibility and functionality, and several are brand new.

***

_Note: Some apps, Hemisphere applets, and parameter editing contexts use special behaviour for the various encoders, buttons, and encoder buttons, which **should** be noted somewhere in these docs. If your encoders don't rotate the way you expect, you can flip the behaviour of one, the other, or both as part of the [Setup / About](Setup-About) calibration routine. See [Hemisphere Gestures](Hemisphere-Gestures) for all button combos within Hemisphere._
