---
layout: default
---
# TwoRings

![DualTM Screenshot](images/DualTM.png)

Adapted from the original [**ShiftReg**](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Shift-Register-(formerly-Turing)) applet, this pair of 32-bit shift registers is designed to be a versatile source of generative sequences. Digital inputs 1 & 2 are still _Clock_ & _p-gate_, respectively. The CV inputs and outputs are assignable.

### I/O

|        |                                 1/3                                 |    2/4     |
| ------ | :-----------------------------------------------------------------: | :--------: |
| TRIG   |                                Clock                                |   p Gate or Reset |
| CV INs | Assignable<br>(Slew, Length, Probability, Quantizer, Range, Transpose, Crossfade) | Assignable |
| OUTs   |                 Assignable (Pitch, Mod, Trig, Gate)                 | Assignable           |


The **Slew** parameter allows extreme smoothing on the output stage for portamento and gentle modulation. It acts as a Decay tail on the Trigger output modes. With modulation of Slew, you can have CV control over decay envelopes, or variable portamento.

## Parameters:
* Length - how many bits are looped in the registers
  - AuxButton toggles shift direction
* p=Probability (%) - when unlocked with the cursor or a gate input on TR2, how likely the current bit will be flipped
  - AuxButton toggles Reset capture mode - TR2 Resets to captured register states
* Quantizer select - for pitch quantization (AuxButton to edit)
* Range - how many discrete notes, in scale degrees (only applies to pitches)
* Slew - smoothing parameter (hybrid linear/logarithmic function)
* Input modes CV1 and CV2 - see below
* Output modes A: and B: - see below

## Output Modes
Each output can be assigned to one of the following:
* Pitch 1 - derived from 8 bits of the FIRST register
* Pitch 2 - derived from 8 bits of the SECOND register
* Pitch 1+2 - a blend of the two pitches, optionally crossfaded via CV
  - in 1.5.x, it was actually the SUM of the two pitches; changed in v1.6
* Mod 1 - unquantized bipolar modulation from FIRST register
* Mod 2 - unquantized bipolar modulation from SECOND register
* Trigger 1 - output trigger pulse when current bit is 1 on the FIRST register
* Trigger 2 - output trigger pulse when current bit is 1 on the SECOND register
* TrigPitch 1 or 2 - combination of Trigger and Pitch output modes
  - a varying quantized voltage with a high pulse at the beginning, optional decay from Slew
  - (useful for pinging filters or oscillators for pitched percussion or PEW PEW noizes)
* Gate 1 - hold high if current bit is 1 on FIRST register
* Gate 2 - hold high if current bit is 1 on SECOND register
* Gate 1+2 - two-level gate output from the sum of the current bits

## Input Modes
Each CV input can be assigned to modulate (bi-polar) one of the following:
* Slew
* Length
* p - probability
* Q - Quantizer select
* Range
* Transpose 1 - offset Pitch 1 (basic voltage adder)
* Transpose 2 - offset Pitch 2
* Crossfade 1+2 - Affects the combo Pitch 1+2 Output mode only
  - at 0V or unpatched, the two pitches are averaged
  - Positive voltage fades toward Pitch 2; negative voltage toward Pitch 1 (i think, lol, somebody should check my math)

### Credits
Authored by djphazer. Adapted from [ShiftReg](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Shift-Register-(formerly-Turing)) by Chysn, with mods by benirose.

Inspired by the original [Turing Machine](https://www.musicthing.co.uk/Turing-Machine/) by Music Thing Modular / Tom Whitwell.
