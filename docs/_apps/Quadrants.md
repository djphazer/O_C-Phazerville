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

Using the dedicated onboard audio codec hardware, the new Audio subsystem provides customizable sound generation and effects processing chains in-the-box!

![Audio Applets Overview](images/AudioAppletOverview.png)

Check the sidebar navigation for details about each Audio Applet.

### Preset Storage

Presets are stored internally in Flash using LittleFS, with 512KB of space allocated. There can be multiple Bank files, with 32 Presets per Bank. Input Mappings and Clock settings are stored _per Preset_. Quantizer scale settings are stored _per Bank_.

If a microSD card is detected, it becomes the primary storage for Bank files, using internal LFS as a fallback when loading if a file is not found on the card. Some utilities for copying between LFS <-> SD card are planned.
