---
layout: default
parent: Hemisphere Config
nav_order: 6
---
# Clock Setup

![Screenshot 2024-06-13 14-09-05](https://github.com/djphazer/O_C-Phazerville/assets/109086194/62d559a1-c09f-46b2-b137-f6e1529e8988)
![Screenshot 2024-06-13 14-09-29](https://github.com/djphazer/O_C-Phazerville/assets/109086194/0ef7eb18-3206-44e0-b1b6-0c8131b3a973)
(TODO: new screenshots)

The **Clock** configuration screens are accessed by dual-pressing both _UP+DOWN_ buttons. The first page provides internal clock tempo controls for standalone operation. The multipliers determine how internally generated clock triggers are fed to the applets. The clock will sync to external pulses on TR1 while it's running, depending on the `Sync=` setting. [Input Mapping](Hemisphere-Input-Mapping) for the applet trigger/gate inputs is also here for convenience.

### Parameters
* **Play/Stop/Pause** (toggle) - indicated by a Clock icon followed by a Pause, Play or Stop icon
  - Can also be cycled with _Left Encoder Long-Press_ on any screen within Hemisphere
  - When Paused, the clock is armed - a single pulse on TR1 will immediately start the clock
* **Tempo** - pretty self-explanatory :P
  - Manual Tap-Tempo - press the encoder button 4 times on this parameter to detect and set the BPM accordingly
* **Swing/Shuffle** - delays every other clock pulse within a beat
* **Sync** - aka PPQN; expected resolution of external clock pulses on TR1
  - set to 0 to ignore incoming pulses
* **Multipliers/Dividers** - virtual triggers generated for each channel in relation to Tempo
  - set to 0 to disable internal clock on that channel; physical triggers will pass thru instead
* **Trigger Input Mapping** - reroute physical trigger inputs (or other signals) to the applets
  - only applies when the internal clock is not running, or if multiplier is set to "x0" for that channel
  - to mimic legacy "Clock Forwarding" function, simply set channel 3 to "TR1" instead of "TR3" (saved settings will automatically load like this)
* **TrigSkip** - probability of output triggers to be ignored on each channel
* **OutSlew** - Slew amount applied to each output
  - triggers become decay envelopes, everything else is smoothed
  - also available on full-screen applet help/config screen
* **OutAtten** - Output attenuverter per channel (-200%..+200% range)

### Notes
* Outgoing MIDI messages for Start, Stop, & Clock (at 24ppqn) are sent via USB automatically. An option to disable this is planned.
* Incoming MIDI Start/Stop/Clock messages intuitively start the internal clock and feed it sync pulses at 2ppqn.
* The internal Clock is available in **Hemisphere** as well as the **[Calibr8or](Calibr8or)** app. However, Clock settings are only stored in the active Preset in Hemisphere.
