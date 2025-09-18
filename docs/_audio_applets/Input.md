---
layout: default
---
# Input (mono/stereo)

![Input screenshot](images/Input.png)

As a mono applet, this injects the signal from the selected audio input into the channel.

As a stereo applet, it will either send the left input to the left channel and the right input to the right channel, or mix the inputs and send the result to both channels.

The simplest usecase of this applet is to process dual mono or stereo audio with other audio applets.
If you can also perform parallel processing of a single input with both channels, or inject the input mid-chain so that the first few applets can process one signal while the rest of the chain processes a different signal.

### Parameters
Mono:
* Mono mode: The first parameter lets you select which input is injected into the channel. Can be left, right, or mixed.

Stereo:
* Mix mode: Determines whether the left and right inputs are mixed and sent to both channels, or not

Both:
* Lvl: The gain applied to the input signal in dBs. CVable.

### Credits
Authored by qiemem (Bryan Head), with mods by djphazer.