TODO (Roadmap)
===

# v2.0
* Eliminate vtable overhead
* T4.1 - expand to 8 channels: Quantermain, Quadraturia, Sequins
* Auto-tuner with floor/ceiling detection (fail gracefully)
* generalized AppletParams for flexible assignment, extra virtual I/O
* Integrate Calibr8or with DAC for global tracking adjustments
* USB Gamepad support
* Config option for LFS vs. SD for preset storage

# ???
* Re-implement Piqued envelopes in an applet
* Audio Applets for T4.1
  - add VCF+VCA to Osc
  - WAVPlay: rework looping/caching; support more metadata tags (tempo, cue points)
* Update Boilerplates - I just assume this needs attention
* MORE MIDI STUFF:
    - MIDI looper applet!
    - MIDI output for all apps?
    - Implement some MIDI SysEx commands, sheesh
    - WebMIDI interface

# APP IDEAS
* Modul8or
  - 8 independent channels, maybe reusing applets
  - various engines (VectorOsc, tideslite, etc.)
  - freely assignable inputs; static channel outputs
* Two Spheres (two applets in series on each side)
* Snake Game
* Tetris

# [DONE]
* T4.1 - expanded to 8 channels: Piqued, Captain MIDI
* MTP Disk mode for file management over USB
* Pong 2.0 with sound effects
* **Fully merge "abandoned/refactoring" branch from pld**
* Pop-up MIDI Map editor
- 3-band EQ / multi-band dynamics
* MIDI mapping for param modulation sources
- multi-mode (HP, BP, LP) for Filt/Fold
* Quadrants Preset Bank switching
* Config files on LittleFS / SD for T4.x
* Unipolar randomize in SequenceX
* better Polyphonic MIDI input tracking
* Multipliers in DivSeq (maybe a separate applet)
* Runtime filtering/hiding of Applets
* QUADRANTS
* Automatic stop for internal Clock
* global quantizer settings in Hemisphere Config
* Flexible input remapping for Hemisphere
* Move calibration routines to a proper App
* add swing/shuffle to internal clock
* applet with modal interchange - MultiScale or ScaleDuet
* Add auto-tuner to Calibr8or
* ProbMeloD - alternate melody on 2nd output
* Fix FLIP_180 calibration
* Add Clock Setup to Calibr8or
* Calibr8or screensaver
* Pull in Automatonnetz
* Sync-Start for internal Clock
* General Config screen (long-press right button)
* better MIDI input message delegation (event listeners?)
* import alternative grids_resources patterns for DrumMap2
* Add Root Note to DualTM
