---
layout: default
---
# Enigma

Enigma is a Shift Register (a.k.a. "Turing Machine") Workstation, designed to enable (nearly) repeatable full compositions by curating a Turing Machine library and chaining Turing Machines into songs. Enigma is in Hemisphere Suite starting with v1.5.

## Workflow
Enigma's workflow involves four screens:

* **Library** contains 40 Turing Machines (hereafter called "Registers"). As you audition a Register in the Library, you may change its length, probability, and rotation. You may "lock" a Register by designating it as a Favorite.

* **Assign** allows you to choose the function of each of the Ornament and Crime's four outputs (A, B, C, D). Each output may be assigned to a source track, a type (note, modulation, gate, or trigger), a scale (for note types), and a MIDI channel (1-16, or Off). On the Assign screen, you may choose whether you're auditioning the Library or the current Song.

* **Song** allows you to chain Registers together on up to four tracks. Each track may contain up to 99 steps, for a total of 396 steps. Each step specifies a Register, the number of times that Register repeats, the probability of a bit flip at each step (see "How a Turing Machine Works," below), and the transposition of notes played.

* **Play** allows you to control the playback of the song by choosing clock divide and loop per track, resetting and starting the song. It also allows you to visually monitor the song's position.

## Navigation and Control Overview
The **LEFT encoder** chooses screens and screen-level selection. Each press of the LEFT encoder button moves to the next screen (in the order Library -> Assign -> Song -> Play). Turning the LEFT encoder selects items as follows:

* Library Screen: The Register (A-1 through E-8)
* Assign Screen: The output (A, B, C, D)
* Song/Play: The track (Track 1 through Track 4)

The **RIGHT encoder** chooses parameters and values on each screen. Each press of the **RIGHT encoder** button moves to the next parameter from left-to-right, top-to-bottom. Turing the RIGHT encoder changes the selected value.

The **UP and DOWN buttons** have different functions on each screen. Each screen will display the function of these buttons for a few seconds. You can dismiss this help by moving any control. During each session, Enigma will pay attention to how long you view the help text and will adjust the time accordingly. If you dismiss a help screen within one second, Enigma will stop showing them to you.

**Long-pressing the DOWN button** allows you to erase the current song and start over. You will be asked to confirm this action. On the confirmation screen, press the RIGHT encoder to erase the song, and the LEFT encoder to cancel.

**Long-pressing the LEFT encoder button** will send data related to the screen via system exclusive (SysEx) dump. Library, output assignment, and song data can be sent and received separately. Long-pressing the RIGHT encoder will return you to the main menu, and send all data via SysEx dump.

## How a Turing Machine Works
The original Turing Machine circuit, by Music Thing's Tom Whitwell, works like this: A random sixteen-bit number is generated. On an incoming clock signal, the binary representation of that number is shifted to the left by one bit. A probability check determines whether the last bit (bit 15) of the pre-shifted number is moved back to the beginning (bit 0) during this rotation, or it changes its value. Then, an output value is determined based on the new sixteen-bit number. Originally, this was the bottom eight bits scaled to a voltage output.

### How Enigma is Different
Enigma uses Turing Machine-like registers as its source material for composition. In a traditional Turing Machine or Turing Machine-like system, the starting register is different with each session. So if your Turing Machine hits upon a melody or modulation pattern that you like, you need to record it somehow, either with an audio recorder or CV recorder. The philosophy of Enigma is that Turing Machines can be durable and re-usable. When you find something you like, you can lock it in place and save it for later, and use it as part of a larger composition.

## Library
The Library is where you manage your Registers. Forty registers are arranged in five banks (A-E) of eight registers (1-8). Use the LEFT encoder to select a register. When you first select a register, it starts with a random sixteen-bit number.

Registers are stored with three pieces of information: The register value itself, the length, and the Favorite status of the Register. Two other controls in the Library (probability and rotation) are only used for shaping the Register.

_Note: The Library cannot be accessed or auditioned while a song is playing. If you're trying to get to the Library, but can't, go to the Play screen and stop the playback._

### Auditioning the Register
A clock signal into Digital 1 will advance the register and calculate a change based on the probability value. The CV at Outputs A-D will be updated. The voltage present at the outputs is determined by the output assignments (see the Assign screen). By default, the assignments are as follows:

* Output A: A note quantized to a semitone scale
* Output B: A modulation amount from 0V-5V
* Output C: A gate (goes high when bit 0 is on, and low when bit 0 is off, capable of spanning multiple advances)
* Output D: A trigger (fired when bit 0 is on)

### Favorites
If you find a melody, modulation pattern, or trigger pattern that you want to keep, press the UP button. This will mark the pattern as a Favorite (a heart icon will indicate this status for each pattern). When a pattern is a Favorite, the following things apply:

* Probability will no longer alter the pattern
* The pattern cannot be overwritten by single-Register SysEx dumps
* The pattern may now be reset using the DOWN button
* The pattern may now be rotated with the RIGHT encoder when the "rotate" icons are shown

When a Register is a Favorite, you may turn off Favorite status by pressing the UP button again. When you do this, the probability will always start out at 0%.

## Assign
The Assign screen is used to specify how the Ornament and Crime's outputs are used. You select the output with the LEFT encoder, and choose and change parameters with the RIGHT encoder. The parameters on the Assign screen, from top to bottom, are as follows:

* **Source:** Specifies which Track is routed to the output. A Track may be assigned to any number of outputs.
* **Type:** Specifies the type of CV that will be generated by the output. See the Types section below.
* **Scale:** Available only for note Types, chooses the scale to which the output is quantized
* MIDI Channel: Specifies the MIDI channel for the output, or Off

While you are on the Assign screen, you may audition from two sets of data, the Library or the Song. Press the UP button to audition from the Library (default), and the DOWN button to audition from the Song. If the song isn't playing, all CV outputs will be at 0V.

### Types
The following types of CV output are available, based on the current state of the Register that's being auditioned:

* **Note:** There are five depths of note data, from 3 bits to 7 bits. At 3-bit depth, melodies will be constrained to 8 notes of the selected scale. At 4-bit depth, 16 notes; at 5-bit depth, 32 notes; at 6-bit depth, 64 notes; and, at 7-bit depth, 128 notes (or the entire MIDI note range) is available. The note types look at the least-significant bits of the current Register.
* **Modulation/Expression:** The least-significant 8 bits are used to generate a modulation value that's scaled to 0-5 volts. Modulation and Expression are identical for CV, but generate different MIDI messages (see below).
* **Gate:** When bit 0 is 1, the assigned output emits 5V until the next time bit 0 is 0. That is, a high gate will span multiple rotations of the Register
* **Trigger:** When bit 0 is 1, a trigger pulse is emitted at the assigned output

When the MIDI channel is set (that is, it's not Off), the Type also determines the MIDI event that's generated:

* **Note:** Sends a Note Off for the output's previous note (if any), and a Note On message based on the specified bit depth. For depths of 3-6, the note is above Middle C (that is, if a three-bit value is 5, the MIDI note number is 65 (Middle C, 60, plus 5). For a depth of 7, the seven-bit value determines the raw MIDI note number. If you want to play notes in rhythm, then set the MIDI Channel of the output to "Off" and specify a MIDI Channel for the next Gate or Trigger output (see below). Note that MIDI output does not observe the Scale setting used by the CV output.
* **Modulation/Expression:** The least-significant 8 bits are scaled down to a seven-bit number (0-127) for sending a MIDI modulation or expression event. Modulation is a continuous controller number of 1, and Expression is a continuous controller number of 11, which is often used for relative volume changes.
* **Gate/Trigger:** If a Note value was calculated in a previous output, but the MIDI Channel is Off, then that note is considered "deferred." A deferred note will be played if a MIDI Channel is assigned to a subsequent Gate or Trigger output. For MIDI output, Gate and Trigger do the same thing.

## Song
Song mode allows you to chain Registers together into long (or short) compositions of up to four Tracks. This is done by creating a series of "Steps" on each Track.

Use the LEFT encoder to choose the Track (Track 1 - Track 4). The RIGHT encoder will cycle through the following settings:

* **Step Number:** The Step number cursor is a blinking highlighted number showing the current Step number. Use the RIGHT encoder to move through the existing steps.
* **Register:** Choose a Register (from A-1 to E-8). When the cursor is over the Register selection, the Favorite status and length of the Register will be shown to the right.
* **Probability:** Probability is checked each time the Register is advanced, allowing the Register to randomly change over time. Note that this only changes the Register on playback, and not in the Library, regardless of the Register's Favorite status. When a new step starts, each Register is returned to its original form in the Library.
* **Number of Plays:** A Register can be played between 1 and 99 times for a single Step.
* **Transposition:** For each Step, notes played from the Track can be transposed over an eight-octave range, from -48 semitones to +48 semitones. Transposition is in semitones, regardless of the scale used, allowing you to use this value to specify a diatonic root.

### Adding and Deleting Steps
A Track starts out with no Steps. To add a Step, press the UP button. This will insert a step _after_ the selected step. The default Register of your new step will be randomly selected from among your Favorite Registers. If you have no Favorites, the default will be A-1.

To delete a Step, press the DOWN button. This will delete the currently-selected step, and the next Step up will now be selected.

## Play
Songs may be played from the Assign or Song screens, but the Play screen provides some extra setup and playback control.

As with the Song screen, the RIGHT encoder is used to select the Track. This screen is formatted a bit differently, as a table with the following information:

* **Track Number**
* **Track Step Location:** Shows the Step number, and the number of times the Register has started playing during this step, after the semicolon
* **Clock Divide value:** Specifies how many clock pulses are required to advance the Register on this track
* **One Shot/Loop:** Specifies whether the Track plays once and stops, or loops. You can set up a short looping triggered rhythm that plays alongside a longer melody; or, a looping melodic ostinato that goes on throughout a composition.
* **Play Status:** The play icon, solid, indicates that the Track is playing. The play icon, blinking, indicates that the Track is paused, but will continue playback when the UP button is pressed. The stop icon indicates that the Track has run out of material to play.

Pressing the RIGHT encoder button alternates between the Clock Divide and the Loop setting on the selected channel.

On the Play screen, the UP button toggles between playback and pause. The current status is shown with an icon in the upper-right corner of the screen. The DOWN button resets the song to the beginning.

### CV Control
The Digital and CV inputs have the same function whenever a song is being played. These controls are as follows:

**Digital 1:** The master clock. Advances all Tracks, observing the Clock Division of each Track. _This is the only CV control that works in the Library_
**Digital 2:** Reset the song
**Digital 3:** Reset the song and start playback
**Digital 4:** Toggle between playback and pause
**CV 1:** Gate Song End. When a high signal is sent to this input, the song will stop playing when all non-looping Tracks have ended. This can be used to end a song for recording, without having to fade out.
**CV 2:** Gate Song Repeat: When a high signal is sent to this input, the song will start over from the beginning when all non-looping Tracks have ended. _When both CV 1 and CV 2 are un-gated, the default behavior is for looping Tracks to just continue looping until otherwise stopped._

## Storage
### Internal Storage
As with other Ornament and Crime applications, the state of the Ornament and Crime can be stored by long-pressing the RIGHT encoder button, and then long-pressing it a second time at the main menu. This will store the following data for Enigma:

* All 40 Registers in the Register Library
* The current output assignments (as seen on the Assign screen)
* 32 Steps of the current Song
* The Track settings, Clock Division and One-Shot/Loop (as seen on the Play screen)

The Ornament and Crime's internal EEPROM is too small to save all 396 possible song Steps, so it only saves the first 32 Steps. If the Song is longer than that, then only 32 Steps will be restored when the module is powered up again. To help determine whether your entire song will be saved, there is a percentage counter in the upper-right corner of the Song screen. When your song exceeds 32 steps in length, the percentage will be highlighted. To save the entire song, you'll need to use System Exclusive storage (see below).

### System Exclusive Storage
As with all Hemisphere Suite applications, returning to the main menu with a long-press of the RIGHT encoder will initiate a SysEx dump of all app data. For Enigma, this includes the entire Register Library, the current output assignments, all Steps of the current song, and the Track settings. If you save this data with a SysEx Librarian, you can dump it back any time Enigma is running to restore the entire app state.

You may initiate dumps of subsets of information by long-pressing the LEFT encoder button. In the Library, this will dump all 40 Registers; on the Assign screen, it will dump the output assignments; on the Song or Play screen, this will dump the Song's steps and Track settings.

When you mark a Register as a Favorite in the Library, Enigma will send SysEx for the single Register. If you send this SysEx back while in the Library, it will be saved to the current Register location, provided that is not marked as a Favorite. If it is, the incoming dump will be ignored.

A single Register dump received outside the Library will be stored in its original location and--again--only if the current occupant of that location is not a Favorite.

## Credits
App © 2018-2022 by Jason Justian and Beige Maze Laboratories. Wiki text by [Chysn](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Enigma), under MIT License
