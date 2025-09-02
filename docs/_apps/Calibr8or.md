---
layout: default
---
# Calibr8or

[![Using Calibr8or Video](http://img.youtube.com/vi/Eyb4olZm7qM/0.jpg)](http://www.youtube.com/watch?v=Eyb4olZm7qM "Using the Calibr8or firmware for Ornament & Crime to make your Eurorack oscillators track correctly")

Read the [full write-up](https://learningmodular.com/calibr8or/) from Chris Meyer.

### Quad Performance Quantizer + Pitch Fine-Tuning App
Commissioned by Chris Meyer ([Alias Zone](https://aliaszone.com/) / [Learning Modular](https://learningmodular.com/)) - this is a **4-channel Pitch CV fine-tuning tool** with performance-oriented transpose controls. Each channel can be clocked for latching of the _Transpose_ value or full S&H of the input. There are 4 preset banks to store and quickly switch configurations.

The Fine Tune controls at the bottom of the main view apply voltage _Scaling_ and _Bias Offset_ to the output stage, per channel. Scaling is in increments of ±0.01% and Bias is effectively ±1/128th of a semitone.

Accurate performance will depend on the calibration of your o_C module itself.

### UI Controls
![Screenshot 2024-06-13 15-55-32](https://github.com/djphazer/O_C-Phazerville/assets/109086194/1993a0d6-eeb6-444d-b6b8-3579fde9c4cc)

- UP / DOWN buttons - switch channels
- Quick-press Left Encoder - switch between Transpose and Fine-Tune controls
- Quick-press Right Encoder - toggle Scale & Root Note selection
- Long-press Left Encoder with cursor on Transpose - cycle Clocked Transpose / S&H modes per channel
- (new in v1.6.777) Long-press Left Encoder with cursor on Fine-Tune controls - Auto-tuner closed loop DAC calibration routine, ported from References
- Long-press DOWN button - preset switching
  - turn or press Left Encoder to toggle Save/Load
  - use Right Encoder to select
  - UP or DOWN to cancel
- Dual-press UP+DOWN for Clock Setup (shared with Hemispheres)

As of v1.6.6, storing a preset will automatically save App Data to EEPROM in the background.
