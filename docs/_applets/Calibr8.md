---
layout: default
---
# Calibr8

![Screenshot 2024-06-13 14-30-47](https://github.com/djphazer/O_C-Phazerville/assets/109086194/4eded94b-8db0-43fb-aaf8-7d6b7305aa81)

**Calibr8** is a 2 channel pitch calibration tool to compensate for imperfect v/Oct tracking in your oscillators. For a 4 channel version with many more features (flexible quantization, sample and hold, auto-calibration), use the full [Calibr8or](Calibr8or) app.

### I/O

|        |          1/3          |          2/4          |
| ------ | :-------------------: | :-------------------: |
| TRIG   |  Transposition clock  |       No effect       |
| CV INs |   Pitch CV input 1    |   Pitch CV input 2    |
| OUTs   | Calibrated pitch CV 1 | Calibrated pitch CV 2 |

_Note: the outputs of Calibr8 are quantized to semitones_

### UI Parameters
* Pitch CV1 scaling factor
* Pitch CV1 transposition
* Pitch CV1 offset (detune)
* Pitch CV2 scaling factor
* Pitch CV2 transposition
* Pitch CV2 offset (detune)

### Credits
Authored by djphazer, as a proof-of-concept for [Calibr8or](Calibr8or)
