---
layout: default
---
# MIDI Input

## Video
[![MIDI Mapping Video Demo](http://img.youtube.com/vi/SpgH4tNvikc/0.jpg)](http://www.youtube.com/watch?v=SpgH4tNvikc "MIDI Input Mapping")

## Advanced MIDI-to-CV For Hemisphere
![hMIDIIn screenshot](images/hMIDIIn.png)

Phazerville greatly expands on the capabilities of the original [**MIDI In** applet](https://github.com/Chysn/O_C-HemisphereSuite/wiki/MIDI-In). MIDI messages coming in via USB are parsed and handled at a high level; the applet acts as a configuration UI, and also passes signals to the outputs.

If you switch to a different applet, the configured incoming MIDI messages are rerouted to the _inputs_ of the selected applet and are _combined with the physical CV or trigger input_. This allows things like modulating parameters via MIDI CC or Pitch Bend, quantizing MIDI Notes to a scale, or triggering sequencer applets with MIDI Note-On. You can use **AttenOff** to scale and offset MIDI CC values. You can transpose **TB-3PO** patterns via MIDI Note and modulate Density with the Velocity, or Aftertouch, etc.

MIDI _Clock_, _Start_, and _Stop_ messages are also handled automatically by the internal **[Clock Setup](https://github.com/djphazer/O_C-Phazerville/wiki/Clock-Setup)** applet. Incoming MIDI Clock is divided from 24 PPQN to 2 PPQN internally.

MIDI _Program Change_ messages can be used to switch among saved HS presets, and the channel on which the O_C listens for PC messages can be selected on the Global tab of the MIDIIn applet. It may be set to a specific channel [1-16], Omni, or Off. When the preset is saved, the PC channel filter is saved globally, so it won't revert when switching presets.

### Channel Settings

By default, all channels are set to "None". A pair of the MIDI In applets can be [saved as a Preset](https://github.com/djphazer/O_C-Phazerville/wiki/Hemisphere-Config) to quickly recall settings.

Each channel filters and translates incoming MIDI messages as output CV from **MIDI In** applet, or to a corresponding logical input for other applets.
- **MIDI Channel** (independent for each CV channel)
- **Mode**
- **Voice** (only used in polyphonic Modes)

The available "monophonic" modes are:
- **None** - disabled
- **Note** - semitone-quantized pitch CV
- **LoNote** - pitch CV corresponding to lowest held note
- **HiNote** - pitch CV corresponding to highest held note
- **PdlNote** - pitch CV corresponding to the "pedal note", or the oldest held note
- **InvNote** - pitch CV corresponding to the newest note, but the keyboard is reversed such that high note keys produce low pitches and vice versa
- **Trig** - standard trigger pulse from _NoteOn_
  - generates Clock pulses at corresponding trigger input
- **Trig1st** - like **Trig**, but only triggers once, until all notes are released. Useful for interesting one-shot effects
- **TrigAlws** - like **Trig**, but also triggers when notes are released (always)
- **Gate** - held high at _NoteOn_, respecting polyphony; goes low when all Notes are Off and sustain pedal is not pressed
  - holds trigger input high, but does not generate a Clock pulse
- **GateInv** - Same as Gate, but high with no notes held, and low when notes are held.
  - Tip: Try setting 2 outputs as Gate and GateInv respectively, then trigger 2 different voices in stereo for a ping-pong effect
- **Veloc** - Positive CV from the _Velocity_ of the most recent _NoteOn_
- **CC#** (auto-learn) - Positive CV from assigned _CC#_
  - when selected, it will display `CC#-1` until a MIDI CC message on the selected MIDI Channel is received, at which point it latches onto the CC# of the message. Basically, select CC mode and wiggle a knob to auto-learn!
- **ChnAft** - Positive CV from _Channel Aftertouch_
- **Bend** - Bipolar CV from _Pitch Bend_

The available "polyphonic" modes are:
- **PolyN** - Polyphonic Note, semitone-quantized pitch CV per-Voice
- **PolyG** - Polyphonic Gate, held high at _NoteOn_, per-Voice; goes low when assigned Voice's note is released
- **PolyV** - Polyphonic Velocity, positive CV from the _Velocity_ of the assigned Voice's _NoteOn_; goes to 0 when the note is released
- **KeyAft** - Polyphonic Aftertouch, positive CV from _Key Aftertouch_ messages by MIDI controllers that support it (some only produce Channel Aftertouch)

Other Modes:
- **Clock** - standard trigger pulse from _MIDI Clock_ (divided internally to 2 PPQN)
- **Run** - high gate output while MIDI Transport state is "running"
- **Start** - standard trigger pulse from _MIDI Start_

Some notes on Polyphonic Mode configuration:

Each CV output can be assigned a _Voice_ number. The highest assigned Voice number determines the size of the polyphony buffer. (Eg. if the max Voice is 3, MIDI-In will keep track of up to 3 notes at a time, for polyphonic modes.)
The way the buffer handles the note messages depends on the selected Poly Config Mode. The available modes are _Reset_, _Rotate_, and _Reuse_.
The polyphonic modes were built to behave just like those of the MIDI-to-CV converter module in VCV Rack.

- **Reset** is the simplest to understand: each time the first note is played, its selected functions (Pitch, Gate, Vel, AT) are output on Voice 1's CV channels. The second note uses Voice 2's channels, and so on. The highest Voice's outputs are overwritten by any newer played notes if all Voices are in use. Releasing a note frees up the corresponding Voice, which will be used by the next played note.

- **Rotate** behaves differently. The last used Voice number is always remembered, and each new note uses the next Voice in the queue, even if that note was recently released. Once the max number of Voices is reached, the "oldest" Voice will be overwritten by the next played note. For example, if the user presses and releases the G3 note over and over, and 4 Voices are allocated, G3 will play on 1, then 2, 3, 4, 1, 2, 3, and so on.
  - It cycles through the assigned voices, and wraps around. Oldest held notes will be overwritten by newer notes as it cycles through.

- **Reuse** behaves just like _Rotate_, except if a played note was recently in the buffer and it has not been overwritten by a different new note yet, that same Voice which it used before is reused, until all Voices are full. If C4 is pressed and released, and it was buffered in Voice 2, each subsequent press of C4 will use Voice 2, until enough new notes are played to overwrite that slot.

Since each CV channel can be assigned a different conversion mode with individual Voice selection for the Polyphonic modes, and each has it's own MIDI channel filter, the configuration can become complicated, but is extremely flexible.

Each CV channel also has a MIDI Activity indicator icon, which blinks when it processes a message, and the last page of the applet UI is a log console which displays the last 5 received messages not ignored by mode and channel filters.
