---
layout: default
---
# Cumulus

![Screenshot 2024-06-13 14-37-59](https://github.com/djphazer/O_C-Phazerville/assets/109086194/4edace40-72bf-4203-8d70-90ee6e83169d)

Cumulus is a 8-Bit operation machine inspired by Nibbler from Schlappi Engineering.

Whenever the a gate goes high at 1/3 the applet performs an operations of the output accumulator (Z) and with the constant (k), the result is saved back into the accumulator (Z). The constant value can either be set by encoder using the menu oand/or modulated by the CV in 2/4. The value of A and B determine which bit of the accumulator is routed to the outputs. The value of A and B are set using the menu and A is modulated using CV 1/3.

### I/O

||1/3|2/4|
|---|:-:|:-:|
|Trig (Digital)|Execute Instruction|Randomize Accumulator|
|CV In|Modulation A location|Modulation k Value|
|Output|Bit at A's position|Bit at B's position|

### UI Parameters

- Instruction
	- `z+k`
	- `z-k`
	- `z*k+1`
	- `(z^k)<<1` (z xor k) and rotate left by 1
	- `(z-k)>>2` (z - k) and rotate right by 2
	- constant value k
- A location
- B location
- k value


Authored by Zerbian. In Phazerville from v1.7.1
