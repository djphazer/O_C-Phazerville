# Piquet

A compact, dual-channel envelope generator inspired by Piqued. Each channel has its own trigger input, CV modulation input, envelope settings, and output, with selectable envelope types, adjustable stage shapes, looping, trigger delays, and CV modulation.

![Piquet applet screenshot — add image here]

## I/O

|  | 1/3 | 2/4 |
|---|---|---|
| TRIG | Trigger Channel 1 | Trigger Channel 2 |
| CV INs | CV modulation Channel 1 | CV modulation Channel 2 |
| OUTs | Envelope Channel 1 | Envelope Channel 2 |

## Controls

- **AUX Button:** Switches between channels. The corresponding parameter is selected on the other channel when possible.

## Envelope Types

- **AD:** Attack followed by decay to zero.
- **AHR:** Attack, hold at the peak, then release.
- **ASR:** Attack, sustain while the gate is high, then release when the gate goes low.
- **ADSR:** Attack, decay to the sustain level, sustain while the gate is high, then release when the gate goes low.
- **L-AD:** Repeating attack-decay envelope.
- **L-AHR:** Repeating attack-hold-release envelope.

## Envelope Parameters

- **Length:** Multiplies stage durations by ×1, ×2, ×4, or ×8.
- **Att:** Sets the attack time.
- **Dec:** Sets the decay time.
- **Sus:** Sets the sustain level.
- **Rel:** Sets the release time.
- **AShp:** Selects the attack envelope shape.
- **DShp:** Selects the decay envelope shape.
- **RShp:** Selects the release envelope shape.
- **Lvl:** Sets the output level.
- **Inv:** Inverts the envelope output.

Parameters that do not apply to the selected envelope type are hidden from navigation.

## Envelope Shapes

Available shapes:

- **Lin:** Linear
- **Exp:** Exponential
- **Qua:** Quartic
- **Sin:** Sine
- **Pla:** Plateau
- **Cli:** Cliff
- **Gat:** Gate
- **BD:** Big Dipper
- **MD:** Medium Dipper
- **LD:** Little Dipper
- **SF:** Sinefold

The graphic updates to reflect the selected envelope's shape and parameters.

## Looping

L-AD and L-AHR envelopes can repeat automatically.

- **CON:** Continuous looping.
- **GTD:** Gate-controlled looping.

The Loop Mode parameter is available only for L-AD and L-AHR envelope types.

## Trigger Delay

Trigger delay can be configured independently for each channel.

- **Off:** Triggers start the envelope without an additional delay.
- **Rpl:** Replace mode replaces the pending delayed trigger with a new trigger.
- **Que:** Queue mode stores up to four pending triggers. When the queue is full, additional triggers are ignored, preserving the existing queued triggers.

**Delay:** Sets the trigger delay time from 0.00 to 3.6 seconds.

## CV Mapping

Each channel's CV modulation input can be assigned to one of the following:

- **None:** No CV modulation.
- **Att:** Attack time.
- **Dec:** Decay/Hold time.
- **Sus:** Sustain level.
- **Rel:** Release time.
- **ADR:** Attack, decay/hold, and release times.
- **Lvl:** Output level.

The CV input affects the selected parameter while the envelope runs.

## Fullscreen

Fullscreen mode displays both channel envelopes simultaneously.

## Credits

Piquet developed by Hans-Henning Klos, Nicholas J. Michalek, and Benjamin AM.
