---
layout: default
---
# Dialectic Ping Pong

_Dialectic Ping Pong_ is a port of the bouncing ball envelope generators from the *Mutable Instruments Peaks* module source code (these are not exposed in the official firmware, but are available through [Dead Man's Catch](https://github.com/timchurches/Mutated-Mutables/releases) alternative firmware). These generators implement a simple but effective simulation of the physics of a ball that is thrown into the air with a certain velocity, from a certain height, and which then returns to Earth (or a planet of your choice) under the influence of (configurable) gravity, and then bounces (with a settable "bounce loss" simulating how hard the ball is pumped up, if it is a basketball), before being pulled back to Earth and bouncing again, and so on.

### Controls

|       | Left Encoder                                               | Right Encoder                                                                                                     |
| ----- | ---------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| TURN  | Select channel A to D to edit (all channels always active) | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited. |
| PRESS |                                                            | Toggle between menu navigation (selection) mode and value editing mode                                            |
| LONG  |                                                            | App selection menu                                                                                                |

|            | Up Button              | Down Button            |
| ---------- | ---------------------- | ---------------------- |
| PRESS      | Increase gravity by 32 | Decrease gravity by 32 |
| LONG PRESS |                        |                        |


### I/O

|     | 1          | 2          | 3          | 4          |
| --- | ---------- | ---------- | ---------- | ---------- |
| TR  | (mappable) | (mappable) | (mappable) | (mappable) |
| CV  | (mappable) | (mappable) | (mappable) | (mappable) |
| OUT | output     | output     | output     | output     |

### Available settings (per-channel)

| Setting         | Meaning                                                                                                                                                                                                                                         |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Gravity`       | _g_, the acceleration due to gravity, from 0 (no gravity) to 255 (gravity on a massive alien planet)                                                                                                                                            |
| `Bounce loss`   | The amount of energy lost by the ball at each bounce (0 to 255). Higher values act like a deflated basketball.                                                                                                                                  |
| `Amplitude`     | Initial amplitude (height) of the ball when the envelope is triggered, 0 to 255.                                                                                                                                                                |
| `Velocity`      | Initial velocity of the ball when the envelope is triggered - the size of the kick or impulse imparted to the ball, if you like. Note that high values will cause the ball to bounce off the roof of the gymnasium that houses these envelopes. |
| `Trigger input` | Trigger input source (TR1 to TR4) for the current channel                                                                                                                                                                                       |
| `CV1 ->`        | Mapping of the CV1 input to a parameter for the selected channel. Values are "off", "grav" (gravity), "bnce" (bounce loss), "ampl" (initial amplitude),  and "vel" (initial velocity).                                                          |
| `CV2 ->`        | As for `CV1->`, but for CV2.                                                                                                                                                                                                                    |
| `CV3 ->`        | As for `CV1->`, but for CV3.                                                                                                                                                                                                                    |
| `CV4 ->`        | As for `CV1->`, but for CV4.                                                                                                                                                                                                                    |
| `Hard reset`    | If set to on, the envelope will instantly restart at the currently set initial amplitude, rather than starting from the height that the ball happens to be at the time when the trigger is received.                                            |

### Screensaver display

The screen is divided into quadrants, each showing a rolling display of the output values on each of channels A to D.

*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
