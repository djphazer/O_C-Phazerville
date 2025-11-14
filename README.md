[![PlatformIO CI](https://github.com/djphazer/O_C-Phazerville/actions/workflows/firmware.yml/badge.svg)](https://github.com/djphazer/O_C-Phazerville/actions/workflows/firmware.yml)

Phazerville Suite - an active o_C firmware fork
===
[![Firmware Update v1.10](http://img.youtube.com/vi/UvlA5_C1aig/0.jpg)](http://www.youtube.com/watch?v=UvlA5_C1aig "Phazerville Suite v1.10 - O_C Firmware Update")

<details><summary>More Videos...</summary>

  [![SynthDad's v1.7 update](http://img.youtube.com/vi/bziSog_xscA/0.jpg)](http://www.youtube.com/watch?v=bziSog_xscA "Ornament and Crime Phazerville 1.7: What's new in this big release!")
  [![SynthDad's video overview](http://img.youtube.com/vi/XRGlAmz3AKM/0.jpg)](http://www.youtube.com/watch?v=XRGlAmz3AKM "Phazerville; newest firmware for Ornament and Crime. Tutorial and patch ideas")
  [![Pigeons, Polyrhythms, Music & Math](http://img.youtube.com/vi/J1OH-oomvMA/0.jpg)](http://www.youtube.com/watch?v=J1OH-oomvMA "Pigeons & Polyrhythms / Music & Math")
  [![DualTM & O_C T4.1 Hardware](http://img.youtube.com/vi/51cchuLNIDU/0.jpg)](http://www.youtube.com/watch?v=51cchuLNIDU "Next-gen O_C T4.1 Hardware + DualTM applet")
</details>

Watch some **video overviews** (above) or check the [**project website**](https://firmware.phazerville.com) for more info.

[Download a firmware **Release**](https://github.com/djphazer/O_C-Phazerville/releases) or [Request a **Custom Build**](https://github.com/djphazer/O_C-Phazerville/discussions/38) (for Teensy 3.2).

Grab Paul's [**Screen Capture**](https://github.com/PaulStoffregen/Phazerville-Screen-Capture) program to view the screen on a PC via USB.

## Hardware Info
There are two distinct _microcontrollers_ aka MCU's (and each has variants) and also two distinct hardware _shields_, and there's some overlap.

### Shields:
* **o_C** - based on the original "ornament & crime" hardware design by **mxmxmx**
  - 4ch ADC / 4ch DAC
  - DAC and OLED share a SPI bus
  - 8HP uO_c by jakplugg - https://github.com/jakplugg/uO_c
  - original 14HP panels & gerbers are in the `hardware` directory
* "O_C T4.1" aka **O.R.N.8** - https://github.com/PaulStoffregen/O_C_T41
  - 8ch ADC / 8ch DAC / 2ch Audio In + 2ch Audio Out
  - SPI0 dedicated for DAC
  - SPI1 dedicated for OLED
  - Serial MIDI In + Out / USB Host MIDI
  - designed by Paul, derived from original

### MCUs:
* Teensy 3.2 - compatible with O_C
* Teensy 4.0 - compatible with O_C
* Teensy 4.1 - compatible with O_C or ORN8

Thus, the 4.x series are pin-compatible drop-in replacements for the old existing O_C hardware. They bring increased CPU, RAM, and Flash capacity, while working with the existing limitations of the design (sharing a SPI bus). Although the 4.1 can technically work with O_C (using the T40 firmware), the form factor of the 4.0 is more fitting, especially on the 8HP model.

The T41 firmware builds primarily target the new O.R.N.8 hardware shield, with new features that take advantage of it (lots of Audio DSP stuff), but many CV and MIDI features will still show up in the T40 builds for old hardware. T32 support is deprecated, but will remain available for Custom Builds, receiving occasional Applet updates.

## Stolen Ornaments

Using [**Benisphere**](https://github.com/benirose/O_C-BenisphereSuite) as a starting point, this project takes the **Hemisphere** ecosystem in new directions, with many new applets and enhancements to existing ones. An effort has been made to collect all the bleeding-edge features from other developers, with the goal of cramming as much functionality and flexibility into the nifty dual-applet design as possible!

I've also included **all of the stock O&C firmware apps** plus a few others, _but they don't all fit in one .hex_. As a courtesy, I provide **pre-built .hex files** with a selection of Apps in my [**Releases**](https://github.com/djphazer/O_C-Phazerville/releases). You can also tell a robot to make a [**Custom Build**](https://github.com/djphazer/O_C-Phazerville/discussions/38) for you... (T3.2 only)

...or clone the repo, customize the `platformio.ini` file, and build it yourself! ;-)
I think the beauty of this module is the fact that it's relatively easy to modify and build the source code to reprogram it. You are free to customize the firmware to work in your system, similar to how you've no doubt already selected a custom set of physical modules.

## How To Hack It

This firmware fork is built using Platform IO, a Python-based build toolchain, available as either a [standalone CLI](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or a [full-featured IDE](https://platformio.org/install/ide), as well as a plugin for VSCode and other existing IDEs. Follow one of those links to get that set up first.

The PlatformIO project for the source code lives within the `software/` directory. From there, you can Build the desired configuration and Upload via USB to your module. In the terminal, I type:
```
pio run -e T41 -t upload
```
Or, for older Teensy 3.2 modules:
```
pio run -e T32 -t upload
```
Or use `T40` for Teensy 4.0. Have a look inside `platformio.ini` for alternative build environment configurations and app flags.

_**Pro-tip**_: If you decide to fork the project, and enable GitHub Actions on your own repo, GitHub will build the files for you... ;)

### Arduino IDE
Instead of Platform IO, you can use the latest version of the Arduino IDE + Teensyduino extension. The newer 2.x series should work, no need to install an old version.

Simply open the `software/src/src.ino` file.

Customize Apps and other flags inside `software/src/OC_options.h`. You can also disable individual applets in `software/src/hemisphere_config.h`.

TODO: This is probably broken for T41, due to an external library or two pulled in by `lib_deps` in the PlatformIO config.

## Credits

Many minds before me have made this project possible. Attribution is present in the git commit log and within individual files.

Thanks & Shoutouts:
* **[Paul Stoffregen](https://github.com/PaulStoffregen)** (PJRC) for Teensy 4.x driver code, new hardware designs, and lots of support!
* **[beau-seidon](https://github.com/beau-seidon)** for polyphonic MIDI handling, **ProbMeloD** mask rotation, **WTVCO**, and free-flowing enthusiasm.
* **[qiemem](https://github.com/qiemem)** (Bryan Head) for **Ebb&LFO** and its _tideslite_ backend, the Audio Applet framework, and many other things.
* **[Logarhythm1](https://github.com/Logarhythm1)** for the incredible **TB-3PO** sequencer, as well as **Stairs**.
* **[herrkami](https://github.com/herrkami)** and **Ben Rosenbach** for their work on **BugCrack**.
* **[benirose](https://github.com/benirose)** also gets massive props for **DrumMap**, **Shredder** and the **ProbDiv / ProbMeloD** applets.

And, of course, thank you to **[Chysn](https://github.com/Chysn)** (RIP) for the clever applet framework from which we've all drawn inspiration - what a legend!

This is a fork of [Benisphere Suite](https://github.com/benirose/O_C-BenisphereSuite) which is a fork of [Hemisphere Suite](https://github.com/Chysn/O_C-HemisphereSuite) by Jason Justian (aka Chysn / [Beige Maze](https://soundcloud.com/beige-maze)).

ornament**s** & crime**s** was a collaborative firmware project by Patrick Dowling (aka **pld**), mxmxmx, and Tim Churches (aka **bennelong.bicyclist**), considerably extending the original firmware for the o_C / ASR eurorack module, designed by **mxmxmx**.

http://ornament-and-cri.me/

## License

Except where otherwise noted in file headers, all code herein is generally considered MIT licensed. However, there are some GPLv3 bits included, so the whole thing is also subject to compliance with the GPL. [More info here](https://ornament-and-cri.me/licensing/).
