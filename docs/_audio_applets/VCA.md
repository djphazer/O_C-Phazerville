---
layout: default
---
# VCA (mono/stereo)

![VCA screenshot](images/VCA.png)

A flexible VCA with level, offset, variable response, optional rectification, and inversion.
In stereo, the VCA is applied to both channels.

### Parameters
* Lvl: The gain applied to the input signal, scaled by CV. CV is normalled to 6v so Lvl will act a basic attenuator without a CV source.
* Off: The offset gain applied to the input signal. Overall gain will be `Lvl * Exp(CV) + Off`.
* Exp: Controls how exponential the CV response is. At 0%, the response is linear. At 100%, the response is fully exponential. CVable.
* Rectify: If enabled, CV will be rectified to remove negative values. If disabled, the VCA will act as a ring modulator/four-quadrant multiplier.
* Invert: If enabled, CV will be inverted. Combined with Off, this allows the VCA to easily be used for ducking in response to an envelope.

### Credits
Authored by qiemem (Bryan Head).