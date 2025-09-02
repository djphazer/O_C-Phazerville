---
layout: default
---
# Neural Net

Neural Net is a highly-configurable logic processor with six Neurons, each of which can contain one of [11 different logic gates](#logic-gate-reference), including a *Threshold Logic Neuron*. Sources for each logic gate can be any of the eight inputs, any of the six Neurons, or a consistent ON/OFF value.

Four Setups can be saved for later use, and Setups can be saved and loaded via MIDI system exclusive.

### I/O

|     | 1                         | 2                         | 3                         | 4                         |
| --- | ------------------------- | ------------------------- | ------------------------- | ------------------------- |
| TR  | **DIG 1** - Logic Gate IN | **DIG 2** - Logic Gate IN | **DIG 3** - Logic Gate IN | **DIG 4** - Logic Gate IN |
| CV  | **CV 1** - Logic Gate IN  | **CV 2** - Logic Gate IN  | **CV 3** - Logic Gate IN  | **CV 4** - Logic Gate IN  |
| OUT |                           |                           |                           |                           |

|       | Left Encoder                                                                                | Right Encoder                                                       |
| ----- | ------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| TURN  | Specifies what is being edited. It can be any of the six Neurons, or the Output assignments | Changes the value at the cursor                                     |
| PRESS | Alternates between the Selection Screen and the Edit Screen                                 | Advances the cursor                                                 |
| LONG  | Turn on the All Connections screen                                                          | Go to the main menu; current Setup's data is also dumped via SysEx. |

|            | Up Button                                | Down Button                                 |
| ---------- | ---------------------------------------- | ------------------------------------------- |
| PRESS      | Selects which Setup (from 1-4) is active | Selects which Setup (from 1-4) is active    |
| LONG PRESS |                                          | Active Setup cab be copied to another Setup |

Neural Net has two basic setup screens, the [Selection Screen](selection-screen) (which shows all six Neurons), and the [Edit Screen](#edit-screen) (which allows editing and monitoring of a single Neuron). The controls work exactly the same way on both screens, but some changes are only visible from the [Edit Screen](#edit-screen).

## selection-screen
The *Selection Screen* shows an overview of all six Neurons, all inputs, and all outputs. Use the left encoder to choose a different Neuron, or the outputs.

The inputs are on the left side, with the Digital inputs (1-4) under the D, and the Control Voltage inputs (1-4) under the V. When an input is high, its corresponding number's display is reversed.

The outputs (A-D) are on the right side. When an output is high, its corresponding letter's display is reversed. If the outputs are selected, a blinking line will appear to the left of the outputs.

The six Neurons are in the center, with Neurons #1, #3, and #5 at the top, and Neurons #2, #4, and #6 at the bottom. Each Neuron shows the symbol of its selected logic gate. If a Neuron is selected, the logic gate type's name will blink, and lines from the Neuron's source(s) and dotted lines to assigned outputs (if any) will be displayed.

**Note**: When a new Neuron is selected, the Neuron's editing cursor is reset to the logic gate type. This means that you can use the right encoder to set the gate type without having to go to the Edit screen.

**Another Note**: Actually, you never _have_ to go to the edit screen. Since the controls work the same way on both screens, you can cursor through the parameters and change everything right from the Selection Screen. But it's hard to do that blind, and you'll probably want to go to the Edit Screen.

Long-press the left encoder to turn on All Connections. This will show source and assignment lines for all Neurons and outputs. Long-press the left encoder again to turn off All Connections.

## edit-screen
Press the left encoder button to alternate between the Selection Screen and the Edit Screen.

The Edit Screen is split into two halves. The left half of the screen shows the logic gate type and the Neuron's parameters. The right half of the screen shows the logic gate symbol, along with its inputs and output state. For the Threshold Logic Neuron, the right half of the screen also allows editing of weights and threshold.

Press the right encoder button to advance the cursor, and turn the right encoder to change the value. The following sources are available: **Dig 1, Dig 2, Dig 3, Dig 4, CV 1, CV 2, CV 3, CV 4, Neuron 1, Neuron 2, Neuron 3, Neuron 4, ON, and OFF**.

### Edit Screen Parameter Names

The meanings of the parameters you'll see on the Edit Screen are below. See [Logic Gate Reference](#logic-gate-reference) for details about what the settings actually do.

- **Opd**: Source of an operand of a logical operation (NOT, AND, OR, XOR, etc.). All operands in a logical gate are "commutative," meaning that the resulting state will be the same regardless of the order of the operands.
- **Data**: Source of the Data input for the Data Flip-Flop.
- **Toggl**: Source of the Toggle input for the Toggle Flip-Flop.
- **Clock**: Source of the Clock input for the Flip-Flop.
- **Set**: Source of the Set input for the Set-Reset Latch.
- **Reset**: Source of the Reset input for the Set-Reset Latch.
- **Den 1-3**: Source of a Dendrite input for the Threshold Logic Neuron.

### Output Assignment

When the outputs are selected, the Edit Screen shows Outputs A-D, with a Neuron assigned to each. The state of the selected Neurons will be sent to the specified outputs.

## Logic Gate Reference

- **NOT** is a unary (single-input) gate whose state is the opposite of its operand.
- **AND** is a binary (two-input) gate whose state is ON if and only if both of its operands are ON.
- **OR** is a binary gate whose state is ON if and only if at least one of its operands are ON.
- **XOR** is a binary gate whose state is ON if and only if exactly one of its operands is ON.
- **NAND** is a binary gate whose state is ON if and only if at least one of its operands are OFF.
- **NOR** is a binary gate whose state is ON if and only if both of its operands are OFF.
- **XNOR** is a binary gate whose state is ON if and only if both of its operands are the same value.
- **D-FF** is a _Data Flip-Flop_. When its Clock input goes high, its state becomes the value of its Data input.
- **T-FF** is a _Toggle Flip-Flop_. When its Clock input goes high, its state flips if the value of its Toggle input is ON.
- **Latch** is a _Set-Reset NOR Latch_. When its Set input goes ON, its state becomes ON. When its Reset input goes ON, its state becomes OFF.
- **TL Neuron** is a Threshold Logic Neuron, a ternary (triple-input) gate whose state is ON if and only if the sum of the weights of ON inputs (dendrites) exceeds its specified threshold. [See here](Threshold-Logic-Neuron) for more information about Threshold Logic Neurons.

## Setups

Neural Net has four independent Setups. A Setup consists of information for Neuron logic gate assignments, source assignments, and output assignments. Use the Up and Down buttons to change the active Setup. The active Setup number is shown at the top of all of the Setup screens.

### Copying Setups
To copy the information for the active Setup to another Setup, long-press the Down button. When you release the button, a Copy screen will open. Choose the copy's destination with the Up and Down buttons, and then push the right encoder ("[Copy]") to execute the copy. To leave the Copy screen without changing any data, push the left encoder ("[Cancel]") or long-press the Down button again.

If you select a Setup as its own copy destination, the display will change to a SysEx dump screen. The right encoder option becomes "[Dump]". If you push the right encoder, Neural Net will send a system exclusive file containing the data for the active Setup.

### Saving Setups
There are two ways to save Setup data.

**Save all four Setups in the module:** Long-press the left encoder to get to the main menu, then long-press the left encoder again to save data for all apps. A solid rectangle will expand from the middle of the screen to confirm the save operation. The Ornament and Crime module will recall the saved Setups on power-up.

**Save using MIDI system exclusive:** Follow the SysEx dump procedure from the Copying Setups section above, and capture the system exclusive dump with SysEx librarian software. The restore a Setup from a SysEx dump, simply send the SysEx file back to the module whenever Neural Net is running.

_Shortcut: When you long-press the right encoder to go to the main menu, the current Setup's data also is dumped via SysEx._

Notes about System Exclusive:

- A Setup from a received SysEx dump will always be placed in the active Setup, _regardless of the Setup's original location._
- You can use system exclusive to change Neural Net in an automated way during a performance.
- Data received from a SysEx dump will overwrite Setups in memory, but not data saved via the first save method; that is, unless you long-press to save the new Setup data, Neural Net will return to its previously-saved state at the next power-up.

© 2018-2022, Jason Justian and Beige Maze Laboratories. This wiki's text is under the MIT License as described under License on the Home page.
