---
layout: default
---
# Upsampled (mono)

![Upsampled screenshot](images/Upsampled.png)

Allows you to process CV sources with the audio pipeline.
This allows you to process any of the original Hemispheres CV applets, such as BugCrack or Dr. LoFi, as though they were audio.
The applet interpolates the CV signals from the original 16.7kHz to 44.1kHz using the selected algorithm.

### Parameters
* Source: The CV input or output to process.
* Interp: The interpolation method to use.
    * ZOH: Zero Order Hold, also known as nearest neighbor interpolation. Will be the most lo-fi sounding, with heavy aliasing.
    * Lin: Linear interpolation.
    * Spl: Cubic Hermite spline.
* Lvl: The gain applied to the CV signal in dBs. CVable.
* AC: Applies a high-pass filter to remove DC bias.

# Credits
Authored by qiemem (Bryan Head).