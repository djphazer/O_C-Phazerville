---
layout: default
---

# FrogSeq

![FrogSeq screenshot](images/FrogSeq.png)

**FrogSeq** is a generative note sequencer inspired by Frogger. Move the frog through three lanes of traffic, where collisions create musical events by changing notes, toggling gates, or triggering ratchets. The frog never dies — getting hit is part of the music.

### I/O

|        | 1/3 | 2/4 |
| ------ | :-: | :-: |
| TRIGs  | Clock | Restore |
| CV INs | Frog X | Frog Y |
| OUTs   | Pitch CV | Trigger |

### Main Page

The main page displays the frog, traffic lanes, and current note.

- **Frog:** Move through the lanes and the Safe zone using the encoder.
- **Movement direction:** Switch between horizontal and vertical movement when controlling the frog.
- **Random:** Randomizes the note sequence.
- **Note Seq:** Move the cursor past Random to access the note sequencer page.

Traffic continues while editing as long as a valid clock is running.

### Collisions

Collisions between the frog and traffic generate musical events. Each traffic lane has its own direction, speed, and flow settings.

- **Note:** Changes a sequence step's note by a random semitone interval.
- **Gate:** Toggles the step's mute state.
- **Ratchet:** Repeats a note to create a ratchet effect. The number and timing of repeats depend on the traffic lane's speed.

The collision effects interact with the 16-step note sequence, creating changing note patterns and rhythms.

### Note Sequencer Page

The Note Seq page contains a 16-step note sequencer and settings for the three traffic lanes.

**Sequence steps**

- Select a step to edit its note.
- Steps can be muted or unmuted.
- The playback indicator shows the current step.

**Traffic lanes**

Each lane has three settings:

- **Direction:** Sets the direction of traffic.
- **Speed:** Selects `/2`, `x1`, `x2`, `x3`, or `x4`.
- **Flow:** Adjusts traffic flow for that lane.

Different lane settings create different collision patterns and rhythmic variations.

### Sequence Storage

FrogSeq provides eight sequence slots. The sequence menu supports **Load**, **Write**, and **Init** operations. A System Save is required for saved slots to persist through a power cycle.
