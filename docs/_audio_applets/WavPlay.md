---
layout: default
---
# WAV File Player

![WavPlay screenshot](images/WavPlay.png)

Designed as a tempo-synced, DJ-style track player for drum stems and such, but also works for oneshots. _Work in Progress!_

Files must reside in the root directory of the SD card, named like "000.WAV", "001.WAV", etc. All sampling rates should work, 16-bit only. If any given file is opened successfully, the UI will show the tempo (or `0`), otherwise it shows `(--)`.

Use AuxButton (X or Y button) for manual Play/Stop. When Sync is enabled and the internal clock is running, a manual Play action waits for the next beat.

### Parameters
* File number
* Start/Retrigger input
* DJ filter (LP/HP)
  - AuxButton to toggle bypass
* DJ filter mod input
* Level
* Level mod input
* Rate / Sync
  - AuxButton to toggle tempo Sync
  - When "Sync" is on, this is a percent relative to internal clock tempo
  - For regular "Rate", this is percent of original track speed
* Rate mod input
* Loop length, in beats
  - only works with internal clock running
* Loop enable

### Credits
Authored by djphazer. Based on the [TeensyVariablePlayback](https://github.com/djphazer/teensy-variable-playback) library by Nic Newdigate.
