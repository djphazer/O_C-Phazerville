---
title: Piquet
layout: default
---

# Piquet

A compact, dual-channel envelope generator inspired by Piqued. Each channel has its own trigger input, CV modulation input, envelope settings, and output. Piquet supports selectable envelope types and shapes, looping, trigger delay, CV modulation, and end-of-cycle cross-triggering.

![Piquet applet screenshot](images/Piquet.png)

## I/O

| | 1/3 | 2/4 |
| --- | --- | --- |
| TRIG | Trigger Channel 1 | Trigger Channel 2 |
| CV INs | CV modulation Channel 1 | CV modulation Channel 2 |
| OUTs | Envelope Channel 1 | Envelope Channel 2 |

## Controls

- **Encoder:** Rotate to navigate parameters, press to enter or exit editing, and rotate while editing to change the selected value.
- **AUX Button:** Switches between channels. The corresponding parameter remains selected where available; otherwise, Piquet selects an appropriate parameter on the other channel.

## Envelope Types

- **AD:** Attack followed by decay to zero.
- **ADR:** Attack, decay to the selected sustain level, then release to zero without waiting for the gate to fall.
- **AR:** Attack, hold at the peak while the gate remains high, then release when the gate goes low. A short trigger produces an attack followed by release.
- **ADSR:** Attack, decay to the sustain level, sustain while the gate is high, then release when the gate goes low.
- **L-AD:** Looping attack-decay envelope.
- **L-ADR:** Looping attack-decay-release envelope, passing through the selected sustain level between decay and release.

## Envelope Parameters

- **Length:** Scales stage durations using `/8`, `/4`, `/2`, `x1`, `x2`, `x4`, or `x8`.
- **Att:** Sets the attack time.
- **Dec:** Sets the decay time.
- **Sus:** Sets the sustain level where applicable.
- **Rel:** Sets the release time.
- **AShp:** Selects the attack shape.
- **DShp:** Selects the decay shape.
- **RShp:** Selects the release shape.
- **Delay Mode:** Selects `Off`, `Rpl`, or `Que` (described below).
- **Delay:** Sets trigger delay when enabled.
- **Send EOC:** Enables or disables triggering the other channel at the end of this channel's envelope.
- **CV Map:** Assigns the channel's CV input to an envelope parameter.
- **Lvl:** Sets the output level.
- **Inv:** Inverts the envelope output.

Parameters that do not apply to the selected envelope type are hidden from navigation.

## Envelope Shapes

Attack, decay, and release shapes can be configured independently where those stages are present:

- **Linear:** Linear
- **Expo:** Exponential
- **Quart:** Quartic
- **Sine:** Sine
- **Plat:** Plateau
- **Cliff:** Cliff
- **Gate:** Gate
- **BigDip:** Big Dipper
- **MedDip:** Medium Dipper
- **LitDip:** Little Dipper
- **SinFld:** Sinefold

The envelope graphic reflects the selected type, stage shapes, and parameters.

## Looping

L-AD and L-ADR can repeat. Their **Loop Mode** parameter offers:

- **CON:** Continuous looping. A trigger toggles looping on or off.
- **GTD:** Gate-controlled looping. Looping is enabled while the channel's gate is high.

The Loop Mode parameter is available only for L-AD and L-ADR.

## Trigger Delay

Each channel has an independent delay mode and time:

- **Off:** A trigger starts the envelope without an added delay.
- **Rpl:** Replace mode keeps one pending delayed trigger; a new trigger replaces it.
- **Que:** Queue mode holds up to four pending triggers. Additional triggers are ignored when the queue is full, preserving the earlier triggers.

**Delay:** Adjustable from 10 ms to 11 seconds.

The clock icon flashes whenever a delayed trigger is pending in Rpl or Que mode and remains steady when the queue is empty.

## End-of-Cycle (EOC) Cross-Triggering

**Send EOC** can be enabled independently for either channel. When that channel's envelope completes, Piquet sends an internal trigger to the opposite channel. The receiving channel handles it through its normal trigger and delay settings.

Enabling Send EOC on both channels allows the channels to trigger one another in succession.

**Looping envelopes (L-AD and L-ADR):** EOC is sent only when looping stops and the final envelope cycle completes, not at the end of each repetition.

## CV Mapping

Each channel's CV modulation input can be assigned to:

- **None:** No CV modulation.
- **Att:** Attack time.
- **Dec:** Decay time.
- **Sus:** Sustain level.
- **Rel:** Release time.
- **ADR:** Attack, decay, and release times together.
- **Lvl:** Output level.

The selected parameter responds to CV modulation while the envelope runs.

## Fullscreen

Fullscreen mode displays both channel envelopes simultaneously.

## Credits

Piquet developed by Hans-Henning Klos, Nicholas J. Michalek, and Benjamin AM.
