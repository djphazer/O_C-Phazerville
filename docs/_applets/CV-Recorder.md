---
layout: default
---
# CV Recorder

![Screenshot 2024-06-13 14-38-41](https://github.com/djphazer/O_C-Phazerville/assets/109086194/289cb265-795b-45a3-b77c-d433ec2a2627)

**CV Recorder** is a two-track 384-step CV recorder with smoothing (linear interpolation) and adjustable start/end points.

### I/O

|        |        1/3        |      2/4      |
| ------ | :---------------: | :-----------: |
| TRIG   | Advance sequencer |     Reset     |
| CV INs |   Track 1 Input   | Track 2 Input |
| OUTs   |      Track 1      |    Track 2    |


### UI Parameters
* Start point
* End point
* Toggle smoothing on/off
* Transport control
  - Play
  - Record Track 1
  - Record Track 2
  - Record Track 1 + 2

**Recording**
To start recording, choose a length by setting the Start and End points. Advance the cursor down to the transport control (which will usually just say "Play"). Turn the encoder to choose which track or tracks you wish to record (1, 2, 1+2). When you press the encoder button, recording will begin. An indicator bar will display on top of the transport control line to indicate remaining steps. Recording will automatically stop once the end point step has been reached, and looping play mode will resume.

### Credits
Adapted from [CV Recorder](https://github.com/Chysn/O_C-HemisphereSuite/wiki/CV-Recorder) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
