---
layout: default
---
# Sequins

_Sequins_ is a basic, **dual-channel sequencer**.

The app provides four 16-step sequences per channel and CV-control over various channel parameters, including scale mask, octave, pulsewidth, clock multiplication and division. The four sequences can be **chained** in various ways, providing sequences from **4 to up to 64 notes**. The sequence data is stored along with the other settings (whenever settings are saved).

### Controls

|       | Left Encoder                         | Right Encoder                                                                                                    |
| ----- | ------------------------------------ | ---------------------------------------------------------------------------------------------------------------- |
| TURN  | select channel                       | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited |
| PRESS | re-sync channels (reset)             | Toggle between menu navigation (selection) mode and value editing mode                                           |
| LONG  | copy selected scale to other channel | app selection menu                                                                                               |

|            | Up Button                         | Down Button                                                                     |
| ---------- | --------------------------------- | ------------------------------------------------------------------------------- |
| PRESS      | toggle octave up (parameter menu) | enter CV menu                                                                   |
| LONG PRESS | invoke screensaver                | toggle sequence-select behaviour (instant update vs. update at end-of-sequence) |

### I/O

|     | 1                 | 2                                  | 3                                           | 4                                           |
| --- | ----------------- | ---------------------------------- | ------------------------------------------- | ------------------------------------------- |
| TR  | clock input #1    | aux clock input #1 / reset/mute #1 | clock input #2                              | aux clock input #2 / reset/mute #2          |
| CV  | (mappable)        | (mappable)                         | (mappable)                                  | (mappable)                                  |
| OUT | CV outputs #1, #2 | CV outputs #1, #2                  | aux outputs #1, #2 (default to gate output) | aux outputs #1, #2 (default to gate output) |

### Description
- the **sequence editor** works similarly to the scale editor in the quantizer modes:
    - in the main menu, select which sequence to edit by adjusting the `sequence #` parameter.
    - then right-click on the item displayed right below (`--> edit`) to **open the sequence-editor**.
    - **mute/unmute** notes by clicking the left encoder: the step value is displayed in a format x+y.yy, where x is the octave (above or below 0.00V) and y.yy gives the semitone offset. for instance `+2+2.23` = 2.23 semitones above octave #2 (=~ 2.000V).
    - adjust the sequence **length** by pointing the cursor to the far right, then turn the right encoder.
    - (**simultaneously** push down the _left_ encoder to also **mute/clear** the steps that are being added while expanding the sequence).
    - to adjust the **pitch**, simply move the cursor to a given slot, then turn the right encoder: this will increase or decrease the pitch in **semitones** (= step size: `1.00`). note these values are **pre-quantization**, so the effect this has will depend on the chosen scale.
    - to **fine-tune** the pitch (= step size: ~ `0.01`), hold down the left encoder **while** turning the right encoder.
    - **note**: the number displayed when adjusting the note values is the actual (pre-quantization) DAC code (that’s far from ideal, i know …). details on what those numbers mean can be found [here](https://ornament-and-cri.me/custom-scales/).
    - in the sequence-editor, you can use the **up/down** buttons to select which sequence to edit (`#1` - `#4`). (this makes it possible to edit sequences ‘offline’, ie make adjustments to, say, sequence #2 while sequence #1 is playing).
    - **copy+paste**: in the sequence-editor, long-press the left encoder to **copy** the selected sequence. select a different sequence slot, then long-press the down button to **paste** it into that slot. copy+paste works across channels. (note: once copied (or pasted), sequence data will be stored in a temporary buffer for about 15 seconds, after which it’ll expire.)
- the `playmode` and `direction` settings offer various ways of moving through the sequence(s):
    - `-` (default): advance by trigger, using the chosen sequence and direction settings.
    - `SEQ+1 - SEQ+3`: ditto, but **cycling** through 2, 3, or 4 adjacent sequences (= allows up to 64 notes).
    - `TR+1 - TR+3`: ditto, but **jumps** to the next sequence only _if/when_ a trigger is received at the **aux.** trigger inputs.
    - `ARP`: **arpeggiate** the sequence.
    - `S+H#1 - S+H#4`: CV-address, when triggered (**sample and hold**).
    - `CV#1 - CV#4`: CV-address, **free-running**.
- note: when in CV-address mode, the `CV adr. range` setting adjusts the resolution of the chosen input (CV1-CV4), relative to the length of the sequence (ie, you’ll either need `5V` or `10V` to move through the entire span of the sequence).
    
- the secondary/aux. clock inputs (TR2, TR4) can be used to either **cycle** through the sequences (see above), or they can be selected as a **reset/mute** signal (`reset/mute`). the options are: `RST2`, `RST4` (reset) and `=HI2`, `=LO2`, `=HI4`, and `=LO4` (mute when aux. clock goes high, respectively low.). NB: When using a clock divider or the like (or anything that will introduce latency relative to the main clock inputs TR1/TR3), it can/will make sense to increase the trigger-to-processing latency (see the `Trigger delay` setting). This will make sure the auxiliary trigger (or control voltage) is processed within the current update window. (Also see _Quantermain_ above for an extended discussion of the trigger delay setting).


### Available settings (per-channel)

|Setting|Meaning|
|---|---|
|`scale`|current scale|
|`--> edit`|edit scale mask (for details see [here](https://ornament-and-cri.me/user-manual-v1_3/#active-note-scale-mask-and-scale-editing))|
|`sequence #`|select sequence `#1 - #4`|
|`--> edit`|edit sequence|
|`playmode`|chain sequences (`SEQ+x`), advance by trigger (`TR+x`), or CV-address|
|`direction`|forward, reverse, pendulum1, pendulum2 (repeat first/last), random, Brownian|
|`-->brown prob`|(if `direction`=`brwn` (Brownian)) probability (_p_) that sequence direction (up or down will reverse on the next step, 0 means _p_=0, 255 means _p_=1)|
|`mult/div`|set clock multiplier / divider|
|`octave`|offset octave|
|`aux. mode`|aux channel output: `gate` (see note below), `copy`, `AD`, `ADR`, `ADSR` (the last three are envelopes, see below for details)|
|`CV adr. range`|toggle `5V/10V` (in CV address mode)|
|`--> pw`|(if `aux. mode`=`gate`) pulse-width of triggers at C/D outputs|
|`--> aux +/-`|(if `aux. mode`=`copy`) offset at C/D outputs (in octaves)|
|`--> att dur`|(if `aux. mode`=`AD`, `ADR` or `ADSR`) duration of the attack segment of the envelope output on the auxiliary channels (C or D outputs)|
|`--> att shape`|(if `aux. mode`=`AD`, `ADR` or `ADSR`) shape of the attack segment of the envelope output on the auxiliary channels (C or D outputs). See the _Piqued_ app for details of envelope segment shapes.|
|`--> dec dur`|(if `aux. mode`=`AD`, `ADR` or `ADSR`) duration of the decay segment of the envelope output on the auxiliary channels (C or D outputs)|
|`--> dec shape`|(if `aux. mode`=`AD`, `ADR` or `ADSR`) shape of the decay segment of the envelope output on the auxiliary channels (C or D outputs). See the _Piqued_ app for details of envelope segment shapes.|
|`--> sus dur`|(if `aux. mode`=`ADSR`) duration of the sustain segment of the envelope output on the auxiliary channels (C or D outputs)|
|`--> sus level`|(if `aux. mode`=`ADR` or `ADSR`) level of the sustain segment of the envelope output (for `ADSR` envelopes), or the level at which the the decay segment transitions into the release segment (for `ADR` envelopes) on the auxiliary channels (C or D outputs).|
|`--> rel dur`|(if `aux. mode`=`ADR` or `ADSR`) duration of the release segment of the envelope output on the auxiliary channels (C or D outputs)|
|`--> rel shape`|(if `aux. mode`=`ADR` or `ADSR`) shape of the release segment of the envelope output on the auxiliary channels (C or D outputs)|
|`--> loops`|(if `aux. mode`=`AD` or `ADR` or `ADSR`) sets the number of loops for each envelope output on the auxiliary channels (C or D outputs), defaults to 1 (see also the Tip below)|
|`reset/mute`|select reset/mute source (mute being more like ‘pause’)|
|`clock src`|choose channel clock source (`TR1` or `TR3`)|
|`trigger delay`|trigger-to-processing latency (accessed via CV menu): details see _Quantermain_ above|

**Note**: There is a bug in the v1.3.3 firmware which prevents the gate auxiliary output from working unless the `mult/div` for that channel is set to `x2` or above. This bug was fixed in v1.3.3b firmware.

### CV menu
- **enter** the CV menu by pushing the down button:
    - use the right encoder to assign CV input channels 1-4 to a channel parameter (currently available parameters are: `transposition` (in octaves), `scale mask`, `sequence number` (1-4), `sequence length`, `direction`, `multiplier/divisor` and `pulsewidth`. The quantiser/sampling `trigger delay` (see notes for `Trigger delay` in _Quantermain_) and the `clock src` (clock source) can also be specified in this sub-menu, as well as CV input mappings to these envelope parameters: `att dur`, `dec dur`, `sus lvl`, `rel dur` and `env loops`. By using CV to control `env loops`, a sort of psuedo-ratchetting effect can be obtained. Other envelope behaviours can also be specified (see the _Piqued_ app for details): `att rest`, `att fall gt` and `dec/rel reset`.
    - **return** to the main menu by either pressing the **up** or **down** buttons, or by moving the cursor to an empty field and then pressing the right encoder button.
    - **clear** all mappings (per channel) by long-pressing the down button (in CV menu)


|       | Left Encoder                         | Right Encoder                                                                                                    |
| ----- | ------------------------------------ | ---------------------------------------------------------------------------------------------------------------- |
| TURN  | select channel                       | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited |
| PRESS | re-sync channels (reset)             | Toggle between menu navigation (selection) mode and value editing mode                                           |
| LONG  | copy selected scale to other channel | app selection menu                                                                                               |

|            | Up Button           | Down Button         |
| ---------- | ------------------- | ------------------- |
| PRESS      | return to main menu | return to main menu |
| LONG PRESS | invoke screensaver  | clear CV mappings   |

### Sequence Editor

|       | Left Encoder              | Right Encoder                                                                                                                                                                                                                                                                            |
| ----- | ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| TURN  | select step               | 1) adjust pitch in semitones; **push left encoder switch** while turning to **fine-tune**. 2) adjust sequence **length** by pointing the cursor to the far right, then turn; **push left encoder switch** while turning to also **clear** the sequence mask (when expanding the pattern) |
| PRESS | activate/de-active step   | exit editor                                                                                                                                                                                                                                                                              |
| LONG  | **copy** selected pattern | – (app selection menu)                                                                                                                                                                                                                                                                   |

|            | Up Button                            | Down Button                              | Down Button + Left Encoder                |
| ---------- | ------------------------------------ | ---------------------------------------- | ----------------------------------------- |
| PRESS      | go to next sequence (edit ‘offline’) | go to previous sequence (edit ‘offline’) |                                           |
| LONG PRESS | – (screensaver)                      | **paste** previously copied sequence     | **clear** sequence (including pitch data) |

*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
