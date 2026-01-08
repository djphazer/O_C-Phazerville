---
layout: default
parent: Config
nav_order: 3
---
# Quantizer Setup

This full-screen page in the [config menu](Hemisphere-Config) is for configuring multiple quantizers (Q-engines) with various settings, which may be quickly selected & edited in any applet that uses quantization.

![New-Q-editor](https://github.com/user-attachments/assets/793711ff-08f8-4910-a1ea-8b1369c244ab)

There are 8 global quantizer engines, each with a **Scale**, **Root Note**, **Octave**, and **Scale Mask** setting. An encoder push will open the popup editor. Use LEFT encoder to move the cursor, RIGHT encoder to modify the selected parameter, and UP/DOWN select buttons to jump octaves. Push either encoder again to exit. These gestures are the same when editing quantization settings within an applet.

### Teensy 3.2 Limitations
On original O_C hardware, settings for the first 4 quantizers can be only saved with [Calibr8or](Calibr8or). Some applets may also save/restore quantizer settings, but this may change.

On T4.0 or T4.1, all settings are stored when you save a [Preset](Hemisphere-Presets).
