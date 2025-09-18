---
layout: default
---
# Mid/Side (stereo)

![Mid/Side screenshot](images/MidSide.png)

A simple applet to facilitate mid/side processing.
The output of the left channel will be `left + right` while the output of the right channel will be `left - right`.
One instance will put the center signal into the left channel and the surround signal into the right channel.
You can then process the center signal and surround signal separately and then recombine them with another instance of Mid/Side.

### Parameter
* Gain: The gain applied to the input signal in dBs. Two instances of mid/side result in a 6dB gain increase. You can use this parameter to compensate.

### Credits
Authored by qiemem (Bryan Head).