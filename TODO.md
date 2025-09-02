TODO (Roadmap)
===

# v1.10
* Config option for LFS vs. SD for preset storage

# v2.0
* T4.1 - expand to 8 channels: Quantermain, Piqued, Quadraturia, Captain MIDI
* **Fully merge "abandoned/refactoring" branch from pld**
  - this is mostly done on the dev/2.0 branch
* Auto-tuner with floor/ceiling detection (fail gracefully)
* generalized AppletParams for flexible assignment, extra virtual I/O
* Pop-up MIDI Map editor
* Integrate Calibr8or with DAC for global tracking adjustments
* USB Gamepad support
* MTP_Teensy lib for file management over USB

# ???
* Audio Applets for T4.1
  - add VCF+VCA to Osc
  - 3-band EQ / multi-band dynamics
  - WAVPlay: rework looping/caching; support more metadata tags (tempo, cue points)
* Update Boilerplates - I just assume this needs attention
* MORE MIDI STUFF:
    - MIDI looper applet!
    - MIDI output for all apps?
    - Implement some MIDI SysEx commands, sheesh
    - WebMIDI interface
* Add BugCrack-style sounds to Pong

# APP IDEAS
* Two Spheres (two applets in series on each side)
* Snake Game
* Tetris

# [DONE]
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
