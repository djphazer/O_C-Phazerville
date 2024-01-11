[![PlatformIO CI](https://github.com/djphazer/O_C-Phazerville/actions/workflows/firmware.yml/badge.svg)](https://github.com/djphazer/O_C-Phazerville/actions/workflows/firmware.yml)

Phazerville Suite with Relabi - an active o_C firmware fork
Phazerville Suite with Relabi - an active o_C firmware fork
===
[![SynthDad's video overview](http://img.youtube.com/vi/XRGlAmz3AKM/0.jpg)](http://www.youtube.com/watch?v=XRGlAmz3AKM "Phazerville; newest firmware for Ornament and Crime. Tutorial and patch ideas")
[![SynthDad's v1.7 update](http://img.youtube.com/vi/bziSog_xscA/0.jpg)](http://www.youtube.com/watch?v=bziSog_xscA "Ornament and Crime Phazerville 1.7: What's new in this big release!")

Watch SynthDad's **video overview** (above) or check the [**Wiki**](https://github.com/djphazer/O_C-BenisphereSuite/wiki) for more info. [Download it here](https://github.com/djphazer/O_C-BenisphereSuite/releases).

## An active fork expanding upon Hemisphere Suite.

This is a fork of djphazer's Phazerville Suite which is a fork of Benisphere which is a fork of Hemisphere! The code is firmware for the Ornament & Crime, a Eurorack synth module that can act as many different modules as selected by the user. This firmware ads the Relabi app that generates chaotic but deterministic control voltage. In addition, it adds a function to the HSVectorOscillator.h file and provides a controller file that allows two Relabi apps to be linked, providing four related chaotic LFOs.

Read more about the suite [here](https://github.com/djphazer/O_C-Phazerville). Please, go there and read about all the people who have contributed code.

## What is Relabi.

Relabi is an alternative method of defining musical time. While rhythm is concerned with recurring pulses, relabi always slips the pulse. You can read all about the concept in [John Berndt's essay]((https://johnberndt.org/relabi/).




### How To Get It

Check the [Releases](https://github.com/djphazer/O_C-BenisphereSuite/releases) section for a .hex file (to be used with the Teensy loader app), or clone the repository and build it yourself! I think the beauty of this module is the fact that it's relatively easy to modify and build the source code to reprogram it. You are free to customize the firmware, similar to how you've no doubt already selected a custom set of physical modules.

### How To Change It

This firmware fork is built using Platform IO, a Python-based build toolchain, available as either a [standalone CLI](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or a [full-featured IDE](https://platformio.org/install/ide), as well as a plugin for VSCode and other existing IDEs.

The project lives within the `software/o_c_REV` directory. From there, you can Build the desired configuration and Upload via USB to your module:
=======
Using [**Benisphere**](https://github.com/benirose/O_C-BenisphereSuite) as a starting point, this branch takes the **Hemisphere** ecosystem in new directions, with several new applets and enhancements to existing ones. An effort has been made to collect all the bleeding-edge features from other developers, with the goal of cramming as much functionality and flexibility into the nifty dual-applet design as possible!

I've also included **all of the stock O&C firmware apps** plus a few others, _but they don't all fit in one .hex_. As a courtesy, I provide **pre-built .hex files** with a selection of Apps in my [**Releases**](https://github.com/djphazer/O_C-Phazerville/releases). You can also tell a robot to make a [**Custom Build**](https://github.com/djphazer/O_C-Phazerville/discussions/38) for you...

...or clone the repo, customize the `platformio.ini` file, and build it yourself! ;-)
I think the beauty of this module is the fact that it's relatively easy to modify and build the source code to reprogram it. You are free to customize the firmware, similar to how you've no doubt already selected a custom set of physical modules.

## How To Hack It

This firmware fork is built using Platform IO, a Python-based build toolchain, available as either a [standalone CLI](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or a [full-featured IDE](https://platformio.org/install/ide), as well as a plugin for VSCode and other existing IDEs. Follow one of those links to get that set up first.

The PlatformIO project for the source code lives within the `software/` directory. From there, you can Build the desired configuration and Upload via USB to your module. In the terminal, I type:

```
pio run -e pewpewpew -t upload
```
Or use `T40` for Teensy 4.0. Have a look inside `platformio.ini` for alternative build environment configurations and app flags.

_**Pro-tip**_: If you decide to fork the project, and enable GitHub Actions on your own repo, GitHub will build the files for you... ;)


=======
### Arduino IDE
Instead of Platform IO, you can use the latest version of the Arduino IDE + Teensyduino extension. The newer 2.x series should work, no need to install an old version.

Simply open the `software/src/src.ino` file.

Customize Apps and other flags inside `software/src/OC_options.h`. You can also disable individual applets in `software/src/hemisphere_config.h`.

