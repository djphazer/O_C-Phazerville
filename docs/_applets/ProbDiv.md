---
layout: default
---
# ProbDiv

![Screenshot 2024-06-13 15-18-59](https://github.com/djphazer/O_C-Phazerville/assets/109086194/40d4e48a-fd31-47c3-8ffa-f5a900c72f99)

**ProbDiv** is a stochastic rhythm generator where you can assign probability to different clock division settings. It takes inspiration from the rhythm section of the [Stochastic Inspiration Generator](https://www.modulargrid.net/e/stochastic-instruments-stochastic-inspiration-generator), except it uses clock divisions instead of note divisions. You can also capture a loop based on the probability settings with anywhere between 1 and 32 steps. It can be used by itself or with [ProbMeloD](ProbMeloD), which it will automatically link to.

At the heart of ProbDiv is a clock divider. There are four clock divisions that can be given probability of being selected, `/1`, `/2`, `/4`, and `/8`. On the first clock input received at Digital 1, a new division is selected based on the probability settings for each division. Once that division is reached, a trigger is sent to Output A/C, another division is selected, and the process starts over. If all divisions have a probability of 0, nothing will be selected and no clocks will be output.

[See it in action!](https://www.youtube.com/watch?v=uR8pLUVNDjI)

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   |  Clock   |  Reset   |
| CV INs |   Loop length  |  Reseed loop using current probabilities (when > 2.5v)   |
| OUTs   |  Divided clock | Skipped pulses (when a division `/1` is active)   |

### UI Parameters
 - Probability per division
 - Enable / disable loop
 - Loop length


## Probabilities

Probabilities are defined as weights from 0-10 for choosing a specific division. For example, if you only have a probability set for `/1` it will always be selected no matter the if the probability is 1 or 10. Or if you have a probability of 10 for both `/1` and `/2`, it will be a 50/50 chance of either division being selected. Probability value can be thought of as the number of raffle tickets entered for each division setting when it’s time to pick a new one, which is any time the current division is reached.

## Looping

Loop can be enabled by setting the loop length > 0. Once loop has been activated, an entire 32 step sequence of divisions will be generated based on the current settings. The length will use a subset of this sequence. If the final division in the sequence goes beyond 32 steps it will get cut off. 

Changing any of the probabilities while loop is enabled will generate a new loop using the new values. Loops are non-deterministic, so changing a probability and then changing it back will still result in a new loop. Changing loop length will not result in a new loop, unless you disable the loop by changing loop length to 0. A voltage of 2.5 or greater on CV2 will reseed the loop (if enabled) and a trigger on Digital 1 will reset the loop (if enabled)

## Pairing with ProbMeloD

If ProbDiv and [ProbMeloD](ProbMeloD) are loaded in each hemisphere, they will automatically link. Clock division outputs from ProbDiv will automatically trigger ProbMeloD, and ProbMeloD will capture a loop when ProbDiv is looping as well. A new loop is generated in both applets when any parameter in ProbDiv or ProbMeloD are changed. When used together they can be treated as a whole probabilistic sequencer!

### Credits
Copied/Adapted from [ProbDiv](https://github.com/benirose/O_C-BenisphereSuite/wiki/ProbDiv) by benirose
