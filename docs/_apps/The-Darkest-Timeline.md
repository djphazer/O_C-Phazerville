---
layout: default
---
# The Darkest Timeline

The Darkest Timeline is a CV/MIDI sequencer application for Ornament and Crime. It is included in the Hemisphere Suite firmware from version 1.4.

## Functional Concepts

### Timelines

Two timelines, each of up to 32 steps, run simultaneously. The timeline at the top of the screen is the Pitch Timeline. The timeline at the bottom of the screen is the Probability Timeline.

### Pitch Timeline

The Pitch Timeline controls pitch data. The CV output can be quantized to a specific scale and root note. The MIDI output sends Note On and Note Off messages whose notes are based on the Pitch Timeline. The Pitch Timeline is shown on the top half of the screen, and each step on the timeline is shown as a vertical bar, with taller bars corresponding to higher pitches (or voltages).

### Probability Timeline

The Probability Timeline controls the probability of trigger CV firing and of MIDI Note On events being sent. For MIDI notes, the Probability Timeline also determines the velocity of the Note On messages. The Probability Timeline is shown on the bottom half of the screen, and each step on the timeline is shown as a vertical bar, with taller bars corresponding to greater probability of the event being fired (from 0% to 100%).

### Length

_Set the sequence length by turning the left encoder._

The sequence has a single length value, from 1 to 32 steps. If the sequencer is advanced past the last step, it will return to the first step of the sequence (see "Index" below). If the sequencer is running in reverse, and is reversed past the first step, it will return to the last step of the sequence.

### Index

_To set the index, press the right encoder to enable the index cursor (a flashing vertical line), and then turn the right encoder to change the index. The index can also be set by providing positive voltage to CV Input 3_

Most sequencers have adjustable sequence lengths, but the sequence usually starts at the first step. The Darkest Timeline allows you to choose the sequence's first step, or index, either from the panel or via CV. When the sequence is reset, it will go back to the index. When the sequence is advanced past its last step, it will go back to the index.

The index is shown as a dashed vertical line. Pressing the right encoder button will toggle the right encoder's function between scrubbing through the sequence and setting the index.

## Outputs

### Pitch Timeline Universe

Outputs A and B are connected to the Pitch Timeline. Output A is the pitch of the left-most step on the display. This is the "Normal Universe" output.

Output B is the "Parallel Universe," which is the corresponding step in the set of steps _after_ the last step in the sequence. Okay, that sounds confusing, so here's an example. Let's say you have a length of 8, and your index is step 1. So your Normal Universe is steps 1, 2, 3, 4, 5, 6, 7, and 8. Meanwhile, Output B will play steps 9, 10, 11, 12, 13, 14, 15, 16.

If the length is greater than 16, the Normal and Parallel Universes will share steps. If the length is 32, the Normal and Parallel Universes will be identical.

### Probability Timeline Universes

Outputs C and D are connected to the Probability Timeline. When the sequencer is advanced to a step, a probability is calculated based on the value in the Probability Timeline. Output C emits a trigger based on this probability; the higher the bar, the higher the probability of C emitting a trigger. At its maximum, the trigger will always fire. At its minimum, the trigger will never fire. This is the "Normal Universe" probability output.

Output D is the "Alternate Universe," which is a trigger based on the _complementary_ probability of the Probability Timeline; if a trigger has a 70% chance of firing in the Normal Universe, it has a 30% chance of firing in the Alternate Universe. The probabilities for both universes are calculated independently, so it's possible for both outputs to fire, or both outputs to not fire on the same step.

### MIDI Output Universes

_To set MIDI channels, press the left encoder button to enter the Setup screen, and keep pressing the button until you get to the MIDI settings. Then use the left encoder to change the channel._

The Darkest Timeline may send notes independently on up to two MIDI channels. The first of these is the Normal Universe, which is a Note On message that may (or may not) be sent, depending on the probability in the Probability Timeline.

The other MIDI out channel is the Alternate Universe, which uses the Parallel Universe's Pitch Timeline values and the Alternate Universe's Probability Timeline values.

In both cases, the velocity of the Note Out message is proportional to the Probability Timeline value used for the calculation.

### Setup Screen

_To access the Setup screen, press the left encoder button._

The Setup screen allows you to set the tuning and MIDI settings. Use the left encoder button to move through the settings (and back to the main screen), and use the left encoder to change the selected value. After a short period of inactivity on the Setup screen, you'll be returned to the main screen. Available settings are:

* **Scale**: Chooses one of the User or built-in scales. This will be used to quantize the CV outputs. It does not affect MIDI output.
* **Root**: Sets the diatonic root note based on the selected scale. This will be used to quantize the CV outputs. It does not affect MIDI output.
* **MIDI Out**: Sets the MIDI channel for the Normal Universe MIDI output, as described above. If this is set to Off, there will be no MIDI notes sent from that universe.
* **MIDI Out Alt**: Sets the MIDI channel for the Alternate Universe MIDI output, as described above. This this is set to Off, there will be no MIDI notes sent from that universe.
* **MIDI In**: Determines whether the recording source is CV or MIDI. If you're recording from CV, this should be set to "Off." Otherwise, input for recording will be expected on the specified channel.
* **Gate/Trigger**: Determines whether the Probability Timeline (outputs C and D) will send a 6ms trigger, or a gate that's a percentage of the clock period.

## Playback Controls

### CV Control of Playback

Inputs:
* Digital 1: A trigger will advance the sequencer forward (by default) or backward
* Digital 2: When gated, a trigger at Digital 1 will move the sequencer backward
* Digital 3: Reset the sequencer to index
* Digital 4: When gated, all probabilities are 100%
* CV 1: CV record value for Pitch Timeline
* CV 2: CV record value for Probability Timeline
* CV 3: Set the index (0V ~ 5V)
* CV 4: Transpose by adding positive or negative voltage to the Pitch timeline

Outputs:
* Output A: CV: Pitch Timeline, Normal Universe
* Output B: CV: Pitch Timeline, Parallel Universe
* Output C: Trigger: Probability Timeline, Normal Universe
* Output D: Trigger: Probability Timeline, Alternate Universe (complementary probability of Output C)

* Encoder: Set A4 frequency
* Encoder push: Reset tuner
### Panel Control of Playback

* Left Encoder: Change sequence length (clockwise to decrease, counterclockwise to increase) from 1 to 32 steps
* Press Right Encoder: Toggle Index editing
* Right Encoder with Index editing off: Scrub through the sequence. If the sequencer is not being clocked with CV 1, calculate probabilities and potentially fire triggers.
* Right Encoder with Index editing on: Change the index. If the sequencer is not being clocked with CV 1, calculate probabilities and potentially fire triggers.

## Recording

_Recording is enabled independently for each timeline. Press the Up button to enable recording for the Pitch Timeline, and the Down button to enable recording for the Probability Timeline. Pressing the button a second time will disable recording on the corresponding timeline._

### CV Recording

If the MIDI In channel is not set on the Setup screen, recording is done via CV. Connect CV sources to CV 1 (for recording into the Pitch Timeline) and/or CV 2 (for recording into the Probability Timeline), and enable recording on one or two timelines, as specified above. When recording is enabled for a timeline, the leftmost step will display a flashing cursor.

When recording is enabled, and set to record CV, a small "CV" icon will appear at the top of the screen.

To record data to the cursor point(s), set the CV at the corresponding input(s), and then advance the sequencer. You can advance the sequencer with a trigger to Digital 1, or by turning the right encoder.

### MIDI Recording

If the MIDI In channel is set on the Setup screen, recording is done via MIDI. Connect your O_C module to a MIDI host (usually a computer), choose "Hemisphere" as the MIDI device, and set the output channel to correspond to The Darkest Timeline's MIDI In channel. When recording is enabled for a timeline, the leftmost step will display a flashing cursor.

When recording is enabled, and set to record MIDI, a small MIDI DIN icon will appear at the top of the screen.

To record data to the cursor point(s), play a note. A value proportional to the note number will be recorded to the Pitch Timeline (if recording is enabled), and a value proportional to the velocity will be recorded to the Probability Timeline (if recording is enabled). The sequencer will advance automatically with each note.

_Please note that a trigger at Digital 1 will also advance the sequencer, so you probably want to turn off these triggers while recording MIDI data._

## Saving The Darkest Timeline Data

The Darkest Timeline's data can be saved in two ways:

(1) **With the built-in Ornament and Crime data storage system** Return to the main menu by long-pressing the right encoder. Then, long-press the right encoder again. This will save the state of all applications in the module.

(2) **System Exclusive Librarian** Your current sequence (both Timelines, length, index, scale, and root) can be dumped to a system exclusive librarian program on a computer for (somewhat) permanent storage. When you long-press the right button to return to the main menu, the sysex dump will be initiated. When setting up your librarian software, select Hemisphere as the MIDI device.

The Darkest Timeline sends your sequence as five sysex messages, so make sure that your librarian captures all of them and stores them in the same file.

To restore your sequence, simply send the dump back any time The Darkest Timeline is running.

**Compatibility Note:** The Darkest Timeline 2.0 is fully-compatible with sysex files from the previous version. The only difference is that scale and root will not be changed.

## Misc. Controls

* Long-Press Left Encoder: Randomize both timelines.
* Long-Press Down Button: Clear both timelines.
