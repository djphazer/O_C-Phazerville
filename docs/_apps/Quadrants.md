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

Many controls are exclusive to new hardware with 5 buttons + 2 encoders.

| Button | Action |
| ------ | ------ |
| Press **A/B/X/Y** | switch corresponding applet into view (NW, NE, SW, SE) |
| Double-click **A/B/X/Y** | view applet full-screen or help/config<br>- adjust input mappings for triggers/CV, or clock trigger multipliers if Clock is running<br>- switch loaded applet |
| Hold **X** or **Y**, and rotate either Encoder | Quick-switch visible applet on each side |
| Press **A + B** | Clock Setup |
| Press **Z** | Start/Stop/Arm the Clock |
| Press **A + Y** or **X + B** (diagonal combos) | Overview |
| Long-press **B** | Config menu (presets, general settings, input mapping, quantizers, etc) |
| Press **A + X** | Load Preset shortcut |
| Press **B + Y** | Input Mapping Config shortcut |
| Press **X + Y** | Audio DSP subsystem<br>- use A or B to jump back out |

The encoders behave intuitively in split-screen views - rotate to move the corresponding Left/Right cursor, push to select or toggle editing.

In full-screen views, rotating **Left Encoder** typically switches pages or makes coarse adjustments; **Right Encoder** moves the cursor, or makes fine adjustments; push either Encoder button to select or toggle editing.

## Input Mapping

Each applet still has only 2 trigger inputs, 2 CV inputs, and 2 outputs. The full-screen Help/Config screen (double-click applet select button) can be used for quick reference or editing of these mappings.

![Applet Config screen](images/Applet-Config-output.png)

The output routing is hardcoded and not configurable - the "North-West" applet uses DAC outputs A & B, "North-East" uses DAC outputs C & D, etc. You can, however, adjust Slew and Attenuversion for each output.

The inputs can be completely reassigned, allowing triggers to be derived from TR1..4 as well as CV1..8 or even looped back from one of the outputs. Same with CV. Complex internal feedback patterns can be achieved this way, but also simple use cases such as clocking all 4 applets from TR1, or sampling the same CV input with several instances of Squanch.

## Audio DSP

![Audio Applet Overview](images/AudioAppletOverview.png)

Using the dedicated onboard audio codec hardware, the new Audio subsystem provides customizable sound generation and effects processing chains in-the-box!

Audio DSP is applet-based, like CV applets, but with a few differences. The UI is divided into two channels of 5 slots each. The left channel goes to the left output, the right channel goes to the right output.

Audio in each channel is processed from top to bottom, with each applet receiving the output of the previous one.

Applet slots can either be dual mono, where you have two independent applets, or stereo, where you have a single applet that processes both channels.

The first slot is dedicated for sound sources, though you can also inject sound sources later on in the chain as well.

The animated bars above and below the slots indicate audio level before and after that slot.

Check the sidebar navigation for details about each Audio Applet.

### Controls

| Button / Encoder | Action |
| ---------------- | ------ |
| Rotate the **Left Encoder** | move left arrow cursor to select an applet slot in the left channel |
| Rotate the **Right Encoder** | move right arrow cursor to select an applet slot in the right channel |
|                               | (stereo applets can be accessed from either side) |
| Press the **Encoder** | Select the applet slot.<br>- while highlighted, rotate the Encoder to change which applet to load in the slot<br>- Press the Encoder again to confirm any change and/or view and interact with the applet<br>- A or B button to jump back out |
| Press **A** or **B** | close an applet's editor if open, or return to CV applet view from the audio slots |

To switch a slot between dual mono and stereo, select that slot with both cursors and press both encoders simultaneously.

## Preset Storage

_Note:_ Using a microSD card is highly recommended! It's more reliable, and doesn't stutter while saving. We've had reports of preset banks disappearing when using internal LFS storage...

Presets are stored internally in Flash using LittleFS, with a 512KB partition, or on a microSD card inserted in the slot on Teensy 4.1. There can be multiple Bank files, with 32 Presets per Bank. Input Mappings and Clock settings are stored _per Preset_. Quantizer scale settings are stored _per Bank_.

If a microSD card is detected, it becomes the primary storage for Bank files, only using internal LFS as a fallback when loading if a file is not found on the card. This means you can effectively copy banks from internal storage to SD by re-saving a preset.
