---
layout: default
---
# Pigeons

[![Stolen Ornaments: Pigeons](http://img.youtube.com/vi/J1OH-oomvMA/0.jpg)](http://www.youtube.com/watch?v=J1OH-oomvMA "Stolen Ornaments: Pigeons, PolyDiv & DivSeq")

![Screenshot 2024-06-13 15-17-14](https://github.com/djphazer/O_C-Phazerville/assets/109086194/9b0b7507-8ed2-4fbd-bde7-f4360b061192)

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Poke Pigeon 1 (top) | Poke Pigeon 2 (bottom) |
| CV INs | Modulo of Pigeon 1 | Modulo of Pigeon 2 |
| OUTs   | Pitch CV from Pigeon 1 | Pitch CV from Pigeon 2 |

_("Poke" == Clock == Trigger)_

## UI Parameters
- Seed value 1
- Seed value 2
- Modulus
- Quantizer selection/edit

## What is the Pigeonhole Principle?
The core concept is related to the Fibonacci Sequence, based on [**this most excellent video**](https://www.youtube.com/watch?v=_aIf4WUCNZU) by Marc Evanstein. Something about the way numbers behave in modular arithmetic being akin to a limited number of pigeonholes for an infinite number of pigeons...

### How does the applet work?
This applet generates two Pitch CV values at the Outputs, each quantized using given Scale and Root Note settings.

There are two **Pigeons** (channels), independently triggered. They are each continuously singing a _note_ (a number representing scale degree) and have  a _modulus_ that limits their range. When triggered, the current and previous _notes_ are added together, divided by the _modulus_ value, and the remainder used for the next _note_ value.

Each Pigeon has a certain number of holes they can visit (the _modulus_). The pair of notes represents the coordinates of the current hole; your Pigeon is guaranteed to revisit the same holes eventually.

Pigeons are easily triggered - by the physical trigger inputs, internal clock pulses, or neighboring trigger sequencer applets (like [ProbDiv](ProbDiv), or [DivSeq](DivSeq)).

The **CV inputs** change the _modulus_ value for each channel, affecting the range of the generated melodic sequence. It is possible to cause both note values to drop to 0 (the root note), and your Pigeon will take a nap there. If that happens, you'll have to nudge it with the encoder, or maybe load a Preset with a MIDI PC message...
