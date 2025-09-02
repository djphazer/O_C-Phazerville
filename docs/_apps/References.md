---
layout: default
---
# References

_References_ is a simple utility app which outputs fixed reference voltages. These are handy when calibrating other modules, such as VCOs.

Note that unlike all the other apps, the settings for _references_ are *not* saved and will revert tot he defaults eat time the module is powered on. This is to save EEPROM storage space - saved settings were not regarded as critical for this app.
### Controls

|       | Left Encoder                                               | Right Encoder                                                                                                     |
| ----- | ---------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| TURN  | Select channel A to D to edit (all channels always active) | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited. |
| PRESS |                                                            | Toggle between menu navigation (selection) mode and value editing mode                                            |
| LONG  |                                                            | App selection menu                                                                                                |

|            | Up Button | Down Button |
| ---------- | --------- | ----------- |
| PRESS      |           |             |
| LONG PRESS |           |             |

### Available settings (per-channel)

|Setting | Meaning |
|---|---|
|`Octave`| sets the octave range. Note that the octave numbering is such that C for octave 0 is 0V, C for octave 1 is 1V and so on.|
|`Semitone`| sets the semitone offset wishing each octave range. One semitone increment is 0.08333333333V (83.333333mV).|
|`Mod range oct` | sets the number of octaves to automatically jump up or down. This can be useful when adjusting trimpots on VCOs.|
|`Mod rate (s)`| sets the rate (as a period in seconds) at which the reference voltage jumps up or down by the number of octaves set by `Mod range oct`.|

### Inputs and outputs

No inputs are used. Reference voltages for channels A to D appear on outputs A to D respectively.

### Screensaver display

The screen is divided into four channel lanes (columns), with the same layout as the _Quantermain_ screensaver, except that the output voltage (rounded to the nearest millivolt) is shown as the top of each lane (column). Note that the minus signs for negative voltages are quite small due to the limited display space, and can be quite hard to see.

*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
