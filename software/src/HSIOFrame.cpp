#include "HSClockManager.h"
#include "HSMIDI.h"
#include "HSIOFrame.h"

// arguments are raw data from MIDI system, so channel starts at 1 (not 0)
void HS::MIDIFrame::ProcessMIDIMsg(const MIDIMessage msg) {
    const uint8_t m_ch = msg.channel - 1;

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
              if (map.function == HEM_MIDI_START_OUT) {
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

    bool log_skip = false;
    uint8_t m_ch_prev = 255; // initialize to invalid channel

    for (MIDIMapping& map : mapping) {
        if (!map.function) continue;

        // skip unwanted MIDI Channels
        if (map.channel != m_ch && map.channel != 16) continue;

        last_midi_channel = m_ch;

        // prevent duplicate log entries
        if (m_ch == m_ch_prev) log_skip = true;
        else log_skip = false;
        m_ch_prev = m_ch;

        bool log_this = map.ProcessMsg(msg, *this) && !log_skip;

        if (log_this) UpdateLog(msg);
    }
}

bool HS::MIDIMapping::ProcessMsg(const MIDIMessage msg, HS::MIDIFrame &state) {
  if (function == HEM_MIDI_NOOP) return false;

  const uint8_t m_ch = msg.channel - 1;
  NoteBuffer &buf = state.note_buffer[m_ch];

  bool log_this = false;

  if (function == HEM_MIDI_LEARN) {
    switch (msg.message) {
      case usbMIDI.NoteOn:
        //TODO: set range based on polyphony, or alternate learn modes
        //if (function_cc < 0)
          //range_low = range_high = msg.data1;
        function = HEM_MIDI_NOTE_OUT;
        function_cc = 0;
        channel = m_ch;
        break;
      case usbMIDI.ControlChange:
        function = HEM_MIDI_CC_OUT;
        function_cc = msg.data1;
        channel = msg.channel - 1;
        break;
      case usbMIDI.PitchBend:
        function = HEM_MIDI_PB_OUT;
        channel = msg.channel - 1;
        break;
      default: break;
    }
  }

  switch (msg.message) {
    case usbMIDI.NoteOn: {
        if (!InRange(msg.data1)) break;
        semitone_mask = semitone_mask | (1u << (msg.data1 % 12));

        // Should this message go out on this channel?
        switch (function) {
            // note # output functions
            case HEM_MIDI_NOTE_OUT:
                output = MIDIQuantizer::CV(state.GetNoteLast(buf));
                break;

            case HEM_MIDI_NOTE_POLY_OUT:
                if (state.CheckPolyVoice(dac_polyvoice)) output = MIDIQuantizer::CV(state.poly_buffer[dac_polyvoice].note);
                break;

            case HEM_MIDI_NOTE_MIN_OUT:
                output = MIDIQuantizer::CV(state.GetNoteMin(buf));
                break;

            case HEM_MIDI_NOTE_MAX_OUT:
                output = MIDIQuantizer::CV(state.GetNoteMax(buf));
                break;

            case HEM_MIDI_NOTE_PEDAL_OUT:
                output = MIDIQuantizer::CV(state.GetNoteFirst(buf));
                break;

            case HEM_MIDI_NOTE_INV_OUT:
                output = MIDIQuantizer::CV(state.GetNoteLastInv(buf));
                break;

            case HEM_MIDI_TRIG_1ST_OUT:
                if (buf.size() != 1) break;
            case HEM_MIDI_TRIG_OUT:
            case HEM_MIDI_TRIG_ALWAYS_OUT:
                ClockOut();
                break;

            case HEM_MIDI_GATE_OUT:
                output = PULSE_VOLTAGE * (12 << 7);
                break;
            case HEM_MIDI_GATE_INV_OUT:
                output = 0;
                break;
            case HEM_MIDI_GATE_POLY_OUT:
                if (state.CheckPolyVoice(dac_polyvoice)) output = PULSE_VOLTAGE * (12 << 7);
                break;

            case HEM_MIDI_VEL_OUT:
                output = (buf.size() > 0) ? Proportion(state.GetVel(buf, 1), 127, HEMISPHERE_MAX_CV) : 0;
                break;
            case HEM_MIDI_VEL_POLY_OUT:
                output = (state.CheckPolyVoice(dac_polyvoice)) ? Proportion(state.poly_buffer[dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;
                break;
        }

        log_this = 1; // Log all MIDI notes. Other stuff is conditional.
        break;
    }
    case usbMIDI.NoteOff: {
        if (!InRange(msg.data1)) break;
        semitone_mask = semitone_mask & ~(1u << (msg.data1 % 12));

        // don't update output when last note is released
        // or if sustain is engaged
        if (buf.size() > 0 && !state.CheckSustainLatch(m_ch)) {
            switch(function) { // note # output functions
                case HEM_MIDI_NOTE_OUT:
                    output = MIDIQuantizer::CV(state.GetNoteLast(buf));
                    break;

                case HEM_MIDI_NOTE_POLY_OUT:
                    if (state.CheckPolyVoice(dac_polyvoice)) output = MIDIQuantizer::CV(state.poly_buffer[dac_polyvoice].note);
                    break;

                case HEM_MIDI_NOTE_MIN_OUT:
                    output = MIDIQuantizer::CV(state.GetNoteMin(buf));
                    break;

                case HEM_MIDI_NOTE_MAX_OUT:
                    output = MIDIQuantizer::CV(state.GetNoteMax(buf));
                    break;

                case HEM_MIDI_NOTE_PEDAL_OUT:
                    output = MIDIQuantizer::CV(state.GetNoteFirst(buf));
                    break;

                case HEM_MIDI_NOTE_INV_OUT:
                    output = MIDIQuantizer::CV(state.GetNoteLastInv(buf));
                    break;
            }
        }

        if (function == HEM_MIDI_TRIG_ALWAYS_OUT) ClockOut();

        if (!state.CheckSustainLatch(m_ch)) {
            if (buf.size() == 0) { // turn mono gate off, only when all notes are off
                if (function == HEM_MIDI_GATE_OUT) output = 0;
                if (function == HEM_MIDI_GATE_INV_OUT) output = PULSE_VOLTAGE * (12 << 7);
            }
            if (function == HEM_MIDI_GATE_POLY_OUT) {
                if (!state.CheckPolyVoice(dac_polyvoice)) output = 0;
            }
        }

        if (function == HEM_MIDI_VEL_OUT)
            output = (buf.size() > 0) ? Proportion(state.GetVel(buf, 1), 127, HEMISPHERE_MAX_CV) : 0;
        if (function == HEM_MIDI_VEL_POLY_OUT)
            output = (state.CheckPolyVoice(dac_polyvoice)) ? Proportion(state.poly_buffer[dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;

        log_this = 1;
        break;
    }
    case usbMIDI.ControlChange: { // Modulation wheel or other CC
        // handle sustain pedal
        if (msg.data1 == 64) {
            if (msg.data2 > 63) {
                if (!state.CheckSustainLatch(m_ch)) state.sustain_latch |= (1 << m_ch);
            } else {
                state.ClearSustainLatch(m_ch);
                if (!(buf.size() > 0)) {
                    switch (function) {
                        case HEM_MIDI_GATE_OUT:
                        case HEM_MIDI_GATE_POLY_OUT:
                            output = 0;
                            break;
                        case HEM_MIDI_GATE_INV_OUT:
                            output = PULSE_VOLTAGE * (12 << 7);
                            break;
                    }
                }
            }
        }

        if (function == HEM_MIDI_CC_OUT) {
            if (function_cc < 0) { // auto-learn CC#
              function_cc = msg.data1;
            }
            if (function_cc == msg.data1) {
                output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
                log_this = 1;
            }
        }
        break;
    }
    case usbMIDI.AfterTouchPoly: {
        if (function == HEM_MIDI_AT_KEY_POLY_OUT) {
            if (state.FindPolyNoteIndex(msg.data1) == dac_polyvoice)
                output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
            log_this = 1;
        }
        break;
    }
    case usbMIDI.AfterTouchChannel: {
        if (function == HEM_MIDI_AT_CHAN_OUT) {
            output = Proportion(msg.data1, 127, HEMISPHERE_MAX_CV);
            log_this = 1;
        }
        break;
    }
    case usbMIDI.PitchBend: {
        if (function == HEM_MIDI_PB_OUT) {
            int data = (msg.data2 << 7) + msg.data1 - 8192;
            output = Proportion(data, 8192, HEMISPHERE_3V_CV);
            log_this = 1;
        }
        break;
    }
  } // switch
  return log_this;
}

void HS::MIDIFrame::Send(const int *outvals) {
    // first pass - calculate things and turn off notes
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
        const uint8_t midi_ch = outchan[i];

        inputs[i] = outvals[i];
        gate_high[i] = inputs[i] > (12 << 7);
        clocked[i] = (gate_high[i] && last_cv[i] < (12 << 7));
        if (abs(inputs[i] - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
            changed_cv[i] = 1;
            last_cv[i] = inputs[i];
        } else changed_cv[i] = 0;

        switch (outfn[i]) {
            case HEM_MIDI_NOTE_OUT:
                if (changed_cv[i]) {
                    // a note has changed, turn the last one off first
                    SendNoteOff(outchan_last[i]);
                    current_note[midi_ch] = MIDIQuantizer::NoteNumber( inputs[i] );
                }
                break;

            case HEM_MIDI_GATE_OUT:
                if (!gate_high[i] && changed_cv[i])
                    SendNoteOff(midi_ch);
                break;

            case HEM_MIDI_CC_OUT:
            {
                const uint8_t newccval = ProportionCV(abs(inputs[i]), 127);
                if (newccval != current_ccval[i])
                    SendCC(midi_ch, outccnum[i], newccval);
                current_ccval[i] = newccval;
                break;
            }
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

        if (outfn[chB] == HEM_MIDI_GATE_OUT) {
            if (clocked[chB]) {
                SendNoteOn(outchan[chB]);
                // no countdown
                outchan_last[chB] = outchan[chB];
            }
        } else if (outfn[chA] == HEM_MIDI_NOTE_OUT) {
            if (changed_cv[chA]) {
                SendNoteOn(outchan[chA]);
                note_countdown[chA] = HEMISPHERE_CLOCK_TICKS * trig_length;
                outchan_last[chA] = outchan[chA];
            }
        }
    }

    // I think this can cause the UI to lag and miss input
    //usbMIDI.send_now();
}

void HS::IOFrame::Load(OC::IOFrame *ioframe) {
    auto triggers = ioframe->digital_inputs.triggered();

    // TODO: configurable clock sync input
    synctrig = triggers & DIGITAL_INPUT_MASK(0);

    // hardcoded to the enum...
    gate_high[0] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_1);
    gate_high[1] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_2);
    gate_high[2] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_3);
    gate_high[3] = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_4);

    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        // Set CV inputs
        inputs[i] = ioframe->cv.pitch_values[i];

        // calculate gates/clocks for all ADC inputs as well
        gate_high[OC::DIGITAL_INPUT_LAST + i] = inputs[i] > GATE_THRESHOLD;

        // some calculations for change detection
        if (abs(inputs[i] - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
            changed_cv[i] = 1;
            last_cv[i] = inputs[i];
        } else changed_cv[i] = 0;
    }

    // Handle clock pulse timing
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
        if (clock_countdown[i] > 0) {
            if (--clock_countdown[i] == 0) outputs[i] = 0;
        }
    }
    for (int i = 0; i < MIDIMAP_MAX; ++i) {
        MIDIMapping& m = MIDIState.mapping[i];
        if (m.trigout_countdown > 0) {
            if (--m.trigout_countdown == 0) m.output = 0;
        }
    }

    // pre-calculate clock triggers
    for (int ch = 0; ch < APPLET_SLOTS * 2; ++ch) {
      bool result = 0;
      const size_t virt_chan = (ch) % (APPLET_SLOTS * 2);

      // clock triggers
      // TODO: implement div/mult within DigitalInputMap and get rid of
      //       this call to clock_m
      if (clock_m.IsRunning() && clock_m.GetMultiply(virt_chan) != 0)
          result = clock_m.Tock(virt_chan);
      else {
          result = trigmap[ch].Clock();
      }

      // Try to eat a boop
      result = result || clock_m.Beep(virt_chan);

      if (result) {
          cycle_ticks[ch] = OC::CORE::ticks - last_clock[ch];
          last_clock[ch] = OC::CORE::ticks;
      }

      clocked[ch] = result;
    }
}

void HS::IOFrame::Send(OC::IOFrame *ioframe) {
    const DAC_CHANNEL chan[DAC_CHANNEL_COUNT] = {
      DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_C, DAC_CHANNEL_D,
#ifdef ARDUINO_TEENSY41
      DAC_CHANNEL_E, DAC_CHANNEL_F, DAC_CHANNEL_G, DAC_CHANNEL_H,
#endif
    };
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
      // OC::DAC::set_pitch_scaled(chan[i], outputs[i], 0);
      // output scaling is built-in now?
      ioframe->outputs.set_pitch_value(chan[i], outputs[i]);
    }
    if (autoMIDIOut) MIDIState.Send(outputs);
}

