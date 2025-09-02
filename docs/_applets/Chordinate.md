---
layout: default
---
# Chordinator

![Screenshot 2024-06-13 14-32-57](https://github.com/djphazer/O_C-Phazerville/assets/109086194/073a1afd-edf6-49b4-b23c-330b9bfc0620)

**Chordinate** is a quantizer with a scale mask: outputting a basis pitch (i.e. the root note, notwithstanding any transposition) and a scale degree offset. The scale mask allows only user-determined intervals relative to the root note to be a valid output on CV 2/4.

### I/O

|        |              1/3               |               2/4                |
| ------ | :----------------------------: | :------------------------------: |
| TRIG   | Sample & hold root note (CV 1) | Sample & hold scale degree (CV2) |
| CV INs |          Basis pitch           |  Scale degree offset (bipolar)   |
| OUTs   |       Basis pitch (thru)       |        Scale degree pitch        |

_Note that sending a trigger to either digital input will initiate a clocked S&H mode, which will persist until power cycling. Root note and scale degree may be held independently, allowing for syncopated voicing changes_

### UI Parameters
* Quantizer scale
* Root note (affects output 2/4 only)
* Scale mask

### Credits
Chordinate is authored by [qiemem](https://github.com/qiemem/O_C-HemisphereSuite)
