---
layout: default
---
# Scope

![Screenshot 2024-06-13 15-24-57](https://github.com/djphazer/O_C-Phazerville/assets/109086194/66b6a983-14c4-460a-9c51-505e860262f0)

**Scope** is a simple CV and clock monitoring tool.

Phazerville Suite has updated it with more options and an X-Y view mode that plots both inputs in 2 dimensions. In v1.7.1, the help screen (double press UP button for left hemisphere, double press DOWN button for right hemisphere) has also been replaced with a full-screen view for maximum resolution.

![Screenshot 2024-06-13 15-26-15](https://github.com/djphazer/O_C-Phazerville/assets/109086194/d37fa85a-462f-40c7-8900-7b38d1887797)
![Screenshot 2024-06-13 15-32-19](https://github.com/djphazer/O_C-Phazerville/assets/109086194/d6f8adb7-0ce3-4b82-9264-5926ebacc0ee)

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock for BPM counter | Sync pulse for display wavelength |
| CV INs | Signal 1   | Signal 2   |
| OUTs   | Pass-thru | Pass-thru |

## UI Parameters
- Mode
  - 1+1 (Scope signal 1, Report signal 1)
  - 1+2 (Scope signal 1, Report signal 2)
  - 2+1 (Scope signal 2, Report signal 1)
  - 2+2 (Scope signal 2, Report signal 2)
  - 1,2 (Scope XY)
- Rate (see notes)
- Freeze (toggle)


### Notes
The "Rate" control is the number of 60µs "ticks" that will elapse between samples. 64 samples will be shown on the display at any time. The display is bi-polar. You can use the encoder to find the best rate at which to view a waveform. _Starting at Hemisphere Suite 1.1: If you send a clock to Digital 2 with the same period as the signal to CV 1 (for example, EOR trigger of Maths while viewing a waveform from Maths), Scope will automatically adjust the sample rate for the best view._

_Note: Everything's approximate, including the CV passthru._

### Credits
Originally authored by Chysn (Jason Justian) with modifications by Tom Waters & djphazer
