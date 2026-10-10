#pragma once
// Self-contained MIDI data types shared by the IOFrame engine and host-side
// tests (no Arduino dependencies — keep it that way; test/test_midi_types.cpp
// includes this directly).
#include <stdint.h>

namespace HS {

// how long a port's UI activity indicator stays lit, in 16.67kHz ticks
constexpr uint16_t kMIDIIndicatorTicks = 2000;

struct MIDINoteData {
    uint8_t note; // data1
    uint8_t vel;  // data2
};

// Fixed-capacity, insertion-ordered note stack. MIDI processing runs inside
// the core timer ISR, so this must never touch the heap. When full, the
// oldest note is dropped to make room for the newest.
struct NoteBuffer {
    static constexpr uint8_t kCapacity = 8;
    MIDINoteData data_[kCapacity];
    uint8_t count = 0;

    MIDINoteData *begin() { return data_; }
    MIDINoteData *end() { return data_ + count; }
    const MIDINoteData *begin() const { return data_; }
    const MIDINoteData *end() const { return data_ + count; }
    uint8_t size() const { return count; }
    bool empty() const { return count == 0; }
    void clear() { count = 0; }
    MIDINoteData &front() { return data_[0]; }
    MIDINoteData &back() { return data_[count ? count - 1 : 0]; }
    MIDINoteData &at(uint8_t i) { return data_[i < count ? i : 0]; }

    void remove(const uint8_t note) {
        uint8_t w = 0;
        for (uint8_t r = 0; r < count; ++r) {
            if (data_[r].note != note) data_[w++] = data_[r];
        }
        count = w;
    }
    void push(const uint8_t note, const uint8_t vel) {
        if (count == kCapacity) { // drop oldest
            for (uint8_t i = 1; i < count; ++i) data_[i - 1] = data_[i];
            --count;
        }
        data_[count++] = {note, vel};
    }
};

// ------------------------------------------------------------------
// CV/trigger -> MIDI output ports
//
// 8 ports: 0-3 = CV inputs 1-4, 4-7 = trigger inputs 1-4.
// Values are explicitly defined and bit-packed into one uint64_t;
// changing them breaks stored setups and the SysEx protocol
// (see docs/hoc-midi-sysex.md).
// ------------------------------------------------------------------
enum CVOutFn : uint8_t {
  CVFN_OFF = 0,
  CVFN_PITCH = 1,       // pitch CV, note gated by paired trigger port
  CVFN_PITCH_FREE = 2,  // new note whenever the (quantized) pitch changes
  CVFN_CC7 = 3,
  CVFN_CC14 = 4,        // CC# data1 (MSB) + data1+32 (LSB)
  CVFN_VELOCITY = 5,    // sampled at note-on by a trigger port (velocity source)
  CVFN_BEND = 6,
  CVFN_AFTERTOUCH = 7,
  CVFN_NRPN7 = 8,       // NRPN addr = data2(MSB)/data1(LSB)
  CVFN_NRPN14 = 9,
  CVFN_PROGCHANGE = 10,
  CVFN_GATE_NOTE = 11,  // CV threshold gates fixed note data1

  CVFN_COUNT
};
enum TrigOutFn : uint8_t {
  TRFN_OFF = 0,
  TRFN_NOTE = 1,        // gate -> note on/off; pitch from paired CV (if PITCH) else data1
  TRFN_NOTE_TRIG = 2,   // fixed-length note data1 on rising edge
  TRFN_NOTE_LATCH = 3,  // rising edge toggles note data1 on/off
  TRFN_CC_MOMENTARY = 4,// CC# data1: data2 on high, 0 on low
  TRFN_CC_LATCH = 5,    // rising edge toggles CC# data1 between data2 and 0
  TRFN_START = 6,
  TRFN_STOP = 7,
  TRFN_CONTINUE = 8,
  TRFN_START_STOP = 9,  // toggles Start/Stop on rising edges
  TRFN_CLOCK = 10,      // MIDI clock per rising edge, scaled by clkdiv
  TRFN_PANIC = 11,

  TRFN_COUNT
};

struct MIDIOutSettings {
  // flags bits
  static constexpr uint8_t FLAG_QUANTIZE = (1 << 0);   // quantize pitch CV
  static constexpr uint8_t FLAG_LEGATO = (1 << 1);     // no retrigger on pitch change
  static constexpr uint8_t FLAG_CV_GATE = (1 << 2);    // gate source: 0=paired trig, 1=CV threshold
  static constexpr uint8_t VEL_SOURCE_SHIFT = 3;       // bits 3-5: 0=fixed data2, 1-4=CV port 1-4
  static constexpr uint8_t VEL_SOURCE_MASK = (7 << 3);

  uint8_t function = 0;   // CVOutFn or TrigOutFn depending on port class (4 bits)
  uint8_t channel = 0;    // MIDI channel 0-15 (4 bits)
  uint8_t data1 = 60;     // note# / CC# / NRPN LSB (7 bits)
  uint8_t data2 = 100;    // on-value / velocity / NRPN MSB (7 bits)
  int8_t transpose = 0;   // -48..48, stored +64 (7 bits)
  uint8_t range_low = 0;  // output clip low (7 bits)
  uint8_t range_high = 127; // output clip high (7 bits)
  uint8_t flags = 0;      // (8 bits)
  uint8_t clkdiv = 0;     // clock divide/mult index (4 bits)

  uint8_t velocity_source() const { return (flags & VEL_SOURCE_MASK) >> VEL_SOURCE_SHIFT; }

  // 55 bits used; layout is the serialized format (EEPROM + SysEx dumps)
  uint64_t Pack() const {
    return (uint64_t(function & 0x0f))
         | (uint64_t(channel & 0x0f) << 4)
         | (uint64_t(data1 & 0x7f) << 8)
         | (uint64_t(data2 & 0x7f) << 15)
         | (uint64_t(uint8_t(transpose + 64) & 0x7f) << 22)
         | (uint64_t(range_low & 0x7f) << 29)
         | (uint64_t(range_high & 0x7f) << 36)
         | (uint64_t(flags) << 43)
         | (uint64_t(clkdiv & 0x0f) << 51);
  }
  void Unpack(const uint64_t data) {
    function   = data & 0x0f;
    channel    = (data >> 4) & 0x0f;
    data1      = (data >> 8) & 0x7f;
    data2      = (data >> 15) & 0x7f;
    transpose  = int8_t((data >> 22) & 0x7f) - 64;
    range_low  = (data >> 29) & 0x7f;
    range_high = (data >> 36) & 0x7f;
    flags      = (data >> 43) & 0xff;
    clkdiv     = (data >> 51) & 0x0f;
    Sanitize();
  }
  void Sanitize() {
    if (transpose < -48 || transpose > 48) transpose = 0;
    if (range_high < range_low) range_high = range_low;
  }
};

struct MIDIOutPort : public MIDIOutSettings {
  // runtime state, not serialized
  bool gated = false;        // note currently on
  bool prev_gate = false;    // gate level last tick (edge detection)
  bool latch_state = false;  // for *_LATCH functions
  uint8_t last_note = 0;     // note# actually sent (for the matching note-off)
  uint8_t last_channel = 0;  // channel it was sent on
  uint16_t last_value = 0xffff; // last 7/14-bit value sent (rate limiting)
  int16_t trigout_countdown = 0; // NOTE_TRIG remaining ticks
  uint8_t lag_count = 0;     // ADC settling delay after gate rise, in ticks
  uint16_t indicator = 0;    // UI activity countdown, in ticks

  void ResetRuntime() {
    gated = prev_gate = latch_state = false;
    last_note = last_channel = 0;
    last_value = 0xffff;
    trigout_countdown = 0;
    lag_count = 0;
    indicator = 0;
  }
};

} // namespace HS
