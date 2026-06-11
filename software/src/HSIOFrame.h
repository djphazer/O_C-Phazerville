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
#include "OC_io.h"
#include "HSMIDI.h"
#include "HSUtils.h"
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "OC_digital_inputs.h"
#include "HSicons.h"
#include "HSClockManager.h"
#include "util/util_macros.h"
#include "util/clkdivmult.h"
#include "src/extern/bjorklund.h"

namespace HS {

static constexpr int GATE_THRESHOLD = 15 << 7; // 1.25 volts
#if defined(__IMXRT1062__)
static constexpr int MIDIMAP_MAX = 32;
static constexpr int IO_CHANNEL_COUNT = 32; // virtual inputs and outputs
#else
static constexpr int MIDIMAP_MAX = 8;
static constexpr int IO_CHANNEL_COUNT = 32;
#endif

struct MIDIFrame;
struct IOFrame;

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
using NoteBuffer = std::vector<MIDINoteData>;

struct PolyphonyData {
    uint8_t note;
    uint8_t vel;
    bool gate;
};

// All this determines the serialized binary format of MIDI Maps
// Values are explicitly defined, because changing them will break preset data.
struct MIDIMapSettings {
  // upper 3 bits
  enum Type : uint8_t {
    NONE = 0,
    PITCH = (1 << 5),
    GATE  = (2 << 5),
    TRIGGER = (3 << 5),
    MODULATOR = (4 << 5),
    CCONTROL = (5 << 5),
    RESERVED0 = (6 << 5),

    TYPE_MASK = (7 << 5),
  };
  enum PitchType : int8_t {
    NOTE_MONO = 0,
    NOTE_POLY = 1,
    NOTE_MIN = 2,
    NOTE_MAX = 3,
    NOTE_PEDAL = 4,
    NOTE_INVERT = 5,

    PITCH_TYPE_COUNT
  };
  enum GateType : int8_t {
    GATE_MONO = 0,
    GATE_POLY = 1,
    GATE_RETRIG = 2,
    GATE_INVERT = 3,

    GATE_RUN = 4,
    GATE_RESET = 5,

    GATE_TYPE_COUNT
  };
  enum TrigType : int8_t {
    TRIG_NORMAL = 0,
    TRIG_FIRST = 1,
    TRIG_ALWAYS = 2,

    TRIG_START = 3,
    TRIG_STOP = 4,
    TRIG_CLOCK = 5, // quarter note, 1ppqn
    TRIG_CLOCK2 = 6, // eighth note, 2ppqn
    TRIG_CLOCK4 = 7, // sixteenth note, 4ppqn
    TRIG_CLOCK8 = 8, // 32nd note, 8ppqn
    TRIG_CLOCK24 = 9, // 24ppqn

    TRIG_TYPE_COUNT
  };
  enum ModType : int8_t {
    MOD_VEL_MONO = 0,
    MOD_VEL_POLY = 1,
    MOD_AT_CHAN = 2,
    MOD_AT_POLY = 3,
    MOD_BEND = 4,

    MOD_TYPE_COUNT
  };
  constexpr int subtype_count(Type t) {
    switch (t) {
      case NONE: return 1;
      case PITCH: return PITCH_TYPE_COUNT;
      case GATE: return GATE_TYPE_COUNT;
      case TRIGGER: return TRIG_TYPE_COUNT;
      case MODULATOR: return MOD_TYPE_COUNT;
      case CCONTROL: return 128;
      default: break;
    }
    return 0;
  }

  int8_t function_cc; // CC#, or index of other subtypes
  uint8_t function; // which type of message
  uint8_t channel; // MIDI channel number
  uint8_t dac_polyvoice; // select which voice to send from output
  int8_t transpose;
  uint8_t range_low, range_high;
};
struct MIDIMapping : protected MIDIMapSettings {
  MIDIMapping() {}
  ~MIDIMapping() {}

  static constexpr size_t Size = 64; // Make this compatible with Packable

  // state
  int16_t trigout_countdown;
  bool gate_retrig;
  uint16_t semitone_mask; // which notes are currently on
  int16_t output; // translated CV values
  int16_t pitch_bend = 0;

  // functions
  const Type get_type() const {
    return Type(function & TYPE_MASK);
  }
  const int8_t get_subtype() const {
    return function_cc;
  }
  const uint8_t get_channel() const {
    return channel;
  }
  const int8_t get_transpose() const {
    return transpose;
  }
  const uint8_t get_voice() const {
    return dac_polyvoice;
  }
  const uint8_t get_low() const {
    return range_low;
  }
  const uint8_t get_high() const {
    return range_high;
  }
  const bool enabled() const {
    return get_type() != NONE;
  }
  const bool learning() const {
    return function_cc < 0;
  }

  const char * const get_label() const {
    if (get_type() == CCONTROL) return "CC#"; // special case for "CC#-1" auto-learn
    if (get_subtype() < 0) return "(learn)";

    switch (get_type()) {
      case NONE: return "None";
      case PITCH:
        switch (PitchType(get_subtype())) {
          case NOTE_MONO:   return "Note";
          case NOTE_POLY:   return "PolyN";
          case NOTE_MIN:    return "LoNote";
          case NOTE_MAX:    return "HiNote";
          case NOTE_PEDAL:  return "PdlNote";
          case NOTE_INVERT: return "InvNote";
          default: break;
        }
        break;
      case GATE:
        switch (GateType(get_subtype())) {
          case GATE_MONO:   return "Gate";
          case GATE_POLY:   return "PolyG";
          case GATE_RETRIG: return "GateRT";
          case GATE_INVERT: return "InvGate";
          case GATE_RUN:    return "RunGate";
          case GATE_RESET:  return "Reset";
          default: break;
        }
        break;
      case TRIGGER:
        switch (TrigType(get_subtype())) {
          case TRIG_NORMAL: return "Trig";
          case TRIG_FIRST:  return "Trig1st";
          case TRIG_ALWAYS: return "TrgAlws";
          case TRIG_START:  return "Start";
          case TRIG_STOP:   return "Stop";
          case TRIG_CLOCK:  return "Clk-1";
          case TRIG_CLOCK2: return "Clk-2";
          case TRIG_CLOCK4: return "Clk-4";
          case TRIG_CLOCK8: return "Clk-8";
          case TRIG_CLOCK24: return "Clk24";
          default: break;
        }
        break;
      case MODULATOR:
        switch (ModType(get_subtype())) {
          case MOD_VEL_MONO: return "Veloc";
          case MOD_VEL_POLY: return "PolyV";
          case MOD_AT_CHAN:  return "ChnAft";
          case MOD_AT_POLY:  return "KeyAft";
          case MOD_BEND:     return "Bend";
          default: break;
        }
        break;
      default: break;
    }
    return "???";
  }

  // Setters
  void Init() {
    function = NONE;
    function_cc = 0;
    dac_polyvoice = 0;
    transpose = 0;
    output = 0;
    pitch_bend = 0;
    range_low = 0;
    range_high = 127;
  }
  // -1 means armed to auto-learn
  void SetPitch(int8_t subtype) {
    function = PITCH;
    function_cc = constrain(subtype, -1, subtype_count(PITCH));
  }
  void SetTrigger(int8_t subtype) {
    function = TRIGGER;
    function_cc = constrain(subtype, -1, subtype_count(TRIGGER));
  }
  void SetGate(int8_t subtype) {
    function = GATE;
    function_cc = constrain(subtype, -1, subtype_count(GATE));
  }
  void SetModulator(int8_t subtype) {
    function = MODULATOR;
    function_cc = constrain(subtype, -1, subtype_count(MODULATOR));
  }
  void SetCC(int8_t subtype) {
    function = CCONTROL;
    function_cc = constrain(subtype, -1, subtype_count(CCONTROL));
  }

  void SetChannel(uint8_t ch) {
    channel = ch;
  }
  void SetTranspose(int8_t tr) {
    transpose = tr;
  }

  const bool IsPitch() const {
    return get_type() == PITCH;
  }
  const bool IsGate() const {
    return get_type() == GATE;
  }
  const bool IsTrigger() const {
    return get_type() == TRIGGER;
  }
  const bool IsClock() const {
    return IsTrigger() && get_subtype() >= TRIG_CLOCK;
  }
  const bool IsMod() const {
    return get_type() == MODULATOR;
  }
  const bool IsCC() const {
    return get_type() == CCONTROL;
  }
  const bool IsPoly() const {
    return (IsPitch() && get_subtype() == NOTE_POLY)
      || (IsGate() && get_subtype() == GATE_POLY)
      || (IsMod() && ((get_subtype() == MOD_AT_POLY) || (get_subtype() == MOD_VEL_POLY)));
  }
  constexpr int clock_mod() const {
    uint8_t mod = 1; // 24ppqn aka 1/96th note
    if (function_cc == TRIG_CLOCK) mod = 24; // quarter note, 1ppqn
    if (function_cc == TRIG_CLOCK2) mod = 12; // eighth note, 2ppqn
    if (function_cc == TRIG_CLOCK4) mod = 6; // sixteenth note, 4ppqn
    if (function_cc == TRIG_CLOCK8) mod = 3; // 32nd note, 8ppqn
    return mod;
  }

  const int ViewOut() const;
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

  void AdjustChannel(int dir) {
    channel = constrain(channel + dir, 0, 16); // 16 = omni
  }

  const Type ordered_types[6] = {
    NONE, PITCH, GATE, TRIGGER, MODULATOR, CCONTROL
  };
  bool AdjustType(int dir) {
    const int count = sizeof(ordered_types);
    int tidx = 0;
    while (get_type() != ordered_types[tidx] && tidx < count) ++tidx;
    if (tidx >= count) tidx = 0;
    tidx += (dir>0?1:-1);
    CONSTRAIN(tidx, 0, count - 1);
    int sidx = constrain(get_subtype(), 0, subtype_count(ordered_types[tidx]) - 1);

    Type oldtype = get_type();
    function = ordered_types[tidx];
    function_cc = sidx;
    return oldtype != get_type();
  }
  void AdjustFunction(int dir) {
    int sidx = get_subtype() + dir;
    if (sidx < 0) {
      if (AdjustType(-1))
        sidx = subtype_count(get_type()) - 1;
    }
    if (sidx >= subtype_count(get_type())) {
      if (AdjustType(1))
        sidx = (get_type() == CCONTROL) ? -1 : 0; // auto-learn for CC#
    }
    CONSTRAIN(sidx, -1, subtype_count(get_type()) - 1);
    function_cc = sidx;
    output = 0;
    pitch_bend = 0;
  }

  void AdjustVoice(int dir) {
    dac_polyvoice = constrain(dac_polyvoice + dir, 0, DAC_CHANNEL_COUNT - 1);
  }

  void AutoLearn() {
    channel = 16; // omni
    //function = NONE;
    function_cc = -1; // auto-learn MIDI CC or precise NoteOn
  }

  void AdjustTranspose(int dir) {
    transpose = constrain(transpose + dir, -48, 48);
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
    // migrate old data
    if (function && get_type() == NONE) {
      switch (function) {
        case HEM_MIDI_NOTE_OUT: SetPitch(NOTE_MONO); break;
        case HEM_MIDI_NOTE_POLY_OUT: SetPitch(NOTE_POLY); break;
        case HEM_MIDI_NOTE_MIN_OUT: SetPitch(NOTE_MIN); break;
        case HEM_MIDI_NOTE_MAX_OUT: SetPitch(NOTE_MAX); break;
        case HEM_MIDI_NOTE_PEDAL_OUT: SetPitch(NOTE_PEDAL); break;
        case HEM_MIDI_NOTE_INV_OUT: SetPitch(NOTE_INVERT); break;

        case HEM_MIDI_GATE_OUT: SetGate(GATE_MONO); break;
        case HEM_MIDI_GATE_POLY_OUT: SetGate(GATE_POLY); break;
        case HEM_MIDI_GATE_INV_OUT: SetGate(GATE_INVERT); break;
        case HEM_MIDI_RUN_OUT: SetGate(GATE_RUN); break;

        case HEM_MIDI_TRIG_OUT: SetTrigger(TRIG_NORMAL); break;
        case HEM_MIDI_TRIG_1ST_OUT: SetTrigger(TRIG_FIRST); break;
        case HEM_MIDI_TRIG_ALWAYS_OUT: SetTrigger(TRIG_ALWAYS); break;
        case HEM_MIDI_START_OUT: SetTrigger(TRIG_START); break;
        case HEM_MIDI_CLOCK_OUT: SetTrigger(TRIG_CLOCK2); break;
        case HEM_MIDI_CLOCK_8_OUT: SetTrigger(TRIG_CLOCK4); break;
        case HEM_MIDI_CLOCK_16_OUT: SetTrigger(TRIG_CLOCK8); break;
        case HEM_MIDI_CLOCK_24_OUT: SetTrigger(TRIG_CLOCK24); break;

        case HEM_MIDI_VEL_OUT: SetModulator(MOD_VEL_MONO); break;
        case HEM_MIDI_VEL_POLY_OUT: SetModulator(MOD_VEL_POLY); break;
        case HEM_MIDI_AT_CHAN_OUT: SetModulator(MOD_AT_CHAN); break;
        case HEM_MIDI_AT_KEY_POLY_OUT: SetModulator(MOD_AT_POLY); break;
        case HEM_MIDI_PB_OUT: SetModulator(MOD_BEND); break;

        case HEM_MIDI_CC_OUT: function = CCONTROL; break;

        default: function = NONE; break;
      }
    }
    // validation for safety
    channel &= 0x1F;
    dac_polyvoice &= 0x0F;
    if (range_low == 0 && range_high == 0) range_high = 127;
    if (range_high < range_low) range_high = range_low;
  }

  bool ProcessMsg(const MIDIMessage msg, MIDIFrame &state);

  DISALLOW_COPY_AND_ASSIGN(MIDIMapping);
};

// Lets PackingUtils know this is Packable as is.
constexpr MIDIMapping& pack(MIDIMapping& input) {
  return input;
}

struct MIDIFrame {
    MIDIMapping mapping[MIDIMAP_MAX];
    MIDIMapping outmap[ADC_CHANNEL_COUNT];

    // MIDI input stuff handled by MIDIIn applet
    NoteBuffer note_buffer[16]; // array of buffers to track all held notes on all channels
    uint8_t last_midi_channel = 0; // for MIDI In activity monitor
    uint16_t sustain_latch; // each bit is a MIDI channel's sustain state

    uint8_t pc_channel = 0; // program change channel filter, used for preset selection
    uint8_t bend_range = 12; // in semitones, for pitch bend
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
        mapping[ch].Init();
        mapping[ch].AdjustVoice(ch / 2 % DAC_CHANNEL_COUNT); // each quad is a unique voice
      }
      for (int ch = 0; ch < ADC_CHANNEL_COUNT; ++ch) {
        outmap[ch].Init();
        if (ch & 1) outmap[ch].SetGate(0);
        else outmap[ch].SetPitch(0);
        //outmap[ch].function_cc = ch + 1; // idk why I did this
      }
      clock_count = 0;
    }

    // getters for access to mappings
    uint8_t get_in_assign(int ch) {
      return mapping[ch].get_type() | mapping[ch].get_subtype();
    }
    uint8_t get_in_channel(int ch) {
      return mapping[ch].get_channel();
    }
    int8_t get_in_transpose(int ch) {
      return mapping[ch].get_transpose();
    }
    bool in_in_range(int ch, uint8_t note) {
      return mapping[ch].InRange(note);
    }

    uint8_t get_out_assign(int ch) {
      return outmap[ch].get_type() | outmap[ch].get_subtype();
    }
    uint8_t get_out_channel(int ch) {
      return outmap[ch].get_channel();
    }
    int8_t get_out_transpose(int ch) {
      return outmap[ch].get_transpose();
    }
    bool in_out_range(int ch, int note) {
      return outmap[ch].InRange(note);
    }

    void UpdateMidiChannelFilter() {
        uint16_t filter = 0;
        bool omni = false;
        for (auto &map : mapping) {
            if (map.get_type() == MIDIMapSettings::NONE) continue;
            if (map.get_channel() < 16) filter |= (1 << map.get_channel());
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
            if (map.get_type() == MIDIMapSettings::NONE || !map.IsPoly()) continue;
            if (map.get_voice() > voice) voice = map.get_voice();
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
    int outchan_last[DAC_CHANNEL_COUNT];
    uint8_t current_note[16]; // note number, per MIDI channel
    uint8_t current_ccval[DAC_CHANNEL_COUNT]; // level 0 - 127, per DAC channel
    int note_countdown[DAC_CHANNEL_COUNT];
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
    void Send(const SlewedValue *outvals);

    void SendAfterTouch(const uint8_t midi_ch, uint8_t val) {
#ifdef ARDUINO_TEENSY41
      if (~midi_msgtx_disable & mMaskUSBDev)   usbMIDI.sendAfterTouch(val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost)  usbHostMIDI[0].sendAfterTouch(val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost2) usbHostMIDI[1].sendAfterTouch(val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskSerial)   MIDI1.sendAfterTouch(val, midi_ch + 1);
#else
        usbMIDI.sendAfterTouch(val, midi_ch + 1);
#endif
    }
    void SendPitchBend(const uint8_t midi_ch, uint16_t bend) {
#ifdef ARDUINO_TEENSY41
      if (~midi_msgtx_disable & mMaskUSBDev)   usbMIDI.sendPitchBend(bend, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost)  usbHostMIDI[0].sendPitchBend(bend, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost2) usbHostMIDI[1].sendPitchBend(bend, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskSerial)   MIDI1.sendPitchBend(bend, midi_ch + 1);
#else
      usbMIDI.sendPitchBend(bend, midi_ch + 1);
#endif
    }

    void SendCC(const uint8_t midi_ch, uint8_t ccnum, uint8_t val) {
#ifdef ARDUINO_TEENSY41
      if (~midi_msgtx_disable & mMaskUSBDev)   usbMIDI.sendControlChange(ccnum, val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost)  usbHostMIDI[0].sendControlChange(ccnum, val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost2) usbHostMIDI[1].sendControlChange(ccnum, val, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskSerial)   MIDI1.sendControlChange(ccnum, val, midi_ch + 1);
#else
      usbMIDI.sendControlChange(ccnum, val, midi_ch + 1);
#endif
    }
    void SendNoteOn(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 100) {
        if (note > 127) note = current_note[midi_ch];
        else current_note[midi_ch] = note;

#ifdef ARDUINO_TEENSY41
      if (~midi_msgtx_disable & mMaskUSBDev)   usbMIDI.sendNoteOn(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost)  usbHostMIDI[0].sendNoteOn(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost2) usbHostMIDI[1].sendNoteOn(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskSerial)   MIDI1.sendNoteOn(note, vel, midi_ch + 1);
#else
      usbMIDI.sendNoteOn(note, vel, midi_ch + 1);
#endif
    }
    void SendNoteOff(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 0) {
        if (note > 127) note = current_note[midi_ch];
#ifdef ARDUINO_TEENSY41
      if (~midi_msgtx_disable & mMaskUSBDev)   usbMIDI.sendNoteOff(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost)  usbHostMIDI[0].sendNoteOff(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskUSBHost2) usbHostMIDI[1].sendNoteOff(note, vel, midi_ch + 1);
      if (~midi_msgtx_disable & mMaskSerial)   MIDI1.sendNoteOff(note, vel, midi_ch + 1);
#else
      usbMIDI.sendNoteOff(note, vel, midi_ch + 1);
#endif
    }
};

// shared IO Frame, updated every tick
// this will allow chaining applets together, multiple stages of processing
struct IOFrame {
    // settings
    bool autoMIDIOut = false;
    bool synctrig = false;
    uint8_t clockinskip[IO_CHANNEL_COUNT];
    uint8_t clockoutskip[IO_CHANNEL_COUNT];
    int8_t output_slew[IO_CHANNEL_COUNT];
    int8_t output_atten[IO_CHANNEL_COUNT]; // -126 (-200%) to 126 (+200%), 63 is 100%

    // pre-calculated clocks, subject to trigger mapping
    bool clocked[OC::DIGITAL_INPUT_LAST + IO_CHANNEL_COUNT];

    // physical input state cache
    bool gate_high[OC::DIGITAL_INPUT_LAST + IO_CHANNEL_COUNT];

    // output value cache, countdowns
    SlewedValue outputs[IO_CHANNEL_COUNT]; // now with Extra Precision!
    int clock_countdown[IO_CHANNEL_COUNT];
    int adc_lag_countdown[IO_CHANNEL_COUNT]; // Time between a clock event and an ADC read event
    // calculated values
    uint32_t last_clock[IO_CHANNEL_COUNT]; // Tick number of the last clock observed by the child class
    uint32_t cycle_ticks[IO_CHANNEL_COUNT]; // Number of ticks between last two clocks
    bool changed_cv[IO_CHANNEL_COUNT]; // Has the input changed by more than 1/8 semitone since the last read?
    int last_cv[IO_CHANNEL_COUNT]; // For change detection

    /* MIDI message queue/cache */
    MIDIFrame MIDIState;

    OC::IOFrame* current_ioframe;
    OC::IOFrame* GetLatestIOFrame() const {
      return current_ioframe;
    }

    void Init() {
      MIDIState.Init();
      for (int i = 0; i < IO_CHANNEL_COUNT; ++i) {
        output_slew[i] = 0;
        output_atten[i] = 60; // default to 100%
        clockinskip[i] = 0;
        clockoutskip[i] = 0;
      }
    }

    const int ViewOut(DAC_CHANNEL ch) const { return outputs[ch].get(output_atten[ch]); }

    // --- Soft IO ---
    int In(int ch) {
      if (ch < ADC_CHANNEL_COUNT)
        return current_ioframe->cv.pitch_values[ch];
      return 0;
    }

    void Out(DAC_CHANNEL channel, int value, bool override = false) {
      outputs[channel].set(value, override);
    }
    void ClockOut(DAC_CHANNEL ch, const int pulselength = HEMISPHERE_CLOCK_TICKS * trig_length) {
        // short circuit if skip probability is zero to avoid consuming random numbers
        if (0 == clockoutskip[ch] || random(100) >= clockoutskip[ch]) {
            clock_countdown[ch] = pulselength;
            // assign to both to override slew - instant attack
            Out(ch, HEMISPHERE_MAX_CV, true);
        }
    }
    void NudgeOutSkip(int ch, int dir) {
        clockoutskip[ch] = constrain(clockoutskip[ch] + dir, 0, 100);
    }
    void NudgeInSkip(int ch, int dir) {
        clockinskip[ch] = constrain(clockinskip[ch] + dir, 0, 100);
    }
    void NudgeSlew(int ch, int dir) {
        output_slew[ch] = constrain(output_slew[ch] + dir, 0, 100);
    }
    void NudgeAtten(int ch, int dir) {
        output_atten[ch] = constrain(output_atten[ch] + dir, -127, 127);
    }
    bool CheckSkip(int ch) {
        // short circuit if skip probability is zero to avoid consuming random numbers
        return (0 == clockinskip[ch] || random(100) >= clockinskip[ch]);
    }

    // --- Hard IO ---
    void Load(OC::IOFrame *ioframe);
    void Send(OC::IOFrame *ioframe);
};

extern IOFrame frame;

#include "CVInputMap.h"

} // namespace HS
