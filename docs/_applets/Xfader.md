---
layout: default
---
# Xfader

![Xfader Screenshot](images/Xfader.png)

**Xfader** is a two-input CV mixer with a balance control and two complementary outputs.

### I/O

|        | 1/3            | 2/4            |
| ------ | :------------: | :------------: |
| TRIG   | Fade Left      | Fade Right     |
| CV INs | Input 1        | Input 2        |
| OUTs   | Mixed Output 1 | Mixed Output 2 |

### UI Control
* Encoder: Adjusts the balance between CV 1 and CV 2
* Press: Toggle show gate-fade rate adjustment

The balance indicator, controlled by the encoder, shows the relationship between the CV 1 and CV 2 signals' relative levels at Output A/C. With the indicator all the way to the left, only CV 1's signal appears at A/C; all the way to the right, only CV 2's signal appears at A/C.

Output B/D is the complement of A/C. That is, the relative levels of CV 1 and CV 2 are the opposite, so that with the balance indicator at the left, only CV 2's signal appears at B/D, etc.

In Xfader, all signals are attenuated by 50% before being sent to the mix.

### NEW!
While TR 1/3 are fed a high gate signal, the fader moves left at an adjustable rate. While TR 2/3 are gated, the fader moves right. If the rate is 0 %/sec or both gates are high simultanously, they have no effect.

AuxButton enables the fader to spring-return to adjustable "center" when gates are released.
