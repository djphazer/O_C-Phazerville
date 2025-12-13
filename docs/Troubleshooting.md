---
title: Troubleshooting
nav_order: 6
---

# Troubleshooting

If applets like Stairs, Carpeggio or Shredder appear unresponsive, you might need to [calibrate your ADC](https://www.youtube.com/shorts/AIadpDclP7M) to eliminate an offset on the inputs. This can be done without a fancy multimeter - go to the **[Setup / About](Setup-About)** app and just skip through all the other Calibration steps. Simply unplug all CV/Trigger inputs and outputs, enter the Calibration routine and ensure that all ADC values are around 0 (-1 < > +1) as you scroll by... then Save at the end.

## Hardware Validation

* To check output calibration, use the **Voltage** applet.
* To check input calibration, use **Scope** applet.
* (T41-specific) To check MIDI, send Clock to it on any of the interfaces (USB Device, USB Host, serial MIDI) and verify that the Clock starts and is sent back out all the other interfaces.

***

If your encoders don't rotate the way you expect, you can flip the behaviour of one, the other, or both as part of the [Setup / About](Setup-About) calibration routine.

After selecting "Calibrate" in Setup / About (short press of LEFT encoder), press either the UP and DOWN buttons to choose your encoder reversal: L, R, both (LR), or neither (normal) — press the RIGHT encoder to accept.

To save the encoder reversal setting, you will need to scroll through the entire calibration routine - rotate LEFT encoder to quickly skip to the last page, press RIGHT encoder to save.

***
## Still stuck?

If you've encountered a reproducible problem with an App or Applet, I'm here to help! Please mention the **full filename** of the firmware HEX file you used and your specific hardware model/vendor when filing an [Issue](https://github.com/djphazer/O_C-Phazerville/issues) on Github.
