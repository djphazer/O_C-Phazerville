---
layout: default
---
# Scale Duet

![Screenshot 2024-06-13 15-22-17](https://github.com/djphazer/O_C-Phazerville/assets/109086194/bdf33873-4522-41ce-bfd2-5d5f75481d3c)

**Scale Duet** is a single-channel quantizer that allows you to switch between two user-defined scales. The scales are edited with an on-screen keyboard.

Quantization is continuous by default, unless a Clock pulse is received. Sending a positive voltage to CV2 will restore continuous operation.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock | Scale 1 (low) or Scale 2 (high) |
| CV INs | Input Signal | Unclock (switch back to continuous) |
| OUTs   | Quantized Pitch | Auto-trigger when Pitch changes |


### UI Controls
* Encoder Push: Toggles the note above the cursor ON or OFF. A small square will appear on the keyboard for notes that will be played when that scale is selected.
* Encoder Turn: Moves through the notes for each scale, and between Scales 1 and 2.

Scale Duet differs from Dual Quantizer in a few important ways:
* Scale Duet plays from two user-defined scales, rather than pre-programmed scales
* It quantizes only one value at a time
