---
layout: default
---
# Binary Counter

![Screenshot 2024-06-13 14-20-22](https://github.com/djphazer/O_C-Phazerville/assets/109086194/020223f3-ef53-4206-b10b-b66430f29853)

**Binary Counter** is weird. It takes four logic inputs (high or low) and provides a binary-summed output and a count output.

### I/O

|        |         1/3          |       2/4       |
| ------ | :------------------: | :-------------: |
| TRIG   |        Bit 3         |      Bit 2      |
| CV INs |        Bit 1         |      Bit 0      |
| OUTs   | Binary sum of inputs | Count of inputs |

**Binary Sum (Outputs A/C)**: each bit contributes a value based on its bit number. Approximate voltages are as follows:
* Bit 0 contributes .33V
* Bit 1 contributes .67V
* Bit 2 contributes 1.33V
* Bit 3 contributes 2.67V

**Count (Output B/D)**: Each bit contributes the same value (approximately 1.25V).

### UI Parameters
* None

### Credits
Adapted from [Binary Counter](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Binary-Counter-(Retired)) © 2018-2022, Jason Justian and Beige Maze Laboratories.
