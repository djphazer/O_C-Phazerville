---
layout: default
---
# CopierMaschine

Works much the same as the original o_C / quantising **ASR** firmware. Except, the mode now incorporates some of the new/improved quantiser features, including a larger selection of **(editable) preset scales** as well as advanced user-scale-edit options (for details, see [Quantermain](Quantermain). 

### Controls

|       | Left Encoder                                                    | Right Encoder                                                                                                                                          |
| ----- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| TURN  | Select scale in main menu; move cursor in scale edit menu       | Navigation mode: move up and down through the menu items. Edit mode: increase or decrease the value being edited; move scale "mask" in scale edit menu |
| PRESS | Activate scale in main menu; add/remove note in scale edit menu | Toggle between menu navigation (selection) mode and value editing mode                                                                                 |
| LONG  |                                                                 | App selection menu                                                                                                                                     |

|            | Up Button                | Down Button                |
| ---------- | ------------------------ | -------------------------- |
| PRESS      | Transpose up: one octave | Transpose down: one octave |
| LONG PRESS |                          |                            |

### I/O

|     | 1           | 2                                    | 3                               | 4                                                     |
| --- | ----------- | ------------------------------------ | ------------------------------- | ----------------------------------------------------- |
| TR  | Clock input | Hold (freeze ring buffer)            | Transpose: Octave up, when high | Transpose: Octave down, when high (overridden by TR3) |
| CV  | Sample in   | Index: ring buffer index (= "delay") | Mask: rotate scale mask         | Transpose: octave up/down (-4/+4)                     |
| OUT | ASR output  | ASR output                           | ASR output                      | ASR output                                            |

### Description

In essence, then, ASR mode works as a cascaded, four-stage sample-and-hold module (see [here](http://www.cyndustries.com/synapse/synapse.cfm?pc=35&folder=sept1976&pic=19) for a classic exposition). Feed a pulse into the left-most digital input (**TR1**), and some CV signal (LFO, ADSR, etc) into the leftmost CV input (**CV1**): on receiving a/the clock, the DAC outputs will be updated, **ASR-style**: the sampled value will be present at output A, the previous sample values shifted down the remaining outputs B, C, and D. 

The ASR mode features additional parameters, including 

 - a rudimentary **delay** (= controlled via the `index` parameter (CV2))
 - **scale 'mask'** rotation (CV3)
 - **hold** (= "freeze" the sample buffer) (TR2), and 
 - **transposition** (in octaves) (TR3, TR4, CV4) 

The `index` parameter works as a delay, sort of: the ASR outputs the sampled values `S[x]` stored at the buffer locations `index * output-stage`, ie `A = S[i*1]`, `B = S[i*2]`, `C = S[i*3]`, `D = S[i*4]`. The default setting or increment is 0 (internally `i = 1`), in which case things boil down to standard ASR behaviour: 

 `A = S[1]`, `B = S[2]`, `C = S[3]`, `D = S[4]`

If the index parameter was set to, say, `i = 8`, the ASR will now output the values stored in the buffer at locations `A = S[8]`, `B = S[16]`, `C = S[24]`, `D = S[32]`, thus delaying output A by 8 clocks, B by 16 clocks, and so on. Ie, modulating the `index` parameter doesn't just delay but allows to create different patterns (based on the contents of the buffer). The size of the buffer is 256. When the `hold` input goes high (using a gate or the like), no further samples will be acquired; when clocked, the four outputs then simply cycle through what's already the buffer (with `index`= 1, the note value at `D` will reappear at output `A`, etc)

Please see the discussion of the `Trigger delay` menu selection in the _Quantermain_ app documentation below - the same considerations apply to the `Trigger delay` setting in _CopierMaschine_.

A brief video demonstration of the _CopierMaschine_ in action, with live audio, is [available on YouTube](https://www.youtube.com/watch?v=9YAf5QNBeoI).

### Available settings (per-channel)

| Setting         | Meaning                                                                             |
| --------------- | ----------------------------------------------------------------------------------- |
| `scale`         | Current scale                                                                       |
| `Root`          | Root note for scale                                                                 |
| `Active notes`  | "scale mask" / active note pattern in the selected scale                            |
| `Index`         | ring buffer index (= "delay") amount                                                | 
| `Mult/att`      | CV "gain", multiplies incoming CV sample value by selected value (range: 0.1 - 2.0) |
| `Trigger delay` | sets the delay between receiving a trigger (for details see QQ below)               |

### Scale edit: 

see [here](Quantermain#Active) for note (scale mask) and scale editing) (user-scales are shared across apps).

### Screensaver display

Four little "Arabesque" patterns, representing the pitch CV output on each of the four channels.

*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
