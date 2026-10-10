#include "HSClockManager.h"
#include "HSMIDI.h"
#include "HSUtils.h"
#include "HSIOFrame.h"

const int HS::MIDIMapping::ViewOut() const {
  if (IsPitch()) return output + (transpose * 128) + Proportion(pitch_bend, 8192, frame.MIDIState.bend_range << 7);
  return output;
}

// arguments are raw data from MIDI system, so channel starts at 1 (not 0)
void HS::MIDIFrame::ProcessMIDIMsg(const MIDIMessage msg) {
    const uint8_t m_ch = msg.channel - 1;
    last_midi_channel = m_ch;

    switch (msg.message) { // System Real Time messages
        case usbMIDI.Clock:
            clock_q = (clock_count % (24/MIDI_CLOCK_PPQN) == 0); // for internal sync @ 2ppqn
            ++clock_count;
            for (MIDIMapping& map : mapping) {
              map.ProcessClock(clock_count);
            }
            if (clock_count == 24) clock_count = 0;
            return;
            break;

        case usbMIDI.Start:
        case usbMIDI.Continue: // treat Continue like Start
            start_q = 1;
            clock_count = 0;
            clock_run = true;

            for (MIDIMapping& map : mapping) {
              if (map.IsClock() && map.get_subtype() == MIDIMapSettings::TRIG_START) {
                map.ClockOut();
              }
            }

            // UpdateLog(message, data1, data2);
            return;
            break;

        case usbMIDI.Stop:
        case usbMIDI.SystemReset:
            stop_q = 1;
            clock_run = false;
            // a way to reset stuck notes
            ClearMonoBuffer();
            ClearSustainLatch();
            ClearPolyBuffer();
            for (MIDIMapping& map : mapping) {
                map.output = 0;
                map.pitch_bend = 0;
                map.trigout_countdown = 0;
            }
            return;
            break;
    }

    if (!CheckMidiChannelFilter(m_ch)) return;

    switch (msg.message) { // Channel Voice messages
        case usbMIDI.NoteOn:
            MonoBufferPush(m_ch, msg.data1, msg.data2);
            PolyBufferPush(m_ch, msg.data1, msg.data2);
            break;

        case usbMIDI.NoteOff:
            MonoBufferPop(m_ch, msg.data1);
            PolyBufferPop(m_ch, msg.data1);
            break;
    }

    bool log_this = false;
    for (MIDIMapping& map : mapping) {
      if (map.ProcessMsg(msg, *this))
        log_this = true;
    }
    if (log_this) UpdateLog(msg);
}

bool HS::MIDIMapping::ProcessMsg(const MIDIMessage msg, HS::MIDIFrame &state) {
  const uint8_t m_ch = msg.channel - 1;

  // Auto-Learn
  if (learning()) {
    switch (msg.message) {
      case usbMIDI.NoteOn:
        if (!enabled()) {
          function = PITCH;
          range_low = 127;
          range_high = 0;
        }
        if (enabled() && IsPitch()) {
          // focused pitch range capture
          // todo: set range based on polyphony, or alternate learn modes
          range_low = min(range_low, msg.data1);
          range_high = max(range_high, msg.data1);
          channel = m_ch;
        }
        break;
      case usbMIDI.ControlChange:
        if (!enabled() || IsCC()) {
          SetCC(msg.data1);
          channel = m_ch;
        }
        break;
      case usbMIDI.PitchBend:
        if (!enabled()) {
          SetModulator(MOD_BEND);
          channel = m_ch;
        }
        break;
      default: break;
    }
  }

  if (!enabled()) return false;
  // skip unwanted MIDI Channels
  if (get_channel() != m_ch && get_channel() != 16)
    return false;

  NoteBuffer &buf = state.note_buffer[m_ch];
  bool log_this = false;

  switch (msg.message) {
    case usbMIDI.NoteOn: {
        if (!InRange(msg.data1)) break;
        semitone_mask = semitone_mask | (1u << (msg.data1 % 12));

        // Should this message go out on this channel?
        switch (get_type()) {
          case PITCH:
            switch (get_subtype()) {
            // note # output functions
            case NOTE_MONO:
                output = MIDIQuantizer::CV(state.GetNoteLast(buf));
                break;

            case NOTE_POLY:
                if (state.CheckPolyVoice(dac_polyvoice)) output = MIDIQuantizer::CV(state.poly_buffer[dac_polyvoice].note);
                break;

            case NOTE_MIN:
                output = MIDIQuantizer::CV(state.GetNoteMin(buf));
                break;

            case NOTE_MAX:
                output = MIDIQuantizer::CV(state.GetNoteMax(buf));
                break;

            case NOTE_PEDAL:
                output = MIDIQuantizer::CV(state.GetNoteFirst(buf));
                break;

            case NOTE_INVERT:
                output = MIDIQuantizer::CV(state.GetNoteLastInv(buf));
                break;
            }
            break;
          case TRIGGER:
            switch (get_subtype()) {
            case TRIG_FIRST:
                if (buf.size() != 1) break;
            case TRIG_NORMAL:
            case TRIG_ALWAYS:
                ClockOut();
                break;
            }
            break;

          case GATE:
            switch (get_subtype()) {
            case GATE_RETRIG:
                if (output > 0) {
                    gate_retrig = true;
                    trigout_countdown = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
                    output = 0;
                    break;
                }
            case GATE_MONO:
                output = PULSE_VOLTAGE * (12 << 7);
                break;
            case GATE_INVERT:
                output = 0;
                break;
            case GATE_POLY:
                if (state.CheckPolyVoice(dac_polyvoice)) output = PULSE_VOLTAGE * (12 << 7);
                break;
            }
            break;

          case MODULATOR:
            switch (get_subtype()) {
            case MOD_VEL_MONO:
                output = (buf.size() > 0) ? Proportion(state.GetVel(buf, 1), 127, HEMISPHERE_MAX_CV) : 0;
                break;
            case MOD_VEL_POLY:
                output = (state.CheckPolyVoice(dac_polyvoice)) ? Proportion(state.poly_buffer[dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;
                break;
            }
            break;
          default: break;
        }

        log_this = 1; // Log all MIDI notes. Other stuff is conditional.
        break;
    }
    case usbMIDI.NoteOff: {
        if (!InRange(msg.data1)) break;
        semitone_mask = semitone_mask & ~(1u << (msg.data1 % 12));

        // this is here instead of up top because of the mask update
        if (learning() && IsPitch() && semitone_mask == 0) {
          SetPitch(NOTE_MONO);
        }

        // don't update output when last note is released
        // or if sustain is engaged
        if (IsPitch() && buf.size() > 0 && !state.CheckSustainLatch(m_ch)) {
            switch(get_subtype()) { // note # output functions
                case NOTE_MONO:
                    output = MIDIQuantizer::CV(state.GetNoteLast(buf));
                    break;

                case NOTE_POLY:
                    if (state.CheckPolyVoice(dac_polyvoice)) output = MIDIQuantizer::CV(state.poly_buffer[dac_polyvoice].note);
                    break;

                case NOTE_MIN:
                    output = MIDIQuantizer::CV(state.GetNoteMin(buf));
                    break;

                case NOTE_MAX:
                    output = MIDIQuantizer::CV(state.GetNoteMax(buf));
                    break;

                case NOTE_PEDAL:
                    output = MIDIQuantizer::CV(state.GetNoteFirst(buf));
                    break;

                case NOTE_INVERT:
                    output = MIDIQuantizer::CV(state.GetNoteLastInv(buf));
                    break;
            }
        }

        if (IsTrigger() && get_subtype() == TRIG_ALWAYS) ClockOut();

        if (IsGate() && !state.CheckSustainLatch(m_ch)) {
          if (buf.size() == 0) { // turn mono gate off, only when all notes are off
            if (get_subtype() == GATE_MONO || get_subtype() == GATE_RETRIG)
              output = 0;
            if (get_subtype() == GATE_INVERT)
              output = PULSE_VOLTAGE * (12 << 7);
          }
          if (get_subtype() == GATE_POLY && !state.CheckPolyVoice(dac_polyvoice))
            output = 0;
        }

        if (IsMod()) {
          if (get_subtype() == MOD_VEL_MONO)
              output = (buf.size() > 0) ? Proportion(state.GetVel(buf, 1), 127, HEMISPHERE_MAX_CV) : 0;
          if (get_subtype() == MOD_VEL_POLY)
              output = (state.CheckPolyVoice(dac_polyvoice)) ? Proportion(state.poly_buffer[dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;
        }

        log_this = 1;
        break;
    }
    case usbMIDI.ControlChange: { // Modulation wheel or other CC
        // handle sustain pedal
        if (msg.data1 == 64) {
          if (msg.data2 > 63) {
            if (!state.CheckSustainLatch(m_ch))
              state.sustain_latch |= (1 << m_ch);
          } else {
            state.ClearSustainLatch(m_ch);
            if (IsGate() && !(buf.size() > 0)) {
              switch (get_subtype()) {
                case GATE_MONO:
                case GATE_POLY:
                case GATE_RETRIG:
                  output = 0;
                  break;
                case GATE_INVERT:
                  output = PULSE_VOLTAGE * (12 << 7);
                  break;
              }
            }
          }
        }

        if (IsCC()) {
          if (get_subtype() == msg.data1) {
            output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
            log_this = 1;
          }
        }
        break;
    }
    case usbMIDI.AfterTouchPoly: {
        if (IsMod() && get_subtype() == MOD_AT_POLY) {
          if (state.FindPolyNoteIndex(msg.data1) == dac_polyvoice)
            output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
          log_this = 1;
        }
        break;
    }
    case usbMIDI.AfterTouchChannel: {
        if (IsMod() && get_subtype() == MOD_AT_CHAN) {
          output = Proportion(msg.data1, 127, HEMISPHERE_MAX_CV);
          log_this = 1;
        }
        break;
    }
    case usbMIDI.PitchBend: {
        if (IsMod()) {
          if (MOD_BEND == get_subtype()) {
            pitch_bend = (msg.data2 << 7) + msg.data1 - 8192;
            output = Proportion(pitch_bend, 8192, HEMISPHERE_3V_CV);
            log_this = 1;
          }
        } else if (IsPitch())
          pitch_bend = (msg.data2 << 7) + msg.data1 - 8192;

        break;
    }
  } // switch
  return log_this;
}

#ifndef NO_HEMISPHERE
void HS::MIDIFrame::Send(const SlewedValue *outvals) {
    // first pass - calculate things and turn off notes
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
        const uint8_t midi_ch = outmap[i].get_channel();

        int input = outvals[i].get();
        gate_high[i] = input > (12 << 7);
        clocked[i] = (gate_high[i] && last_cv[i] < (12 << 7));
        if (abs(input - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
            changed_cv[i] = 1;
            last_cv[i] = input;
        } else changed_cv[i] = 0;

        // outmaps don't discriminate on subtypes, we keep it simple
        switch (outmap[i].get_type()) {
          case MIDIMapSettings::PITCH:
            if (changed_cv[i]) {
              // a note has changed, turn the last one off first
              SendNoteOff(outchan_last[i]);
              current_note[midi_ch] = MIDIQuantizer::NoteNumber(input);
            }
            break;

          case MIDIMapSettings::GATE:
            if (!gate_high[i] && changed_cv[i])
              SendNoteOff(midi_ch);
            break;

          case MIDIMapSettings::CCONTROL: {
            const uint8_t newccval = ProportionCV(abs(input), 127);
            if (newccval != current_ccval[i]) {
              SendCC(midi_ch, outmap[i].get_subtype(), newccval);
              current_ccval[i] = newccval;
            }
            break;
          }

          default: break;
        }

        // Handle clock pulse timing
        if (note_countdown[i] > 0) {
            if (--note_countdown[i] == 0) SendNoteOff(outchan_last[i]);
        }
    }

    // 2nd pass - send eligible notes
    for (int i = 0; i < 2; ++i) {
        const int chA = i*2;
        const int chB = chA + 1;

        if (outmap[chB].IsGate()) {
            if (clocked[chB]) {
                SendNoteOn(outmap[chB].get_channel());
                // no countdown
                outchan_last[chB] = outmap[chB].get_channel();
            }
        } else if (outmap[chA].IsPitch()) {
            if (changed_cv[chA]) {
                SendNoteOn(outmap[chA].get_channel());
                note_countdown[chA] = HEMISPHERE_CLOCK_TICKS * trig_length;
                outchan_last[chA] = outmap[chA].get_channel();
            }
        }
    }

    // I think this can cause the UI to lag and miss input
    //usbMIDI.send_now();
}
#endif // !NO_HEMISPHERE

// ------------------------------------------------------------------
// CV/trigger -> MIDI engine
//
// Ports 0..kCVOutPorts-1 are the CV inputs, then the trigger inputs.
// Trigger port k is paired with CV port k: a TRFN_NOTE trigger takes
// its pitch from the paired CV port when that port is CVFN_PITCH
// (transpose/quantize from the CV port; channel/velocity/range from
// the trigger port). Runs every tick inside the core timer ISR — no
// heap, no blocking, sends rate-limited to value changes.
// ------------------------------------------------------------------
namespace {
  // MIDI clocks emitted per rising edge in TRFN_CLOCK, by clkdiv index
  constexpr uint8_t kClocksPerEdge[16] = {1, 2, 3, 4, 6, 8, 12, 24, 48, 96,
                                          1, 1, 1, 1, 1, 1};
  constexpr int kEngine5V = 5 * (12 << 7); // == HSAPPLICATION_5V
  constexpr int kEngine3V = 3 * (12 << 7); // == HSAPPLICATION_3V
}

void HS::MIDIFrame::PanicOutputs() {
    for (auto &p : outports) {
        if (p.gated) {
            SendNoteOff(p.last_channel, p.last_note, 0);
            p.gated = false;
        }
        p.latch_state = false;
        p.trigout_countdown = 0;
        p.lag_count = 0;
    }
}

namespace HS {
namespace {

  // pitch of CV port i, honoring its quantize flag and transpose
  uint8_t NoteForCVPort(const int i, const MIDIOutPort &cvport) {
      const int cv = cvmap[i].In();
      if (cvport.flags & MIDIOutSettings::FLAG_QUANTIZE) {
          // Process() returns a semitone COUNT, not a pitch CV — do not
          // feed it back through NoteNumber's /128 scaling
          const int semis = input_quant[i].Process(cv);
          const int note = semis + cvport.transpose + 12 * OC::DAC::kOctaveZero;
          return constrain(note, 0, 127);
      }
      return MIDIQuantizer::NoteNumber(cv, cvport.transpose);
  }

  // velocity for a note from port p: fixed data2, or sampled from a CV input
  uint8_t VelocityForPort(const MIDIOutPort &p) {
      const uint8_t src = p.velocity_source();
      if (!src || src > ADC_CHANNEL_COUNT) return p.data2;
      int v = Proportion(cvmap[src - 1].In(), kEngine5V, 127);
      CONSTRAIN(v, 1, 127);
      return v;
  }

} // anonymous namespace
} // namespace HS

void HS::MIDIFrame::ProcessOutputs(IOFrame &f) {
    // ---- CV ports: continuous senders ----
    for (int i = 0; i < kCVOutPorts; ++i) {
        MIDIOutPort &p = outports[i];
        if (p.indicator) --p.indicator;
        if (!p.function) continue;

        const int cv = cvmap[i].In();

        switch (CVOutFn(p.function)) {
          case CVFN_PITCH:    // sampled by the paired trigger port
          case CVFN_VELOCITY: // sampled at note-on via velocity_source
            break;

          case CVFN_PITCH_FREE: {
            if (!f.changed_cv[i]) break;
            const uint8_t note = NoteForCVPort(i, p);
            if (p.gated && note == p.last_note) break;
            if (note < p.range_low || note > p.range_high) break;
            const uint8_t vel = VelocityForPort(p);
            if (p.gated && (p.flags & MIDIOutSettings::FLAG_LEGATO)) {
                // legato overlap: new note first, then release the old one
                SendNoteOn(p.channel, note, vel);
                SendNoteOff(p.channel, p.last_note, 0);
            } else {
                if (p.gated) SendNoteOff(p.last_channel, p.last_note, 0);
                SendNoteOn(p.channel, note, vel);
            }
            PushOutLog(i, 0, p.channel, note, vel);
            p.gated = true;
            p.last_note = note;
            p.last_channel = p.channel;
            p.indicator = kMIDIIndicatorTicks;
            break;
          }

          case CVFN_GATE_NOTE: {
            const bool gate = f.gate_high[OC::DIGITAL_INPUT_LAST + i];
            if (gate && !p.gated) {
                SendNoteOn(p.channel, p.data1, VelocityForPort(p));
                PushOutLog(i, 0, p.channel, p.data1, p.data2);
                p.gated = true;
                p.last_note = p.data1;
                p.last_channel = p.channel;
                p.indicator = kMIDIIndicatorTicks;
            } else if (!gate && p.gated) {
                SendNoteOff(p.last_channel, p.last_note, 0);
                PushOutLog(i, 1, p.last_channel, p.last_note, 0);
                p.gated = false;
                p.indicator = kMIDIIndicatorTicks;
            }
            break;
          }

          case CVFN_CC7:
          case CVFN_AFTERTOUCH:
          case CVFN_NRPN7:
          case CVFN_PROGCHANGE: {
            int v = Proportion(cv, kEngine5V, 127);
            CONSTRAIN(v, (int)p.range_low, (int)p.range_high);
            if (uint16_t(v) == p.last_value) break;
            p.last_value = v;
            switch (CVOutFn(p.function)) {
              case CVFN_CC7:
                SendCC(p.channel, p.data1, v);
                PushOutLog(i, 2, p.channel, p.data1, v);
                break;
              case CVFN_AFTERTOUCH:
                SendAfterTouch(p.channel, v);
                PushOutLog(i, 3, p.channel, 0, v);
                break;
              case CVFN_PROGCHANGE: SendProgramChange(p.channel, v); break;
              default: // NRPN7: address (99/98), then 7-bit value (6)
                SendCC(p.channel, 99, p.data2);
                SendCC(p.channel, 98, p.data1);
                SendCC(p.channel, 6, v);
                break;
            }
            p.indicator = kMIDIIndicatorTicks;
            break;
          }

          case CVFN_CC14:
          case CVFN_BEND:
          case CVFN_NRPN14: {
            int v;
            if (CVOutFn(p.function) == CVFN_BEND) {
                v = Proportion(cv + kEngine3V, kEngine3V * 2, 16383);
                CONSTRAIN(v, 0, 16383);
            } else {
                v = Proportion(cv, kEngine5V, 16383);
                CONSTRAIN(v, (int)p.range_low << 7, ((int)p.range_high << 7) | 0x7f);
            }
            if (uint16_t(v) == p.last_value) break;
            p.last_value = v;
            switch (CVOutFn(p.function)) {
              case CVFN_BEND:
                SendPitchBend(p.channel, v);
                PushOutLog(i, 4, p.channel, v & 0x7f, (v >> 7) & 0x7f);
                break;
              case CVFN_CC14: // MSB on data1, LSB on data1+32
                SendCC(p.channel, p.data1, (v >> 7) & 0x7f);
                SendCC(p.channel, p.data1 + 32, v & 0x7f);
                PushOutLog(i, 2, p.channel, p.data1, (v >> 7) & 0x7f);
                break;
              default: // NRPN14
                SendCC(p.channel, 99, p.data2);
                SendCC(p.channel, 98, p.data1);
                SendCC(p.channel, 6, (v >> 7) & 0x7f);
                SendCC(p.channel, 38, v & 0x7f);
                break;
            }
            p.indicator = kMIDIIndicatorTicks;
            break;
          }

          default: break;
        }
    }

    // ---- trigger ports: edge-driven senders ----
    for (int k = 0; k < kTrigOutPorts; ++k) {
        MIDIOutPort &p = outports[kCVOutPorts + k];
        if (p.indicator) --p.indicator;

        // fixed-length note tail runs regardless of current function
        if (p.trigout_countdown > 0 && --p.trigout_countdown == 0 && p.gated) {
            SendNoteOff(p.last_channel, p.last_note, 0);
            PushOutLog(kCVOutPorts + k, 1, p.last_channel, p.last_note, 0);
            p.gated = false;
        }

        // Gate source: the TR jack, or — for pulse sources too weak for the
        // digital input's hardware threshold — any CV input via the ADC's
        // 1.25V threshold (FLAG_CV_GATE; clkdiv selects the CV jack).
        // Not available for TRFN_CLOCK, where clkdiv is the multiplier.
        bool gate;
        if ((p.flags & MIDIOutSettings::FLAG_CV_GATE)
             && TrigOutFn(p.function) != TRFN_CLOCK) {
            const int src = p.clkdiv % kCVOutPorts;
            gate = f.gate_high[OC::DIGITAL_INPUT_LAST + src];
        } else {
            gate = trigmap[k].Gate();
        }
        const bool rising = gate && !p.prev_gate;
        const bool falling = !gate && p.prev_gate;
        p.prev_gate = gate;

        if (!p.function) continue;

        switch (TrigOutFn(p.function)) {
          case TRFN_NOTE: {
            // ADC settling lag between gate edge and stable CV (see HSApplication)
            if (rising) p.lag_count = 96;
            if (p.lag_count && --p.lag_count == 0) {
                const bool paired_pitch =
                    outports[k].function == CVFN_PITCH && k < kCVOutPorts;
                const uint8_t note = paired_pitch
                    ? NoteForCVPort(k, outports[k])
                    : uint8_t(constrain(p.data1 + p.transpose, 0, 127));
                if (note >= p.range_low && note <= p.range_high) {
                    if (p.gated) SendNoteOff(p.last_channel, p.last_note, 0);
                    const uint8_t vel = VelocityForPort(p);
                    SendNoteOn(p.channel, note, vel);
                    PushOutLog(kCVOutPorts + k, 0, p.channel, note, vel);
                    p.gated = true;
                    p.last_note = note;
                    p.last_channel = p.channel;
                    p.indicator = kMIDIIndicatorTicks;
                }
            }
            // retrack pitch while the gate is held
            if (gate && !p.lag_count && p.gated &&
                outports[k].function == CVFN_PITCH && f.changed_cv[k]) {
                const uint8_t note = NoteForCVPort(k, outports[k]);
                if (note != p.last_note &&
                    note >= p.range_low && note <= p.range_high) {
                    if (p.flags & MIDIOutSettings::FLAG_LEGATO) {
                        SendNoteOn(p.channel, note, VelocityForPort(p));
                        SendNoteOff(p.last_channel, p.last_note, 0);
                    } else {
                        SendNoteOff(p.last_channel, p.last_note, 0);
                        SendNoteOn(p.channel, note, VelocityForPort(p));
                    }
                    PushOutLog(kCVOutPorts + k, 0, p.channel, note, p.data2);
                    p.last_note = note;
                    p.last_channel = p.channel;
                    p.indicator = kMIDIIndicatorTicks;
                }
            }
            if (falling) {
                p.lag_count = 0;
                if (p.gated) {
                    SendNoteOff(p.last_channel, p.last_note, 0);
                    PushOutLog(kCVOutPorts + k, 1, p.last_channel, p.last_note, 0);
                    p.gated = false;
                    p.indicator = kMIDIIndicatorTicks;
                }
            }
            break;
          }

          case TRFN_NOTE_TRIG:
            if (rising) {
                if (p.gated) SendNoteOff(p.last_channel, p.last_note, 0);
                SendNoteOn(p.channel, p.data1, VelocityForPort(p));
                PushOutLog(kCVOutPorts + k, 0, p.channel, p.data1, p.data2);
                p.gated = true;
                p.last_note = p.data1;
                p.last_channel = p.channel;
                p.trigout_countdown = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_NOTE_LATCH:
            if (rising) {
                p.latch_state = !p.latch_state;
                if (p.latch_state) {
                    SendNoteOn(p.channel, p.data1, VelocityForPort(p));
                    PushOutLog(kCVOutPorts + k, 0, p.channel, p.data1, p.data2);
                    p.gated = true;
                    p.last_note = p.data1;
                    p.last_channel = p.channel;
                } else if (p.gated) {
                    SendNoteOff(p.last_channel, p.last_note, 0);
                    PushOutLog(kCVOutPorts + k, 1, p.last_channel, p.last_note, 0);
                    p.gated = false;
                }
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_CC_MOMENTARY:
            if (rising) {
                SendCC(p.channel, p.data1, p.data2);
                PushOutLog(kCVOutPorts + k, 2, p.channel, p.data1, p.data2);
                p.indicator = kMIDIIndicatorTicks;
            }
            if (falling) {
                SendCC(p.channel, p.data1, 0);
                PushOutLog(kCVOutPorts + k, 2, p.channel, p.data1, 0);
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_CC_LATCH:
            if (rising) {
                p.latch_state = !p.latch_state;
                SendCC(p.channel, p.data1, p.latch_state ? p.data2 : 0);
                PushOutLog(kCVOutPorts + k, 2, p.channel, p.data1, p.latch_state ? p.data2 : 0);
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_START:
            if (rising) { SendRealTime(usbMIDI.Start); p.indicator = kMIDIIndicatorTicks; }
            break;
          case TRFN_STOP:
            if (rising) { SendRealTime(usbMIDI.Stop); p.indicator = kMIDIIndicatorTicks; }
            break;
          case TRFN_CONTINUE:
            if (rising) { SendRealTime(usbMIDI.Continue); p.indicator = kMIDIIndicatorTicks; }
            break;
          case TRFN_START_STOP:
            if (rising) {
                p.latch_state = !p.latch_state;
                SendRealTime(p.latch_state ? usbMIDI.Start : usbMIDI.Stop);
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_CLOCK:
            if (rising) {
                for (uint8_t n = 0; n < kClocksPerEdge[p.clkdiv]; ++n)
                    SendRealTime(usbMIDI.Clock);
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          case TRFN_PANIC:
            if (rising) {
                PanicOutputs();
                panic_request = true; // app Loop() finishes with the CC sweep
                p.indicator = kMIDIIndicatorTicks;
            }
            break;

          default: break;
        }
    }
}

void HS::IOFrame::Load(OC::IOFrame *ioframe) {
    current_ioframe = ioframe; // cache that ish

    // hardcoded to the enum...
    gate_high[0] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_1);
    gate_high[1] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_2);
    gate_high[2] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_3);
    gate_high[3] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_4);

    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        // Set CV inputs
        const int input = ioframe->cv.pitch_values[i];

        // calculate gates/clocks for all ADC inputs as well
        gate_high[DIGITAL_INPUT_COUNT + i] = input > GATE_THRESHOLD;

        // some calculations for change detection
        if (abs(input - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
            changed_cv[i] = 1;
            last_cv[i] = input;
        } else changed_cv[i] = 0;
    }

    // Handle clock pulse timing
    for (int i = 0; i < IO_CHANNEL_COUNT; ++i) {
        if (clock_countdown[i] > 0) {
            if (--clock_countdown[i] == 0) Out(i, 0);
        }
    }
    for (int i = 0; i < MIDIMAP_MAX; ++i) {
        MIDIMapping& m = MIDIState.mapping[i];
        if (m.trigout_countdown > 0) {
            if (--m.trigout_countdown == 0) {
                if (m.gate_retrig) {
                    m.gate_retrig = false;
                    m.output = PULSE_VOLTAGE * (12 << 7);
                } else {
                    m.output = 0;
                }
            }
        }
    }
#ifdef USB_GAMEPAD
    for (int i = 0; i < GAMEPAD_MAP_MAX; ++i) {
      GamepadMapping& g = GamepadState.mapping[i];
      if (g.trigout_countdown > 0) {
        if (--g.trigout_countdown == 0) g.output = 0;
      }
    }
#endif

    ProcessClock();

    // pre-calculate clock triggers
    for (int ch = 0; ch < APPLET_SLOTS * 2; ++ch) {
      bool result = 0;
      const size_t virt_chan = (ch) % (APPLET_SLOTS * 2);

      // clock triggers
      // TODO: implement div/mult within DigitalInputMap and get rid of
      //       this call to clock_m
      if (clock_m.IsRunning() && clock_m.GetMultiply(virt_chan) != 0)
          result = clock_m.Tock(virt_chan) && CheckSkip(virt_chan);
      else {
          result = trigmap[ch].Clock() && CheckSkip(ch);
      }

      // Try to eat a boop
      result = result || (clock_m.Beep(virt_chan) && CheckSkip(virt_chan));

      if (result) {
          cycle_ticks[ch] = HS::get_tick() - last_clock[ch];
          last_clock[ch] = HS::get_tick();
      }

      clocked[ch] = result;
    }
}

void HS::IOFrame::Send(OC::IOFrame *ioframe) {
    for (int i = 0; i < IO_CHANNEL_COUNT; ++i) {
      /*
       * envelope output!
      if (output_slew[i] < 0) {
        uint8_t gate_state = 0;
        const int target = outputs[i].get_target();
        // TODO: rising/falling edge detection in SlewedValue?
        const bool outgate_high = (target > GATE_THRESHOLD);
        const bool outgate_rising = outgate_high && ((target - output_diff[i]) < GATE_THRESHOLD);
        const bool outgate_falling = !outgate_high && ((target - output_diff[i]) > GATE_THRESHOLD);

        if (outgate_rising)
          gate_state |= peaks::CONTROL_GATE_RISING;

        if (outgate_high)
          gate_state |= peaks::CONTROL_GATE;
        else if (outgate_falling)
          gate_state |= peaks::CONTROL_GATE_FALLING;

        const int value = GetEnvelope(i).ProcessSingleSample(gate_state); // 0 to 32767
        ioframe->outputs.set_pitch_value(i, Proportion(value, 32767, HEMISPHERE_MAX_CV));

        continue;
      }
       */

      outputs[i].push(output_slew[i]);
      if (i < DAC_CHANNEL_COUNT)
        ioframe->outputs.set_pitch_value(i, outputs[i].get(output_atten[i]));
    }

#ifndef NO_HEMISPHERE
    if (autoMIDIOut) MIDIState.Send(outputs);
#endif
}
