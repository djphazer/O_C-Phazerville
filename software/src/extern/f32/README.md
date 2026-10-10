# Vendored float32 audio core

Minimal subset vendored for the float32 engine migration (24-bit I/O). Do not edit
in place without noting divergence here.

Sources:
- `AudioStream_F32.*`, `AudioSettings_F32.*`, `AudioConvert_F32.h`, `AudioMixer_F32.*` from
  [chipaudette/OpenAudio_ArduinoLibrary](https://github.com/chipaudette/OpenAudio_ArduinoLibrary)
  @ `22014fdef317f9acb042b63e0260cc8cddd0c839` (MIT)
- `input_i2s2_F32.*`, `output_i2s2_F32.*`, `basic_DSPutils.*` from
  [hexeguitar/hexefx_audiolib_F32](https://github.com/hexeguitar/hexefx_audiolib_F32)
  @ `ef8b85d07513300e0f70213737fe47a111788e16` (MIT)

The I2S2 drivers run 32-bit frames on the wire (SAI2, same pins/BCLK as the stock
16-bit I2S2 objects), preserving full 24-bit codec resolution into
`audio_block_f32_t`. F32 objects share the stock `AudioStream::update_all()`
software interrupt, so int16 and float graphs coexist; bridge with
`AudioConvert_I16toF32` / `AudioConvert_F32toI16`.

Local divergences from upstream (marked with "Xenomorphicle" comments in-file):
- `input_i2s2_F32.cpp` / `output_i2s2_F32.cpp` / `AudioStream_F32.cpp` /
  `AudioSettings_F32.cpp` / `basic_DSPutils.cpp` / `AudioMixer_F32.cpp`:
  whole-file `#ifdef ARDUINO_TEENSY41` guards (non-T4.1 envs build these TUs
  empty; T4.0 envs `lib_ignore` the Audio library entirely).
- `AudioStream_F32.{h,cpp}`: `AudioConnection_F32` is pointer-based with a
  default constructor, re-`connect(...)`, and `disconnect()`, mirroring the
  stock int16 `AudioConnection` API so F32 patch cables can be pooled and
  re-patched on applet hot-swap.
