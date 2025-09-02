---
layout: default
---
# Carpeggio

![Screenshot 2024-06-13 14-31-57](https://github.com/djphazer/O_C-Phazerville/assets/109086194/291b577c-c139-4e04-81db-a900f47de58b)

**Carpeggio** is an arpeggiator/sequencer that uses coordinates on a 4x4 Cartesian plane to select notes. Notes can be assigned to the plane from among 55 chord patterns. Carpeggio can also be clocked and reset, to function as a more traditional arpeggiator. Additionally, any of the 16 notes may be edited on the fly, allowing Carpeggio to function as a Cartesian or traditional 16-step sequencer.

### I/O

|        |                 1/3                  |                  2/4                  |
| ------ | :----------------------------------: | :-----------------------------------: |
| TRIG   | Advance to the next step (x, then y) |            Reset to (1,1)             |
| CV INs |              X position              |              Y position               |
| OUTs   |          Quantized pitch CV          | Modulation output proportional to x*y |

### UI Parameters
* Chord
* Transposition
* Toggle random/linear note order
* Edit current note

_Note: edited notes are shuffled when random note order is toggled. However, returning to linear note order erases any note edits._

### Cartesian Operation
Provide x and y CV (0-5V) to the CV inputs. The position will be displayed with cross hairs on the grid. To select the note at the current position for playing, provide a clock signal to Digital 1.

### Linear Operation
To use Carpeggio as a more traditional linear arpeggiator or sequencer, disconnect the x and y CV (or set them both to 0V). A clock to Digital 1 will play the note at the current position, and then advance to the next position, from left to right, and top to bottom.

### Note Editing
When the cursor is flashing under the note number near the bottom of the screen, the note at the current step may be edited with the encoder. Such edits will be lost on power-down, or when the chord is changed.

### Changing Chords
When the cursor is flashing under the chord name, select a chord with the encoder. If you change the chord and push the encoder, the sixteen steps will be filled with notes from that chord, removing any note edits you've made. When you're changing chords, a checkmark will appear next to the currently-selected chord. If you press the encoder button at this chord, your note changes will not be overwritten.

### Transposition
Carpeggio has a four octave transposition range, 24 semitones in either direction.

### Credits
_Thanks to Roel Das for writing the chord pattern code, and generously allowing it to be used here._

Adapted from [Carpeggio](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Carpeggio-Cartesian-Arpeggiator) © 2018-2022, Jason Justian and Beige Maze Laboratories.

Shuffled operation added by benirose
