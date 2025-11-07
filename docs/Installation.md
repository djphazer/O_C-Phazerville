---
title: Installation
nav_order: 2
---

# Installation

**Phazerville Suite** is free, open-source software (firmware) for the Ornament + Crime module. The licenses under which the software is released permit anyone to freely install and use the firmware on copies of the module, to modify it, and to provide copies to others. Third-party module manufacturers who re-use portions of the O+C software in their modules or devices should ensure that they meet the obligations imposed by the licenses under which the O+C source code is released - details are [here](https://ornament-and-cri.me/licensing/).

## Firmware upload methods
_NB/FAQ: just updating the firmware (on a calibrated module) doesn’t require re-calibration — the calibration values are not overwritten when you install new versions of the firmware. Newly built modules, on the other hand, must be calibrated in order to function properly._

There are a few ways of getting the firmware onto your module:

* [Method A](#method-a): upload a pre-compiled HEX file. this is easy and quick!
* [Method B](#method-b): install the Arduino IDE and the Teensyduino add-on and compile the code yourself.
* [Method C](#method-c): install PlatformIO and compile the code yourself.
* Either way, you’ll need: a micro-usb cable (make sure this isn’t for charging only, but data transfer).
* NB: the following steps assume that you have cut the usb trace. see [here](https://ornament-and-cri.me/build-it/#cut_trace) for an example of the Teensy 3.2.

## Method A
uploading the HEX file

### step 1): install the [Teensy Loader](https://www.pjrc.com/teensy/loader.html) program
- the Teensy Loader is available from [the PJRC website](https://www.pjrc.com/teensy/loader.html)

### step 2): download the binary HEX file
- download the latest released version of the firmware image file (.hex) from [the Releases page](https://github.com/djphazer/O_C-Phazerville/releases)

### step 3): open the HEX file in the Teensy Loader
- open the HEX file in the Teensy Loader application
- make sure a USB cable is connected to the Teensy, and that the O+C module is powered up
- press the program push switch on the Teensy board (on the back of the O+C module) **OR if you are updating from Phazerville v1.8.1 or later, you can reflash _without_ accessing the back of the module:**
1. Navigate to the Setup / About App
2. Turn the LEFT encoder — the display should read "Reflash"
3. Press the LEFT encoder to enter Flash Upgrade Mode, and proceed with the remaining instructions
- click the Program icon, or choose Program from the Operation menu in Teensy Loader
- (you should briefly see a progress bar as the firmware is uploaded)
- click the reboot icon or choose Operation > Reboot

Your O+C should now run the updated firmware (resp. come to life, if newly built). If this is a newly built module, proceed to [Calibration](Setup-About#calibration-routine) (just updating the firmware doesn’t require re-calibration)

## Method B
Compiling the firmware with Arduino IDE + Teensyduino

### step 1): get the IDE + teensyduino add-on
- if you don’t have it already, you need to install the [Arduino IDE](https://www.arduino.cc/en/software) as well as the [Teensyduino](https://www.pjrc.com/teensy/td_download.html) add-on.
- unlike past firmwares, the latest versions of each should work just fine.

### step 2): clone or download the firmware source code repository to your computer
- clone from `https://github.com/djphazer/O_C-Phazerville`
- use the `phazerville` branch (which is the default) - this is the latest “production” released code
- `dev/*` and other branches contain bleeding-edge code which may or may not contain bugs

### step 3): compile
- Once the libraries and source code for the firmware are in place, you should be able to compile it. Open the file called `src.ino`. Now make sure you:
1. select "Teensy 3.2/3.1" in `Tools > Board`
2. select "MIDI" in `Tools > USB Type`
3. select "120 MHz (overclock)" in `Tools > CPU Speed`
4. select "Smallest Code" in `Tools > Optimize`
- compile. and upload to your board (since you’ve cut the usb trace, the module needs to be powered from your eurorack PSU): the display should come to life now.

## Method C
Compiling the firmware with PlatformIO

### step 1): [install PlatformIO](https://platformio.org/install)
- This firmware fork can also be built using Platform IO, a Python-based build toolchain, available as either a [standalone CLI](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or a [full-featured IDE](https://platformio.org/install/ide), as well as a plugin for VSCode and other existing IDEs.
- Follow one of these links to get that set up first.

### step 2): clone or download the firmware source code repository to your computer
- (same as step 2 for Method B above)

### step 3): compile
- Navigate to the `software/` directory in the source code. From there, you can use PlatformIO to Build the desired configuration and Upload via USB to your module.
- For example, in the terminal, I type:
```
pio run -e pewpewpew -t upload
```
- In VSCode or other IDE plugins, you'll see commands labeled "Build" and "Upload" under "General" for the various build targets.
- Have a look inside `platformio.ini` for alternative build configurations - VOR, flipped screen, Teensy 4.x, etc. - and to customize various app flags.

## Calibrate

If this is a newly built module, proceed to [Calibration](Setup-About#calibration-routine) after installation. Just updating the firmware doesn’t require re-calibration - the calibration values are not overwritten when you install new versions of the firmware.
