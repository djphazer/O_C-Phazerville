---
layout: default
---
# Ebb & LFO

<!--![Relabi screenshot](images/EbbAndLFO.png)
-->
Relabi screenshot: TBD

This is an app for generating continuous CV and gates based on John Berndt's concept of [Relabi](https://www.johnberndt.org/relabi/Relabi_essay.htm). Relabi is a chaotic system for organizing time that shares many qualities with rhythm, but also breaks the expectations of regularity. For a human listener, Relabi feels like recurring events centered around a self-erasing pulse. Read more of Berndt's essay to understand the philosphy and initial goals of this approach.

There may be many ways to achieve Relabi. This method is based on FM feedback between a chain of three LFOs, deriving GATE patterns through thresholds of these LFOs.

### I/O

This App links the hemispheres together when placed in both hemispheres. This causes the hemispheres to share the same LFOs. In this state, the main controls appear on the left hemisphere and a larger set of VU meters appears on the right. Otherwise, there is a two page screen of controls for the app with output selections on the second page.

#### One Instance: When Relabi App is only in the Left or Right Hemisphere

|        |                          1/3                            |    2/4     | 
| ------ | :---------------------------------------------------: | :--------: |
| TRIG   |  Resets the Phase of All LFOs      |   Unused    | 
| CV INs |  Frequency Multiplier for All LFOs | Offset to FM Amount of All LFOs |
| OUTs   |  Assignable (LFOs 1-3, GATES 1-3, <br />STEP, TRIGGER)   | Assignable (LFOs 1-3, GATES 1-3, STEP, TRIGGER) |

#### Linked: When Relabi App is in Both Hemispheres

|        |                          1                          |    2     | 3/4 |
| ------ | :---------------------------------------------------: | :--------: | :-----: |
| TRIG   |  Resets the Phase of All LFOs     |   Unused    | Unused |
| CV INs |  Frequency Offset for All LFOs | Offset to FM Amount of All LFOs | Unused |
| OUTs   |  Assignable (LFOs 1-3, GATES 1-3, <br />STEP, TRIGGER)   | Assignable (LFOs 1-3, GATES 1-3, STEP, TRIGGER) | Assignable (LFOs 1-3, GATES 1-3, <br />STEP, TRIGGER) |

## Parameters:

Scroll through the parameters to access a second page. When the hemispheres are linked (Relabi on each side), the second hemisphere only shows output assignments with large VU meters.

### Page 1
* LFO Selector - Selection of three LFOs. When chosen, Page 1 will display parameters for that LFO.
* FREQ - Base frequency of the chosen LFO.
* XFM - Cross modulation to the chosen LFO from another.
* PHAS - Starting phase of the LFO when TRIG 1 receives a reset trigger. This allows the chaotic system to always reset to the same starting conditions, providing a repeatable deteriministic pattern, like a random seed.
* THRS - Threshold for the LFO. When the LFO exceeds this threshold, the corresponding GATE goes high.


### Page 2
* FREQx numerator - A multiplier to all three LFOs' frequency settings.
* FREQx denominator - A divisor to all three LFOs' frequency settings.
* A - Output Mode of OUT A
* B - Output Mode of OUT B
* C - Output Mode of OUT C (Appears as A when Relabi is only in the right hemisphere. Maybe, we should update this to be C all the time.)
* D - Output Mode of OUT D (Appears as B when Relabi is only in the right hemisphere.)


### Output Modes
Each output can be one of:
* LFO 1 CV
* LFO 2 CV
* LFO 3 CV
* LFO 1 GATE
* LFO 2 GATE
* LFO 3 GATE
* STEPS - derived from combining the three GATES into one CV
* TRIGGERS - sends a trigger when any of the three GATES changes state.


# Credits
The original applet was written by **TricksterSam** (Samuel Burt) based on software he wrote for John Berndt.

