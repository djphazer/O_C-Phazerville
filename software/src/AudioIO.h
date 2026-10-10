#pragma once

#include <Audio.h>

namespace OC {
  namespace AudioIO {
    // total block size including header is 260 bytes
    // 252 * 260 fits nicely into two 32KB pages
    const int AUDIO_MEMORY = 252;
    // float32 pool (RAM2 heap, ~528 bytes/block); boundary converters, the
    // F32 I2S drivers, and F32-native applets draw from this. The Delay
    // applet alone can hold ~10 blocks per channel in flight (8 taps + I/O).
    //
    // 80 -> 128 when the standalone effect apps landed. A stereo Delay wants
    // ~20 of these on its own, and the effect-slot app (below) runs
    // CONCURRENTLY with whatever Quadrants already has in its four slots
    // rather than instead of it -- that is the whole point of claiming a
    // separate summing-route source. Exhausting this pool is not a graceful
    // degradation: allocate_f32() returns NULL and the stage that asked for a
    // block silently drops it, which is a click or a dropout, not a warning.
    // 128 blocks costs ~25 KB more of the RAM2 heap and is comfortably under
    // the pool's own hard ceiling of 192 (AudioStream_F32.cpp:38).
    const int F32_AUDIO_MEMORY = 128;

    // OutputStream()'s destination is a summing point (AudioSummingRoute,
    // Audio/AudioMixer.h), not a plain relay: Quadrants' own audio-applet
    // chain-tail (AudioAppletSubapp.h), Tweighty's background engine output
    // (apps/TweightyApp.h), and Sampler's 8-voice mix (apps/SamplerApp.h)
    // are ALL wired to it (Quadrants and Sampler permanently at Init(),
    // Tweighty lazily on its own first RESUME) and all need to be audible
    // simultaneously -- see AudioIO.cpp's output_route comment.
    // Source-major input layout: source `s`'s channel `c` is at index
    // `s * kOutputRouteChannels + c`. Quadrants' chain-tail predates this
    // and keeps targeting index == channel unchanged (source slot 0);
    // Tweighty and Sampler each claim their own slot explicitly.
    constexpr uint8_t kOutputRouteChannels = 2;
    constexpr uint8_t kOutputRouteSources = 4;
    constexpr uint8_t kOutputRouteTweightySlot = 1;
    constexpr uint8_t kOutputRouteSamplerSlot = 2;
    // ONE slot SHARED by every standalone full-screen effect app (Delay, and
    // Reverb/Samverb behind it). It is CLAIMED on entry, not owned: opening
    // an effect app disconnects whichever effect app held the slot last and
    // connects its own tail here.
    //
    // Shared rather than one slot each, deliberately. Three reverbs
    // permanently wired to the same input would sum into each other, hold
    // F32 blocks and burn CPU forever, whether or not anyone is listening to
    // them -- and the F32 pool is finite (F32_AUDIO_MEMORY above).
    //
    // Claimed rather than released on Suspend, equally deliberately: this
    // instrument's idiom is that audio keeps sounding after you leave the
    // screen (Quadrants and Tweighty both stay live when backgrounded).
    // Claiming gives exactly one live effect AND keeps it running.
    //
    // What must never happen is an effect app sharing slot 0 with Quadrants'
    // chain tail. Two sources on one AudioPassthrough channel means whichever
    // update() the ISR runs last per block silently wins -- the bug
    // AudioIO.cpp:41-48 documents. Hence a slot of its own.
    constexpr uint8_t kOutputRouteEffectSlot = 3;

    AudioStream& InputStream(int interface = 0);
    AudioStream& OutputStream();
    void Init();
  }
}
