---
layout: default
---
# Combin8

![Combin8 Screenshot](images/Combin8.png)

Two 3:1 CV mixer channels with selectable output modes, input muting, and quantization.

Each channel combines its main input with two configurable AUX inputs. The two channels operate independently, but can be cascaded into a 5:1 mixer.

## Controls

- **Input mapping:** AUX input sources can be selected through the input mapping editor on the front page. The main input is accessedd through the help menu.

- **AUX Button:**
  - On an output selection, opens the channel's Q-engine settings.
  - On Inputs:AUX1 or AUX2, toggles between muted and unmuted states.

## Output Modes

Each channel has three selectable output modes:

- **NRM:** The main input and unmuted AUX inputs are mixed continuously.
- **SUM:** The main input and unmuted AUX inputs are summed together. The sum is sampled and held when the channel receives a trigger.
- **IN:** The main input is sampled and held when the channel receives a trigger, while the unmuted AUX inputs continue to pass through live.

The output signal can be quantized or offset using the Q-engine.

  ## Meters

- **Input meters:** Vertical meters beside the input icons show the CV level of the main input and each AUX input.
- **Mix meter:** The thin horizontal meter shows the combined CV level before quantization. Positive values extend from left to right, while negative values extend from right to left.
- **Output meter:** The thicker horizontal meter shows the output CV level after quantization. Positive values extend from left to right, while negative values extend from right to left.

## Credits

Original Combin8 applet by Nicholas J. Michalek
