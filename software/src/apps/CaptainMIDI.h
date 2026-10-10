// Copyright (c) 2018, Jason Justian
//
// Menu & screen cursor Copyright (c) 2016 Patrick Dowling
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

// See https://www.pjrc.com/teensy/td_midi.html
//
// Adapted for T4.x and 8-channel hardware by djphazer

#pragma once

static constexpr int MIDI_SETUP_COUNT = 4;
static constexpr int MIDI_PARAMETER_COUNT = 40;
static constexpr int MIDI_CURRENT_SETUP = (MIDI_PARAMETER_COUNT * MIDI_SETUP_COUNT);
static constexpr int MIDI_SETTING_COUNT = (MIDI_CURRENT_SETUP + 1);
static constexpr int MIDI_LOG_MAX_SIZE = 101;

enum MIDI_IN_FUNCTION : uint8_t {
  // T32 only, deprecated
    MIDI_IN_OFF,
    MIDI_IN_NOTE = HEM_MIDI_NOTE_OUT,
    MIDI_IN_GATE = HEM_MIDI_GATE_OUT,
    MIDI_IN_TRIGGER = HEM_MIDI_TRIG_OUT,
    MIDI_IN_VELOCITY = HEM_MIDI_VEL_OUT,
    MIDI_IN_MOD = HEM_MIDI_CC_OUT,
    MIDI_IN_AFTERTOUCH = HEM_MIDI_AT_CHAN_OUT,
    MIDI_IN_PITCHBEND = HEM_MIDI_PB_OUT,
    MIDI_IN_EXPRESSION,
    MIDI_IN_PAN,
    MIDI_IN_HOLD,
    MIDI_IN_BREATH,
    MIDI_IN_Y_AXIS,

    // clock divisions
    MIDI_IN_CLOCK_4TH = HEM_MIDI_CLOCK_OUT,
    MIDI_IN_CLOCK_8TH = HEM_MIDI_CLOCK_8_OUT,
    MIDI_IN_CLOCK_16TH = HEM_MIDI_CLOCK_16_OUT,
    MIDI_IN_CLOCK_24PPQN = HEM_MIDI_CLOCK_24_OUT,
};

using Type = MIDIMapSettings::Type;
using PitchType = MIDIMapSettings::PitchType;
using GateType = MIDIMapSettings::GateType;

// labels for the CV/trigger -> MIDI engine functions (HSMIDITypes.h)
const char* const cv_out_fn_names[HS::CVFN_COUNT] = {
    "--", "Pitch", "FreeN", "CC", "CC14", "Veloc", "Bend", "Aft", "NRPN", "NRP14", "PgmC", "GateN"
};
const char* const trig_out_fn_names[HS::TRFN_COUNT] = {
    "--", "Note", "Trig", "Latch", "CC", "CCLat", "Start", "Stop", "Cont", "Run", "Clock", "Panic"
};

const char* const midi2cv_label[] = {
  "MIDI > A", "MIDI > B", "MIDI > C", "MIDI > D",
  "MIDI > E", "MIDI > F", "MIDI > G", "MIDI > H",
};
const char* const cv2midi_label[] = {
  "CV1>MIDI", "CV2>MIDI", "CV3>MIDI", "CV4>MIDI",
  "CV5>MIDI", "CV6>MIDI", "CV7>MIDI", "CV8>MIDI",
};
const char* const trig2midi_label[] = {
  "TR1>MIDI", "TR2>MIDI", "TR3>MIDI", "TR4>MIDI",
  "TR5>MIDI", "TR6>MIDI", "TR7>MIDI", "TR8>MIDI",
};

const char* const captain_off_on[2] = { "Off", "On" };
const char* const captain_vel_src[9] = {
  "Fixed", "CV1", "CV2", "CV3", "CV4", "CV5", "CV6", "CV7", "CV8"
};
const char* const captain_gate_src[9] = {
  "TRjack", "CV1", "CV2", "CV3", "CV4", "CV5", "CV6", "CV7", "CV8"
};
const char* const captain_clkdiv[16] = {
  "x1", "x2", "x3", "x4", "x6", "x8", "x12", "x24", "x48", "x96",
  "x1", "x1", "x1", "x1", "x1", "x1"
};

// per channel
const settings::ValueAttributes CaptainSettings[] = {
  // 0: Assigned function, MIDI-to-CV
  { 0, 0, HEM_MIDI_MAX_FUNCTION, "", midi_fn_name, settings::STORAGE_TYPE_U8 },
  // 1: CV-to-MIDI
  { 0, 0, HS::CVFN_COUNT-1, "", cv_out_fn_names, settings::STORAGE_TYPE_U8 },
  // 2: Trigger-to-MIDI
  { 0, 0, HS::TRFN_COUNT-1, "", trig_out_fn_names, settings::STORAGE_TYPE_U8 },

  // 3: Channel
  { 0, 0, 16, "", midi_channels, settings::STORAGE_TYPE_U8 },
  // 4: Transpose
  { 0, -48, 48, "", NULL, settings::STORAGE_TYPE_I8 },
  // 5: Range Low
  { 0, 0, 127, "", midi_note_numbers, settings::STORAGE_TYPE_U8 },
  // 6: Range High
  { 0, 0, 127, "", midi_note_numbers, settings::STORAGE_TYPE_U8 },

  // 7: Data 1 as note number
  { 0, 0, 127, "", midi_note_numbers, settings::STORAGE_TYPE_U8 },
  // 8: Data 1 as plain number (CC#/NRPN LSB)
  { 0, 0, 127, "", NULL, settings::STORAGE_TYPE_U8 },
  // 9: Data 2 (on-value / velocity / NRPN MSB)
  { 0, 0, 127, "", NULL, settings::STORAGE_TYPE_U8 },
  // 10: Off/On (quantize, legato)
  { 0, 0, 1, "", captain_off_on, settings::STORAGE_TYPE_U8 },
  // 11: Velocity source
  { 0, 0, ADC_CHANNEL_COUNT < 7 ? ADC_CHANNEL_COUNT : 7, "", captain_vel_src, settings::STORAGE_TYPE_U8 },
  // 12: Clock multiplier
  { 0, 0, 15, "", captain_clkdiv, settings::STORAGE_TYPE_U8 },
  // 13: Poly voice
  { 0, 0, DAC_CHANNEL_COUNT - 1, "", NULL, settings::STORAGE_TYPE_U8 },
  // 14: Trigger gate source (TR jack or a CV input via ADC threshold)
  { 0, 0, ADC_CHANNEL_COUNT, "", captain_gate_src, settings::STORAGE_TYPE_U8 },
};

enum CaptainsKeys : uint16_t {
  SETUP_KEY = 0,

  // upper 7 bits of mapping key
  INPUT_MAP_KEY = 1 << 9,
  OUTPUT_MAP_KEY = 2 << 9,
};

const char* const midi_messages[7] = {
    "Note", "Off", "CC#", "Aft", "Bend", "SysEx", "Diag"
};

// Teensy USB device state: nonzero once a host has configured us,
// cleared on bus reset. The closest thing to "is the computer there".
extern "C" volatile uint8_t usb_configuration;

//#define MIDI_DIAGNOSTIC
struct CaptainMIDILog {
    bool midi_in; // 0 = out, 1 = in
    char io; // 1, 2, 3, 4, A, B, C, D
    uint8_t message; // 0 = Note On, 1 = Note Off, 2 = CC, 3 = Aftertouch, 4 = Bend, 5 = SysEx, 6 = Diagnostic
    uint8_t channel; // MIDI channel
    int16_t data1;
    int16_t data2;

    void DrawAt(int y) {
        if (message == 5) {
            int app_code = static_cast<char>(data1);
            if (app_code > 0) {
                graphics.setPrintPos(1, y);
                graphics.print("SysEx: ");
                if (app_code == 'M') graphics.print("Captain MIDI");
                if (app_code == 'H') graphics.print("Hemisphere");
                if (app_code == 'D') graphics.print("D. Timeline");
                if (app_code == 'E') graphics.print("Scale Ed");
                if (app_code == 'T') graphics.print("Enigma");
                if (app_code == 'W') graphics.print("Waveform Ed");
                if (app_code == '_') graphics.print("O_C EEPROM");
                if (app_code == 'B') graphics.print("Backup");
                if (app_code == 'N') graphics.print("Neural Net");
            }
        } else {
            graphics.setPrintPos(1, y);
            if (midi_in) graphics.print(">");
            graphics.print(io);
            if (!midi_in) graphics.print(">");
            graphics.print(" ");
            graphics.print(midi_channels[channel]);
            graphics.setPrintPos(37, y);

            graphics.print(midi_messages[message]);
            graphics.setPrintPos(73, y);

            uint8_t x_offset = (data2 < 100) ? 6 : 0;
            x_offset += (data2 < 10) ? 6 : 0;

            if (message == 0 || message == 1) {
                graphics.print(midi_note_numbers[data1]);
                graphics.setPrintPos(91 + x_offset, y);
                graphics.print(data2); // Velocity
            }

            if (message == 2 || message == 3) {
                if (message == 2) graphics.print(data1); // Controller number
                graphics.setPrintPos(91 + x_offset, y);
                graphics.print(data2); // Value
            }

            if (message == 4) {
                if (data2 > 0) graphics.print("+");
                graphics.print(data2); // Aftertouch or bend value
            }

            if (message == 6) {
                graphics.print(data1);
                graphics.print("/");
                graphics.print(data2);
            }
        }
    }
};

OC_APP_CLASS(AppCaptainMIDI, TWOCCS("MI"), "Captain MIDI", "MIDI I/O"),
  public HSApplication, public SystemExclusiveHandler
{
public:
    // number of stored setups, and the serialized size of one setup:
    // all in-maps + all out-ports, one packed uint64 each.
    // T3 EEPROM cost scales with this — builds with many apps sharing the
    // pool override it (see platformio.ini; the static_assert in
    // apps/_config.h is the budget gate)
#ifndef CAPTAIN_SETUP_COUNT
#define CAPTAIN_SETUP_COUNT 1
#endif
    static constexpr int kNumSetups = CAPTAIN_SETUP_COUNT;
    static constexpr size_t kSetupBlobWords =
        MIDIMAP_MAX + HS::MIDIFrame::kOutPorts;
    // version/active/reserved header + setups (see SaveAppData)
    static constexpr size_t kCaptainStorageSize =
        4 + kNumSetups * kSetupBlobWords * sizeof(uint64_t);

#ifdef __IMXRT1062__
  // T4.x persists to PhzConfig (LittleFS) instead of the EEPROM pool
  OC_APP_INTERFACE_DECLARE(AppCaptainMIDI, 0);
#else
  OC_APP_INTERFACE_DECLARE(AppCaptainMIDI, AppCaptainMIDI::kCaptainStorageSize);
#endif

    static constexpr int MIDI_INDICATOR_COUNTDOWN = 2000;

    OC::menu::ScreenCursor<OC::menu::kScreenLines> cursor;       // overview rows
    OC::menu::ScreenCursor<OC::menu::kScreenLines> param_cursor; // detail rows

    static constexpr int kNumRows = DAC_CHANNEL_COUNT + HS::MIDIFrame::kOutPorts;

    void Start() {
        detail_port = -1;
        display = 0;
        cursor.Init(0, kNumRows - 1);
        param_cursor.Init(0, 0);
        log_index = 0;
        log_view = 0;
        active_setup = 0;
        // seed all setups from the freshly-initialized live state
        for (int s = 0; s < kNumSetups; ++s) PackSetup(s);
        Reset();
    }

    // pack the live mapping/outports state into setups[s]
    void PackSetup(int s) {
        for (int i = 0; i < MIDIMAP_MAX; ++i)
            setups[s].inmaps[i] = frame.MIDIState.mapping[i].Pack();
        for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
            setups[s].outports[i] = frame.MIDIState.outports[i].Pack();
    }

    // Clamp a packed in-map blob so stale/garbage EEPROM data can never
    // index a name table out of bounds (that reads a wild pointer:
    // garbled draws at best, a hard fault at worst).
    static uint64_t SanitizeInMapPacked(uint64_t d) {
        auto byte = [&d](int n) { return uint8_t((d >> (n * 8)) & 0xff); };
        auto setbyte = [&d](int n, uint8_t v) {
            d = (d & ~(uint64_t(0xff) << (n * 8))) | (uint64_t(v) << (n * 8));
        };
        // function: type in upper 3 bits, CCONTROL (5) is the highest
        if ((byte(1) >> 5) > 5) setbyte(1, 0);
        if (byte(2) > 16) setbyte(2, 16);                       // channel
        const int8_t t = int8_t(byte(4));                       // transpose
        if (t < -48 || t > 48) setbyte(4, 0);
        if (byte(5) > 127) setbyte(5, 0);                       // range_low
        if (byte(6) > 127) setbyte(6, 127);                     // range_high
        if (byte(3) >= DAC_CHANNEL_COUNT) setbyte(3, 0);        // polyvoice
        return d;
    }

    // apply setups[s] to the live state
    void UnpackSetup(int s) {
        for (int i = 0; i < MIDIMAP_MAX; ++i) {
            setups[s].inmaps[i] = SanitizeInMapPacked(setups[s].inmaps[i]);
            frame.MIDIState.mapping[i].Unpack(setups[s].inmaps[i]);
        }
        for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i) {
            HS::MIDIOutPort &o = frame.MIDIState.outports[i];
            o.Unpack(setups[s].outports[i]);
            // clamp per port class: garbage functions crash the label lookup
            const uint8_t fn_count = (i >= HS::MIDIFrame::kCVOutPorts)
                ? HS::TRFN_COUNT : HS::CVFN_COUNT;
            if (o.function >= fn_count) o.function = 0;
            if (o.velocity_source() > ADC_CHANNEL_COUNT)
                o.flags &= ~HS::MIDIOutSettings::VEL_SOURCE_MASK;
            o.ResetRuntime();
            setups[s].outports[i] = o.Pack(); // keep the blob canonical too
        }
        frame.MIDIState.UpdateMidiChannelFilter();
        frame.MIDIState.UpdateMaxPolyphony();
    }

    void Suspend() {
        PackSetup(active_setup);
#ifdef __IMXRT1062__
        StoreData();
#endif
        OnSendSysEx();
    }

#ifdef __IMXRT1062__
    void StoreData() {
        PhzConfig::setValue(SETUP_KEY, active_setup);
        for (int s = 0; s < kNumSetups; ++s) {
          for (int i = 0; i < MIDIMAP_MAX; ++i)
            PhzConfig::setValue(INPUT_MAP_KEY + i + s*MIDIMAP_MAX, setups[s].inmaps[i]);
          for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
            PhzConfig::setValue(OUTPUT_MAP_KEY + i + s*HS::MIDIFrame::kOutPorts, setups[s].outports[i]);
        }
        PhzConfig::save_config("CAPTAIN.DAT");
    }
    void Resume() {
        PhzConfig::load_config("CAPTAIN.DAT");
        uint64_t data = 0;
        PhzConfig::getValue(SETUP_KEY, data);
        active_setup = constrain((int)data, 0, kNumSetups - 1);
        for (int s = 0; s < kNumSetups; ++s) {
          for (int i = 0; i < MIDIMAP_MAX; ++i)
            if (PhzConfig::getValue(INPUT_MAP_KEY + i + s*MIDIMAP_MAX, data))
              setups[s].inmaps[i] = data;
          for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
            if (PhzConfig::getValue(OUTPUT_MAP_KEY + i + s*HS::MIDIFrame::kOutPorts, data))
              setups[s].outports[i] = data;
        }
        UnpackSetup(active_setup);
        LeaveDetail();
        saved_checksum = live_checksum_prev = LiveChecksum();
    }
#else
    // T3.x: setups[] persist via SaveAppData/RestoreAppData (EEPROM pool)
    void Resume() {
        UnpackSetup(active_setup);
        LeaveDetail();
        saved_checksum = live_checksum_prev = LiveChecksum();
    }
#endif

    void Controller() {
        // Drain incoming MIDI traffic; cap per-tick work to bound ISR time
        int budget = 4;
        while (budget-- > 0 && poll_midi(usbMIDI)) {}
#ifdef ARDUINO_TEENSY41
        budget = 4;
        while (budget-- > 0 && poll_midi(usbHostMIDI[0])) {}
        budget = 4;
        while (budget-- > 0 && poll_midi(usbHostMIDI[1])) {}
        budget = 4;
        while (budget-- > 0 && poll_midi(MIDI1)) {}
#endif

        // Convert CV/trigger inputs to outgoing MIDI messages
        frame.MIDIState.ProcessOutputs(frame);

        // set CV outputs from MIDI mappings
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ch++)
        {
            // Handle clock timing
            if (indicator_in[ch] > 0) --indicator_in[ch];

            MIDIMapping &map = frame.MIDIState.mapping[ch];

            // ViewOut, not output: the bend term lives there. Driving the
            // raw value meant pitch bend was received, stored and DRAWN --
            // the row's level bar moved -- while the jack never carried it.
            // Found by measurement: a -2.5 cent/semitone correction sent
            // over bend changed a tuning fit by exactly nothing while the
            // display swore it was happening (2026-08-28, tune_check
            // --corrected). ViewOut returns raw output for non-pitch maps
            // and is a plain multiply, safe at 16.67kHz.
            Out(ch, map.ViewOut());
        }
    }

    void View() const;
    void MainView() const {
        if (copy_mode) DrawCopyScreen();
        else if (display) DrawLogScreen();
        else if (detail_port >= 0) DrawDetail();
        else DrawOverview();
    }

    // ---- two-level navigation ----
    // Overview: one row per port; turn R to scroll, press R to open the port.
    // Detail: all parameters of one port; turn R scrolls, press R edits,
    // press L goes back. Turn L: switch setup (overview) / switch port
    // (detail) / scroll (log view).

    int DetailParamCount(int row) const {
        if (row < DAC_CHANNEL_COUNT) return 6;
        const int p = row - DAC_CHANNEL_COUNT;
        return (p >= HS::MIDIFrame::kCVOutPorts) ? 10 : 10;
    }

    void EnterDetail(int row) {
        detail_port = row;
        param_cursor.Init(0, DetailParamCount(row) - 1);
    }
    void LeaveDetail() { detail_port = -1; }

    const char* DetailParamLabel(int row, int par) const {
        static const char* const in_labels[6] = {
            "Assign", "Channel", "Transpose", "Range Low", "Range High", "Voice" };
        static const char* const cv_labels[10] = {
            "Function", "Channel", "Data 1", "Data 2", "Transpose",
            "Range Low", "Range High", "Quantize", "Legato", "Vel Source" };
        static const char* const tr_labels[10] = {
            "Function", "Channel", "Data 1", "Data 2", "Transpose",
            "Range Low", "Range High", "Vel Source", "Clock Mult",
            "Gate Src" };
        if (row < DAC_CHANNEL_COUNT) return in_labels[par];
        const int p = row - DAC_CHANNEL_COUNT;
        return (p >= HS::MIDIFrame::kCVOutPorts) ? tr_labels[par] : cv_labels[par];
    }

    // attr index into CaptainSettings for the value column / edit icon
    int DetailParamAttr(int row, int par) const {
        if (row < DAC_CHANNEL_COUNT) {
            static const uint8_t a[6] = { 0, 3, 4, 5, 6, 13 };
            return a[par];
        }
        const int p = row - DAC_CHANNEL_COUNT;
        const bool is_trig = p >= HS::MIDIFrame::kCVOutPorts;
        const HS::MIDIOutPort &o = frame.MIDIState.outports[p];
        if (par == 0) return is_trig ? 2 : 1;
        if (par == 2) { // data1: note names when it denotes a note
            const bool note_d1 = is_trig
                ? (o.function >= HS::TRFN_NOTE && o.function <= HS::TRFN_NOTE_LATCH)
                : (o.function == HS::CVFN_GATE_NOTE);
            return note_d1 ? 7 : 8;
        }
        if (is_trig) {
            static const uint8_t a[10] = { 2, 3, 8, 9, 4, 5, 6, 11, 12, 14 };
            return a[par];
        }
        static const uint8_t a[10] = { 1, 3, 8, 9, 4, 5, 6, 10, 10, 11 };
        return a[par];
    }

    int DetailParamValue(int row, int par) const {
        if (row < DAC_CHANNEL_COUNT) {
            MIDIMapping &m = frame.MIDIState.mapping[row];
            switch (par) {
              case 0: return m.get_type() | m.get_subtype();
              case 1: return m.get_channel();
              case 2: return m.get_transpose();
              case 3: return m.get_low();
              case 4: return m.get_high();
              case 5: return m.get_voice();
            }
            return 0;
        }
        const int p = row - DAC_CHANNEL_COUNT;
        const bool is_trig = p >= HS::MIDIFrame::kCVOutPorts;
        const HS::MIDIOutPort &o = frame.MIDIState.outports[p];
        switch (par) {
          case 0: return o.function;
          case 1: return o.channel;
          case 2: return o.data1;
          case 3: return o.data2;
          case 4: return o.transpose;
          case 5: return o.range_low;
          case 6: return o.range_high;
        }
        if (is_trig) {
            if (par == 7) return o.velocity_source();
            if (par == 8) return o.clkdiv;
            if (par == 9) // gate source: 0 = TR jack, 1..N = CV input
                return (o.flags & HS::MIDIOutSettings::FLAG_CV_GATE)
                    ? (o.clkdiv % HS::MIDIFrame::kCVOutPorts) + 1 : 0;
        } else {
            if (par == 7) return (o.flags & HS::MIDIOutSettings::FLAG_QUANTIZE) ? 1 : 0;
            if (par == 8) return (o.flags & HS::MIDIOutSettings::FLAG_LEGATO) ? 1 : 0;
            if (par == 9) return o.velocity_source();
        }
        return 0;
    }

    void EditDetailParam(int row, int par, int dir) {
        if (row < DAC_CHANNEL_COUNT) {
            MIDIMapping &m = frame.MIDIState.mapping[row];
            switch (par) {
              case 0: m.AdjustFunction(dir); break;
              case 1: m.AdjustChannel(dir); break;
              case 2: m.AdjustTranspose(dir); break;
              case 3: m.AdjustRangeLow(dir); break;
              case 4: m.AdjustRangeHigh(dir); break;
              case 5: m.AdjustVoice(dir); break;
            }
            return;
        }
        const int p = row - DAC_CHANNEL_COUNT;
        const bool is_trig = p >= HS::MIDIFrame::kCVOutPorts;
        HS::MIDIOutPort &o = frame.MIDIState.outports[p];
        const int fn_count = is_trig ? HS::TRFN_COUNT : HS::CVFN_COUNT;
        const int max_vel_src = ADC_CHANNEL_COUNT < 7 ? ADC_CHANNEL_COUNT : 7;
        auto edit_vel_src = [&](int d) {
            int src = constrain(int(o.velocity_source()) + d, 0, max_vel_src);
            o.flags = (o.flags & ~HS::MIDIOutSettings::VEL_SOURCE_MASK)
                    | (src << HS::MIDIOutSettings::VEL_SOURCE_SHIFT);
        };
        switch (par) {
          case 0:
            o.function = constrain(o.function + dir, 0, fn_count - 1);
            o.ResetRuntime();
            break;
          case 1: o.channel = constrain(o.channel + dir, 0, 15); break;
          case 2: o.data1 = constrain(o.data1 + dir, 0, 127); break;
          case 3: o.data2 = constrain(o.data2 + dir, 0, 127); break;
          case 4: o.transpose = constrain(o.transpose + dir, -48, 48); break;
          case 5: o.range_low = constrain(o.range_low + dir, 0, o.range_high); break;
          case 6: o.range_high = constrain(o.range_high + dir, o.range_low, 127); break;
          default:
            if (is_trig) {
                if (par == 7) edit_vel_src(dir);
                if (par == 8) o.clkdiv = constrain(o.clkdiv + dir, 0, 15);
                if (par == 9) {
                    const int cur = (o.flags & HS::MIDIOutSettings::FLAG_CV_GATE)
                        ? (o.clkdiv % HS::MIDIFrame::kCVOutPorts) + 1 : 0;
                    const int v = constrain(cur + dir, 0, ADC_CHANNEL_COUNT);
                    if (v == 0) {
                        o.flags &= ~HS::MIDIOutSettings::FLAG_CV_GATE;
                    } else {
                        o.flags |= HS::MIDIOutSettings::FLAG_CV_GATE;
                        o.clkdiv = v - 1;
                    }
                }
            } else {
                if (par == 7 && dir) o.flags ^= HS::MIDIOutSettings::FLAG_QUANTIZE;
                if (par == 8 && dir) o.flags ^= HS::MIDIOutSettings::FLAG_LEGATO;
                if (par == 9) edit_vel_src(dir);
            }
            break;
        }
    }

    void SelectSetup(int setup_number) {
        // moving to another setup?
        if (setup_number != get_setup_number()) {
          PackSetup(active_setup); // store current settings
          active_setup = setup_number;
          UnpackSetup(active_setup);
          Reset();
        }
    }

    void LeftEncoderTurn(int dir) {
        if (display) {
            // Scroll Log view
            if (log_index > 6) log_view = constrain(log_view + dir, 0, log_index - 6);
        } else if (detail_port >= 0) {
            // browse adjacent ports without leaving the editor
            const int row = constrain(detail_port + dir, 0, kNumRows - 1);
            if (row != detail_port) EnterDetail(row);
        } else {
            SwitchSetup(dir);
        }
    }

    void SwitchSetup(int dir) {
        if (copy_mode) {
            copy_setup_target = constrain(copy_setup_target + dir, 0, kNumSetups - 1);
        } else {
            int new_setup = constrain(get_setup_number() + dir, 0, kNumSetups - 1);
            SelectSetup(new_setup);
        }
    }

    void ToggleDisplay() {
        if (copy_mode) copy_mode = 0;
        else if (detail_port >= 0) LeaveDetail(); // back to overview
        else display = 1 - display;
    }

    void Reset() {
        // Reset the interface states
        for (int ch = 0; ch < DAC_CHANNEL_COUNT; ch++)
        {
            note_in[ch] = -1;
            indicator_in[ch] = 0;
            Out(ch, 0);
        }
        frame.MIDIState.clock_count = 0;
    }

    // Queue a panic; the MIDI traffic is sent from Loop(), not the ISR/UI
    void Panic() {
        Reset();
        frame.MIDIState.panic_request = true;
    }

    // build+send one protocol frame: F0 7D 62 4D <ver> <cmd> <payload> F7
    void SendSyxFrame(uint8_t cmd, const uint8_t *payload, uint8_t plen) {
        uint8_t buf[64];
        uint8_t n = 0;
        buf[n++] = 0xF0; buf[n++] = 0x7D; buf[n++] = 0x62; buf[n++] = 'M';
        buf[n++] = SYX_PROTO_VERSION;
        buf[n++] = cmd;
        for (uint8_t i = 0; i < plen && n < 62; ++i) buf[n++] = payload[i] & 0x7f;
        buf[n++] = 0xF7;
        usbMIDI.sendSysEx(n, buf, true);
    }

    // the 7 in-map / 9 out-port parameter values from a packed blob,
    // encoded exactly as GET_PARAM returns them
    static void InMapParams(uint64_t d, uint8_t out[7]) {
        const uint8_t fcc = d & 0xff;
        out[0] = uint8_t((d >> 8) & 0xff) >> 5;              // type
        out[1] = (fcc == 0xff) ? 127 : (fcc & 0x7f);         // subtype/CC
        out[2] = (d >> 16) & 0xff;                           // channel
        out[3] = uint8_t(int8_t((d >> 32) & 0xff) + 64);     // transpose+64
        out[4] = (d >> 40) & 0x7f;                           // range_low
        out[5] = (d >> 48) & 0x7f;                           // range_high
        out[6] = (d >> 24) & 0xff;                           // polyvoice
    }
    static void OutPortParams(uint64_t d, uint8_t out[9]) {
        HS::MIDIOutSettings tmp;
        tmp.Unpack(d);
        out[0] = tmp.function; out[1] = tmp.channel;
        out[2] = tmp.data1; out[3] = tmp.data2;
        out[4] = uint8_t(tmp.transpose + 64);
        out[5] = tmp.range_low; out[6] = tmp.range_high;
        out[7] = tmp.flags; out[8] = tmp.clkdiv;
    }

    // full-setup dump as a stream of DUMP_DATA packets + DUMP_END
    void SendDump(uint8_t s) {
        if (s == active_setup) PackSetup(s); // canonicalize live edits
        constexpr int kRecBytes =
            (2 + kGlobalParamCount) + MIDIMAP_MAX * (2 + kInParamCount)
            + HS::MIDIFrame::kOutPorts * (2 + kOutParamCount);
        uint8_t rec[kRecBytes];
        int rn = 0;

        rec[rn++] = CLS_GLOBAL; rec[rn++] = 0;
        for (uint8_t par = 0; par < kGlobalParamCount; ++par) {
            uint8_t v = 0;
            get_param(CLS_GLOBAL, 0, par, v);
            rec[rn++] = v;
        }
        for (int i = 0; i < MIDIMAP_MAX; ++i) {
            rec[rn++] = CLS_IN; rec[rn++] = i;
            InMapParams(setups[s].inmaps[i], &rec[rn]);
            rn += kInParamCount;
        }
        for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i) {
            const bool trig = i >= HS::MIDIFrame::kCVOutPorts;
            rec[rn++] = trig ? CLS_TR : CLS_CV;
            rec[rn++] = trig ? (i - HS::MIDIFrame::kCVOutPorts) : i;
            OutPortParams(setups[s].outports[i], &rec[rn]);
            rn += kOutParamCount;
        }

        // chunk whole records into <=49-byte packet payloads
        uint8_t xorsum = 0;
        for (int i = 0; i < rn; ++i) xorsum ^= rec[i];
        // pre-count packets
        uint8_t total = 0;
        for (int i = 0; i < rn; ) {
            int take = 0;
            while (i + take < rn) {
                const uint8_t cls = rec[i + take];
                const int rlen = 2 + ((cls == CLS_GLOBAL) ? kGlobalParamCount
                               : (cls == CLS_IN) ? kInParamCount : kOutParamCount);
                if (take + rlen > 49) break;
                take += rlen;
            }
            i += take;
            ++total;
        }
        uint8_t seq = 0;
        for (int i = 0; i < rn; ) {
            uint8_t pkt[52];
            int n = 0;
            pkt[n++] = s; pkt[n++] = seq; pkt[n++] = total;
            while (i < rn) {
                const uint8_t cls = rec[i];
                const int rlen = 2 + ((cls == CLS_GLOBAL) ? kGlobalParamCount
                               : (cls == CLS_IN) ? kInParamCount : kOutParamCount);
                if (n - 3 + rlen > 49) break;
                memcpy(&pkt[n], &rec[i], rlen);
                n += rlen;
                i += rlen;
            }
            SendSyxFrame(SYX_DUMP_DATA, pkt, n);
            ++seq;
        }
        const uint8_t endpkt[3] = {s, seq, uint8_t(xorsum & 0x7f)};
        SendSyxFrame(SYX_DUMP_END, endpkt, 3);
    }

    // ---- auto-save ----
    // Persist every change (UI or SysEx) once edits settle for a few
    // seconds. Debounced so encoder sweeps don't hammer the EEPROM.
    static constexpr uint8_t kAutoSaveSettleSecs = 3;
    uint32_t autosave_last_check = 0;
    uint32_t live_checksum_prev = 0;
    uint32_t saved_checksum = 0;
    uint8_t settle_seconds = 0;

    uint32_t LiveChecksum() const {
        uint32_t h = 2166136261u ^ active_setup;
        auto mix = [&h](uint64_t d) {
            h = (h ^ uint32_t(d)) * 16777619u;
            h = (h ^ uint32_t(d >> 32)) * 16777619u;
        };
        for (int i = 0; i < MIDIMAP_MAX; ++i)
            mix(frame.MIDIState.mapping[i].Pack());
        for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
            mix(frame.MIDIState.outports[i].Pack());
        for (int s = 0; s < kNumSetups; ++s) {
            for (int i = 0; i < MIDIMAP_MAX; ++i) mix(setups[s].inmaps[i]);
            for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i) mix(setups[s].outports[i]);
        }
        mix(uint64_t(frame.MIDIState.bend_range) | (uint64_t(frame.MIDIState.poly_mode) << 8)
            | (uint64_t(frame.MIDIState.pc_channel) << 16) | (uint64_t(HS::trig_length) << 24));
        return h;
    }

    void PersistNow() {
        PackSetup(active_setup);
#ifdef __IMXRT1062__
        StoreData();
#else
        OC::save_app_data(); // global settings + every app's EEPROM chunk
#endif
        saved_checksum = live_checksum_prev = LiveChecksum();
        settle_seconds = 0;
    }

    void AutoSaveTick() {
        const uint32_t now = millis();
        if (now - autosave_last_check < 1000) return;
        autosave_last_check = now;
        const uint32_t cs = LiveChecksum();
        if (cs != live_checksum_prev) {
            live_checksum_prev = cs; // still being edited; restart settle timer
            settle_seconds = 0;
        } else if (cs != saved_checksum && ++settle_seconds >= kAutoSaveSettleSecs) {
            PersistNow();
        }
    }

    bool LiveConfigDirty() const {
        for (int i = 0; i < MIDIMAP_MAX; ++i)
            if (frame.MIDIState.mapping[i].Pack() != setups[active_setup].inmaps[i]) return true;
        for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
            if (frame.MIDIState.outports[i].Pack() != setups[active_setup].outports[i]) return true;
        return false;
    }

    // runs in the main loop (not the ISR): drains deferred work
    void DoLoop() {
        auto &hMIDI = frame.MIDIState;

        if (syx_rx_len) {
            HandleSysEx(syx_rx_buf, syx_rx_len);
            syx_rx_len = 0;
        }

        AutoSaveTick();

        // surface engine sends in the log display
        HS::MIDIFrame::OutLogEvent e;
        while (hMIDI.PopOutLog(e)) {
            if (e.message == 4) { // bend: reassemble signed value
                const int bend = ((e.data2 << 7) | e.data1) - 8192;
                UpdateLog(0, e.port, 4, e.channel, 0, bend);
            } else {
                UpdateLog(0, e.port, e.message, e.channel, e.data1, e.data2);
            }
        }

        if (hMIDI.panic_request) {
            hMIDI.panic_request = false;
            hMIDI.PanicOutputs(); // targeted note-offs for engine notes
            for (int ch = 0; ch < 16; ++ch) {
                hMIDI.SendCC(ch, 120, 0); // All Sound Off
                hMIDI.SendCC(ch, 123, 0); // All Notes Off
            }
        }

        if (!syx_pending) return;
        const uint8_t pending = syx_pending;
        syx_pending = 0;

        if (pending & PEND_REPLY) {
            SendSyxFrame(syx_reply[0], &syx_reply[1], syx_reply_len - 1);
        }
        if (pending & PEND_IDENTITY) {
            // Universal Device Inquiry reply: mfr 7D, family "hOC", fw version
            const uint8_t reply[] = {
                0xF0, 0x7E, 0x7F, 0x06, 0x02,
                0x7D, 'h', 'O', 'C', SYX_PROTO_VERSION,
                2, 0, 1, 0, 0xF7,
            };
            usbMIDI.sendSysEx(sizeof(reply), reply, true);
        }
        if (pending & PEND_INFO) {
            const uint8_t info[] = {
                2, // schema: 2 adds n_dac_out and class 4 (DAC)
                2, 0, 1, // firmware version
                MIDIMAP_MAX,
                HS::MIDIFrame::kCVOutPorts, HS::MIDIFrame::kTrigOutPorts,
                kNumSetups, uint8_t(active_setup),
                uint8_t(LiveConfigDirty() ? 1 : 0),
                // How many CV outputs this board actually has. Everything
                // else in this payload counts something else -- in_maps is
                // 32, and kCVOutPorts/kTrigOutPorts are the jacks you patch
                // INTO -- so a host had no way to ask this and had to assert
                // it instead. Four on an hOC, eight on the Xenomorpher.
                // Appended last: older hosts zip field names against the
                // payload and simply stop early.
                DAC_CHANNEL_COUNT,
            };
            SendSyxFrame(SYX_INFO_R, info, sizeof(info));
        }
        if (pending & PEND_DUMP) {
            SendDump(syx_dump_setup);
        }
        if (pending & PEND_SAVE) {
            PersistNow();
            const uint8_t echo = SYX_SAVE;
            SendSyxFrame(SYX_ACK, &echo, 1);
        }
        if (pending & PEND_LOAD) {
            // revert to the stored blob, discarding live edits
            active_setup = syx_load_target;
            UnpackSetup(active_setup);
            Reset();
            const uint8_t echo[2] = {SYX_LOAD, syx_load_target};
            SendSyxFrame(SYX_ACK, echo, 2);
        }
        if (pending & PEND_FACTORY) {
            hMIDI.Init();
            for (int s = 0; s < kNumSetups; ++s) PackSetup(s);
            active_setup = 0;
            Reset();
            const uint8_t echo = SYX_FACTORY;
            SendSyxFrame(SYX_ACK, &echo, 1);
        }
    }

    // ------------------------------------------------------------------
    // SysEx protocol v1 — see docs/hoc-midi-sysex.md
    // Frame: F0 7D 62 4D <ver> <cmd> <body...> F7, all bytes 7-bit, <=60 total
    // ------------------------------------------------------------------
    static constexpr uint8_t SYX_PROTO_VERSION = 1;
    enum SyxCmd : uint8_t {
      SYX_INFO = 0x01, SYX_GET = 0x02, SYX_SET = 0x03, SYX_GET_DUMP = 0x04,
      SYX_SAVE = 0x06, SYX_LOAD = 0x07, SYX_FACTORY = 0x08, SYX_PANIC = 0x09,
      SYX_SELECT = 0x0A,
      SYX_ACK = 0x40, SYX_INFO_R = 0x41, SYX_GET_R = 0x42,
      SYX_DUMP_DATA = 0x44, SYX_DUMP_END = 0x45,
      SYX_NAK = 0x7E,
    };
    enum SyxErr : uint8_t {
      SYXERR_VERSION = 1, SYXERR_CMD = 2, SYXERR_ADDR = 3,
      SYXERR_VALUE = 4, SYXERR_BUSY = 5, SYXERR_CHECKSUM = 6,
    };
    // CLS_DAC is the physical CV output (A..H), as against CLS_IN which is
    // the MIDI map that feeds it. It exists because output voltage scaling
    // lived only in the I/O settings menu: a host could bind a pitch to
    // output A over SysEx but could not tell the module that A drives a
    // Buchla at 1.2V/oct, so every interval played 20% flat until somebody
    // walked over and set it by hand. Nothing about that was a hardware
    // limit -- the per-channel setting already existed and IOFrameToChannel
    // already consults it every tick; the wire simply had no word for it.
    enum SyxClass : uint8_t { CLS_GLOBAL = 0, CLS_IN = 1, CLS_CV = 2,
                              CLS_TR = 3, CLS_DAC = 4 };
    static constexpr uint8_t kGlobalParamCount = 7;
    static constexpr uint8_t kInParamCount = 7;
    static constexpr uint8_t kOutParamCount = 9;
    static constexpr uint8_t kDacParamCount = 2;   // scaling, autotune

    // parameter access; value encoding matches the spec (transpose stored +64,
    // in-map subtype 127 = auto-learn)
    bool get_param(uint8_t cls, uint8_t idx, uint8_t par, uint8_t &value) {
        auto &M = frame.MIDIState;
        switch (cls) {
          case CLS_GLOBAL:
            if (idx != 0) return false;
            switch (par) {
              case 0: value = 2; return true; // schema (2 = has class 4 / n_dac_out)
              case 1: value = active_setup; return true;
              case 2: value = kNumSetups; return true;
              case 3: value = M.bend_range; return true;
              case 4: value = M.poly_mode; return true;
              case 5: value = M.pc_channel; return true;
              case 6: value = HS::trig_length; return true;
              default: return false;
            }
          case CLS_IN: {
            if (idx >= MIDIMAP_MAX || par >= kInParamCount) return false;
            uint64_t d = M.mapping[idx].Pack();
            const uint8_t b[7] = {
              uint8_t(d & 0xff),         // function_cc
              uint8_t((d >> 8) & 0xff),  // function (type in upper 3 bits)
              uint8_t((d >> 16) & 0xff), // channel
              uint8_t((d >> 24) & 0xff), // dac_polyvoice
              uint8_t((d >> 32) & 0xff), // transpose (raw int8)
              uint8_t((d >> 40) & 0xff), // range_low
              uint8_t((d >> 48) & 0xff), // range_high
            };
            switch (par) {
              case 0: value = b[1] >> 5; return true;                 // type
              case 1: value = (b[0] == 0xff) ? 127 : (b[0] & 0x7f); return true; // subtype/CC (127=learn)
              case 2: value = b[2]; return true;                      // channel
              case 3: value = uint8_t(int8_t(b[4]) + 64); return true; // transpose+64
              case 4: value = b[5]; return true;
              case 5: value = b[6]; return true;
              case 6: value = b[3]; return true;                      // polyvoice
            }
            return false;
          }
          case CLS_CV:
          case CLS_TR: {
            const int p = (cls == CLS_TR) ? HS::MIDIFrame::kCVOutPorts + idx : idx;
            const int limit = (cls == CLS_TR) ? HS::MIDIFrame::kTrigOutPorts
                                              : HS::MIDIFrame::kCVOutPorts;
            if (idx >= limit || par >= kOutParamCount) return false;
            const HS::MIDIOutPort &o = M.outports[p];
            switch (par) {
              case 0: value = o.function; return true;
              case 1: value = o.channel; return true;
              case 2: value = o.data1; return true;
              case 3: value = o.data2; return true;
              case 4: value = uint8_t(o.transpose + 64); return true;
              case 5: value = o.range_low; return true;
              case 6: value = o.range_high; return true;
              case 7: value = o.flags; return true;
              case 8: value = o.clkdiv; return true;
            }
            return false;
          }
          case CLS_DAC: {
            if (idx >= DAC_CHANNEL_COUNT || par >= kDacParamCount) return false;
            const OC::IOSettings &io = io_settings();
            switch (par) {
              case 0: value = uint8_t(io.get_output_scaling(idx)); return true;
              case 1: value = io.autotune_data_enabled(idx) ? 1 : 0; return true;
            }
            return false;
          }
          default: return false;
        }
    }

    // returns 0 on success, else a SyxErr
    uint8_t set_param(uint8_t cls, uint8_t idx, uint8_t par, uint8_t value) {
        auto &M = frame.MIDIState;
        switch (cls) {
          case CLS_GLOBAL:
            if (idx != 0) return SYXERR_ADDR;
            switch (par) {
              case 1:
                if (value >= kNumSetups) return SYXERR_VALUE;
                SelectSetup(value);
                return 0;
              case 3:
                if (value < 1 || value > 24) return SYXERR_VALUE;
                M.bend_range = value; return 0;
              case 4:
                if (value > 2) return SYXERR_VALUE;
                M.poly_mode = value;
                M.UpdateMaxPolyphony();
                return 0;
              case 5:
                if (value > 16) return SYXERR_VALUE;
                M.pc_channel = value; return 0;
              case 6:
                if (value < 1) return SYXERR_VALUE;
                HS::trig_length = value; return 0;
              case 0: case 2: return SYXERR_VALUE; // read-only
              default: return SYXERR_ADDR;
            }
          case CLS_IN: {
            if (idx >= MIDIMAP_MAX || par >= kInParamCount) return SYXERR_ADDR;
            uint64_t d = M.mapping[idx].Pack();
            auto setbyte = [&d](int n, uint8_t v) {
              d = (d & ~(uint64_t(0xff) << (n * 8))) | (uint64_t(v) << (n * 8));
            };
            switch (par) {
              case 0:
                if (value > 5) return SYXERR_VALUE;
                setbyte(1, value << 5);
                break;
              case 1: setbyte(0, (value == 127) ? 0xff : value); break;
              case 2:
                if (value > 16) return SYXERR_VALUE;
                setbyte(2, value); break;
              case 3:
                if (value < 16 || value > 112) return SYXERR_VALUE;
                setbyte(4, uint8_t(int8_t(value) - 64)); break;
              case 4: setbyte(5, value & 0x7f); break;
              case 5: setbyte(6, value & 0x7f); break;
              case 6:
                if (value >= DAC_CHANNEL_COUNT) return SYXERR_VALUE;
                setbyte(3, value); break;
            }
            M.mapping[idx].Unpack(d);
            M.UpdateMidiChannelFilter();
            M.UpdateMaxPolyphony();
            return 0;
          }
          case CLS_CV:
          case CLS_TR: {
            const int p = (cls == CLS_TR) ? HS::MIDIFrame::kCVOutPorts + idx : idx;
            const int limit = (cls == CLS_TR) ? HS::MIDIFrame::kTrigOutPorts
                                              : HS::MIDIFrame::kCVOutPorts;
            const int fn_count = (cls == CLS_TR) ? HS::TRFN_COUNT : HS::CVFN_COUNT;
            if (idx >= limit || par >= kOutParamCount) return SYXERR_ADDR;
            HS::MIDIOutPort &o = M.outports[p];
            switch (par) {
              case 0:
                if (value >= fn_count) return SYXERR_VALUE;
                o.function = value;
                o.ResetRuntime();
                break;
              case 1:
                if (value > 15) return SYXERR_VALUE;
                o.channel = value; break;
              case 2: o.data1 = value & 0x7f; break;
              case 3: o.data2 = value & 0x7f; break;
              case 4:
                if (value < 16 || value > 112) return SYXERR_VALUE;
                o.transpose = int8_t(value) - 64; break;
              case 5: o.range_low = value & 0x7f; break;
              case 6: o.range_high = value & 0x7f; break;
              case 7: o.flags = value; break;
              case 8:
                if (value > 15) return SYXERR_VALUE;
                o.clkdiv = value; break;
            }
            o.Sanitize();
            return 0;
          }
          case CLS_DAC: {
            if (idx >= DAC_CHANNEL_COUNT || par >= kDacParamCount)
              return SYXERR_ADDR;
            OC::IOSettings &io = mutable_io_settings();
            switch (par) {
              case 0:
                // Scaling applies BEFORE the clamp in PitchToScaledDAC, so
                // 1.2x on an output carrying a 0..10V controller pins every
                // value over ~106 to full scale. That is the caller's
                // business, not ours: a module cannot know whether output E
                // is a note or a filter sweep. Range-check and obey.
                if (value >= OC::VOLTAGE_SCALING_LAST) return SYXERR_VALUE;
                io.apply_value(
                    OC::IOSettings::channel_setting(OC::IO_SETTING_A_SCALING, idx),
                    value);
                break;
              case 1:
                if (value > 1) return SYXERR_VALUE;
                io.apply_value(
                    OC::IOSettings::channel_setting(OC::IO_SETTING_A_TUNING, idx),
                    value);
                break;
            }
            return 0;
          }
          default: return SYXERR_ADDR;
        }
    }

    // pending SysEx work: flags set in ISR context, drained in DoLoop()
    enum SyxPending : uint8_t {
      PEND_REPLY = 1, PEND_IDENTITY = 2, PEND_INFO = 4,
      PEND_DUMP = 8, PEND_SAVE = 16, PEND_LOAD = 32, PEND_FACTORY = 64,
    };
    volatile uint8_t syx_pending = 0;
    uint8_t syx_reply[8]; // cmd + payload for ACK/NAK/GET_R
    uint8_t syx_reply_len = 0;
    uint8_t syx_dump_setup = 0;
    uint8_t syx_load_target = 0;
    // incoming dump bookkeeping
    uint8_t syx_rx_xor = 0;
    uint8_t syx_rx_packets = 0;

    void QueueReply(uint8_t cmd, std::initializer_list<uint8_t> payload) {
        syx_reply[0] = cmd;
        uint8_t n = 1;
        for (uint8_t b : payload) {
            if (n >= sizeof(syx_reply)) break;
            syx_reply[n++] = b;
        }
        syx_reply_len = n;
        syx_pending |= PEND_REPLY;
    }
    void QueueAck(std::initializer_list<uint8_t> echo) { QueueReply(SYX_ACK, echo); }
    void QueueNak(uint8_t cmd, uint8_t err) { QueueReply(SYX_NAK, {cmd, err}); }

    /* Suspend hook / manual [DUMP]: send the active setup over SysEx */
    void OnSendSysEx() {
        syx_dump_setup = active_setup;
        syx_pending |= PEND_DUMP;
    }

    // ISR context: stage the frame; DoLoop() parses it.
    void OnReceiveSysEx() {
        if (syx_rx_len) return;  // one frame in flight; next one re-triggers
        const uint8_t *sx = usbMIDI.getSysExArray();
        unsigned len = usbMIDI.getSysExArrayLength();
        if (len > sizeof(syx_rx_buf)) len = sizeof(syx_rx_buf);
        memcpy((void *)syx_rx_buf, sx, len);
        syx_rx_len = len;
    }
    volatile uint16_t syx_rx_len = 0;
    uint8_t syx_rx_buf[96];

    void HandleSysEx(const uint8_t *sx, unsigned len) {
        // Universal Device Inquiry: F0 7E <dev> 06 01 F7
        if (len >= 6 && sx[1] == 0x7E && sx[3] == 0x06 && sx[4] == 0x01) {
            syx_pending |= PEND_IDENTITY;
            return;
        }
        if (len < 7 || sx[1] != 0x7D || sx[2] != 0x62) return; // not ours
        if (sx[3] != 'M') { // another app's dump passing by; just log it
            UpdateLog(1, 0, 5, 0, sx[3], 0);
            return;
        }
        const uint8_t ver = sx[4];
        const uint8_t cmd = sx[5];
        const uint8_t *body = sx + 6;
        const unsigned blen = (len >= 7) ? len - 7 : 0; // strip F0..cmd and F7
        if (ver != SYX_PROTO_VERSION) { QueueNak(cmd, SYXERR_VERSION); return; }

        UpdateLog(1, 0, 5, 0, 'M', cmd);

        switch (cmd) {
          case SYX_INFO: syx_pending |= PEND_INFO; break;

          case SYX_GET: {
            uint8_t value = 0;
            if (blen >= 3 && get_param(body[0], body[1], body[2], value))
                QueueReply(SYX_GET_R, {body[0], body[1], body[2], value});
            else QueueNak(cmd, SYXERR_ADDR);
            break;
          }
          case SYX_SET: {
            if (blen < 4) { QueueNak(cmd, SYXERR_ADDR); break; }
            const uint8_t err = set_param(body[0], body[1], body[2], body[3]);
            if (err) QueueNak(cmd, err);
            else QueueAck({body[0], body[1], body[2], body[3]});
            break;
          }
          case SYX_GET_DUMP: {
            uint8_t s = (blen >= 1) ? body[0] : 0x7f;
            if (s == 0x7f) s = active_setup;
            if (s >= kNumSetups) { QueueNak(cmd, SYXERR_VALUE); break; }
            syx_dump_setup = s;
            syx_pending |= PEND_DUMP;
            break;
          }
          case SYX_DUMP_DATA: { // host -> device restore, applied live
            if (blen < 3) { QueueNak(cmd, SYXERR_ADDR); break; }
            const uint8_t seq = body[1];
            if (seq == 0) { syx_rx_xor = 0; syx_rx_packets = 0; }
            unsigned i = 3; // past setup#/seq/total
            uint8_t err = 0;
            while (i + 1 < blen) {
                const uint8_t cls = body[i], idx = body[i + 1];
                const uint8_t np = (cls == CLS_GLOBAL) ? kGlobalParamCount
                                 : (cls == CLS_IN) ? kInParamCount : kOutParamCount;
                if (i + 2 + np > blen) { err = SYXERR_ADDR; break; }
                for (uint8_t par = 0; par < np; ++par) {
                    const uint8_t v = body[i + 2 + par];
                    // skip read-only globals; tolerate per-param rejects
                    if (cls == CLS_GLOBAL && (par == 0 || par == 1 || par == 2)) continue;
                    set_param(cls, idx, par, v);
                }
                for (unsigned b = i; b < i + 2 + np; ++b) syx_rx_xor ^= body[b];
                i += 2 + np;
            }
            ++syx_rx_packets;
            if (err) QueueNak(cmd, err);
            else QueueAck({seq});
            break;
          }
          case SYX_DUMP_END: {
            if (blen < 3) { QueueNak(cmd, SYXERR_ADDR); break; }
            const bool ok = (body[1] == syx_rx_packets) && (body[2] == (syx_rx_xor & 0x7f));
            syx_rx_xor = 0;
            syx_rx_packets = 0;
            if (ok) QueueAck({body[0]});
            else QueueNak(cmd, SYXERR_CHECKSUM);
            break;
          }
          case SYX_SAVE: syx_pending |= PEND_SAVE; break;
          case SYX_LOAD:
            if (blen < 1 || body[0] >= kNumSetups) { QueueNak(cmd, SYXERR_VALUE); break; }
            syx_load_target = body[0];
            syx_pending |= PEND_LOAD;
            break;
          case SYX_FACTORY:
            if (blen >= 2 && body[0] == 0x21 && body[1] == 0x42)
                syx_pending |= PEND_FACTORY;
            else QueueNak(cmd, SYXERR_VALUE);
            break;
          case SYX_PANIC:
            frame.MIDIState.panic_request = true;
            QueueAck({});
            break;
          case SYX_SELECT:
            if (blen < 1 || body[0] >= kNumSetups) { QueueNak(cmd, SYXERR_VALUE); break; }
            SelectSetup(body[0]);
            QueueAck({body[0]});
            break;
          default:
            QueueNak(cmd, SYXERR_CMD);
            break;
        }
    }

   void ToggleCopyMode() {
       copy_mode = 1 - copy_mode;
       copy_setup_source = get_setup_number();
       copy_setup_target = copy_setup_source + 1;
       if (copy_setup_target > 3) copy_setup_target = 0;
   }

   void ToggleCursor() {
       if (copy_mode) CopySetup(copy_setup_target, copy_setup_source);
       else if (detail_port < 0) EnterDetail(cursor.cursor_pos());
       else param_cursor.toggle_editing();
   }

   /* Perform a copy or sysex dump */
   void CopySetup(int target, int source) {
       if (source == target) {
           OnSendSysEx();
       } else {
           PackSetup(active_setup); // capture live edits first
           setups[target] = setups[source];
           if (target == active_setup) UnpackSetup(active_setup);
           else SelectSetup(target);
       }
       copy_mode = 0;
   }

private:
    // Housekeeping
    int8_t detail_port; // -1 = overview list, else the open port row
    bool display; // 0=Setup Edit 1=Log
    bool copy_mode; // Copy mode on/off
    int active_setup; // index of current setup
    int copy_setup_source; // Which setup is being copied?
    int copy_setup_target; // Which setup is being copied to?

    CaptainMIDILog log[MIDI_LOG_MAX_SIZE];
    int log_index; // Index of log for writing
    int log_view; // Current index for viewing

    // MIDI In
    // TODO: replace with data from MIDIFrame
    int note_in[DAC_CHANNEL_COUNT]; // track active note per DAC channel
    uint16_t indicator_in[DAC_CHANNEL_COUNT]; // A MIDI indicator will display next to MIDI In assignment

    // MIDI Out state lives in frame.MIDIState.outports[] (engine-owned)

    // stored setups: packed in-maps + out-ports (see PackSetup/UnpackSetup)
    struct SetupBlob {
        uint64_t inmaps[MIDIMAP_MAX];
        uint64_t outports[HS::MIDIFrame::kOutPorts];
    };
    SetupBlob setups[kNumSetups];

    // 8-char port names for the overview rows
    const char* RowLabel(int pos) const {
        if (pos < DAC_CHANNEL_COUNT) return midi2cv_label[pos];
        const int p = pos - DAC_CHANNEL_COUNT;
        if (p < HS::MIDIFrame::kCVOutPorts) return cv2midi_label[p];
        return trig2midi_label[p - HS::MIDIFrame::kCVOutPorts];
    }

    // the function currently assigned to a row, as a safe display string
    const char* RowFunctionName(int pos) const {
        if (pos < DAC_CHANNEL_COUNT)
            return frame.MIDIState.mapping[pos].get_label();
        const int p = pos - DAC_CHANNEL_COUNT;
        const HS::MIDIOutPort &o = frame.MIDIState.outports[p];
        if (p >= HS::MIDIFrame::kCVOutPorts)
            return trig_out_fn_names[o.function < HS::TRFN_COUNT ? o.function : 0];
        return cv_out_fn_names[o.function < HS::CVFN_COUNT ? o.function : 0];
    }

    // live physical-input state, left of the activity area:
    // CV rows get a level bar, trigger rows a gate lamp, MIDI-in rows
    // a bar for the CV they are currently outputting
    void DrawRowInputState(int pos, int y) const {
        const int fullscale = HSAPPLICATION_5V * 2; // 10V unipolar
        if (pos < DAC_CHANNEL_COUNT) {
            const int out = frame.MIDIState.mapping[pos].ViewOut();
            const int h = constrain(out * 8 / fullscale, 0, 8);
            if (h > 0) graphics.drawRect(50, y + 9 - h, 3, h);
            return;
        }
        const int p = pos - DAC_CHANNEL_COUNT;
        if (p < HS::MIDIFrame::kCVOutPorts) {
            const int h = constrain(HS::cvmap[p].In() * 8 / fullscale, 0, 8);
            if (h > 0) graphics.drawRect(50, y + 9 - h, 3, h);
        } else {
            const int k = p - HS::MIDIFrame::kCVOutPorts;
            if (HS::trigmap[k].Gate()) graphics.drawRect(49, y + 2, 5, 6);
            else graphics.drawFrame(49, y + 3, 5, 5);
        }
    }

    // draw MIDI activity for a row: icon, or the sounding note name
    void DrawRowActivity(int pos, int y) const {
        if (pos < DAC_CHANNEL_COUNT) {
            if (indicator_in[pos] > 0 || note_in[pos] > -1)
                graphics.drawBitmap8(70, y + 2, 8, MIDI_ICON);
            return;
        }
        const HS::MIDIOutPort &o = frame.MIDIState.outports[pos - DAC_CHANNEL_COUNT];
        if (o.gated && o.last_note <= 127) {
            graphics.setPrintPos(70, y + 2);
            graphics.print(midi_note_numbers[o.last_note]);
        } else if (o.indicator > 0) {
            graphics.drawBitmap8(70, y + 2, 8, MIDI_ICON);
        }
    }

    // header badge: inverted USB = host connected, struck through = not
    void DrawUSBStatus(int x) const {
        graphics.setPrintPos(x, 1);
        graphics.print("USB");
        if (usb_configuration) {
            graphics.invertRect(x - 2, 0, 21, 9);
        } else {
            graphics.drawLine(x - 2, 9, x + 18, 0);
        }
    }

    void DrawOverview() const {
        gfxHeader("Captain");
        DrawUSBStatus(48);
        gfxPrint(128 - 48, 1, "Setup ");
        gfxPrint(get_setup_number() + 1);
        if (LiveConfigDirty()) gfxPrint("*"); // unsaved changes

        OC::menu::SettingsList<OC::menu::kScreenLines, 0, 76> settings_list(cursor);
        OC::menu::SettingsListItem list_item;
        while (settings_list.available()) {
            const int current = settings_list.Next(list_item);
            if (current >= kNumRows) { list_item.DrawCustom(); continue; }

            DrawRowInputState(current, list_item.y);
            DrawRowActivity(current, list_item.y);
            list_item.SetPrintPos();
            graphics.print(RowLabel(current));
            // right column: assigned function; press to open the editor
            graphics.setPrintPos(0, list_item.y + 2); // reset for DrawDefault path
            list_item.DrawDefault(RowFunctionName(current), 0, CaptainSettings[0]);
        }
    }

    void DrawDetail() const {
        const int row = detail_port;
        gfxHeader(RowLabel(row));
        DrawUSBStatus(52);
        gfxPrint(128 - 48, 1, "Setup ");
        gfxPrint(get_setup_number() + 1);
        if (LiveConfigDirty()) gfxPrint("*");

        OC::menu::SettingsList<OC::menu::kScreenLines, 0, 76> settings_list(param_cursor);
        OC::menu::SettingsListItem list_item;
        const int n_params = DetailParamCount(row);
        while (settings_list.available()) {
            const int current = settings_list.Next(list_item);
            if (current >= n_params) { list_item.DrawCustom(); continue; }

            list_item.SetPrintPos();
            graphics.print(DetailParamLabel(row, current));
            const int value = DetailParamValue(row, current);
            const auto &attr = CaptainSettings[DetailParamAttr(row, current)];
            if (row < DAC_CHANNEL_COUNT && current == 0) {
                // in-map Assign: composite value; print its label string
                list_item.DrawDefault(frame.MIDIState.mapping[row].get_label(), value, attr);
            } else {
                list_item.DrawDefault(value, attr);
            }
        }
    }

    void DrawLogScreen() {
        gfxHeader("IO Ch Type  Values");
        if (log_index) {
            for (int l = 0; l < 6; l++)
            {
                int ix = l + log_view; // Log index
                if (ix < log_index) {
                    log[ix].DrawAt(l * 8 + 15);
                }
            }

            // Draw scroll
            if (log_index > 6) {
                graphics.drawFrame(122, 14, 6, 48);
                int y = Proportion(log_view, log_index - 6, 38);
                y = constrain(y, 0, 38);
                graphics.drawRect(124, 16 + y, 2, 6);
            }
        }
    }

    void DrawCopyScreen() {
        gfxHeader("Copy");

        graphics.setPrintPos(8, 28);
        graphics.print("Setup ");
        graphics.print(copy_setup_source + 1);
        graphics.print(" -");
        graphics.setPrintPos(58, 28);
        graphics.print("> ");
        if (copy_setup_source == copy_setup_target) graphics.print("SysEx");
        else {
            graphics.print("Setup ");
            graphics.print(copy_setup_target + 1);
        }

        graphics.setPrintPos(0, 55);
        graphics.print("[CANCEL]");

        graphics.setPrintPos(90, 55);
        graphics.print(copy_setup_source == copy_setup_target ? "[DUMP]" : "[COPY]");
    }

    int get_setup_number() const {
        return active_setup;
    }

    // CV/trigger -> MIDI conversion lives in HS::MIDIFrame::ProcessOutputs
    // (HSIOFrame.cpp); the old per-app implementation is gone.
    // read one message from the device; returns whether one was processed
    template <typename T1>
    bool poll_midi(T1 &device) {
        if (!device.read()) return false;

        uint8_t message = device.getType();
        uint8_t channel = device.getChannel();
        uint8_t data1 = device.getData1();
        uint8_t data2 = device.getData2();

        // Handle system exclusive dump for Setup data.
        // Copy only — parsing runs from Loop() so the whole SysEx stack
        // stays out of the ISR (and out of ITCM on Teensy 4).
        if (message == HEM_MIDI_SYSEX) OnReceiveSysEx();

        HS::frame.MIDIState.ProcessMIDIMsg({channel, message, data1, data2});
        return true;
    }

    uint8_t get_in_assign(int ch) {
      return frame.MIDIState.get_in_assign(ch);
    }

    uint8_t get_in_channel(int ch) {
      return frame.MIDIState.get_in_channel(ch);
    }

    int8_t get_in_transpose(int ch) {
      return frame.MIDIState.get_in_transpose(ch);
    }

    bool in_in_range(int ch, int note) {
      return frame.MIDIState.in_in_range(ch, note);
    }

    void UpdateLog(bool midi_in, int ch, uint8_t message, uint8_t channel, int16_t data1, int16_t data2) {
        // Don't log SysEx unless the user is on the log display screen
        if (message == 5 && display == 0) return;

        char io = midi_in ? ('A' + ch) : ('1' + ch);
        log[log_index++] = {midi_in, io, message, channel, data1, data2};
        if (log_index == MIDI_LOG_MAX_SIZE) {
            for (int i = 0; i < MIDI_LOG_MAX_SIZE - 1; i++)
            {
                memcpy(&log[i], &log[i+1], sizeof(log[i + 1]));
            }
            log_index--;
        }
        log_view = log_index - 6;
        if (log_view < 0) log_view = 0;
    }

};

////////////////////////////////////////////////////////////////////////////////
//// App Functions
////////////////////////////////////////////////////////////////////////////////
FLASHMEM void AppCaptainMIDI::Init() { BaseStart(); }

FLASHMEM size_t AppCaptainMIDI::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
#ifdef __IMXRT1062__
  return 0; // T4.x persists via PhzConfig (see StoreData)
#else
  // NOTE: a save captures the live state only if Suspend()/PackSetup ran;
  // the framework saves from the settings menu, which suspends the app first.
  stream_buffer.Write(uint8_t(1)); // schema version
  stream_buffer.Write(uint8_t(active_setup));
  stream_buffer.Write(uint16_t(0)); // reserved
  for (int s = 0; s < kNumSetups; ++s) {
    for (int i = 0; i < MIDIMAP_MAX; ++i)
      stream_buffer.Write(setups[s].inmaps[i]);
    for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
      stream_buffer.Write(setups[s].outports[i]);
  }
  return stream_buffer.written();
#endif
}
FLASHMEM size_t AppCaptainMIDI::RestoreAppData(util::StreamBufferReader &stream_buffer) {
#ifdef __IMXRT1062__
  return 0;
#else
  const uint8_t version = stream_buffer.Read<uint8_t>();
  if (version != 1) return stream_buffer.read();
  // NB: constrain() is a macro that re-evaluates its argument — never
  // feed it a stream read directly (it desyncs the whole stream)
  const uint8_t stored_setup = stream_buffer.Read<uint8_t>();
  active_setup = constrain(stored_setup, 0, kNumSetups - 1);
  stream_buffer.Read<uint16_t>(); // reserved
  for (int s = 0; s < kNumSetups; ++s) {
    for (int i = 0; i < MIDIMAP_MAX; ++i)
      setups[s].inmaps[i] = stream_buffer.Read<uint64_t>();
    for (int i = 0; i < HS::MIDIFrame::kOutPorts; ++i)
      setups[s].outports[i] = stream_buffer.Read<uint64_t>();
  }
  UnpackSetup(active_setup);
  return stream_buffer.read();
#endif
}

void AppCaptainMIDI::Process(OC::IOFrame *ioframe) {
  BaseController(ioframe);
}

void AppCaptainMIDI::GetIOConfig(OC::IOConfig &ioconfig) const
{
  using namespace OC;
  ioconfig.digital_inputs[DIGITAL_INPUT_1].set("Gate1");
  ioconfig.digital_inputs[DIGITAL_INPUT_2].set("Gate2");
  ioconfig.digital_inputs[DIGITAL_INPUT_3].set("Gate3");
  ioconfig.digital_inputs[DIGITAL_INPUT_4].set("Gate4");

  ioconfig.cv[0].set("Ch1 CV");
  ioconfig.cv[1].set("Ch2 CV");
  ioconfig.cv[2].set("Ch3 CV");
  ioconfig.cv[3].set("Ch4 CV");
  ioconfig.cv[4].set("Ch5 CV");
  ioconfig.cv[5].set("Ch6 CV");
  ioconfig.cv[6].set("Ch7 CV");
  ioconfig.cv[7].set("Ch8 CV");

  ioconfig.outputs[0].set("Ch1", OUTPUT_MODE_PITCH);
  ioconfig.outputs[1].set("Ch2", OUTPUT_MODE_PITCH);
  ioconfig.outputs[2].set("Ch3", OUTPUT_MODE_PITCH);
  ioconfig.outputs[3].set("Ch4", OUTPUT_MODE_PITCH);
  ioconfig.outputs[4].set("Ch5", OUTPUT_MODE_PITCH);
  ioconfig.outputs[5].set("Ch6", OUTPUT_MODE_PITCH);
  ioconfig.outputs[6].set("Ch7", OUTPUT_MODE_PITCH);
  ioconfig.outputs[7].set("Ch8", OUTPUT_MODE_PITCH);
}

FLASHMEM
void AppCaptainMIDI::HandleAppEvent(OC::AppEvent event) {
  if (event == OC::APP_EVENT_SUSPEND) {
    Suspend();
  }
  if (event == OC::APP_EVENT_RESUME) {
    Resume();
  }
  if (event == OC::APP_EVENT_FLUSH) {
    PackSetup(active_setup);
#ifdef __IMXRT1062__
    StoreData();  // preset-bus capture: persist CAPTAIN.DAT
#endif
  }
}

FLASHMEM void AppCaptainMIDI::Loop() { DoLoop(); } // deferred (non-ISR) work

FLASHMEM
void AppCaptainMIDI::DrawMenu() const { BaseView(); }

FLASHMEM
void AppCaptainMIDI::View() const { MainView(); }

FLASHMEM void AppCaptainMIDI::DrawScreensaver() const {
    BaseScreensaver(true);
}
void AppCaptainMIDI::DrawDebugInfo() const { }

FLASHMEM void AppCaptainMIDI::HandleButtonEvent(const UI::Event &event) {
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS)
        ToggleCursor();
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) Panic();
        if (event.type == UI::EVENT_BUTTON_PRESS) ToggleDisplay();
    }

    if (event.control == OC::CONTROL_BUTTON_A && event.type == UI::EVENT_BUTTON_PRESS)
        SwitchSetup(-1);
    if (event.control == OC::CONTROL_BUTTON_B) {
        if (event.type == UI::EVENT_BUTTON_PRESS) SwitchSetup(1);
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) ToggleCopyMode();
    }
}

FLASHMEM void AppCaptainMIDI::HandleEncoderEvent(const UI::Event &event) {
    if (event.control == OC::CONTROL_ENCODER_R) {
        if (detail_port < 0) {
            cursor.Scroll(event.value); // overview: pick a port
        } else if (param_cursor.editing()) {
            EditDetailParam(detail_port, param_cursor.cursor_pos(), event.value);
        } else {
            param_cursor.Scroll(event.value); // detail: pick a parameter
        }
    }
    if (event.control == OC::CONTROL_ENCODER_L) {
        LeftEncoderTurn(event.value);
    }
}
