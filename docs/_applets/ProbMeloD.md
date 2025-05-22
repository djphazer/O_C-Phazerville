---
layout: default
---
# ProbMeloD

![ProbMeloD Screenshot](images/ProbMeloD.png)

**ProbMeloD** is a stochastic melody generator inspired by the [Melodicer](https://www.modulargrid.net/e/vermona-melodicer) and [Stochastic Inspiration Generator](https://www.modulargrid.net/e/stochastic-instruments-stochastic-inspiration-generator). It allows you to assign probability to each note in a chromatic scale, as well as set the range of octaves to pick notes from. It can be used by itself or with [ProbDiv](ProbDiv), which it will automatically link to.

ProbMeloD has 2 channels, which can output independently clocked pitch values based on the same note ranges and probabilities.

[See it in action!](https://www.youtube.com/watch?v=uR8pLUVNDjI)

### I/O

|        | 1/3        | 2/4          |
| ------ | :-:        | :-:          |
| TRIG   | Clock A    |  Clock B     |
| CV INs | Low range* |  High range* |
| OUTs   | Pitch A    |  Pitch B     |

*default params (see CV Mapping table below)

### UI Parameters
 - Low range
 - High range
 - Probability per note
 - Scale mask rotation (encoder adjusts in semitone increments)

## Probabilities

Probabilities are defined as weights from 0-10 for choosing a specific note. For example, if you only have a probability set for C it will always be selected no matter the probability. Or if you have a probability 10 for both C and D, it will be a 50/50 chance of either note being selected. Probability value can be thought of as the number of raffle tickets entered for each note when it’s time to pick a new one, which is any time a trigger is received on Digital 1.

## Range

ProbMeloD has a range of 5 octaves. Lower and upper range are displays in an _octave_._semitone_ notation, so a range of `1.1` to `1.12` will cover the entire first octave, or C through B if your oscillator is tuned to C.

## Pairing with ProbDiv and Looping

ProbMeloD does not have any looping functionality on its own, but can loop when paired with [ProbDiv](ProbDiv). Like ProbDiv, a new loop is generated in both applets when any parameter in ProbDiv or ProbMeloD are changed. If ProbDiv and ProbMeloD are loaded in each hemisphere, they will automatically link. Clock division outputs from ProbDiv will automatically trigger ProbMeloD, and ProbMeloD will capture a loop when ProbDiv is looping as well. When used together they can be treated as a whole probabilistic sequencer!

## Scale Mask Rotation

As of Phazerville Release v1.9, ProbMeloD supports scale mask rotation by scale degrees or in semitone steps, to simulate chord progressions and key signature changes.

#### Mask
Specific notes can be preserved or excluded as probablilites are rotated through the masked scale by CV input. This is similar to changing chords within a fixed key signature.

To exclude a note, set the probability to "-1" which will be indicated by an X, and the slider on the note will disappear. Excluded notes will never be played, just like included notes with probability 0 won't, but they will also be skipped when rotating the pattern of probabilites. The 0's will rotate through the included notes just like any other enabled scale degree, whereas excluded notes are fixed in place.

If you set your probabilities right (a clearly defined Root note is recommended), mask rotation responds intuitively to V/Oct keyboard input and MIDI (see [MIDI-Input](MIDI-Input) for help with configuration).

As a simple example patch, set the probabilities as desired, then send ProbMeloD output 2 to it's own "Mask" parameter, output 1 to an oscillator, and clock both channels at different rates. Output 1 should produce randomized arpeggios at the rate of trig input 1, and the chord shapes *may or may not change* at the rate of trig input 2.

#### Semi
The entire pattern of probabilites INCLUDING those not in the scale mask can be shifted by CV modulating "Semi" or manually by moving the encoder after pressing to enter edit mode with the cursor underneath the output indicar bar to the right of the keyboard. Change to a new key and press again to return to normal navigation.

CV modulate "Semi" in increments of 5 and 7 semitones (perfect IV and V) when sequencing, to maximize tonal pleasantness. Or don't, the rules were made up by people who are no longer around to enforce them.

## CV Mapping

CV inputs can be mapped to combinations of parameters as shown in the table below. Note the +, -, and * characters which indicate the secondary parameter modified by that input. When a parameter is modified by both inputs, the values are summed.

|               | Lower   | Upper   | Semi | Mask |
| ------------- | :-:     | :-:     | :-:  | :-:  |
| Lower / Upper | CV1     | CV2     | -    | -    |
| Semi  / Mask  | -       | -       | CV1  | CV2  |
| Semi  / Upper | -       | CV2     | CV1  | -    |
| Semi  / Lower | CV2     | -       | CV1  | -    |
| Semi  / Pos   | CV2     | CV2     | CV1  | -    |
| Mask  / Upper | -       | CV2     | -    | CV1  |
| Mask  / Lower | CV2     | -       | -    | CV1  |
| Mask  / Pos   | CV2     | CV2     | -    | CV1  |
| Semi+ / Mask  | -       | CV1     | CV1  | CV2  |
| Semi- / Mask  | CV1     | -       | CV1  | CV2  |
| Semi* / Mask  | CV1     | CV1     | CV1  | CV2  |
| Semi  / Mask+ | -       | CV2     | CV1  | CV2  |
| Semi  / Mask- | CV2     | -       | CV1  | CV2  |
| Semi  / Mask* | CV2     | CV2     | CV1  | CV2  |
| Semi* / Mask* | CV1+CV2 | CV1+CV2 | CV1  | CV2  |


## Credits
Copied/Adapted from [ProbMeloD](https://github.com/benirose/O_C-BenisphereSuite/wiki/ProbMeloD) by benirose.

Modified by beau.seidon, djphazer, and qiemem.