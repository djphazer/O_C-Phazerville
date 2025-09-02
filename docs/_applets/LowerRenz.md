---
layout: default
---
# LowerRenz

![Screenshot 2024-06-13 15-07-49](https://github.com/djphazer/O_C-Phazerville/assets/109086194/eb87fafc-5c79-4f52-9f03-a6c7baa09c10)

LowerRenz is a single Lorenz-only modulation generator based on the O_C's own Low-Rentz Dual Lorenz/Rössler Generator, which is itself based on an Easter Egg from Mutable Instruments Streams. It is a model of a 2-dimensional "[strange attractor](https://www.dynamicmath.xyz/strange-attractors/)", practically functioning as a random LFO which orbits 2 poles (the low and high ends of the voltage range), and occasionally flipping between them. Changing the value of "Rho" will have subtle effects on the stability of the output.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Reset generator   |  Freeze outputs (gate)   |
| CV INs |   Frequency modulation (bipolar)  |  Rho modulation (bipolar)   |
| OUTs   |  Lorenz X value   |  Lorenz Y value   |

### UI Parameters
 - Frequency
 - Rho
