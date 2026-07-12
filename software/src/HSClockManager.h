// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// A "tick" is one ISR cycle, which happens 16666.667 times per second, or a million
// times per minute. A "tock" is a metronome beat.

#pragma once

#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include "OC_core.h"
#include "HSMIDI.h"
#include "HSUtils.h"
#include <functional>
#include <vector>

namespace HS {

static constexpr uint16_t CLOCK_TEMPO_MIN = 1;
static constexpr uint16_t CLOCK_TEMPO_MAX = 0xFFFF;
static constexpr uint32_t CLOCK_TICKS_MIN = 1000000 / CLOCK_TEMPO_MAX;
static constexpr uint32_t CLOCK_TICKS_MAX = 1000000 / CLOCK_TEMPO_MIN;

constexpr int MIDI_CLOCK_PPQN = 2;
constexpr int MIDI_OUT_PPQN = 24;
constexpr int CLOCK_MAX_MULTIPLE = 24;
constexpr int CLOCK_MIN_MULTIPLE = -31; // becomes /32
// Auto-stop only after the external clock has been silent for this many clock
// intervals. Larger than 1 so a single late/dropped pulse (e.g. jitter from
// Pamela's Workout) doesn't cause a spurious stop/start.
static constexpr int CLOCK_SYNC_LOST_INTERVALS = 4;
// Accept an external interval only if it's within [DEN/NUM .. NUM/DEN] of the
// expected one. Anything outside is a hiccup -- too long (missed/late pulse) or
// too short (spurious/bounced pulse) -- and is ignored, so the internal clock
// stays steady instead of lurching.
static constexpr int CLOCK_SYNC_OUTLIER_NUM = 3;
static constexpr int CLOCK_SYNC_OUTLIER_DEN = 2; // 3/2x (long) and 2/3x (short)
// If hiccups persist this many pulses in a row, it's a real tempo change: re-lock.
static constexpr int CLOCK_SYNC_MAX_OUTLIERS = 3;
// Once locked, move only 1/N toward each new measurement (tempo) or phase error,
// so a single off pulse barely moves the internal clock while sustained change is
// still followed over a few pulses. Higher = steadier but slower to track.
static constexpr int CLOCK_SYNC_TEMPO_SLEW = 4;
static constexpr int CLOCK_SYNC_PHASE_SLEW = 2;
// Ignore phase errors smaller than this (in ticks) to avoid jittering on noise.
static constexpr int CLOCK_SYNC_NUDGE_MIN = 4;

class ClockManager {
public:
    enum ClockOutput {
        LEFT_CLOCK1,
        LEFT_CLOCK2,
        RIGHT_CLOCK1,
        RIGHT_CLOCK2,
        LEFT2_CLOCK1,
        LEFT2_CLOCK2,
        RIGHT2_CLOCK1,
        RIGHT2_CLOCK2,
        MIDI_CLOCK,
        ONE_BEAT_CLOCK,
        SIXTEENTH_CLOCK,
        NR_OF_CLOCKS
    };

    uint16_t tempo; // The set tempo, for display somewhere else
    uint16_t tempo_setting;
    uint32_t ticks_per_beat; // Based on the selected tempo in BPM
    bool running = 0; // Specifies whether the clock is running for interprocess communication
    bool paused = 0; // Specifies whethr the clock is paused
    bool auto_reset = 0; // on clock start
    bool midi_out_enabled = 1;

    bool tickno = 0;
    bool extsync = false; // locked into an external clock; will stop after timeout
    uint8_t sync_outlier_count = 0; // consecutive over-long external intervals ignored
    uint32_t clock_tick[2] = {0,0}; // previous ticks when a physical clock was received on DIGITAL 1
    uint32_t beat_tick = 0; // The tick to count from
    uint32_t beat_count = 0;
    bool tock[NR_OF_CLOCKS] = {0,0,0,0,0,0,0,0,0,0,0}; // The current tock value
    int8_t tocks_per_beat[NR_OF_CLOCKS] = {0,0, 0,0, 0,0, 0,0, MIDI_OUT_PPQN, 1, 4}; // Multiplier
    int count[NR_OF_CLOCKS] = {0,0,0,0, 0,0,0,0, 0,0,0}; // Multiple counter, 0 is a special case when first starting the clock
    int8_t shuffle = 0; // 0 to 100
    int8_t shuffle_setting = 0;

    int clock_ppqn = 4; // external clock multiple
    bool cycle = 0; // Alternates for each tock, for display purposes

    bool boop[8] = {0,0,0,0,0,0,0,0}; // Manual triggers

    std::queue<Task> syncfn_queue;

    ClockManager() {
        SetTempoBPM(120);
    }

    void EnableMIDIOut() { midi_out_enabled = 1; }
    void DisableMIDIOut() { midi_out_enabled = 0; }

    void SetMultiply(int multiply, int ch = 0) {
        multiply = constrain(multiply, CLOCK_MIN_MULTIPLE, CLOCK_MAX_MULTIPLE);
        tocks_per_beat[ch] = multiply;
    }

    // adjusts the expected clock multiple for external clock pulses
    void SetClockPPQN(int clkppqn) {
        clock_ppqn = constrain(clkppqn, 0, 24);
    }

    /* Set ticks per tock, based on one million ticks per minute divided by beats per minute.
     * This is approximate, because the arithmetical value is likely to be fractional, and we
     * need to live with a certain amount of imprecision here. So I'm not even rounding up.
     */
    void SetTempoBPM(uint16_t bpm) {
        bpm = constrain(bpm, CLOCK_TEMPO_MIN, CLOCK_TEMPO_MAX);
        ticks_per_beat = 1000000 / bpm;
        tempo_setting = tempo = bpm;
    }
    
    void SetTempoFromTaps(uint32_t *taps, int count) {
        uint32_t total = 0;
        for (int i = 0; i < count; ++i) {
            total += taps[i];
        }
        
        // update the tempo
        uint32_t clock_diff = total / count;
        ticks_per_beat = constrain(clock_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX); // time since last clock is new tempo
        tempo_setting = tempo = 1000000 / ticks_per_beat; // imprecise, for display purposes
    }

    int8_t GetMultiply(int ch = 0) {return tocks_per_beat[ch];}
    int GetClockPPQN() { return clock_ppqn; }

    void SetShuffle(int8_t sh_) { shuffle_setting = shuffle = constrain(sh_, 0, 99); }
    int8_t GetShuffle() { return shuffle_setting; }

    /* Gets the current tempo. This can be used between client processes, like two different
     * hemispheres.
     */
    uint16_t GetTempo() {return tempo_setting;}
    float GetTempoFloat() {
      return 1000000.0f / ticks_per_beat;
    }
    uint32_t GetTempoTicks() {return ticks_per_beat;}
    uint32_t GetCycleTicks(int ch = 0) {
      if (tocks_per_beat[ch] > 0) return ticks_per_beat / tocks_per_beat[ch];
      if (tocks_per_beat[ch] < 0) return ticks_per_beat * (1 - tocks_per_beat[ch]);
      return 0;
    }

    void BeatSync(std::function<void()> func) {
      // TODO: prevent duplicates...
      syncfn_queue.emplace(func);
    }
    void ProcessBeatSync() {
      if (syncfn_queue.empty()) return;
      // Things that should only happen on the downbeat
      // such as: preset load, multiplier change, etc...
      while (!syncfn_queue.empty()) {
        syncfn_queue.front()();
        syncfn_queue.pop();
      }
    }

    const uint32_t BeatTick() const {
      return beat_tick; // + beat_count * ticks_per_beat;
    }
    // Reset - Resync multipliers, optionally skipping the first tock
    void Reset(bool count_skip = 0) {
      ++beat_count;
      beat_tick = OC::CORE::ticks;
      if (!count_skip) {
        beat_count = 0;
        ClearClockTicks();
        cycle = 1;
      }

      for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
        if (tocks_per_beat[ch] > 0 || !count_skip) count[ch] = count_skip;
      }

      cycle = 1 - cycle;
    }

    // Nudge - Used to align the internal clock with incoming clock pulses
    // The rationale is that it's better to be short by 1 than to overshoot by 1
    void Nudge(int diff) {
        if (diff > 0) diff--;
        if (diff < 0) diff++;
        beat_tick += diff; // hmmmm
    }

    // Forget the last two external clock edges (no lock, no tempo tracking).
    void ClearClockTicks() {
        clock_tick[0] = 0;
        clock_tick[1] = 0;
    }

    // Advance every multiplier/divider channel for `now`, firing their tocks and
    // triggering the beat-sync / reset actions when their counts line up.
    void UpdateTocks(uint32_t now) {
        // Reset only when all multipliers have been met
        bool reset = true;
        // Process beat sync actions when any multiplier is met
        bool beatsync = false;

        for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
            if (tocks_per_beat[ch] == 0) { // disabled
                tock[ch] = 0; continue;
            }

            if (tocks_per_beat[ch] > 0) { // multiply
                uint32_t next_tock_tick = BeatTick() + count[ch]*ticks_per_beat / static_cast<uint32_t>(tocks_per_beat[ch]);
                if (shuffle && MIDI_CLOCK != ch && count[ch] % 2 == 1 && count[ch] < tocks_per_beat[ch])
                    next_tock_tick += shuffle * ticks_per_beat / 100 / static_cast<uint32_t>(tocks_per_beat[ch]);

                tock[ch] = now >= next_tock_tick;
                if (tock[ch]) {
                  ++count[ch]; // increment multiplier counter
                  if (1 == count[ch]) beatsync = true;
                }

                beatsync = beatsync || (count[ch] > tocks_per_beat[ch]); // multiplier has been exceeded
                reset = reset && (count[ch] > tocks_per_beat[ch]);
            } else { // division: -1 becomes /2, -2 becomes /3, etc.
                int div = 1 - tocks_per_beat[ch];
                uint32_t next_beat = BeatTick() + (count[ch] ? ticks_per_beat : 0);
                bool beat_exceeded = (now >= next_beat);
                if (beat_exceeded) {
                    ++count[ch];
                    tock[ch] = (count[ch] % div) == 1;
                }
                else
                    tock[ch] = 0;

                // resync on every beat
                beatsync = beatsync || beat_exceeded;
                reset = reset && beat_exceeded;
                if (tock[ch]) count[ch] = 1;
            }

        }
        if (reset) Reset(1); // skip the one we're already on
        if (beatsync && !syncfn_queue.empty())
          ProcessBeatSync();
    }

    // Lock the internal clock to an incoming external pulse: reject hiccups, then
    // slew tempo and phase toward the real external rate. Returns true when the
    // edge is spurious (too short) and should be dropped without being recorded.
    bool TrackExternalClock(uint32_t now, int ppqn, bool clocked) {
        if (!clocked || !clock_tick[tickno] || !ppqn) return false;

        uint32_t clock_diff = now - clock_tick[tickno];

        // too slow, reset clock tracking
        if (ppqn * clock_diff > CLOCK_TICKS_MAX) {
            ClearClockTicks();
            return false;
        }

        // need two previous clock ticks to update tempo and sync
        if (!clock_tick[1-tickno] || !clock_diff) return false;

        // Once locked, reject hiccups: an interval well outside the expected one
        // -- too long (missed/late pulse) or too short (spurious/bounced pulse) --
        // is ignored so the internal clock keeps running steady. If such intervals
        // persist, it's a real tempo change, so we re-lock.
        const uint32_t expected = ticks_per_beat / static_cast<uint32_t>(ppqn);
        const bool too_long  = clock_diff * CLOCK_SYNC_OUTLIER_DEN > expected * CLOCK_SYNC_OUTLIER_NUM;
        const bool too_short = clock_diff * CLOCK_SYNC_OUTLIER_NUM < expected * CLOCK_SYNC_OUTLIER_DEN;
        const bool outlier = extsync && expected && (too_long || too_short);

        if (outlier && sync_outlier_count < CLOCK_SYNC_MAX_OUTLIERS) {
            ++sync_outlier_count;
            // a too-short edge is likely spurious: drop it entirely so it doesn't
            // shift tracking. A too-long one is a real (late) edge, so keep it --
            // the next interval is then measured correctly.
            return too_short;
        }

        // Snap when first locking or when a change is confirmed (outliers
        // persisted); otherwise slew gently for a rock-steady clock.
        const bool snap = !extsync || outlier;
        sync_outlier_count = 0;

        if (snap) {
            uint32_t avg_diff = (clock_diff + (clock_tick[tickno] - clock_tick[1-tickno])) / 2;
            ticks_per_beat = constrain(ppqn * avg_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX);
        } else {
            // ease 1/N toward the newly measured tempo
            const uint32_t measured = constrain(ppqn * clock_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX);
            int32_t delta = int32_t(measured) - int32_t(ticks_per_beat);
            int32_t step = delta / CLOCK_SYNC_TEMPO_SLEW;
            if (0 == step) step = (delta > 0) - (delta < 0); // always creep toward target
            ticks_per_beat = constrain(int32_t(ticks_per_beat) + step, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX);
        }
        tempo_setting = tempo = 1000000 / ticks_per_beat; // imprecise, for display purposes

        int ticks_per_clock = ticks_per_beat / ppqn; // rounded down

        // time since last beat
        int tick_offset = now - BeatTick();

        // too long ago? time til next beat
        if (tick_offset > ticks_per_clock / 2) tick_offset -= ticks_per_beat;

        // within half a clock pulse of the nearest beat AND significantly large
        if (abs(tick_offset) < ticks_per_clock / 2 && abs(tick_offset) > CLOCK_SYNC_NUDGE_MIN)
            // align fully while snapping, gently once locked
            Nudge(snap ? tick_offset : tick_offset / CLOCK_SYNC_PHASE_SLEW);

        extsync = true;
        return false;
    }

    // call this on every tick when clock is running, before all Controllers
    void SyncTrig(bool clocked, bool midi_sync = false) {
        const uint32_t now = OC::CORE::ticks;
        if (midi_sync) DisableMIDIOut();
        const int ppqn = (midi_sync || !midi_out_enabled) ? MIDI_CLOCK_PPQN : clock_ppqn;

        // don't sync to non-MIDI triggers if MIDI sync is active
        if (!midi_sync && !midi_out_enabled) clocked = false;

        UpdateTocks(now);

        // track the external clock (tempo + phase); spurious edges are dropped
        const bool ignore_edge = TrackExternalClock(now, ppqn, clocked);

        if (!ignore_edge) {
            if (clocked) { // record the physical edge
                tickno = 1 - tickno;
                clock_tick[tickno] = now;
            }
            // auto-stop only after the external clock has been silent for several
            // intervals, so a single late/dropped pulse doesn't stop/start us
            else if (extsync && ppqn && now - clock_tick[tickno] > ticks_per_beat * CLOCK_SYNC_LOST_INTERVALS / ppqn) {
                Stop();
                Start(true); // re-arm
            }
        }
    }

    void Start(bool p = 0) {
        Reset();
        running = 1;
        paused = p;
        auto_reset = !p;
        if (!p && midi_out_enabled) {
            // TODO: DeferTask?
#ifdef ARDUINO_TEENSY41
            if (~midi_clktx_disable & mMaskUSBDev)
              usbMIDI.sendRealTime(usbMIDI.Start);
            if (~midi_clktx_disable & mMaskUSBHost)
              usbHostMIDI[0].sendRealTime(usbMIDI.Start);
            if (~midi_clktx_disable & mMaskUSBHost2)
              usbHostMIDI[1].sendRealTime(usbMIDI.Start);
            if (~midi_clktx_disable & mMaskSerial)
              MIDI1.sendRealTime(midi::MidiType(usbMIDI.Start));
#else
            usbMIDI.sendRealTime(usbMIDI.Start);
#endif
        }
    }

    void Stop() {
        running = 0;
        paused = 0;
        extsync = false;
        sync_outlier_count = 0;
        if (midi_out_enabled) {
#ifdef ARDUINO_TEENSY41
            // TODO: DeferTask?
            if (~midi_clktx_disable & mMaskUSBDev)
              usbMIDI.sendRealTime(usbMIDI.Stop);
            if (~midi_clktx_disable & mMaskUSBHost)
              usbHostMIDI[0].sendRealTime(usbMIDI.Stop);
            if (~midi_clktx_disable & mMaskUSBHost2)
              usbHostMIDI[1].sendRealTime(usbMIDI.Stop);
            if (~midi_clktx_disable & mMaskSerial)
              MIDI1.sendRealTime(midi::MidiType(usbMIDI.Stop));
#else
            usbMIDI.sendRealTime(usbMIDI.Stop);
#endif
        }
        EnableMIDIOut();
    }

    void Pause() {paused = 1;}

    void Modulate(int tempo_diff, int shuffle_diff) {
      shuffle = constrain(shuffle_setting + shuffle_diff, 0, 99);
      tempo = constrain(tempo_setting + tempo_diff, CLOCK_TEMPO_MIN, CLOCK_TEMPO_MAX);
      ticks_per_beat = 1000000 / tempo;
    }

    bool IsRunning() const {return (running && !paused);}

    bool IsPaused() const {return paused;}

    // beep boop
    void Boop(int ch = 0) {
        boop[ch] = true;
    }
    bool Beep(int ch = 0) {
        if (boop[ch]) {
            boop[ch] = false;
            return true;
        }
        return false;
    }

    /* Returns true if the clock should fire on this tick, based on the current tempo and multiplier */
    bool Tock(int ch = 0) const {
        return tock[ch];
    }

    // Returns true if MIDI Clock should be sent on this tick
    bool MIDITock() const {
        return midi_out_enabled && Tock(MIDI_CLOCK);
    }

    bool EndOfBeat(int ch = 0) const {
      return BeatTick() == OC::CORE::ticks;
    }

    bool Cycle(int ch = 0) {return cycle;}
};

extern ClockManager clock_m;

}

#endif // CLOCK_MANAGER_H
