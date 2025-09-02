---
layout: default
---
# TrigSeq

![TrigSeq screenshot](images/TrigSeq.png)

[Video demo](https://youtu.be/qPRspJApd1Y)

**TrigSeq** - now titled "Trig8x2" for clarity - is a dual eight-step trigger sequencer with shared Clock and Reset.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIG   | Clock    | Reset    |
| CV INs | Swap Channels if > 3v | Offset position for Reset (square outline)   |
| OUTs   | Ch A Trigger    | Ch B Trigger    |


## UI Parameters
* Edit steps (cursor selects 4 steps at a time)
  - Ch 1 1st half
  - Ch 1 2nd half
  - Ch 2 1st half
  - Ch 2 2nd half
* Ch 1 length
* Ch 2 length

### Step Editing

The cursor appears over four steps at a time. Turning the encoder selects a binary representation of the bit pattern. Each half of the sequence has sixteen possible values. That is, turning the encoder clockwise will cycle through

* (silence)
* ---X
* --X-
* --XX
* -X--
* -X-X
* -XX-
* -XXX
* X---
* X--X
* X-X-
* X-XX
* XX--
* XX-X
* XXX-
* XXXX
