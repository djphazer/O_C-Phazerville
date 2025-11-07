---
layout: default
---
# Scale Editor

*Scale Editor* is a standalone application for editing and managing user-defined microtonal scales, and importing scales from the internal scale library, or via system exclusive. (A web-based Scala-to-Hemisphere Suite converter was previously available...) [Video demo](https://www.youtube.com/watch?v=UKX79rkSdIQ)

### I/O

|        |                                                         1                                                         |                                      2                                      | 3   | 4   |
| ------ | :--------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------- | --- | --- |
| TR     |                                                                                                                   |                                                                             |     |     |
| CV INs |                                      CV 1 is input for unquantized pitch CV.                                      |                                                                             |     |     |
| OUTs   | The quantized pitch appears at Output A, using the currently-selected user scale. Pitch is quantized in real time | Quantized pitch for the currently-selected user scale and the selected note |     |     |

### Basic Controls and Navigation

|       | Left Encoder                                                                              | Right Encoder           |
| ----- | ----------------------------------------------------------------------------------------- | ----------------------- |
| TURN  | Changes the value of the currently-selected note by as little as 1.28 cents sharp or flat | Selects note or length. |
| PRESS | Undoes the most recent note value change                                                  |                         |
| LONG  | Sends a system exclusive dump of the current scale data.                                  |                         |

|            | Up Button               | Down Button                                            |
| ---------- | ----------------------- | ------------------------------------------------------ |
| PRESS      | Chooses the user scale. | Chooses the user scale.                                |
| LONG PRESS |                         | Long-pressing the Down button opens the Import screen. |

* Right Encoder: Push the right encoder button to toggle between those two functions. For Output B, note selection can be used to set the octave of the currently-selected note's output. On the Import screen, the right encoder chooses the scale, and the right encoder button executes the import.

## Choosing a User Scale
Press the Up and Down buttons to choose a scale (from USER1 to USER4).

## Selecting and Editing Notes
Use the right encoder to choose which note you're currently editing. The value of the note, in cents from the root, is shown at the bottom of the display.

Turn the left encoder to change the value by as little as 1.28 cents. Turn clockwise for sharp, and counterclockwise for flat. Note values cannot overlap or exceed adjacent nodes, so the range of each note will be constrained based on the settings of the next note.

## Undo
If you make a mistake, you don't need to remember the old value of the note. Press the left encoder button to undo a note value change. You can undo a change until you select a different note.

## Monitoring
You can hear the changes you're making in real time with two different monitoring methods. Both methods update pitch in real time as you make changes:

* Full Scale Monitoring: Patch unquantized CV into Input 1, and patch Output A into an oscillator. Output A will continuously quantize Input 1 according to the currently-selected scale. The octave played will correspond to the input.
* Selected Note Monitoring: Output B will continuously output the quantized value of the currently-selected note. You can change the note's octave (within a 5-octave range) by turning the right encoder beyond the first or last note.

## Changing Scale Length
User scales may contain between 4 and 16 notes. Press the right encoder button to enter the Scale Length screen. The display will show the number of notes. Now, use the right encoder to choose the number of notes. When you're done, press the right encoder to return to the Note Edit screen.

## Importing Scales
The Import screen allows you to copy any scale in the scale library to the currently-selected user scale. Long-press the Down button to enter the Import screen. Then use the right encoder to choose a scale. To import the specified scale into the current user scale, press the right encoder ("[IMPORT]"). If you change your mind, press the left encoder ("[CANCEL]").

## System Exclusive
You may initiate a MIDI system exclusive dump of the current scale by long-pressing the left encoder, or by long-pressing the right encoder to return to the main menu. You may capture the system exclusive dump with a sysex librarian program on a connected computer.

To restore a scale from a sysex dump, simply initiate the dump from the computer while Scale Editor is running. The received scale will be placed into the currently-selected scale.

## Scala Format Import
There was previously a web-based tool and an archive of SysEx files converted from the Scala Scale Archive... the Beige Maze website is now defunct. If anyone can recover these, please file an Issue on GitHub.
