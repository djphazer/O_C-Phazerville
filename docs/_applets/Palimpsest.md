---
layout: default
---
# Palimpsest Accent Sequencer

![Screenshot 2024-06-13 15-16-10](https://github.com/djphazer/O_C-Phazerville/assets/109086194/8aca80c8-8dbb-4af7-9626-55a7551c8842)

_Palimpsest_ is an accent sequencer that composes a pattern by way of a repeated sequence of trigger impressions. A/C is the accent output, and B/D is a trigger output sent when the level of the composed step is around 3V

_Palimpsest_ for Ornament and Crime is a port of this developer's alternate firmware for Mutable Instruments Peaks.

The idea behind Palimpsest is to write sequences gradually, using a pair of trigger signals. One of the triggers (Digital 1) clocks the sequencer. The other trigger (Digital 2) is a "Brush," which adds a CV value ("Compose") to the current step. If the sequencer is clocked without a brush trigger having arrived during that step, a CV value ("Decompose") is _subtracted_ from that step.

The results can be subtle, with a small Compose value and small or zero Decompose value. Or more dramatic shifts can be made, with larger values.

The sequence length can be between 2 and 16 steps. _When the cursor is on the Length setting, the sequence is locked, and incoming triggers will not affect it._

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock    | Brush (gate)    |
| CV INs | Decompose    | Compose    |
| OUTs   | Accent    | Trigger    |


### UI Parameters
* Compose
* Decompose
* Length
