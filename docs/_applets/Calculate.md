---
layout: default
---
# Calculate

![Calculate screenshot](images/Calculate.png)

**Calculate** can perform two separate arithmetical operations on two CV inputs: each outputting either the minimum voltage, the maximum, the sum, the difference, or the average. It can also function as a basic Sample & Hold or a Random CV noise source.


### I/O

|        |                             1/3                             |                  2/4                  |
| ------ | :---------------------------------------------------------: | :-----------------------------------: |
| TRIG   | Ch1 Clock (for Rnd or S&H modes) | Ch2 Clock (for Rnd or S&H modes) |
| CV INs |                          Operand A                          |               Operand B               |
| OUTs   |                Channel 1 calculation results                |     Channel 2 calculation results     |


### UI Parameters
* Operator A
* Operator B

The following operators are available:
* **Min:** The result is the lower voltage of the two operands
* **Max:** The result is the higher voltage of the two operands
* **Sum:** The result is the sum of the two operands, up to 5 volts
* **Diff:** The result is the absolute value of the difference between the two operands
* **Mean:** The result is the mean (average) of the two operands (that is, Sum divided by 2)
* **S&H:** When the channel is clocked, the output is held at the voltage of the corresponding input.
  - _Note: the input will still provide an unclocked operand for the other calculation_
* **Rnd:** A random value between 0 and 5 volts appears at the corresponding output.
  - When the channel is clocked, a random value is held at the corresponding output until the next clock. To reset the input (that is, to turn off clocked operation), place the cursor on the Rnd operator and turn the encoder. If you turn the encoder clockwise, clocked operation will be turned off without changing away from the Rnd operator. When Rnd is clocked, a clock icon will appear to the right of the operator selector.
  - When "Rnd" is selected for both channels, Clock on channel 1 is normalized to channel 2.
  - _Note: the corresponding input will have no effect on the random output, but will still provide an unclocked operand for the other calculation_

### Credits
Adapted from [Calculate](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Calculate) © 2018-2022, Jason Justian and Beige Maze Laboratories.
