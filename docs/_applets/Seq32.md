---
layout: default
---
# Seq32

![Seq32 Screenshot](images/Seq32.png)

(updated in v1.8.2) The **Seq32** applet reinterprets the 8 pattern storage slots from the original **Sequins** app as 32-step note sequences. (This will corrupt your Sequins patterns, if you have any!)

Each step has a bipolar range of -32 to +31 (scale degrees) and can be Muted or Accented. Accented steps output a longer gate (25%, 50%, 75% or 100% aka tied to the next note), with optional glide. Mute means no gate is sent and the Pitch output doesn't change.

_**Important:**_ In addition to storing applet settings in a Preset, a manual [EEPROM Save](Saving-State) is necessary to save changes to the global patterns!

### I/O

|        |         1/3        |        2/4          |
| ------ | :----------------: | :-----------------: |
| TRIG   |        Clock       |       Reset (Rest in Rec. Mode)        |
| CV INs | Transpose<br>(or Pitch CV for Record) | Sequence Select<br>(or Accent gate) |
| OUTs   |        Pitch       |    Gate/Trigger     |

### UI Parameters
* Pattern select #1-8
* Length, per pattern
* Transpose
* Quantizer scale settings (popup editor)
* Gate Length for Accented steps / Glide toggle (AuxButton)
* Record mode
  - Clock trigger records note and advances
  - Reset trigger records rest (muted note) and advances
  - Encoder moves playhead
  - AuxButton toggles mute for current step
* Edit notes

### AuxButton Actions
With certain parameters highlighted for editing, the select button will execute a secondary action:
* Pattern # - Clear pattern
* Length - Randomize current pattern completely
* Transpose - Randomize pitches of current pattern, using transpose as the range
* Accent Gate Length - toggles a slight portamento on accented notes
* Record Mode - mute/unmute current step

### Step Editing
Use the encoder to move the cursor to a step, and push to toggle editing the note value. While editing, use the AuxButton to toggle Mute. Double-click to toggle Accent, indicated by a solid square.

### Record mode
When the small record icon is engaged, stepwise CV recording is active. When a Clock trigger is received, CV1 is captured as pitch, CV2 is measured as Accent (gate threshold at 2v), and the sequencer advances. A Reset trigger does the same, except it inserts a muted step (a rest) before advancing.

While record mode is engaged, the encoder will move the playhead, and AuxButton toggles mute for the current note. Pressing the encoder disengages record mode.
