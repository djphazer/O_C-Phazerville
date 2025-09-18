---
layout: default
---
# Clock-To-Gate

![Clk2Gate screenshot](images/Clk2Gate.png)

**Clk2Gate** is a variable-length gate generator with two independent channels. Each channel is triggered - like most applets - with the rising edge of a clock pulse at the corresponding Trig input.

When triggered, a channel output is set high for a calculated amount of time before returning to zero. The duration of the gate is based on the time between the previous two incoming clock triggers on that channel, with the Length parameter applied as a percentage (duty cycle). A gate with Length of 100% stays high until a shorter gate is triggered, regardless of cycle time between triggers.

The Variance parameter provides a range of randomization for Length - either additive or subtractive. The CV input modulates the Length value before Variance is applied.

Skip Probability randomly ignores triggers, much like Clock Skipper.

### I/O

|        |                1/3                    |                  2/4                  |
| ------ | :-----------------------------------: | :-----------------------------------: |
| TRIG   | Channel 1 Clock         | Channel 2 Clock         |
| CV INs | Ch 1 Gate Length        | Ch 2 Gate Length        |
| OUTs   | Ch 1 Gate               | Ch 2 Gate               |

### UI Parameters
* Ch 1 Length (duty cycle)
* Ch 1 Variance (%)
* Ch 1 Skip Probability (%)
* Ch 2 Length
* Ch 2 Variance (%)
* Ch 2 Skip Probability (%)

### Future Ideas
Instead of clock skipping, the probability could determine whether Variance is applied or not. This would match the behavior of the Westlicht Performer feature that inspired this applet.

CV inputs could potentially modulate Variance or Skips instead of Length, with destination assigned using the AuxButton action.
