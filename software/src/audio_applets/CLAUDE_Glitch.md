# CLAUDE notes — GlitchApplet

Reference doc for future sessions working on the Glitch stutter/ratchet applet.
Read this before touching either file below.

## Files

| File | Role |
|------|------|
| `software/src/Audio/AudioEffectGlitch.h` | AudioStream DSP core — header-only |
| `software/src/audio_applets/GlitchApplet.h` | Applet UI wrapper — header-only |
| `software/src/hemisphere_audio_config.h` | Registration (tuple pools) |
| `docs/_audio_applets/Glitch.md` | User-facing documentation |

Branch: `GlitchDSP` (off clean `phazerville`, not `StutterDSP` — that had AdvKrpsStrng merged in and overflowed RAM1).

---

## Architecture

### DSP tier: `AudioEffectGlitch`

Inherits `AudioStream` (1 input, 1 output). Header-only.

Owns a `GlitchBuffer<int16_t>` (thin subclass of `ExtAudioBuffer<int16_t>`) stored
inline — no heap allocation beyond `g_buffer.Acquire()`.

`GlitchBuffer` exists *only* to expose two protected members of `AudioBuffer`:
- `write_ix` → `GetWriteIx()` (needed to compute `slice_start` at hold onset)
- `buffer[]` → `ReadAt(idx)` (absolute-index read inside the stutter loop)

`ExtAudioBuffer::buffer` is `nullptr` until `Acquire()` is called. `IsReady()`
checks this. Always guard UI and DSP paths with `IsReady()`.

#### Volatile parameter handoff

`update()` runs in the audio interrupt (higher priority than the ISR that calls
`Controller()`). Parameters written by `Controller()` are `volatile`:

```cpp
volatile bool    hold_
volatile size_t  slice_samples_
volatile uint8_t mode_
volatile uint8_t ratchet_
```

Each is read once into a `const` local at the top of `update()` and then that
local is used throughout — this ensures consistency within a single 128-sample block.

#### Hold onset

Detected by `cur_hold && !was_held_` at the *top* of `update()`, before the
`g_buffer.Write(in)` call. This means `write_ix` still points to where the freshest
content ends, so `slice_start_ = (wi + buf_size - cur_slice) % buf_size` captures the
most recent `cur_slice` samples.

Do NOT move the onset check below `g_buffer.Write()`.

#### loop_len vs cur_slice

These are the same in FWD/REV/PING modes. They diverge only in RATCHET mode:

```cpp
const size_t loop_len = (cur_mode == MODE_RATCHET && cur_ratchet > 1)
    ? std::max(cur_slice / (size_t)cur_ratchet, (size_t)AUDIO_BLOCK_SAMPLES)
    : cur_slice;
```

`cur_slice` is still used for `slice_start_` capture (so the full clock slice is
recorded as context). `loop_len` is used for all loop mechanics: fade boundary
calculation, pos wrap, and ping-pong toggle. Never swap them.

Minimum `loop_len` is `AUDIO_BLOCK_SAMPLES` (128) — the floor prevents degenerate
zero-length loops when `cur_slice / cur_ratchet` rounds to zero.

#### No modulo in the hot loop

The 128-sample inner loop uses single-branch subtraction wrap only:

```cpp
// FWD:
read_ptr = slice_start_ + p;
if (read_ptr >= buf_size) read_ptr -= buf_size;

// REV:
read_ptr = slice_start_ + cur_slice - 1 - p;
if (read_ptr >= buf_size) read_ptr -= buf_size;
```

`slice_samples_` is constrained in `setSliceSamples()` to `<= buf_size / 2`, so
there is at most one wrap, and one subtraction always corrects it. Do not add modulo.

The one `%` that does appear (`slice_start_ = (wi + buf_size - cur_slice) % buf_size`)
is outside the loop and acceptable.

#### Micro-fades

64-sample linear ramps at loop start and end suppress clicks. Skipped when
`loop_len < 128` (would overlap). Fade is a float multiplied onto the int16 sample
before `Clip16()` — cheap enough for the hot path.

---

### Applet tier: `GlitchApplet<Channels>`

Mirrors the `DelayApplet<Channels>` pattern throughout.

#### Per-channel struct `GlitchChannel`

Owns `AudioEffectGlitch glitch_stream` and `AudioMixer<2> wet_dry_mixer`.
The mixer slot convention is:
- `DRY_CH = 0`
- `WET_CH = 1`

`GlitchChannel` constructor passes the correct buffer size to `glitch_stream`
based on `external_psram_size`. Do not allocate the buffer in the constructor
itself — that happens in `Start()` via `glitch_stream.Acquire()`.

#### Clock tracking

```cpp
clock_count++;
if (clock_source.Clock()) {
    clock_base_secs = clock_count / 16666.0f;
    clock_count = 0;
}
```

`clock_source.Clock()` returns true on a rising edge. `clock_count / 16666.0f`
converts ISR ticks (16666/sec) to seconds. Default `clock_base_secs = 0.5f` = 120 BPM
until the first clock edge arrives.

#### Mode and ratchet CV modulation

Modulation is computed in `Controller()`, clamped, and sent to the DSP:

```cpp
int eff_mode = constrain(
    (int)mode + (int)roundf(mode_cv.InF() * (NUM_MODES - 1)),
    0, NUM_MODES - 1);
uint8_t eff_ratchet = (uint8_t)constrain(
    (int)ratchet + (int)roundf(ratchet_cv.InF() * 5.0f),
    1, 6);
```

`InF()` returns a float in roughly [-1, +1] relative to attenuversion. Full positive
sweep covers the whole 0–3 mode range or 1–6 ratchet range.

#### Cursor navigation skip

When `mode != MODE_RATCHET`, cursor movement skips over `RATCHET` and `RATCHET_CV`
by stepping an extra position in the requested direction. This keeps the encoder
feeling natural when those rows aren't visible. If you add more conditionally-visible
rows, extend this pattern.

The `View()` method also conditionally renders either the ratchet row (y=45) + mix row
(y=55), or just the mix row (y=45), based on the live `mode` value.

#### Display: InputName() for DigitalInputMap cursors

`clock_source` and `hold_input` pass `.InputName()` as the `str` argument to
`gfxEndCursor`. This replaces the icon with a legible name (e.g. `CL1`, `TR2`) in
the cursor box. CVInputMap sources already do this via the last argument convention —
keep all three consistent.

#### AuxButton / manual hold

`manual_hold_ ^= 1` toggles a bool. `Controller()` ORs it with the hardware gate:

```cpp
bool held = hold_input.Gate() || manual_hold_;
```

`View()` inverts the `Hld:` label when `manual_hold_` is true as a persistent indicator.

#### Data packing

```cpp
data[0] = PackPackables(pack<3>(div), pack<2>(mode), pack<3>(ratchet), mix);
data[1] = PackPackables(clock_source, hold_input, mix_cv);
data[2] = PackPackables(mode_cv, ratchet_cv);
```

`data[0]`: `div` needs 3 bits (0–5), `mode` needs 2 bits (0–3), `ratchet` needs 3 bits
(1–6), `mix` fills remaining. `data[2]` is new — was not used before ratchet was added.
`CONFIG_SIZE = 4` so slots 0–3 are available; slot 3 is still free.

---

## Registration in `hemisphere_audio_config.h`

```cpp
// mono_processors_pool tuple:
GlitchApplet<MONO>,

// stereo_processors_pool tuple:
GlitchApplet<STEREO>,
```

Both pools are `DMAMEM` so the large `GlitchChannel` members (including the inline
`GlitchBuffer` struct — not the PSRAM allocation itself, just the struct wrapper) live
in uncached DMAMEM, which is fine.

---

## RAM budget

Build on `GlitchDSP` off clean `phazerville`:

```
RAM1: free for local variables: 24640 bytes
RAM2: free for malloc/new:      345184 bytes
FLASH: free:                    7681028 bytes
```

**Do not branch GlitchDSP off StutterDSP or any branch containing AdvKrpsStrng.**
AdvKrpsStrng pushes the code size to just below a 32KB ITCM alignment boundary; any
additional code (even ~2 KB) crosses it, padding the binary by ~32 KB and overflowing
RAM1 by ~9 KB. Build on a clean `phazerville` base.

---

## Build command

```bash
cd software && ~/.platformio/penv/Scripts/pio.exe run -e T41
```

Check the `teensy_size` lines at the end of the output. Warn if RAM1 free drops below ~8 KB.

---

## Known gotchas

1. **`IsReady()` is not `const`** in `ExtAudioBuffer` — causes a "discards qualifiers"
   warning when called on a `const` object. Not an error; build succeeds. Fix would
   require adding `const` to `ExtAudioBuffer::IsReady()` upstream.

2. **`write_ix` after `Write()`** — `Write(in)` advances `write_ix` by 128 samples.
   The hold onset check reads `write_ix` *before* `Write()` so it captures the end of
   the last fully-written block, not the next one. This is intentional.

3. **Ping-pong `going_fwd` hoisting** — `going_fwd` is hoisted before the sample loop
   for branch prediction. It gets updated inside the loop only on slice wrap (rare).
   This is correct: `ping_fwd_` persists across blocks and only flips at wrap.

4. **Static constexpr array ODR** — C++14 requires out-of-line definitions for
   `static constexpr` array members of a template class. They are at the bottom of
   `GlitchApplet.h`. If you add a new `static constexpr` array member, add an
   out-of-line definition there too, or the linker will warn.

5. **RATCHET mode cursor skip direction** — the skip logic steps `next += direction`
   a second time when landing on a ratchet row in non-RAT mode. This assumes the
   encoder always moves by 1. If encoder acceleration is ever added, revisit.
