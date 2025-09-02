---
layout: default
---
# Captain MIDI

Captain MIDI is a highly-configurable CV-to-MIDI and MIDI-to-CV interface. It supports four outputs for MIDI-to-CV and four inputs for CV-to-MIDI. It features multi-channel operation, polyphonic note distribution (up to four notes), transposition, and note-range for layers and/or splits. It keeps a log of the last 100 MIDI messages. Up to four complete Setups can be saved in memory. Setups can be copied from the panel, or saved and retrieved via MIDI system exclusive dump.

## Videos
* [https://youtu.be/PN6EEVkcqJ8](https://youtu.be/PN6EEVkcqJ8)
* [https://youtu.be/l1i4FIAcubw](https://youtu.be/l1i4FIAcubw)
* [https://youtu.be/pzHHOJ3jrCk](https://youtu.be/pzHHOJ3jrCk) (Uhost)

## References
[Blog post on Captain MIDI with SuperCollider](https://madskjeldgaard.dk/posts/hemisphere/)

## Connection
The back of the Ornament and Crime module has a micro USB port. This is what you use to make a MIDI connection to a USB host, usually a Windows, macOS, or Linux computer. Hemisphere Suite uses a class-compliant MIDI interface, which should be recognized as "Hemisphere" by your operating system and DAW/MIDI software.

## Basic Controls and Navigation
* RIGHT encoder: Selects parameter or value. Pushing the RIGHT encoder switches between parameter selection and value editing.
* LEFT encoder: Selects Setup screen (Assign, Channel, Transpose, Range High, Range Low). Pushing the LEFT encoder toggles between the Setup screen and the Log Display. When the log is displayed, the LEFT encoder scrolls through the last 100 MIDI events sent and/or received. A LEFT encoder button long-press (held for about 2 seconds, and released) initiates a Panic! function: Captain MIDI sends Note Off for all notes and on all channels.
* UP/DOWN buttons: Selects which Setup (from 1-4) is active. If the DOWN button is long-pressed, the active Setup may be copied to another Setup.

## Setup Screen Basics
Each of the Setup screens has eight parameters. The input or output (I/O) name is shown on the left, and the value is on the right. The parameters are arranged in groups of four, with the MIDI-to-CV parameters and then CV-to-MIDI parameters:

* A name like "MIDI > ?" indicates that a MIDI message of the assigned type, on the assigned channel, is routed to the corresponding letter output.
* A name like "? > MIDI" indicates that a MIDI message of the assigned type, on the assigned channel, is sent when the voltage at the corresponding CV input changes or, in the case of notes, when a gate is received at the corresponding digital input.
If the assigned type is Note or Legato, and a MIDI channel is set, an eighth-note icon appears next to the parameter name. This indicates that note-specific parameters (transpose, range) apply to the assignment.

Each parameter line also displays a MIDI indicator icon when its assignment is sending or receiving MIDI data. For note assignments, a note name is displayed instead of a MIDI icon.

## Assign Setup Screen
On this screen, you determine which MIDI messages are sent or received.

### MIDI-to-CV (MIDI In) Types
* **Note:** When a Note On message is received on the specified channel, the assigned output sends a quantized pitch value corresponding to the message's MIDI note number. Note may be assigned to any number of outputs. If Note is assigned to multiple outputs on the same channel, reception will be polyphonic; each new simultaneous note will be picked up by a different output until no more Note outputs are available. You may have up to four notes of polyphony by assigning all four outputs on the same MIDI channel; but you'll have to find another way to control your VCAs, since you won't have any outputs for gates.
* **Gate:** When a Note On message is received on the specified channel, the assigned output sends a high (approx. 5 volt) signal, which remains high until the corresponding Note Off message is received.
* **Trig:** When a Note On message is received on the specified channel, a trigger is sent on the assigned output.
* **Veloc:** When a Note On message is received on the specified channel, the assigned output sends CV between 0 and 5 volts proportional to the velocity of the Note On message. When the corresponding Note Off message is received, the assigned output goes to 0V.
* **Mod:** When a continuous controller message is received for CC#1 (modulation wheel) on the assigned MIDI channel, the assigned output sends CV between 0 and 5 volts proportional to the CC value.
* **Aft: **When aftertouch is received on the assigned MIDI channel, the assigned output sends CV between 0 and 5 volts proportional to the aftertouch value.
* **Bend:** When pitch bend is received on the assigned MIDI channel, the assigned output sends CV between about -3V and 3V proportional to the pitch bend value.
* **Expr, Pan, Brth: **Similar to Mod, but with different controller numbers (Expression: #11, Pan: #10, Breath: #2). These output values are maintained until the controller changes.
* **Hold: **When a hold pedal controller message is received on the assigned MIDI channel, the assigned output sends 5V until the pedal is released.
* **yAxis:** Similar to Mod, with with controller number #74. This is used as the Y-Axis controller for three-directional controllers that support MPE.
* **Clock:** There are four clock settings, which send triggers to the assigned CV output at various divisions of an incoming MIDI beat clock. The settings are: Qtr (which triggers every 24 clocks), 8th (12 clocks), 16th (6 clocks), and 24ppq (sends a trigger with every clock). Note that clock is a real-time message, and doesn't require a MIDI channel to be assigned. A clock indicator on the parameter line moves every eighth note.

### CV-to-MIDI (MIDI Out) Types
Note: When Note or Leg. (Legato) is assigned to an input, the digital input and the CV input are used at the same time. When the digital input goes high, the voltage at the CV input is quantized into a MIDI note number, and a Note On message is sent on the specified channel. When the digital input goes low, a Note Off message is sent.

* **Leg. (Legato):** Works like Note, except that Captain MIDI watches the CV input for pitch changes. If the pitch changes by a semitone or more, a Note Off is sent (regardless of whether the gate has gone low) and a new Note On is sent for the new pitch. This is a more natural setting for humans playing CV controllers (like Pressure Points, Tetrapad, Keystep) because it doesn't require a player to completely disengage the controller before playing another note. The Note setting is more appropriate for sequenced melodies.
* **Veloc:** Usually, a Note On message uses a default velocity of 100. However, if Veloc is assigned to the same channel as a Note or Leg. assignment, the Note On velocity will be proportional to the voltage at the assigned input. Note that if two Veloc assignments are made on the same channel, only the lowest-numbered assignment will be used for Note On velocity.
* **Mod:** A change in positive voltage at the assigned input will cause a MIDI controller change for CC#1 (modulation wheel) to be sent on the specified channel.
* **Aft:** A change in positive voltage at the assigned input will cause a MIDI channel aftertouch message to be sent on the specified channel.
* **Bend:** A change in bi-polar voltage at the assigned input will cause a MIDI pitch bend message to be sent on the specified channel. Positive voltage is positive bend, and negative voltage is negative bend. The range is about -3V to +3V.
* **Hold: **Voltage over around 2.5 volts at the assigned input will cause a MIDI Hold "on" (127) controller message to be sent. When the assigned input goes to 0 volts, a MIDI Hold "off" (0) message is sent.
Expr, Pan, Brth, yAxis: Similar to Mod, but with different controller numbers (Expression: #11, Pan: #10, Breath: #2, Y-Axis: #74).


## MIDI Channel Setup Screen
This screen sets the MIDI channel for each assignment. For the MIDI-to-CV ("MIDI > ?") assignments, this determines which channel Captain MIDI is listening on. For the CV-to-MIDI ("? > MIDI") assignments, this determines which channel Captain MIDI is transmitting on.

When the channel is set to "Off" the assignment is effectively muted; no MIDI data will be received or sent.

## Transpose Setup Screen
The transpose screen sets transpose values over a four-octave (-24 to +24 semitone) range. The transpose value is added to outgoing voltage for MIDI-to-CV assignments, and is added to MIDI note numbers for CV-to-MIDI assignments.

Transpose is a note-specific screen, and only assignments set to Note or Legato, and have a MIDI channel set, will be shown.

## Note Range Low/High Setup Screens
These screens set the low and high values transmitted or recognized by the assignment, between the lowest MIDI note (C -1) and the highest (G9). Values outside the range will be ignored. Range checking is applied after transposition. So if a Note On is out of range, but transposition takes it into range, the note will be played; if a Note On is in range, but transposition would take it out of range, the note will be ignored.

Range screens are note-specific screens, and only assignments set to Note or Legato, and have a MIDI channel set, will be shown.

## Setups
Captain MIDI has four independent Setups. A Setup consists of information for assignments, channels, transposition, and note range for all eight inputs and outputs. Use the UP and DOWN buttons to change the active Setup. The active Setup number is shown at the top of all of the Setup screens.

### Copying Setups
To copy the information for the active Setup to another Setup, long-press the DOWN button. When you release the button, a Copy screen will open. Choose the copy's destination with the UP and DOWN buttons, and then push the RIGHT encoder ("[Copy]") to execute the copy. To leave the Copy screen without changing any data, push the LEFT encoder ("[Cancel]") or long-press the DOWN button again.

If you select a Setup as its own copy destination, the display will change to a SysEx dump screen. The RIGHT encoder option becomes "[Dump]". If you push the RIGHT encoder, Captain MIDI will send a system exclusive file containing the data for the active Setup.

### Saving Setups
There are two ways to save Setup data.

Save all four Setups in the module: Long-press the LEFT encoder to get to the main menu, then long-press the LEFT encoder again to save data for all apps. A bar indicator will confirm the save operation. The Ornament and Crime module will recall the saved Setups on power-up.

Save using MIDI system exclusive: Follow the SysEx dump procedure from the Copying Setups section above, and capture the system exclusive dump with SysEx librarian software. To restore a Setup from a SysEx dump, simply send the SysEx file back to the module whenever Captain MIDI is running.

_Shortcut: When you long-press the RIGHT encoder to go to the main menu, the current Setup's data also is dumped via SysEx._

Notes about System Exclusive:

* A Setup from a received SysEx dump will always be placed in the active Setup, regardless of the Setup's original location.
* You can use system exclusive to change Captain MIDI's handling of MIDI data in an automated way during a performance.
* Data received from a SysEx dump will overwrite Setups in memory, but not data saved via the first save method; that is, unless you long-press to save the new Setup data, Captain MIDI will return to its previously-saved state at the next power-up.

## Log Display
Captain MIDI logs the most recent 100 MIDI messages, in and out. Push the LEFT encoder button to see the MIDI log. When the log is being displayed, use the LEFT encoder to scroll through the events. Push the LEFT encoder button again to return to the Setup screens.

Captain MIDI can be used to identify the applications associated with Hemisphere Suite system exclusive data. Open the log and send the system exclusive file to Captain MIDI. If it's a Hemisphere Suite file, Captain MIDI will identify it. Note that SysEx messages are logged only while you are viewing the Log Display.

## Panic! Function
Long-press the LEFT encoder if notes get hung. Captain MIDI will send Note Off messages on all 16 MIDI channels for all MIDI notes.

## Credits
App © 2018-2022 by Jason Justian and Beige Maze Laboratories. Wiki text by [Chysn](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Captain-MIDI), under MIT License
