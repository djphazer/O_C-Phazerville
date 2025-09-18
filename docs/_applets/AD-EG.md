---
layout: default
---
# AD EG

![AD EG screenshot](images/AD-EG.png)

**AD-EG** is an attack/decay envelope generator with forward and reverse trigger inputs, and end-of-cycle trigger output.

### I/O

|        |             1/3             |                  2/4                  |
| ------ | :-------------------------: | :-----------------------------------: |
| TRIGs  |  Triggers the AD envelope   | Triggers the envelope in reverse (DA) |
| CV INs | Attack modulation (bipolar) |      Decay modulation (bipolar)       |
| OUTs   |          Envelope           |         End of Cycle trigger          |


Note that triggering the envelope in reverse does not change the CV modulation controls, meaning that CV 1 controls the effective "attack" segment regardless of direction.

### UI Parameters
* Attack duration
* Decay Duration 

### Credits
Adapted from [AD EG](https://github.com/Chysn/O_C-HemisphereSuite/wiki/AD-EG) © 2018-2022, Jason Justian and Beige Maze Laboratories. 
