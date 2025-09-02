---
layout: default
---
# TL Neuron

![Screenshot 2024-06-13 15-45-50](https://github.com/djphazer/O_C-Phazerville/assets/109086194/015773fd-7d0d-4646-95d3-5ee0a960b381)

[Video demo](https://youtu.be/NdHY-eDipkY)

**Threshold Logic Neuron** is a three-input programmable logic gate.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |   Dendrite 1 (True if high)  |  Dendrite 2 (True if high)   |
| CV INs |  Dendrite 3 (True if > 5v)   |   None  |
| OUTs   |  Gate if sum of Dendrite weights > Axon threshold   |  Same   |


## UI Parameters
* Weights (dendrites 1, 2, 3)
* Axon threshold


Threshold Logic Neuron is a three-input/single-output logic gate. Each of three inputs ("Dendrites") is given a weight (in the case of Hemisphere, a weight of between -9 and 9). The output ("Axon") is given a threshold (in Hemisphere, between -27 and 27). When the sum of the weights of high inputs exceeds the threshhold, the output goes high.

The neuron can be used to reproduce common logic gates; for example:

* Dendrite 1: w=3
* Dendrite 2: w=3
* Dendrite 3: w=3
* Axon Output: t=8

This reproduces and AND gate, because all Dendrites need to go high (for a sum of 9) to exceed the threshold of 8.

* Dendrite 1: w=3
* Dendrite 2: w=3
* Dendrite 3: w=3
* Axon Output: t=2

This reproduces an OR gate, because only one Dendrite needs to go high to exceed the threshold of 2.

The values can also be negative. So

* Dendrite 1: w=-3
* Dendrite 2: w=-3
* Axon Output: t=-5

This is a two-input NAND gate, because the output is high (because 0>-5) unless both Dendrite 1 and 2 are high, which brings the sum below the threshold.

But you're not limited to reproducing standard Boolean operations. You can create your own logical operations. For example, you can do this:

* Dendrite 1: w=2
* Dendrite 2: w=2
* Dendrite 3: w=5
* Axon Output: t=3, and mult the output into Dendrite 3

This creates a type of state memory: when Dendrites 1 and 2 go high, the output goes high (2+2>3), and the output is sent back to Dendrite 3, which forces the output to remain high, whatever happens to Dendrites 1 and 2 later.

A variation of that theme is

* Dendrite 1: w=2
* Dendrite 2: w=-2
* Dendrite 3: w=2
* Axon Output: t=1, and mult the output into Dendrite 3

Now, Dendrite 1 fires the axon, which feeds back into itself. Dendrite 1 can go low again and the output stays high. But a high signal at Dendrite 2 will reset the memory unless Dendrite 1 is still on.
