---
layout: default
---
# Panner

<!-- TODO: add screenshot -> ![Panner Screenshot](images/Panner.png) -->

**Panner** takes a single CV signal and distributes it between two complementary outputs, like the pan control on a mixer channel. It is derived from [Xfader](Xfader), but where Xfader mixes two inputs down, Panner spreads one input out.

### I/O

|        | 1/3        | 2/4         |
| ------ | :--------: | :---------: |
| TRIG   | Hard Left  | Hard Right  |
| CV INs | Signal     | Pan CV      |
| OUTs   | Left       | Right       |

### UI Control
* Encoder: Adjusts the pan position

The two level bars show the gain currently applied to the left and right outputs. They are complementary: with the position all the way left, the full signal appears at OUT 1/3 and nothing at OUT 2/4; all the way right is the reverse; at center both outputs get half. Unlike Xfader, the signal is not pre-attenuated, so a centered panner gives each side 50% of the input level.

The bar below the level meters shows the position. The vertical tick marks the base position set by the encoder, and the small caret marks the live position after CV and gates are applied.

### Pan CV

CV 2/4 is unipolar and adds a rightward offset to the encoder position over a 0..5V range. With the encoder at hard left, 0..5V sweeps the full width; with the encoder at center, it sweeps from center to hard right. To get a full bipolar sweep around center, set the encoder to hard left and offset the incoming CV so that 2.5V lands at center.

### Gate Overrides

The trigger inputs override the position entirely, ignoring both the encoder and the pan CV for as long as they are held high:

* TR 1/3 high: hard left
* TR 2/4 high: hard right
* Both high: dead center

This is momentary rather than latching, so the position snaps back to encoder + CV control as soon as the gates are released. A zap icon appears beside the position bar while an override is active. Useful for performative hard-panning, or for slamming a wandering CV-modulated panner back to center on a downbeat.

### Credits
Authored by Eric Gao, based on Xfader by Jason Justian.
