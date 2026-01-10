---
layout: default
parent: Config
nav_order: 7
---
# Clock Setup

![Screenshot 2024-06-13 14-09-05](https://github.com/djphazer/O_C-Phazerville/assets/109086194/62d559a1-c09f-46b2-b137-f6e1529e8988)
![Screenshot 2024-06-13 14-09-29](https://github.com/djphazer/O_C-Phazerville/assets/109086194/0ef7eb18-3206-44e0-b1b6-0c8131b3a973)
(TODO: new screenshots)

The **Clock** configuration screens are accessed by dual-pressing both _UP + DOWN_ (or A + B) buttons.

The first page provides tempo controls for standalone operation. The multipliers/dividers determine how internally generated ("virtual" or "logical") clock triggers are fed to the applets. The clock will sync to external pulses on TR1 while it's running, depending on the `Sync=` setting. [Input Mappings](Hemisphere-Input-Mapping) for the applet trigger/gate inputs are also here for convenience.

The Clock Tempo and Swing settings can be modulated with CV via the [Metronome](Metronome) applet.

### Parameters
* **Play/Stop/Pause** (toggle) - indicated by a Clock icon followed by a Pause, Play or Stop icon
  - Can also be cycled with _Left Encoder Long-Press_ on any screen within Hemispheres
  - When Paused, the clock is armed for sync-start - a single pulse on TR1 will immediately start the clock
* **Tempo** - Beats Per Minute
  - one "Beat" is also known as a Quarter Note, the unit of measurement for PPQN (Pulses Per Quarter Note)
  - Manual Tap-Tempo - press the encoder button 4 times on this parameter to detect and set the BPM accordingly
* **Swing/Shuffle** - delays every other clock pulse within a beat
* **Sync** - aka PPQN; expected resolution of external clock pulses on TR1
  - set to 0 to ignore incoming pulses; TR1 will still start the clock when armed/paused.
* **Multipliers/Dividers** - virtual triggers generated for each channel in relation to Tempo
  - set to 0 to disable internal clock on that channel; physical triggers will pass thru instead
* **Trigger Input Mapping** - reroute physical trigger inputs (or other signals) to the applets
  - only applies when the internal clock is not running, or if multiplier is set to "x0" for that channel
  - to mimic legacy "Clock Forwarding" function, simply set channel 3 to "TR1" instead of "TR3" (saved settings will automatically load like this)

#### Extra Settings
Scroll a little further to find some additional settings. These aren't really related to the internal clock, but affect the behavior of the applet outputs. They might be moved in the future.
* **TrigSkip** - probability of output triggers to be ignored on each channel
* **OutSlew** - Slew amount applied to each output
  - triggers become decay envelopes, everything else is smoothed
  - also available on full-screen applet help/config screen

### Notes
* Outgoing MIDI messages for Start, Stop, & Clock (at 24ppqn) are sent via USB automatically.
* Incoming MIDI Start/Stop/Clock messages intuitively start the internal clock and feed it sync pulses. Pulses at TR1 are ignored while MIDI Clock is being received.
* The internal Clock is available in **Hemispheres**/**Quadrants** as well as the **[Calibr8or](Calibr8or)** App. However, Clock settings are only stored in the active Preset in Hemispheres/Quadrants.
