---
layout: default
---
# Logic

![Screenshot 2024-06-13 15-06-36](https://github.com/djphazer/O_C-Phazerville/assets/109086194/40763bf9-02a5-4ea3-a42f-3ba4b55713f2)

https://youtu.be/mHZCaMH_Dgk

Logic is a two-input logic module that performs two logical operations at once.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Operand 1 | Operand 2 |
| CV INs | Gate type select for "-CV-" mode | Gate type for "-CV-" mode |
| OUTs   | Result Ch1 (high/low)  | Result Ch2 (high/low) |


### UI Parameters
* Logical gate type or "-CV-" for each channel

### Notes
The Digital Inputs act as two logical operands, in the form of gate signals with high being True and low being False.
CV Inputs 1 and 2 set the logical gate when the gate selected for the corresponding channel is "-CV-".
The results of the logical operations are output as a high (5 volt, when True) or low (0 volt, when False) signal.

Available gate types are:
* **AND**: True when both inputs are True
* **OR**: True when either or both inputs are True
* **XOR**: True when both inputs are different than the other input
* **NAND**: True when either input is False
* **NOR**: True when both inputs are False
* **XNOR**: True when both inputs are the same

Another option, "-CV-" is available. When set to "-CV-" the logical gate type will be set via CV using the corresponding CV input.

### Credits
Adapted from [Logic](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Logic) by Chysn.
