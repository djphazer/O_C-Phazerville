---
layout: default
---
# Relabi

<!--![Relabi screenshot](images/EbbAndLFO.png)
-->
Relabi screenshot: TBD

This is an app for generating continuous CV and gates based on John Berndt's concept of [Relabi](https://www.johnberndt.org/relabi/Relabi_essay.htm). Relabi is a chaotic system for organizing time that shares many qualities with rhythm, but also breaks the expectations of regularity. For a human listener, Relabi feels like recurring events centered around a self-erasing pulse. Read more of Berndt's essay to understand the philosphy and initial goals of this approach.

There may be many ways to achieve Relabi. This method is based on FM feedback between a chain of three LFOs, deriving GATE patterns through thresholds of the combined Relabi wave.

### I/O

This App links the hemispheres together when placed in both hemispheres. This causes the hemispheres to share the same LFOs. In this state, the main controls appear on the left hemisphere and a larger set of VU meters and gate indicators appears on the right. Otherwise, there is a five page screen of controls for the app with output selections on the first page.

#### One Instance: When Relabi App is only in the Left or Right Hemisphere

|        |                          1/3                            |    2/4     | 
| ------ | :---------------------------------------------------: | :--------: |
| TRIG   |  Resets the Phase of All LFOs      |   Unused    | 
| CV INs |  Frequency Multiplier for All LFOs | Offset to FM Amount of All LFOs |
| OUTs   |  Assignable (GATES 1-4, RELABI WAVE, <br />LFOs 1-3)   | Assignable (GATES 1-4, RELABI WAVE, <br />LFOs 1-3) |

#### Linked: When Relabi App is in Both Hemispheres

|        |                          1                          |    2     | 3/4 |
| ------ | :---------------------------------------------------: | :--------: | :-----: |
| TRIG   |  Resets the Phase of All LFOs     |   Unused    | Unused |
| CV INs |  Frequency Offset for All LFOs | Offset to FM Amount of All LFOs | Unused |
| OUTs   |  Assignable (GATES 1-4, RELABI WAVE, <br />LFOs 1-3)   | Assignable (GATES 1-4, RELABI WAVE, <br />LFOs 1-3) | Assignable (GATES 1-4, RELABI WAVE, <br />LFOs 1-3) |

## Parameters:

Scroll through the controls to access the display pages. When the hemispheres are linked (Relabi on each side), the right hemisphere only shows output assignments with large VU meters and four indicators the flash when the corresponding gates are exceeded.

### Page 1
The three LFO rows show each LFO's FREQ and PHAS settings, with corresponding VU meters. A/B select the output assignments for OUT A and OUT B along the bottom.

### Pages 2–5
Each page displays the Relabi wave and a line showing the setting of one of the four gate thresholds. A numerical display shows the percentage value of the threshold associated with that page. It's color inverts based on whether the Relabi wave currently exceeds the threshold.

### Output Modes
Each output can be one of:
* GATE 1
* GATE 2
* GATE 3
* GATE 4
* RELABI WAVE
* LFO 1
* LFO 2
* LFO 3


# Credits
The original applet was written by **TricksterSam** (Samuel Burt) based on software he wrote for John Berndt.
