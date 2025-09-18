---
layout: default
---
# MultiScale

![Screenshot 2024-06-13 15-14-50](https://github.com/djphazer/O_C-Phazerville/assets/109086194/bcdb67e0-b545-4386-b35d-8270a87b00a7)

Similar to [Scale Duet Quantizer](ScaleDuet), **MultiScale** provides 4 separate editable scale masks, with CV control to switch between them. Unlike ScaleDuet, this applet outputs a trigger on B/D when the active scale changes. Eventually, the functionality of the two applets will be merged.

The number next to the Play icon indicates the scale mask currently playing (selectable by CV input 2/4). The number next to the Pencil icon indicates the scale mask currently being edited and displayed in the UI.

Operation is continuous by default, until a Clock is received.

## I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock | Unclock (switch back to continuous) |
| CV INs | Input Signal | Scale select |
| OUTs   | Quantized Pitch | Trigger when Scale select changes |

## UI Parameters
- Scale selection for edit
- Enable/disable notes in currently editable mask

### Credits
Authored by zerbian
