---
layout: default
---
# Stairs

![Screenshot 2024-06-13 15-41-28](https://github.com/djphazer/O_C-Phazerville/assets/109086194/bdc69fdb-906f-48fe-bd73-89843b0b4890)

**Stairs** is a stepped, clocked voltage generator based on the Noise Engineering Clep Diaz's 'step' and 'rand' modes. On each input clock pulse, the output voltage advances to the next 'step,' where the first step is always 0v and the last always 5v, with even voltage divisions on the intermediate steps.

## I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock    | Reset (can be held)    |
| CV INs | Number of Steps    | Current Step (overrides Clock if > 0v)  |
| OUTs   | Stepped CV output 0-5v    | Beginning-of-Cycle trigger    |


## UI Parameters
- Steps
- Direction (up, down, pendulum)
- Random (toggle)

### Steps
Controls the total number of steps taken from 0 to 5v or 5v to 0 volts.

### Direction
Sets the direction the voltage changes in on each step: up, up+down, or down. This effectively gives up ramp, triangle, and down ramp shapes. The step count is effectively doubled in up+down mode, except that the bottom and top steps are not repeated when changing directions.

### Random
Turning random ON makes each step's voltage deviate a little bit from where it would be, but the overall direction of movement is preserved.

## Credits
Authored by [Logarhythm1](https://github.com/Logarhythm1/O_C-HemisphereSuite/tree/logarhythm-branch)
