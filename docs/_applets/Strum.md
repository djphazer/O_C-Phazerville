---
layout: default
---
# Strum

![Strum Screenshot](images/Strum.png)

Strum is an arpeggiator. It generates fast sequences of up to six pitches from a selected scale, along with trigger pulses. This is ideal for achieving chord tones with voices with round-robin polyphony, like Rings, but also nice for slower arp sequences - spacing goes up to 500ms.

Incoming triggers intuitively run through the sequence either forward or backward. If retriggered in the middle of the sequence, it will immediately jump to the next step, allowing unique grooves to emerge from tempo-synced pulses.

New in 1.8.2: Step Mode - Press the AuxButton while the chord is highlighted for editing to toggle Step Mode. In this mode, each clock tick advances to the next interval, rather than strumming through the entire chord.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Strum Up | Strum Down |
| CV INs | Chord Root Transpose | Spacing (default) /<br>Quantizer select |
| OUTs   | Pitch CV | Trigger Pulse |

### UI Parameters
* Quantizer select - AuxButton to edit
* Spacing - AuxButton to switch CV2 modulation destination (spacing or quantizers)
* Length
* Edit Intervals 1-6

### Credits
Authored by qiemem, with mods by djphazer
