# O_C-Phazerville Fork — Claude Context

## Repository
- Fork of `djphazer/O_C-Phazerville` at `uglifruit/O_C-Phazerville`
- Target hardware: **Teensy 4.1** (build env `T41`)
- Build system: **PlatformIO** via `~/.platformio/penv/Scripts/pio.exe`
- Build command: `cd software && ~/.platformio/penv/Scripts/pio.exe run -e T41`
- Hex output: `software/.pio/build/T41/*.hex`

## Branch Strategy
Each applet lives on its own branch off `phazerville` (the upstream main). This keeps diffs minimal and PRs clean for upstream submission.

| Branch | Purpose |
|--------|---------|
| `phazerville` | Upstream base — do not commit here |
| `UgliApp` | CV passthrough + voltage display applet |
| `MarkoV` | Markov chain melodic generator applet |

To add a new applet branch: `git checkout phazerville && git checkout -b <name>`

## Adding a New Applet
Two files must be changed, everything else is self-contained in the applet header:

1. **`software/src/applets/<Name>.h`** — the applet class (see below)
2. **`software/src/hemisphere_config.h`** — two additions:
   - `#include "applets/<Name>.h"` (alphabetical order near line 104)
   - `DeclareApplet<Name>{id, category}` in the `AppletRegistry reg{...}` block
   - ID must be unique across all applets (highest in use: 93 for MarkoV)
   - Category bitmask: `0x01`=LFO, `0x02`=Sequencer, `0x04`=Clock, `0x08`=Pitch, `0x10`=Utility, `0x20`=MIDI, `0x40`=Logic, `0x80`=Audio

## Applet Class Structure
All applets extend `HemisphereApplet`. Pure virtual methods that must be implemented:

```cpp
const char* applet_name()          // max 9 chars
void Start()                       // one-time init
void Controller()                  // runs every tick (~16kHz ISR)
void View()                        // render 64x64 display region
void OnEncoderMove(int direction)  // +1 or -1
uint64_t OnDataRequest()           // serialize state for save
void OnDataReceive(uint64_t data)  // deserialize state
// SetHelp() in protected:         // I/O label strings
```

Key optional overrides: `OnButtonPress()`, `AuxButton()`, `Reset()`

## Key API Reference

### I/O
```cpp
bool Clock(int ch)                 // rising edge on digital input ch (0 or 1)
bool Gate(int ch)                  // current gate state
int In(int ch)                     // raw CV input (0 to HEMISPHERE_MAX_INPUT_CV)
float InF(int ch)                  // normalized CV 0.0-1.0
void Out(int ch, int cv)           // set DAC output
void ClockOut(int ch)              // emit trigger pulse on output ch
void StartADCLag(int ch)           // call after Clock() to delay CV read
bool EndOfADCLag(int ch)           // true ~33 ticks after StartADCLag
```

### Quantizer
```cpp
int Quantize(int ch, int cv)                          // quantize cv through this hemisphere's channel ch
int HS::Quantize(int abs_ch, int cv)                  // quantize through absolute channel
int HS::QuantizerLookup(int abs_ch, int note_number)  // note index → CV
void HS::QuantizerEdit(int abs_ch)                    // open quantizer editor UI
HS::qview = channel; HS::PokePopup(QUANTIZER_POPUP);  // show quantizer popup
```

### Maths helpers
```cpp
int Proportion(int value, int max_value, int max_out) // scale value
void Modulate(T &param, int ch, int min, int max)     // CV-modulate a param
Pack(data, PackLocation{bit, width}, value)           // serialize to uint64_t
Unpack(data, PackLocation{bit, width})                // deserialize
```

### Display (64x64 per hemisphere, x auto-offset)
```cpp
gfxPrint(x, y, val)               // text/numbers at position
gfxPos(x, y)                      // set cursor; use graphics.printf() after
gfxRect(x, y, w, h)               // filled rectangle
gfxLine(x, y, x2, y2)             // line
gfxFrame(x, y, w, h)              // outline rectangle
gfxInvert(x, y, w, h)             // invert region
gfxCursor(x, y, w)                // solid underline cursor
gfxSpicyCursor(x, y, w)           // dotted underline cursor (hints at special action)
gfxIcon(x, y, bitmap)             // 8x8 icon
bool EditMode()                    // true when encoder button is held/active
bool CursorBlink()                 // true during blink-on phase
void MoveCursor(cursor, dir, max)  // navigate cursor with bounds
```

### Constants
```cpp
ONE_OCTAVE = 1536              // CV units per octave (12 << 7)
HEMISPHERE_MAX_INPUT_CV        // max ADC CV value (~6V range)
QUANT_CHANNEL_COUNT            // number of quantizer channels available
io_offset                      // hemisphere's base channel offset (0 or 2)
```

## MarkoV Applet
**File:** `software/src/applets/MarkoV.h`
**Branch:** `MarkoV`
**Registry ID:** 93, category `0x02` (Sequencer)

### Concept
First-order Markov chain melodic generator. 8 states represent scale degrees. On each clock, the next state is chosen by weighted random selection from the current row of a transition matrix. Three profiles define the matrix weights.

### I/O
| Jack | Function |
|------|---------|
| Dig 1 | Clock — advances the chain |
| Dig 2 | Reset — short press = root (state 0), long press (5000 ticks) = random state |
| CV 1 | Chaos offset — adds to encoder-set chaos_base; higher V = flatter distribution |
| CV 2 | Transpose — raw V/Oct offset added after quantization |
| Out A | Quantized pitch |
| Out B | Trigger pulse — fires only when quantized pitch actually changes |

### Parameters (encoder-navigated, 3 cursors)
| Cursor | Param | Range | Notes |
|--------|-------|-------|-------|
| 0 | Matrix | S / T / J | Pentatonic Stability, Chromatic Tension, Jazz |
| 1 | Scale | Q1–Q4 | Absolute quantizer channel; Aux opens editor |
| 2 | Chaos | 0–100% | Baseline; CV 1 adds on top |

### Chaos Implementation
Integer fixed-point only (no floats in hot path):
```
chaos_256 = (chaos_pct * 256) / 100
w[j] = (profile_weight[j] * (256 - chaos_256) + 8 * chaos_256) >> 8
```
At chaos=0: pure profile weights. At chaos=100: all weights = 8 (uniform).
Profile weights use 20:1 ratios (max 22, min 1) so profiles are clearly audible.

### Trigger Logic
Output B compares the **quantized CV value** (`prev_cv`) not the raw Markov state. Two different states that quantize to the same pitch will not fire a trigger.

### Reset & Seed
- Short press Dig 2 → return to `seed` state (repeatable reset point)
- Long press Dig 2 (≥5000 ticks) → pick new `random(NUM_STATES)`, store as `seed`
- Seed is persisted in `OnDataRequest`

### Display Layout
```
Matrix                ← cursor label at top (y=6): "Matrix", "Scale", or "Chaos"
[S]  [Q1]  [42%]     ← param line (y=15), cursor underlines below (y=23)
─────────────────     ← separator (y=25)
█  ██ █  ██ █  ██    ← scrolling bar graph, 8 bars, oldest→newest left→right
─────────────────     ← baseline (y=63)
```

### Registry Position
Placed between MidiLoop and hMIDIIn in hemisphere_config.h (alphabetical M section).

### Data Persistence (OnDataRequest bit layout)
| Bits | Field | Width |
|------|-------|-------|
| 0–1 | profile | 2 |
| 2–4 | state | 3 |
| 5–6 | qselect | 2 |
| 7–13 | chaos_base | 7 |
| 14–16 | seed | 3 |

## UgliApp Applet
**File:** `software/src/applets/UgliApp.h`
**Branch:** `UgliApp`
**Registry ID:** 92, category `0x10` (Utility)

Minimal CV passthrough diagnostic. Reads both CV inputs, passes them to matching outputs, displays live voltage on screen. No parameters, no persistence.
