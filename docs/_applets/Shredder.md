---
layout: default
---
# Shredder

![Shredder Screenshot](images/Shredder.png)

**Shredder** is a 2-channel, 16-step CV sequencer on a 4x4 grid, inspired by the [Noise Engineering Mimetic Digitalis](https://noiseengineering.us/products/mimetic-digitalis). Each channel can have random values "shred" to the sequence within a specified range. Ranges can be from 1 to 5 volts unipolar, or 1 to 3 volts bi-polar. Additionally, one or both channels can run through the quantizer.

[See it in action!](https://youtu.be/yLr3vQJm3wM)

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Clock   |  Reset   |
| CV INs | X position    |  Y position   |
| OUTs   |  Pitch Ch 1   | Pitch Ch 1    |

## UI Parameters
 * Octave range
 * Quantized channels
 * Quantizer edit
 * AuxButton: Shred selected channel
    - Highlight channel octave range and press the select button — UP or DOWN, depending on Hemisphere
 * (new in v1.8.3) Shred-on-Reset toggle, per channel
    - Highlighted means that channel will be randomized on Reset

_Note: Due to memory limitations, generated sequences cannot be saved. However, all parameters will be saved and new sequences will be generated from those settings on power up._

### Credits
Copied/Adapted from [Shredder](https://github.com/benirose/O_C-BenisphereSuite/wiki/Shredder) by benirose
