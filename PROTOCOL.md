# Phazerville XLOC — Host Protocol Reference

**Canonical spec** for every host-side tool that talks to this firmware
(desktop viewer, WebSerial viewer, iPad app, DataStrm web app). The firmware is
the source of truth: if you change a command or frame format here, update this
file in the same commit, and bump the tools in
[calsynth/phazerville-tools](https://github.com/calsynth/phazerville-tools) to match.

Firmware source of record:
- Serial + remote control: `software/src/Main.cpp` (`loop()` serial handler, `RemoteControl()`)
- Serial frame render: `software/src/quad_capture.h`, `software/src/apps/Quadrants.h`
- MIDI SysEx transport: `software/src/quad_capture_midi.h`
- DataStrm: `software/src/applets/DataStrm.h`, `software/src/apps/DataStream.h`

## Transports at a glance

| Tool | Transport | USB type needed |
|---|---|---|
| Desktop viewer (`phazerville_quad.py`) | USB **Serial** (CDC) | `USB_MIDI_SERIAL` |
| WebSerial viewer (`phazerville_quad.html`) | USB **Serial** (CDC) | `USB_MIDI_SERIAL` |
| iPad app | USB **MIDI SysEx** (sub-id `0x7A`) | `USB_MIDI_SERIAL` |
| DataStrm web app | Musical USB **MIDI** (ch 1–8) | `USB_MIDI_SERIAL` |

`-DUSB_MIDI_SERIAL` provides MIDI **and** Serial, so one build serves all four.
For the +audio build use `-DUSB_MIDI_AUDIO_SERIAL`. With a MIDI-only USB type
(`-DUSB_MIDI`) the Serial transport is dead (`if (Serial ...)` is false) and the
desktop/web viewers get nothing.

**Build flag:** all capture/remote-control hooks in `Main.cpp` compile only when
`-DQUAD_CAPTURE` is defined (set in `platformio.ini` alongside
`-DUSB_MIDI_SERIAL`). Without it the firmware builds like stock Phazerville —
no extra serial commands, no SysEx `0x7A` handler. If a build ignores viewer
commands, check this flag first. (DataStrm is independent — plain musical MIDI,
always available when the applet is enabled.)

---

## 1. Serial (CDC) — desktop & WebSerial viewers

Open the module's USB serial port (9600 baud, though CDC ignores it). Host sends
**one ASCII command byte**; some commands stream a reply.

### Display frame requests → reply = ASCII hex + `\n`
Reply is the framebuffer as hex, **2 uppercase chars per byte**, then a newline.

| Byte | Renders | Bytes | Hex chars | Notes |
|---|---|---|---|---|
| `Q` | Quadrants 4-up, 128×128 | 2048 | 4096 | only when Quadrants is the active app |
| `A` | Audio DSP stack, 128×64 | 1024 | 2048 | Quadrants only |
| `M` | MIDI map page, 128×64 | 1024 | 2048 | Quadrants only |
| any other (e.g. `S`) | stock live OLED, 128×64 | 1024 | 2048 | works in any app |

`Q`/`A`/`M` reply with **nothing** if Quadrants isn't the current app — negotiate
by sending `Q`; a 4096-char line = quad-capable, a 2048-char line or silence =
fall back to the stock any-byte frame.

### Text-reply commands
| Byte | Reply | Meaning |
|---|---|---|
| `V` | `inL,inR,outL,outR\n` — four floats 0.0–1.0 (4 dp) | audio input + output peak levels (v1.4; hosts may ignore the extras) |
| `T` | `left,right,full,preset\n` — ints | control state (focused slots / view / preset) |

### Oscilloscope — `O` `<slot>` `<sel>` `<win>` (v1.2: dual slots + time base)
Four-byte command: `O`, then slot `A`/`B` (two independent scope channels),
then a source selector, then a time-window digit. Selects that slot's source
and window **and** streams a snapshot of its recent waveform. Quadrants only
(silent otherwise). Firmware: `quad_scope.h`, hooks in `Quadrants.h`.

| Sel | Source | Sample units | Base rate (win `0`) | Window (256 samples) |
|---|---|---|---|---|
| `1`–`8` | CV out A–H (post-conversion DAC value) | millivolts | ~1042 Hz (16.67 kHz ÷ 16) | ~0.25 s |
| `a`–`h` | CV in 1–8 (calibrated pitch value) | millivolts | ~1042 Hz | ~0.25 s |
| `t`–`w` | trigger in 1–4 (gate state) | 0 / 5000 mV | ~1042 Hz | ~0.25 s |
| `L` / `R` | audio out left / right (final stack stream) | raw 16-bit PCM | ~22.05 kHz (44.1 kHz ÷ 2) | ~11.6 ms |
| `i` / `j` | audio in left / right (input object) | raw 16-bit PCM | ~22.05 kHz | ~11.6 ms |

`win` `0`–`3` multiplies the decimation by 1/2/4/8, stretching the 256-sample
window: CV ≈ **0.25 / 0.5 / 1 / 2 s**, audio ≈ **11.6 / 23 / 46 / 93 ms**
(effective rate divides accordingly).

Reply: **512 bytes** as ASCII hex (1024 uppercase chars + `\n`, same chunked
sender as `Q`/`A`) = 256 **big-endian signed int16** samples, oldest first.
The 512-byte length is unique (frames are 1024/2048 B), so hosts can reuse the
frame transaction with `expectBytes = 512`. Switching a slot's source or
window clears its ring, so the first reply after a change is mostly zeros;
poll again. Each slot's audio tap follows the last applet in the audio stack,
so it always shows the signal at the output jack. Host-side: source, rate,
and scaling are known from the request — no header line is sent.

*v1.1 (single slot, `O` `<sel>`) is retired; hosts must send all four bytes.*

### MIDI monitor — `N` (v1.3)
Dumps the firmware's MIDI in/out event ring. Quadrants only (silent
otherwise). Firmware: `quad_midilog.h`; hooks in the Quadrants MIDI pump
(IN, all devices), the `MIDIFrame::Send*` wrappers and MIDI-thru (OUT).
Clock, Active Sensing, and SysEx are not logged (floods / display transport).

Reply: one line, `N:` + **32 events × 12 uppercase hex chars**:

```
seq(4) message(2) dirchan(2) d1(2) d2(2)
```

`seq` is a rolling 16-bit counter, 0 = empty slot, never 0 after wrap. The
ring is dumped in storage order — hosts must track the highest `seq` seen
(wraparound-aware) and render only newer events, sorted by `seq`.
`message` = raw MIDI status (0x80 off / 0x90 on / 0xB0 CC / 0xC0 PC /
0xD0 aftertouch / 0xE0 bend, low nibble clear; 0xFx system). `dirchan`:
bit 7 = direction (1 = OUT), low 7 bits = channel 1–16 (0 = none). Bend
value = `(d2 << 7) | d1`, centered at 8192.

### Action commands (no frame reply)
| Bytes | Action |
|---|---|
| `~` `<sel>` | remote control — inject a button/encoder (next byte = selector, see below) |
| `W` `<slot+33>` | save current state to preset slot (`slot` byte is the slot number **+ 33**) |
| `B` | stream the whole bank file to the host (backup) |

### `~` remote-control selectors (the byte after `~`)
| Sel | Action | Sel | Action |
|---|---|---|---|
| `A` `B` `X` `Y` `Z` | buttons A/B/X/Y/Z | `l` / `r` | left / right encoder **push** |
| `(` / `)` | left encoder −1 / +1 | `[` / `]` | right encoder −1 / +1 |
| `0`–`3` | activate quadrant slot | `<` / `>` | focused slot: prev / next applet |
| `o` / `p` | prev / next preset | `S` / `Q` | enter Audio-Setup view / back to Quadrants |

### Framebuffer byte layout (all hex frames)
Standard weegfx / SSD1306 page format. A 128×64 page = 1024 bytes: 8 horizontal
bands of 8 vertical pixels. Byte `band*128 + x` holds column `x` of that band;
bit 0 = top pixel, bit 7 = bottom. The 128×128 `Q` frame is two stacked pages:
**top 1024 B = applets {NW, NE}**, **bottom 1024 B = applets {SW, SE}**.
Reference decoder: `phazerville_quad.py` (desktop viewer).

---

## 2. MIDI SysEx — iPad / Android (sub-id `0x7A`)

For hosts that get USB MIDI but not USB serial (iPadOS/iOS natively; Android
Chrome via Web MIDI — mobile Chrome has no Web Serial). Mirrors the serial
commands. Manufacturer `0x7D` ("non-commercial"), sub-id `0x7A`. v1.4 extends
the original `Q`/`A`/`V` set with `T`/`O`/`N`/`~`.

```
request  (host → module):  F0 7D 7A <cmd> [args…] F7
reply Q  (module → host):  F0 7D 7A 51 <4096 nibbles> F7    -> 2048 B (128×128)
reply A  (module → host):  F0 7D 7A 41 <2048 nibbles> F7    -> 1024 B (128×64)
reply V  (module → host):  F0 7D 7A 56 <l7> <r7> F7         -> in L/R level 0..127
reply T  (module → host):  F0 7D 7A 54 <left> <right> <full+1> <preset+1> F7
request O:                 F0 7D 7A 4F <slot> <src> <win> F7   (ASCII, as §1)
reply O  (module → host):  F0 7D 7A 4F <1024 nibbles> F7    -> 512 B scope snapshot
reply N  (module → host):  F0 7D 7A 4E <384 nibbles> F7     -> 192 B MIDI-monitor ring
request ~:                 F0 7D 7A 7E <selector> F7          (no reply; remote control)
```

**Nibble encoding:** every framebuffer byte is sent as two 7-bit-safe bytes —
high nibble `(b >> 4) & 0x0F` then low nibble `b & 0x0F` — to respect SysEx's
7-bit data rule. Reassemble as `byte = (hi << 4) | lo`. `Q`/`A` payloads decode to
the same page layout as §1. Musical MIDI is untouched; SysEx requests piggyback on
the Quadrants `usbMIDI.read()` pump and are serviced in the main loop.

---

## 3. DataStrm — musical MIDI (channels 1–8)

Drives the module's 8 outputs from a browser feed. **No SysEx** (that's reserved
for §2), so the two never collide. Slot `n` (0–7) → output A–H → **MIDI channel n+1**.

| Function | MIDI message on channel `n+1` |
|---|---|
| **CV** | 14-bit **pitch bend** |
| **Trig** | **Note On → Note Off** (any note); pulse on rising edge |
| **Gate** | **Note On / Note Off** held |
| **Heartbeat** | app re-sends bend values every **500 ms** → applet shows `LINK`; `NO SIG` blinks ~2 s after the stream stops |

Quadrant → output/channel map (DataStrm applet, 2 outs per quadrant):
quadrant 1 → A/B → ch 1/2 · quadrant 2 → C/D → ch 3/4 · quadrant 3 → E/F → ch 5/6 ·
quadrant 4 → G/H → ch 7/8. Per-output scaling: mode CV/Trig/Gate, V min/V max
(−3.0…+6.0 V, 0.1 V steps), slew 0–100. The full-screen **Data Stream** app exposes
all 8 outputs at once.

---

## Changing the protocol

1. Edit the firmware handler + this file in the **same commit**.
2. Update the matching tool(s) in `phazerville-tools` and note the firmware commit
   they target.
3. Bumping a frame size or command byte is a breaking change — negotiation (send
   `Q`, measure the reply length) is the intended forward-compat mechanism.
