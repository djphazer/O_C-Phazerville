---
layout: default
---
# WAV Recorder

Records live audio from the signal chain directly to the SD card as a standard 16-bit PCM `.WAV` file. Sits transparently in the chain — audio passes through unmodified at all times, including while recording.

Files are written to the SD card root and named `R001.WAV`, `R002.WAV`, up to `R999.WAV`. Each recording starts a new file; existing files are never overwritten.

A 1 MB PSRAM buffer sits between the audio engine and the SD card, absorbing the occasional multi-hundred-millisecond write stall that SD cards produce during wear-levelling. This keeps the audio path clean even on slower cards.

---

### Getting started

1. Insert a FAT32-formatted SD card.
2. Place **WavRec** in any processor slot (1–4) in the signal chain — typically near the end.
3. Patch a gate or trigger to the **RecTrig** input, or use the **Aux** button.
4. A rising edge starts recording. Another rising edge (or Aux button press) stops it.

The file is finalised and closed the moment recording stops. It is immediately readable by any standard audio application.

---

### Display

```
  IDLE  [CLK1]       ←  state + trigger source
  R042.WAV           ←  next filename to be created
  AUX/CV:            ←  usage hint (idle) / recording timer (active)
  rec/stop
```

While recording:
```
  REC   [CLK1]       ←  state label inverted
  R042.WAV           ←  filename currently being written
  00:03.271          ←  elapsed time  MM:SS.mmm
  ████████░░░░░░░    ←  VU peak bar (clips when last 6px invert)
```

The applet name in the slot selector reads **`WavRec*`** (blinking) while recording — visible even when browsing other slots.

---

### Parameters

| Parameter | Description |
|-----------|-------------|
| **RecTrig** | Gate or trigger input that toggles recording on/off. Assignable to any CV or digital input via the standard input map editor. |

**Aux button** always toggles recording regardless of the RecTrig assignment — useful for one-handed operation without a patch.

---

### Stereo vs mono

In **mono** slots (left or right channel independently), one mono `.WAV` file is recorded. In a **stereo** slot, one 2-channel interleaved stereo `.WAV` file is written — not two separate files.

---

### Status indicators

| Display | Meaning |
|---------|---------|
| `IDLE` | Standby. Shows next filename. |
| `REC` (inverted) | Recording in progress. Shows timer and VU bar. |
| `LOCKED` | Another instance of WavRec is already recording. Only one file can be written at a time. |
| `OVERFLOW!` | The PSRAM ring buffer filled before data could be flushed — audio data was lost. This should not occur in normal use; if it does, try a faster SD card. |
| `No PSRAM` | The Teensy 4.1 has no external PSRAM. Recording is not available. |
| `NO SD CARD!` | No SD card detected. |

---

### Savestate behaviour

Recording stops automatically — and the file is properly closed — whenever a preset is saved or loaded. The current take is preserved; the next recording will start a new file.

---

### File recovery after power loss

If the unit loses power during a recording, the `.WAV` file body contains valid sample data but the RIFF header size fields will read as zero. Most DAWs will reject or misread the file. Recovery options:

- Hex-edit bytes 4–7 and 40–43 to contain the correct file and data sizes (file size − 8, and file size − 44 respectively, both little-endian).
- Use `ffmpeg`: `ffmpeg -f s16le -ar 44100 -ac 1 -i Rxx.WAV recovered.wav` (use `-ac 2` for stereo).

---

### Notes

- Files must be 16-bit PCM. Sample rate is always 48000 Hz to match the Teensy 4.1 audio engine.
- Up to 999 recordings can be stored (`R001.WAV`–`R999.WAV`). The applet scans for the next free slot on load.
- The applet occupies processor slots 1–4 only. It cannot be used as a source (slot 0).
- In a dual-mono setup (WavRec on both left and right sides simultaneously), only the first instance to trigger will record. The other shows `LOCKED` until the first stops.

---

### Credits

Authored by Andy Jenkinson 'uglifruit' using ClaudeCode.
Released under the MIT License.
