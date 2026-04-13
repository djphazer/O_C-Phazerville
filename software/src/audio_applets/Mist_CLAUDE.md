# Mist Applet — Claude Notes

Two files form this applet:
- `software/src/audio_applets/MistApplet.h` — the HemisphereAudioApplet UI/control layer
- `software/src/Audio/AudioEffectMist.h` — the AudioStream DSP engine

---

## Architecture

```
Input ──► AudioEffectMist (grain_stream) ──► AudioMixer<2> ──► Output
Input ──────────────────────────────────►  (dry channel)
```

`MistApplet<Channels>` is a template instantiated for both `MONO` and `STEREO` in the audio pools (`hemisphere_audio_config.h`). Both instantiations compile independently, so code size roughly doubles — keep methods lean.

### MistChannel struct

Each channel owns:
- `AudioEffectMist grain_stream` — records audio, spawns/plays grains
- `AudioMixer<2> wet_dry_mixer` — blends wet (grain cloud) and dry (live input)

The PSRAM buffer (`g_buffer`) is allocated lazily via `Acquire()`/`Release()` in `Start()`/`Stop()`. On hardware without PSRAM, it falls back to half the buffer size in OCRAM.

---

## DSP Engine (AudioEffectMist)

### Buffer

`MistCircBuffer<int16_t>` is a thin wrapper over `ExtAudioBuffer<int16_t>` that exposes `GetWriteIx()` and `RawBuffer()` for absolute-position grain reads. Buffer lives in PSRAM (EXTMEM). Size: 1 second at 44100 Hz = 44100 samples = ~86 KB.

### Grain lifecycle

`update()` runs every audio block (128 samples, ~344 Hz):
1. Write incoming audio to circular buffer (unless frozen)
2. Per sample: advance `spawn_phase_` by `density/AUDIO_SAMPLE_RATE_EXACT`; when it crosses 1.0, call `spawnGrain()`
3. Per active grain: compute Hann window (`sin²(π×t)`), Hermite-interpolate the read position, accumulate output; advance `read_ptr` by `pitch`; retire grain when `phase >= grain_len`
4. Scale output by `GRAIN_SCALE = 0.25` (equal-power for 16 simultaneous grains)

### Per-grain pitch spread (PSprd)

`setPitchSpread(float semis)` sets `psprd_` (0–12 semitones). In `spawnGrain()`, each new grain gets a random offset: `grain_pitch *= powf(2.0f, rand_semis / 12.0f)`. The applet maps the 0–100% UI value via a quadratic curve `semis = 12 × (psprd/100)²` to give more resolution at the low end.

### Thread safety

`volatile` params (`pos_`, `density_`, `size_`, `spray_`, `pitch_`, `psprd_`, `freeze_`) are written from `Controller()` (timer ISR context) and read once at the top of `update()` (audio ISR). No mutex needed — reads are atomic on Cortex-M7 for `float`/`bool`.

---

## UI Layer (MistApplet)

### Two-page layout

8 parameters split across two pages, 4 per page at y=15/25/35/45 (10px spacing):

| Page | Params |
|------|--------|
| 1 (`cursor < PITCH`) | Pos, Den, Sz, Spr |
| 2 (`cursor >= PITCH`) | Pt, PSp, Frz, Mix |

Page indicator `"1/2"` / `"2/2"` at (x=46, y=56). The grain activity bar (one filled pixel per active grain, up to 16) sits at y=7.

Page is determined purely from cursor position — no extra state needed.

### Cursor enum

```
POS=0, POS_CV, DENSITY, DENSITY_CV, SIZE, SIZE_CV,
SPRAY, SPRAY_CV, PITCH, PITCH_CV, PSPRD, PSPRD_CV,
FREEZE, MIX, MIX_CV, CURSOR_LENGTH(=15)
```

FREEZE has only one cursor position (no `_CV` pair) — it uses `DigitalInputMap`, not `CVInputMap`.

### AuxButton

Latches `manual_freeze_` — useful for live performance without a gate cable. The Frz row label inverts while latched.

### Data packing

```cpp
data[0] = PackPackables(pos, density, size, spray, pitch, psprd, mix)
data[1] = PackPackables(pos_cv, density_cv, size_cv)
data[2] = PackPackables(spray_cv, pitch_cv, psprd_cv, mix_cv)
data[3] = PackPackables(freeze_input)
```

---

## Memory Notes

### RAM1 (ITCM) pressure — the ongoing constraint

The T41 build is tight on RAM1 (512 KB holds both code and data). Mist is large because:
- `update()` has a doubly-nested hot loop (128 samples × up to 16 grains) with `sinf()` and Hermite interpolation
- Both `MistApplet<MONO>` and `MistApplet<STEREO>` are instantiated, roughly doubling all compiled code

**What NOT to do:**
- Do not add scrolling UI (lambdas + CursorToRow helpers get aggressively inlined by LTO, inflating ITCM)
- `FLASHMEM` annotations have no effect on this build because `-DTEENSY_OPT_SMALLEST_CODE_LTO` causes LTO to devirtualize and inline everything — the section attribute is silently ignored

**Levers if RAM1 overflows again:**
1. Reduce `MAX_GRAINS` (currently 16) — halves grain loop iterations and saves ~1–2 KB
2. Replace `sinf()` in the Hann window with a polynomial approximation — avoids the libm call overhead
3. Remove `MistApplet<STEREO>` from `stereo_processors_pool` in `hemisphere_audio_config.h` if stereo Mist is not needed — halves compiled code

### PSRAM (EXTMEM)

The grain buffer itself is in PSRAM via `ExtAudioBuffer::Acquire()`. The audio applet tuple pools are `DMAMEM` (RAM2). Neither contributes to the RAM1 problem.

---

## Known Gotchas

- `g_buffer.Write(in)` is skipped when `cur_freeze` is true but the `GetWriteIx()` still returns the last write position, so grain reads remain coherent during freeze.
- `grain_len` is clamped between `AUDIO_BLOCK_SAMPLES` (minimum, avoids zero-length grains) and `buf_size/2` (maximum, avoids wrapping artifacts).
- `read_ptr` is a `float` to allow fractional pitch; wrap logic uses `>= buf_size` / `< 0` guards rather than modulo to avoid expensive division in the hot path.
- `spawn_phase_` is NOT volatile — it is only ever touched inside the audio interrupt, so no protection is needed.
