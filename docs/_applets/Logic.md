---
layout: default
---
# Logic

![Logic Screenshot](images/Logic.png)

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
| Gate Name | Truth Table Pattern | Operation            | Description                                                 |
| -         | :-:                 | -                    | -                                                           |
| **FALS**  | 0000                | 0                    | Never True.                                                 |
| **AND**   | 0001                | A & B                | True when both inputs are True.                             |
| **INHA**  | 0010                | A & ~B               | "B inhibits A", True when A is True, except when B is True. |
| **BUFA**  | 0011                | A                    | "Buffer A", True when A is True.                            |
| **INHB**  | 0100                | ~A & B               | "A inhibits B", True when B is True, except when A is True. |
| **BUFB**  | 0101                | B                    | "Buffer B", True when B is True.                            |
| **XOR**   | 0110                | (A & ~B) \| (~A & B) | True when both inputs are different than the other input.   |
| **OR**    | 0111                | A \| B               | True when either or both inputs are True.                   |
| **NOR**   | 1000                | ~(A \| B)            | True when both inputs are False.                            |
| **XNOR**  | 1001                | (A & B) \| (~A & ~B) | True when both inputs are the same.                         |
| **NOTB**  | 1010                | ~B                   | True when B is False.                                       |
| **IMPA**  | 1011                | A \| ~B              | "B implies A", True unless B is True and A is False.        |
| **NOTA**  | 1100                | ~A                   | True when A is False.                                       |
| **IMPB**  | 1101                | ~A \| B              | "A implies B", True unless A is True and B is False.        |
| **NAND**  | 1110                | ~(A & B)             | True when either input is False.                            |
| **TRUE**  | 1111                | 1                    | Always True.                                                |


Another option, "-CV-" is available. When set to "-CV-" the logical gate type will be set via CV using the corresponding CV input.

### Credits
Adapted from [Logic](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Logic) by Chysn.

Expanded by beau-seidon to include full selection of 2-input gates, using [this](https://mathcenter.oxford.emory.edu/site/cs170/gates/) reference.
