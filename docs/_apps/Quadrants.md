---
layout: default
---
# Quadrants

![O_CT4.1 Control Guide](images/front_panel_desc.png)

**Quadrants** is an alternate frontend for Hemispheres applets, upgraded to host **four** CV applets at a time, taking advantage of new 8-channel hardware shields with Teensy 4.1. There are also 5 slots per channel of Audio DSP processing applets.

## Dungeon Map

This quick-reference diagram shows how to navigate between various screens and functions.

![Quadrants Quickstart Diagram](images/Quadrants_Quickstart.png)

### Controls

Some controls are exclusive to new hardware with 5 buttons + 2 encoders:
* Press **A/B/X/Y** - switch corresponding applet into view (NW, NE, SW, SE)
* Double-click **A/B/X/Y** - view applet full-screen or help/config
  - adjust input mappings for triggers/CV, or clock trigger multipliers if Clock is running
  - switch loaded applet
* Press **Z** - Start/Stop/Arm the Clock
* Press **A + Y** or **X + B** - Overview
* Press **A + X** - Load Preset shortcut
* Press **B + Y** - Input Mapping Config shortcut
* Press **X + Y** - Audio DSP subsystem

Other controls are identical to Hemispheres:
* Press **A + B** - Clock Setup
* Long-press **A** - Invoke Screensaver (global)
* Long-press **B** - Config menu (presets, general settings, input mapping, quantizers, etc)

The encoders behave intuitively in split-screen views - rotate to move the corresponding Left/Right cursor, push to select or toggle editing. In full-screen views, rotating **Left Encoder** typically switches pages or makes coarse adjustments; **Right Encoder** moves the cursor, or makes fine adjustments; push either Encoder button to select or toggle editing.

### Input Mapping

Each applet still has only 2 trigger inputs, 2 CV inputs, and 2 outputs.

The output routing is hardcoded and not configurable - the "North-West" applet uses DAC outputs A & B, "North-East" uses DAC outputs C & D, etc.

The inputs, however, can be completely reassigned, allowing triggers to be derived from TR1..4 as well as CV1..8 or even looped back from one of the outputs. Same with CV. Complex internal feedback patterns can be achieved this way, but also simple use cases such as clocking all 4 applets from TR1, or sampling the same CV input with several instances of Squanch.

### Audio DSP

![Audio Applet Overview](images/AudioAppletOverview.png)

Using the dedicated onboard audio codec hardware, the new Audio subsystem provides customizable sound generation and effects processing chains in-the-box!
Audio DSP is applet-based, like CV applets, but with a few differences.
The AudioDSP is divided into two channels of 5 slots each.
The left channel goes to the left output, the right channel goes to the right output.
Audio in each channel is processed from top to bottom, with each applet receiving the output of the previous one.
Applet slots can either be dual mono, where you have two independent applets, or stereo, where you have a single applet that processes both channels.
The first slot is a dedicated for sound sources, though you can also inject sound sources later on in the chain as well.
The animated bars above and below the slots indicate audio level before and after that slot.

Check the sidebar navigation for details about each Audio Applet.

#### Controls

* Rotate the **Left Encoder** to select an applet in the left channel or the **Right Encoder** to select an applet in the right channel.
* Press the **Encoder** to highlight the selected applet in that channel. Once highlighted, rotate the encoder to change which applet is present in that slot. Press the encoder again to edit the applet's parameters.
* Press **A/B** to close an applet's editor if open, or return to CV applet view if no applet's editor is open.
* To switch a slot between dual mono and stereo, select that slot with both encoders and press them simultaneously.

### Preset Storage

Presets are stored internally in Flash using LittleFS, with 512KB of space allocated. There can be multiple Bank files, with 32 Presets per Bank. Input Mappings and Clock settings are stored _per Preset_. Quantizer scale settings are stored _per Bank_.

If a microSD card is detected, it becomes the primary storage for Bank files, using internal LFS as a fallback when loading if a file is not found on the card. Some utilities for copying between LFS <-> SD card are planned.
