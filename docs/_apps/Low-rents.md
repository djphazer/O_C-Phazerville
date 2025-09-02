---
layout: default
---
# Low-rents

Low-rents is a port of the [Lorenz attractor](https://en.wikipedia.org/wiki/Lorenz_system) modulation generator from the "Easter egg" in the Mutable Instruments [Streams](http://mutable-instruments.net/modules/streams) module, to which [Rössler attractors](https://en.wikipedia.org/wiki/Rössler_attractor) have been added. 

Two independent function generators are provided (referred to here as Generator 1 and Generator 2), with each generator calculating *both* the Lorenz and Rössler functions simultaneously, using the same phase accumulator, but with the rate/speed of each generator independently settable. Both the Lorenz and the Rössler functions output three values (_x_, _y_ & _z_), and various combinations of these can be mapped to the four output channels. The chaotic strange attractors work best as slow modulation functions.

### Controls 

|       | Left Encoder                                                            | Right Encoder                                                                                                     |
| ----- | ----------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| TURN  | Increase or decrease frequency of currently selected generator (1 or 2) | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited. |
| PRESS | Toggle currently selected generator frequency control                   | Toggle between menu navigation (selection) mode and value editing mode                                            |
| LONG  | Reset to defaults                                                       | App selection menu                                                                                                |

|            | Up Button                                                  | Down Button                                               |
| ---------- | ---------------------------------------------------------- | --------------------------------------------------------- |
| PRESS      | Increment frequency of currently selected generator  by 32 | Decrement frequency of currently selected generator by 32 |
| LONG PRESS |                                                            |                                                           |

### I/O

|     | 1                                                          | 2                                                          | 3                                                          | 4                                                                                      |
| --- | ---------------------------------------------------------- | ---------------------------------------------------------- | ---------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| TR  | Reset phase of generator 1                                 | Reset phase of generator 2                                 | Rest phase of both generators                              | Freeze (both generators stop in their tracks and hold current value while TR4 is high) |
| CV  | Frequency/speed of generator 1                             | Rho or c parameter for generator 1                         | Frequency/speed of generator 2                             | Rho or c parameter for generator 2                                                     |
| OUT | Mappable outputs from the two generators (see table below) | Mappable outputs from the two generators (see table below) | Mappable outputs from the two generators (see table below) | Mappable outputs from the two generators (see table below)                             |

Note that the output voltage range of the O+C module is asymmetrical (about -3V to +6V) - because it was designed to process pitch CVs. Therefore the output of the _Low-rents_ app is not centred about 0V.

### Settings

| Setting   | Meaning                                                                                |
| --------- | -------------------------------------------------------------------------------------- |
| `Freq 1`  | Frequency/speed of generator 1, range is 0-255                                         |
| `Freq 2`  | Frequency/speed of generator 2, range is 0-255                                         |
| `Rho/c 1` | Rho (for Lorenz attractor) or c (for the Rössler attractor) parameters for generator 1 |
| `Rho/c 2` | Rho (for Lorenz attractor) or c (for the Rössler attractor) parameters for generator 2 |
| `out A`   | output mapping for output A. Available choices shown in the table below.               |
| `out B`   | output mapping for output B. Available choices shown in the table below.               |
| `out C`   | output mapping for output C. Available choices shown in the table below.               |
| `out D`   | output mapping for output D. Available choices shown in the table below.               |


| Output mapping value | Meaning                                                                                            |
| -------------------- | -------------------------------------------------------------------------------------------------- |
| `Lx1`                | Generator 1 Lorenz attractor _x_ value                                                             |
| `Ly1`                | Generator 1 Lorenz attractor _y_ value                                                             |
| `Lz1`                | Generator 1 Lorenz attractor _z_ value                                                             |
| `Lx2`                | Generator 2 Lorenz attractor _x_ value                                                             |
| `Ly2`                | Generator 2 Lorenz attractor _y_ value                                                             |
| `Lz2`                | Generator 2 Lorenz attractor _z_ value                                                             |
| `Rx1`                | Generator 1 Rössler attractor _x_ value                                                            |
| `Ry1`                | Generator 1 Rössler attractor _y_ value                                                            |
| `Rz1`                | Generator 1 Rössler attractor _z_ value                                                            |
| `Rx2`                | Generator 2 Rössler attractor _x_ value                                                            |
| `Ry2`                | Generator 2 Rössler attractor _y_ value                                                            |
| `Rz2`                | Generator 2 Rössler attractor _z_ value                                                            |
| `Lx1+Rx1`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 1 Rössler attractor _x_ value          |
| `Lx1+Rz1`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 1 Rössler attractor _z_ value          |
| `Lx1+Ly2`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 2 Lorenz attractor _y_ value           |
| `Lx1+Lz2`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 2 Lorenz attractor _z_ value           |
| `Lx1+Rx2`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 2 Rössler attractor _x_ value          |
| `Lx1+Rz2`            | Sum of Generator 1 Lorenz attractor _x_ value and Generator 2 Rössler attractor _z_ value          |
| `Lx1xLy1`            | Bit-wise XOR of Generator 1 Lorenz attractor _x_ value and Generator 1 Lorenz attractor _y_ value  |
| `Lx1xLx2`            | Bit-wise XOR of Generator 1 Lorenz attractor _x_ value and Generator 2 Lorenz attractor _x_ value  |
| `Lx1xRx1`            | Bit-wise XOR of Generator 1 Lorenz attractor _x_ value and Generator 1 Rössler attractor _x_ value |
| `Lx1xRx2`            | Bit-wise XOR of Generator 1 Lorenz attractor _x_ value and Generator 2 Rössler attractor _x_ value |

The Rho and c parameters for the Lorenz and Rössler attractors respectively determine the degree of variability in the chaotic generator system. Note that the values have been constrained so that the functions do not collapse, but some combinations of extreme settings may cause the generator functions to collapse completely. If this happens, change the Rho/c setting and send a rest pulse to the relevant trigger input to reset the function generator.

### Screensaver display
The screensaver show the A and B outputs in a vectorscope (X/Y) display on the left half of the screen, and the C and D outputs as a vectorscope display on the right half of the screen.
 
*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
