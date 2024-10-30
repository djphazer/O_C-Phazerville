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

#include "HSMIDI.h"

namespace HS {

static constexpr uint16_t CLOCK_TEMPO_MIN = 1;
static constexpr uint16_t CLOCK_TEMPO_MAX = 300;
static constexpr uint32_t CLOCK_TICKS_MIN = 1000000 / CLOCK_TEMPO_MAX;
static constexpr uint32_t CLOCK_TICKS_MAX = 1000000 / CLOCK_TEMPO_MIN;

constexpr int MIDI_OUT_PPQN = 24;
constexpr int CLOCK_MAX_MULTIPLE = 24;
constexpr int CLOCK_MIN_MULTIPLE = -31; // becomes /32

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
        NR_OF_CLOCKS
    };

    uint16_t tempo; // The set tempo, for display somewhere else
    uint32_t ticks_per_beat; // Based on the selected tempo in BPM
    bool running = 0; // Specifies whether the clock is running for interprocess communication
    bool paused = 0; // Specifies whethr the clock is paused
    bool auto_reset = 0; // on clock start
    bool midi_out_enabled = 1;

    bool tickno = 0;
    bool extsync = false; // locked into an external clock; will stop after timeout
    uint32_t clock_tick[2] = {0,0}; // previous ticks when a physical clock was received on DIGITAL 1
    uint32_t beat_tick = 0; // The tick to count from
    bool tock[NR_OF_CLOCKS] = {0,0,0,0,0,0,0,0,0}; // The current tock value
    int16_t tocks_per_beat[NR_OF_CLOCKS] = {0,0, 0,0, 0,0, 0,0, MIDI_OUT_PPQN}; // Multiplier
    int count[NR_OF_CLOCKS] = {0,0,0,0, 0,0,0,0, 0}; // Multiple counter, 0 is a special case when first starting the clock
    int8_t shuffle = 0; // 0 to 100

    int clock_ppqn = 4; // external clock multiple
    bool cycle = 0; // Alternates for each tock, for display purposes

    bool boop[8] = {0,0,0,0,0,0,0,0}; // Manual triggers

    void (*sync_func)(); // callback function

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
        tempo = bpm;
    }
    
    void SetTempoFromTaps(uint32_t *taps, int count) {
        uint32_t total = 0;
        for (int i = 0; i < count; ++i) {
            total += taps[i];
        }
        
        // update the tempo
        uint32_t clock_diff = total / count;
        ticks_per_beat = constrain(clock_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX); // time since last clock is new tempo
        tempo = 1000000 / ticks_per_beat; // imprecise, for display purposes
    }

    int GetMultiply(int ch = 0) {return tocks_per_beat[ch];}
    int GetClockPPQN() { return clock_ppqn; }

    void SetShuffle(int8_t sh_) { shuffle = constrain(sh_, 0, 99); }
    int8_t GetShuffle() { return shuffle; }

    /* Gets the current tempo. This can be used between client processes, like two different
     * hemispheres.
     */
    uint16_t GetTempo() {return tempo;}

    void BeatSync(void (*func)()) {
      sync_func = func;
    }
    void ProcessBeatSync() {
      // TODO: things that should only happen on the downbeat
      // such as: preset load, multiplier change, etc...
      if (sync_func != nullptr) {
        sync_func();
        sync_func = nullptr;
      }
    }

    // Reset - Resync multipliers, optionally skipping the first tock
    void Reset(bool count_skip = 0) {
        beat_tick = OC::CORE::ticks;
        if (0 == count_skip) {
            clock_tick[0] = 0;
            clock_tick[1] = 0;
        }
        for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
            if (tocks_per_beat[ch] > 0 || 0 == count_skip) count[ch] = count_skip;
        }

        cycle = 1 - cycle;
    }

    // Nudge - Used to align the internal clock with incoming clock pulses
    // The rationale is that it's better to be short by 1 than to overshoot by 1
    void Nudge(int diff) {
        if (diff > 0) diff--;
        if (diff < 0) diff++;
        beat_tick += diff;
    }

    // call this on every tick when clock is running, before all Controllers
    void SyncTrig(bool clocked, bool hard_reset = false) {
        //if (!IsRunning()) return;
        if (hard_reset) Reset();

        const uint32_t now = OC::CORE::ticks;

        // Reset only when all multipliers have been met
        bool reset = 1;
        // Process beat sync actions when any multiplier is met
        bool beatsync = 0;

        // count and calculate Tocks
        for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
            if (tocks_per_beat[ch] == 0) { // disabled
                tock[ch] = 0; continue;
            }

            if (tocks_per_beat[ch] > 0) { // multiply
                uint32_t next_tock_tick = beat_tick + count[ch]*ticks_per_beat / static_cast<uint32_t>(tocks_per_beat[ch]);
                if (shuffle && MIDI_CLOCK != ch && count[ch] % 2 == 1 && count[ch] < tocks_per_beat[ch])
                    next_tock_tick += shuffle * ticks_per_beat / 100 / static_cast<uint32_t>(tocks_per_beat[ch]);

                tock[ch] = now >= next_tock_tick;
                if (tock[ch]) ++count[ch]; // increment multiplier counter

                beatsync = beatsync || (count[ch] > tocks_per_beat[ch]); // multiplier has been exceeded
                reset = reset && (count[ch] > tocks_per_beat[ch]);
            } else { // division: -1 becomes /2, -2 becomes /3, etc.
                int div = 1 - tocks_per_beat[ch];
                uint32_t next_beat = beat_tick + (count[ch] ? ticks_per_beat : 0);
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
        if (beatsync) ProcessBeatSync();

        // handle syncing to physical clocks
        if (clocked && clock_tick[tickno] && clock_ppqn) {

            uint32_t clock_diff = now - clock_tick[tickno];

            // too slow, reset clock tracking
            if (clock_ppqn * clock_diff > CLOCK_TICKS_MAX) {
                clock_tick[0] = 0;
                clock_tick[1] = 0;
            }

            // if there are two previous clock ticks, update tempo and sync
            if (clock_tick[1-tickno] && clock_diff) {
                uint32_t avg_diff = (clock_diff + (clock_tick[tickno] - clock_tick[1-tickno])) / 2;

                // update the tempo
                ticks_per_beat = constrain(clock_ppqn * avg_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX);
                tempo = 1000000 / ticks_per_beat; // imprecise, for display purposes

                int ticks_per_clock = ticks_per_beat / clock_ppqn; // rounded down

                // time since last beat
                int tick_offset = now - beat_tick;

                // too long ago? time til next beat
                if (tick_offset > ticks_per_clock / 2) tick_offset -= ticks_per_beat;

                // within half a clock pulse of the nearest beat AND significantly large
                if (abs(tick_offset) < ticks_per_clock / 2 && abs(tick_offset) > 4)
                    Nudge(tick_offset); // nudge the beat towards us

                extsync = true;
            }
        }
        // clock has been physically ticked
        if (clocked) {
            tickno = 1 - tickno;
            clock_tick[tickno] = now;
        }
        else if (extsync && clock_ppqn && now - clock_tick[tickno] > ticks_per_beat * 2 / clock_ppqn) {
          // auto-stop
          Stop();
          Start(true); // re-arm
        }
    }

    void Start(bool p = 0) {
        Reset();
        running = 1;
        paused = p;
        auto_reset = !p;
        if (!p && midi_out_enabled) {
            usbMIDI.sendRealTime(usbMIDI.Start);
#if defined(__IMXRT1062__)
            usbHostMIDI.sendRealTime(usbMIDI.Start);
#ifdef ARDUINO_TEENSY41
            MIDI1.sendRealTime(midi::MidiType(usbMIDI.Start));
#endif
#endif
        }
    }

    void Stop() {
        running = 0;
        paused = 0;
        extsync = false;
        if (midi_out_enabled) {
            usbMIDI.sendRealTime(usbMIDI.Stop);
#if defined(__IMXRT1062__)
            usbHostMIDI.sendRealTime(usbMIDI.Stop);
#ifdef ARDUINO_TEENSY41
            MIDI1.sendRealTime(midi::MidiType(usbMIDI.Stop));
#endif
#endif
        }
    }

    void Pause() {paused = 1;}

    bool IsRunning() {return (running && !paused);}

    bool IsPaused() {return paused;}

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
    bool Tock(int ch = 0) {
        return tock[ch];
    }

    // Returns true if MIDI Clock should be sent on this tick
    bool MIDITock() {
        return midi_out_enabled && Tock(MIDI_CLOCK);
    }

    bool EndOfBeat(int ch = 0) {
      return beat_tick == OC::CORE::ticks;
    }

    bool Cycle(int ch = 0) {return cycle;}
};

extern ClockManager clock_m;

}

#endif // CLOCK_MANAGER_H
