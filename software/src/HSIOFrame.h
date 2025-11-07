/* Copyright (c) 2023-2024 Nicholas J. Michalek & Beau Sterling
 *
 * IOFrame & friends
 *   - making applet I/O more flexible and portable
 *
 * Some processing logic adapted from the MIDI In applet
 *
 */

#pragma once

#include <vector>
#include "OC_config.h"
#include "HSMIDI.h"
#include "HSUtils.h"
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "OC_digital_inputs.h"
#include "HSicons.h"
#include "HSClockManager.h"

namespace HS {

static constexpr int GATE_THRESHOLD = 15 << 7; // 1.25 volts
#if defined(__IMXRT1062__)
static constexpr int MIDIMAP_MAX = 32;
#else
static constexpr int MIDIMAP_MAX = 8;
#endif
static constexpr int TRIGMAP_MAX = OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT + MIDIMAP_MAX;
static constexpr int CVMAP_MAX = ADC_CHANNEL_COUNT + DAC_CHANNEL_COUNT + MIDIMAP_MAX;

struct MIDIMessage {
  // values expected from MIDI library, so channel starts at 1 (one), not zero
  uint8_t channel, message, data1, data2;

  const uint8_t chan() const { return channel - 1; }
  const uint8_t note() const { return data1; }
  const uint8_t vel() const { return data2; }
  const bool IsNote() const { return message == HEM_MIDI_NOTE_ON; }
};

using MIDILogEntry = MIDIMessage;
/*
struct MIDILogEntry {
    uint8_t message;
    uint8_t data1;
    uint8_t data2;
};
*/

struct MIDINoteData {
    uint8_t note; // data1
    uint8_t vel;  // data2
};

struct PolyphonyData {
    uint8_t note;
    uint8_t vel;
    bool gate;
};

struct MIDIMapSettings {
  int8_t function_cc; // CC#, or some secret parameter for non-CC functions ;)
  uint8_t function; // which type of message
  uint8_t channel; // MIDI channel number
  uint8_t dac_polyvoice; // select which voice to send from output
  int8_t transpose;
  uint8_t range_low, range_high;
};
struct MIDIMapping : public MIDIMapSettings {
  static constexpr size_t Size = 64; // Make this compatible with Packable

  // state
  int16_t trigout_countdown;
  uint16_t semitone_mask; // which notes are currently on
  int16_t output; // translated CV values

  const bool IsClock() const {
    return (function >= HEM_MIDI_CLOCK_OUT);
  }
  const bool IsTrigger() const {
    return (function == HEM_MIDI_TRIG_OUT
         || function == HEM_MIDI_TRIG_1ST_OUT
         || function == HEM_MIDI_TRIG_ALWAYS_OUT
         || function == HEM_MIDI_START_OUT
         || IsClock());
  }
  constexpr int clock_mod() const {
    uint8_t mod = 1;
    if (function == HEM_MIDI_CLOCK_OUT) mod = 12;
    if (function == HEM_MIDI_CLOCK_8_OUT) mod = 6;
    if (function == HEM_MIDI_CLOCK_16_OUT) mod = 3;
    return mod;
  }
  void ClockOut() {
    trigout_countdown = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
    output = HEMISPHERE_MAX_CV;
  }
  void ProcessClock(int count) {
    if ( IsClock() && ((count-1) % clock_mod() == 0) )
      ClockOut();
  }
  const bool InRange(uint8_t note) const {
    return (note >= range_low && note <= range_high);
  }

  void AdjustTranspose(int dir) {
    transpose = constrain(transpose + dir, -24, 24);
  }
  void AdjustRangeLow(int dir) {
    range_low = constrain(range_low + dir, 0, range_high);
  }
  void AdjustRangeHigh(int dir) {
    range_high = constrain(range_high + dir, range_low, 127);
  }
  uint64_t Pack() const {
    return PackPackables(function_cc, function, channel, dac_polyvoice, transpose, range_low, range_high);
  }
  void Unpack(uint64_t data) {
    UnpackPackables(data, function_cc, function, channel, dac_polyvoice, transpose, range_low, range_high);
    // validation for safety
    if (function > HEM_MIDI_MAX_FUNCTION) function = HEM_MIDI_NOOP;
    channel &= 0x1F;
    dac_polyvoice &= 0x0F;
    if (range_low == 0 && range_high == 0) range_high = 127;
    if (range_high < range_low) range_high = range_low;
  }
};

// Lets PackingUtils know this is Packable as is.
constexpr MIDIMapping& pack(MIDIMapping& input) {
  return input;
}

using NoteBuffer = std::vector<MIDINoteData>;

struct MIDIFrame {
    MIDIMapping mapping[MIDIMAP_MAX];
    MIDIMapping outmap[ADC_CHANNEL_COUNT];

    // MIDI input stuff handled by MIDIIn applet
    NoteBuffer note_buffer[16]; // note buffer to track all held notes on all channels
    uint8_t last_midi_channel = 0; // for MIDI In activity monitor
    uint16_t sustain_latch; // each bit is a MIDI channel's sustain state

    uint8_t pc_channel = 0; // program change channel filter, used for preset selection
    static constexpr uint8_t PC_OMNI = 0;

    PolyphonyData poly_buffer[DAC_CHANNEL_COUNT]; // buffer for polyphonic data tracking
    uint8_t max_voice = 1;
    int poly_mode = 0;
    int8_t poly_rotate_index = -1;
    uint16_t midi_channel_filter = 0; // each bit state represents a channel. 1 means enabled. all 0's means Omni (no channel filter)
    bool any_channel_omni = false;

    // Clock/Start/Stop are handled by ClockSetup applet
    bool clock_run = 0;
    bool clock_q;
    bool start_q;
    bool stop_q;
    uint8_t clock_count; // MIDI clock counter (24ppqn)
    uint32_t last_msg_tick; // Tick of last received message

    void Init() {
      // TODO: populate with some sensible defaults
      for (int ch = 0; ch < MIDIMAP_MAX; ++ch) {
        mapping[ch].function = HEM_MIDI_NOOP;
        mapping[ch].transpose = 0;
        mapping[ch].output = 0;
        mapping[ch].dac_polyvoice = ch / 2 % DAC_CHANNEL_COUNT; // each quad is a unique voice
        mapping[ch].range_low = 0;
        mapping[ch].range_high = 127;
      }
      for (int ch = 0; ch < ADC_CHANNEL_COUNT; ++ch) {
        outmap[ch].function = HEM_MIDI_NOOP;
        outmap[ch].transpose = 0;
        outmap[ch].output = 0;
        outmap[ch].range_low = 0;
        outmap[ch].range_high = 127;
      }
      clock_count = 0;
    }

    // getters for access to mappings
    uint8_t get_in_assign(int ch) {
      return mapping[ch].function;
    }
    uint8_t get_in_channel(int ch) {
      return mapping[ch].channel;
    }
    int8_t get_in_transpose(int ch) {
      return mapping[ch].transpose;
    }
    bool in_in_range(int ch, uint8_t note) {
      return mapping[ch].InRange(note);
    }

    uint8_t get_out_assign(int ch) {
      return outmap[ch].function;
    }
    uint8_t get_out_channel(int ch) {
      return outmap[ch].channel;
    }
    int8_t get_out_transpose(int ch) {
      return outmap[ch].transpose;
    }
    bool in_out_range(int ch, int note) {
      return (note >= outmap[ch].range_low && note <= outmap[ch].range_high);
    }

    void UpdateMidiChannelFilter() {
        uint16_t filter = 0;
        bool omni = false;
        for (auto &map : mapping) {
            if (map.function == HEM_MIDI_NOOP) continue;
            if (map.channel < 16) filter |= (1 << map.channel);
            else omni = true;
        }
        midi_channel_filter = filter;
        any_channel_omni = omni;
    }

    bool CheckMidiChannelFilter(const uint8_t m_ch) {
        return any_channel_omni || midi_channel_filter & (1 << m_ch);
    }

    void UpdateMaxPolyphony() { // find max voice number to determine how much to buffer
        int voice = 0;
        for (auto &map : mapping) {
            if (map.function == HEM_MIDI_NOOP) continue;
            if (map.dac_polyvoice > voice) voice = map.dac_polyvoice;
        }
        if (max_voice != voice+1) {
            ClearPolyBuffer();
            max_voice = voice+1;
        }
    }

    bool CheckPolyVoice(const uint8_t voice) {
        return (poly_buffer[voice].gate);
    }

    int FindNextAvailPolyVoice(const uint8_t note) {
        if (max_voice == 1) return 0;

        switch (poly_mode) {
            case POLY_RESET:
                for (int v = 0; v < max_voice; ++v)
                    if (!CheckPolyVoice(v)) return v;
                return max_voice - 1;
                break;
            case POLY_REUSE:
                for (int v = 0; v < max_voice; ++v)
                    if (poly_buffer[v].note == note) return v;
                // fallthrough
            case POLY_ROTATE:
                for (int v = 0; v < max_voice; ++v) {
                    if (++poly_rotate_index >= max_voice) poly_rotate_index = 0;
                    if (!CheckPolyVoice(poly_rotate_index)) return poly_rotate_index;
                }
                // if no voices empty
                if (++poly_rotate_index >= max_voice) poly_rotate_index = 0;
                return poly_rotate_index;
                break;
            default:
                return 0;
        }
    }

    int FindPolyNoteIndex(const uint8_t note) {
        for (int v = 0; v < max_voice; ++v)
            if (poly_buffer[v].note == note) return v;
        return -1;
    }

    void WritePolyNoteData(const uint8_t note, const uint8_t vel, const uint8_t voice) {
        poly_buffer[voice].note = note;
        poly_buffer[voice].vel = vel;
        poly_buffer[voice].gate = 1;
    }

    void ClearPolyVoice(const uint8_t voice) {
        poly_buffer[voice].vel = 0;
        poly_buffer[voice].gate = 0;
    }

    void PolyBufferPush(const uint8_t m_ch, const uint8_t note, const uint8_t vel) {
        if (CheckMidiChannelFilter(m_ch))
            WritePolyNoteData(note, vel, FindNextAvailPolyVoice(note));
    }

    void PolyBufferPop(const uint8_t m_ch, const uint8_t note) {
        if (CheckMidiChannelFilter(m_ch)) {
            for (uint8_t v = 0; v < max_voice; ++v) {
                if (poly_buffer[v].note == note) ClearPolyVoice(v);
            }
        }
    }

    void ClearPolyBuffer() {
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ++ch) {
            ClearPolyVoice(ch);
        }
    }

    void RemoveNoteData(NoteBuffer &buffer, const uint8_t note) {
        buffer.erase(
            std::remove_if(buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
                return data.note == note;
            }),
            buffer.end()
        );
    }

    void MonoBufferPush(const uint8_t m_ch, const uint8_t note, const uint8_t vel) {
        if (CheckMidiChannelFilter(m_ch)) {
            RemoveNoteData(note_buffer[m_ch], note); // if new note is already in buffer, promote to latest and update velocity
            note_buffer[m_ch].push_back({note, vel}); // else just append to the end
        }
    }

    void MonoBufferPop(const uint8_t m_ch, const uint8_t note) {
        if (CheckMidiChannelFilter(m_ch)) {
            RemoveNoteData(note_buffer[m_ch], note);
            if (note_buffer[m_ch].size() == 0) note_buffer[m_ch].shrink_to_fit(); // free up memory when MIDI is not used
        }
    }

    void ClearMonoBuffer(const int8_t m_ch = -1) {
        if (m_ch > 0) {
            note_buffer[m_ch].clear();
            note_buffer[m_ch].shrink_to_fit();
        } else { // clear on all channels if no args passed
            for (uint8_t c = 0; c < 16; ++c) {
                note_buffer[c].clear();
                note_buffer[c].shrink_to_fit();
            }
        }
    }

    // int GetNote(std::vector<MIDINoteData> &buffer, const int n) {
    //     return buffer.at(buffer.size()-n).note;
    // }

    int GetNoteFirst(NoteBuffer &buffer) {
        return buffer.front().note;
    }

    int GetNoteLast(NoteBuffer &buffer) {
        return buffer.back().note;
    }

    int GetNoteLastInv(NoteBuffer &buffer) {
        return 127 - buffer.back().note;
    }

    int GetNoteMin(NoteBuffer &buffer) {
        uint8_t m = 127;
        std::for_each (buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
            if (data.note < m) m = data.note;
        });
        return m;
    }

    int GetNoteMax(NoteBuffer &buffer) {
        uint8_t m = 0;
        std::for_each (buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
            if (data.note > m) m = data.note;
        });
        return m;
    }

    int GetVel(NoteBuffer &buffer, const int n) {
        return buffer.at(buffer.size()-n).vel;
    }

    void ClearSustainLatch(int8_t m_ch = -1) {
        if (m_ch > 0) sustain_latch &= ~(1 << m_ch);
        else { // clear on all channels if no args passed
            for (uint8_t c = 0; c < 16; ++c)
                sustain_latch &= ~(1 << c);
        }
    }

    bool CheckSustainLatch(const uint8_t m_ch) {
        return sustain_latch & (1 << m_ch);
    }

    // MIDI output stuff
    int outchan[DAC_CHANNEL_COUNT] = {
        0, 0, 1, 1,
#ifdef ARDUINO_TEENSY41
        2, 2, 3, 3,
#endif
    };
    int outchan_last[DAC_CHANNEL_COUNT] = {
        0, 0, 1, 1,
#ifdef ARDUINO_TEENSY41
        2, 2, 3, 3,
#endif
    };
    int outfn[DAC_CHANNEL_COUNT] = {
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
#ifdef ARDUINO_TEENSY41
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
#endif
    };
    uint8_t outccnum[DAC_CHANNEL_COUNT] = {
        1, 1, 1, 1,
#ifdef ARDUINO_TEENSY41
        5, 6, 7, 8,
#endif
    };
    uint8_t current_note[16]; // note number, per MIDI channel
    uint8_t current_ccval[DAC_CHANNEL_COUNT]; // level 0 - 127, per DAC channel
    int note_countdown[DAC_CHANNEL_COUNT];
    int inputs[DAC_CHANNEL_COUNT]; // CV to be translated
    int last_cv[DAC_CHANNEL_COUNT];
    bool clocked[DAC_CHANNEL_COUNT];
    bool gate_high[DAC_CHANNEL_COUNT];
    bool changed_cv[DAC_CHANNEL_COUNT];

    // Logging
    MIDIMessage log[7];
    int log_index;

    void UpdateLog(const MIDIMessage msg) {
        log[log_index++] = msg;
        if (log_index == 7) {
            for (int i = 0; i < 6; i++) {
                memcpy(&log[i], &log[i+1], sizeof(log[i+1]));
            }
            log_index--;
        }
        last_msg_tick = OC::CORE::ticks;
    }
    void UpdateLog(uint8_t message, uint8_t data1, uint8_t data2) {
        UpdateLog({0, message, data1, data2});
    }

    void ProcessMIDIMsg(const MIDIMessage msg);
    void Send(const int *outvals);

    void SendAfterTouch(const uint8_t midi_ch, uint8_t val) {
        usbMIDI.sendAfterTouch(val, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendAfterTouch(val, midi_ch + 1);
        MIDI1.sendAfterTouch(val, midi_ch + 1);
#endif
    }
    void SendPitchBend(const uint8_t midi_ch, uint16_t bend) {
        usbMIDI.sendPitchBend(bend, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendPitchBend(bend, midi_ch + 1);
        MIDI1.sendPitchBend(bend, midi_ch + 1);
#endif
    }

    void SendCC(const uint8_t midi_ch, uint8_t ccnum, uint8_t val) {
        usbMIDI.sendControlChange(ccnum, val, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendControlChange(ccnum, val, midi_ch + 1);
        MIDI1.sendControlChange(ccnum, val, midi_ch + 1);
#endif
    }
    void SendNoteOn(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 100) {
        if (note > 127) note = current_note[midi_ch];
        else current_note[midi_ch] = note;

        usbMIDI.sendNoteOn(note, vel, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendNoteOn(note, vel, midi_ch + 1);
        MIDI1.sendNoteOn(note, vel, midi_ch + 1);
#endif
    }
    void SendNoteOff(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 0) {
        if (note > 127) note = current_note[midi_ch];
        usbMIDI.sendNoteOff(note, vel, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendNoteOff(note, vel, midi_ch + 1);
        MIDI1.sendNoteOff(note, vel, midi_ch + 1);
#endif
    }
};

// shared IO Frame, updated every tick
// this will allow chaining applets together, multiple stages of processing
struct IOFrame {
    static constexpr int EXTRA_PRECISION = 4;

    // settings
    bool autoMIDIOut = false;
    uint8_t clockskip[DAC_CHANNEL_COUNT] = {0};
    int8_t output_slew[DAC_CHANNEL_COUNT] = {0};

    // pre-calculated clocks, subject to trigger mapping
    bool clocked[OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_COUNT];

    // physical input state cache
    bool gate_high[OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_COUNT];
    int inputs[ADC_CHANNEL_COUNT];

    // output value cache, countdowns
    int outputs[DAC_CHANNEL_COUNT]; // now with Extra Precision!
    int output_diff[DAC_CHANNEL_COUNT];
    int outputs_target[DAC_CHANNEL_COUNT];
    int clock_countdown[DAC_CHANNEL_COUNT];
    int adc_lag_countdown[ADC_CHANNEL_COUNT]; // Time between a clock event and an ADC read event
    // calculated values
    uint32_t last_clock[ADC_CHANNEL_COUNT]; // Tick number of the last clock observed by the child class
    uint32_t cycle_ticks[ADC_CHANNEL_COUNT]; // Number of ticks between last two clocks
    bool changed_cv[ADC_CHANNEL_COUNT]; // Has the input changed by more than 1/8 semitone since the last read?
    int last_cv[ADC_CHANNEL_COUNT]; // For change detection

    /* MIDI message queue/cache */
    MIDIFrame MIDIState;

    void Init() {
      MIDIState.Init();
    }

    const int ViewOut(DAC_CHANNEL ch) const { return outputs[ch] >> EXTRA_PRECISION; }

    // --- Soft IO ---
    void Out(DAC_CHANNEL channel, int value, bool override = false) {
        output_diff[channel] += value - outputs_target[channel];
        outputs_target[channel] = value;
        if (override) outputs[channel] = value << EXTRA_PRECISION;
    }
    void ClockOut(DAC_CHANNEL ch, const int pulselength = HEMISPHERE_CLOCK_TICKS * trig_length) {
        // short circuit if skip probability is zero to avoid consuming random numbers
        if (0 == clockskip[ch] || random(100) >= clockskip[ch]) {
            clock_countdown[ch] = pulselength;
            // assign to both to override slew - instant attack
            Out(ch, HEMISPHERE_MAX_CV, true);
        }
    }
    void NudgeSkip(int ch, int dir) {
        clockskip[ch] = constrain(clockskip[ch] + dir, 0, 100);
    }
    void NudgeSlew(int ch, int dir) {
        output_slew[ch] = constrain(output_slew[ch] + dir, 0, 100);
    }

    // --- Hard IO ---
    void Load();

    void Send() {
        const DAC_CHANNEL chan[DAC_CHANNEL_COUNT] = {
          DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_C, DAC_CHANNEL_D,
#ifdef ARDUINO_TEENSY41
          DAC_CHANNEL_E, DAC_CHANNEL_F, DAC_CHANNEL_G, DAC_CHANNEL_H,
#endif
        };

        for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
          const int target = outputs_target[i] << EXTRA_PRECISION;
          if (output_slew[i]) {
            int diff = target - outputs[i];
            int delta = 1;
            if (output_slew[i] <= 50)
              delta += 250 - 4*output_slew[i];
            else
              delta += 100 - output_slew[i];
            CONSTRAIN(delta, 0, abs(diff));
            if (diff < 0) delta = -delta;
            outputs[i] += delta;
          } else
            outputs[i] = target;

          OC::DAC::set_pitch_scaled(chan[i], outputs[i] >> EXTRA_PRECISION, 0);
        }
        // oh no, this is certainly broken now...
        if (autoMIDIOut) MIDIState.Send(outputs);
    }

};

extern IOFrame frame;

#include <CVInputMap.h>

} // namespace HS
