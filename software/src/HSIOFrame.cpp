#include "HSClockManager.h"
#include "HSMIDI.h"
#include "HSUtils.h"
#include "HSIOFrame.h"

// arguments are raw data from MIDI system, so channel starts at 1 (not 0)
void HS::MIDIFrame::ProcessMIDIMsg(const MIDIMessage msg) {
    const uint8_t m_ch = msg.channel - 1;

    switch (msg.message) { // System Real Time messages
        case usbMIDI.Clock:
            clock_q = (clock_count % (24/MIDI_CLOCK_PPQN) == 0); // for internal sync @ 2ppqn
            ++clock_count;
            for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                mapping[ch].ProcessClock(clock_count);
            }
            if (clock_count == 24) clock_count = 0;
            return;
            break;

        case usbMIDI.Start:
        case usbMIDI.Continue: // treat Continue like Start
            start_q = 1;
            clock_count = 0;
            clock_run = true;

            for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                if (mapping[ch].function == HEM_MIDI_START_OUT) {
                    mapping[ch].ClockOut();
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
            for (int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                mapping[ch].output = 0;
                mapping[ch].trigout_countdown = 0;
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

    for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
        MIDIMapping &map = mapping[ch];
        if (map.function == HEM_MIDI_NOOP) continue;

        // skip unwanted MIDI Channels
        if (map.channel != m_ch && map.channel != 16) continue;

        last_midi_channel = m_ch;

        // prevent duplicate log entries
        if (m_ch == m_ch_prev) log_skip = true;
        else log_skip = false;
        m_ch_prev = m_ch;

        bool log_this = false;

        switch (msg.message) {
            case usbMIDI.NoteOn: {
                if (map.function == HEM_MIDI_LEARN) {
                  //TODO: set range based on polyphony, or alternate learn modes
                  //if (map.function_cc < 0)
                    //map.range_low = map.range_high = msg.data1;
                  map.function = HEM_MIDI_NOTE_OUT;
                  map.function_cc = 0;
                  map.channel = msg.channel - 1;
                }
                if (!map.InRange(msg.data1)) break;
                map.semitone_mask = map.semitone_mask | (1u << (msg.data1 % 12));

                // Should this message go out on this channel?
                switch (map.function) { // note # output functions
                    case HEM_MIDI_NOTE_OUT:
                        map.output = MIDIQuantizer::CV(GetNoteLast(note_buffer[m_ch]));
                        break;

                    case HEM_MIDI_NOTE_POLY_OUT:
                        if (CheckPolyVoice(map.dac_polyvoice)) map.output = MIDIQuantizer::CV(poly_buffer[map.dac_polyvoice].note);
                        break;

                    case HEM_MIDI_NOTE_MIN_OUT:
                        map.output = MIDIQuantizer::CV(GetNoteMin(note_buffer[m_ch]));
                        break;

                    case HEM_MIDI_NOTE_MAX_OUT:
                        map.output = MIDIQuantizer::CV(GetNoteMax(note_buffer[m_ch]));
                        break;

                    case HEM_MIDI_NOTE_PEDAL_OUT:
                        map.output = MIDIQuantizer::CV(GetNoteFirst(note_buffer[m_ch]));
                        break;

                    case HEM_MIDI_NOTE_INV_OUT:
                        map.output = MIDIQuantizer::CV(GetNoteLastInv(note_buffer[m_ch]));
                        break;

                    case HEM_MIDI_TRIG_1ST_OUT:
                        if (note_buffer[m_ch].size() != 1) break;
                    case HEM_MIDI_TRIG_OUT:
                    case HEM_MIDI_TRIG_ALWAYS_OUT:
                        map.ClockOut();
                        break;

                    case HEM_MIDI_GATE_OUT:
                        map.output = PULSE_VOLTAGE * (12 << 7);
                        break;
                    case HEM_MIDI_GATE_INV_OUT:
                        map.output = 0;
                        break;
                    case HEM_MIDI_GATE_POLY_OUT:
                        if (CheckPolyVoice(map.dac_polyvoice)) map.output = PULSE_VOLTAGE * (12 << 7);
                        break;

                    case HEM_MIDI_VEL_OUT:
                        map.output = (note_buffer[m_ch].size() > 0) ? Proportion(GetVel(note_buffer[m_ch], 1), 127, HEMISPHERE_MAX_CV) : 0;
                        break;
                    case HEM_MIDI_VEL_POLY_OUT:
                        map.output = (CheckPolyVoice(map.dac_polyvoice)) ? Proportion(poly_buffer[map.dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;
                        break;
                }


                if (!log_skip) log_this = 1; // Log all MIDI notes. Other stuff is conditional.
                break;
            }
            case usbMIDI.NoteOff: {
                if (!map.InRange(msg.data1)) break;
                map.semitone_mask = map.semitone_mask & ~(1u << (msg.data1 % 12));

                // don't update output when last note is released
                // or if sustain is engaged
                if (note_buffer[m_ch].size() > 0 && !CheckSustainLatch(m_ch)) {
                    switch(map.function) { // note # output functions
                        case HEM_MIDI_NOTE_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteLast(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_POLY_OUT:
                            if (CheckPolyVoice(map.dac_polyvoice)) map.output = MIDIQuantizer::CV(poly_buffer[map.dac_polyvoice].note);
                            break;

                        case HEM_MIDI_NOTE_MIN_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteMin(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_MAX_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteMax(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_PEDAL_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteFirst(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_INV_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteLastInv(note_buffer[m_ch]));
                            break;
                    }
                }

                if (map.function == HEM_MIDI_TRIG_ALWAYS_OUT) map.ClockOut();

                if (!CheckSustainLatch(m_ch)) {
                    if (!(note_buffer[m_ch].size() > 0)) { // turn mono gate off, only when all notes are off
                        if (map.function == HEM_MIDI_GATE_OUT) map.output = 0;
                        if (map.function == HEM_MIDI_GATE_INV_OUT) map.output = PULSE_VOLTAGE * (12 << 7);
                    }
                    if (map.function == HEM_MIDI_GATE_POLY_OUT) {
                        if (!CheckPolyVoice(map.dac_polyvoice)) map.output = 0;
                    }
                }

                if (map.function == HEM_MIDI_VEL_OUT)
                    map.output = (note_buffer[m_ch].size() > 0) ? Proportion(GetVel(note_buffer[m_ch], 1), 127, HEMISPHERE_MAX_CV) : 0;
                if (map.function == HEM_MIDI_VEL_POLY_OUT)
                    map.output = (CheckPolyVoice(map.dac_polyvoice)) ? Proportion(poly_buffer[map.dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;

                if (!log_skip) log_this = 1;
                break;
            }
            case usbMIDI.ControlChange: { // Modulation wheel or other CC
                // handle sustain pedal
                if (msg.data1 == 64) {
                    if (msg.data2 > 63) {
                        if (!CheckSustainLatch(m_ch)) sustain_latch |= (1 << m_ch);
                    } else {
                        ClearSustainLatch(m_ch);
                        if (!(note_buffer[m_ch].size() > 0)) {
                            switch (map.function) {
                                case HEM_MIDI_GATE_OUT:
                                case HEM_MIDI_GATE_POLY_OUT:
                                    map.output = 0;
                                    break;
                                case HEM_MIDI_GATE_INV_OUT:
                                    map.output = PULSE_VOLTAGE * (12 << 7);
                                    break;
                            }
                        }
                    }
                }

                if (map.function == HEM_MIDI_LEARN) {
                  map.function = HEM_MIDI_CC_OUT;
                  map.function_cc = msg.data1;
                  map.channel = msg.channel - 1;
                }

                if (map.function == HEM_MIDI_CC_OUT) {
                    if (map.function_cc < 0) { // auto-learn CC#
                      map.function_cc = msg.data1;
                    }
                    if (map.function_cc == msg.data1) {
                        map.output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
                        if (!log_skip) log_this = 1;
                    }
                }
                break;
            }
            case usbMIDI.AfterTouchPoly: {
                if (map.function == HEM_MIDI_AT_KEY_POLY_OUT) {
                    if (FindPolyNoteIndex(msg.data1) == map.dac_polyvoice)
                        map.output = Proportion(msg.data2, 127, HEMISPHERE_MAX_CV);
                    if (!log_skip) log_this = 1;
                }
                break;
            }
            case usbMIDI.AfterTouchChannel: {
                if (map.function == HEM_MIDI_AT_CHAN_OUT) {
                    map.output = Proportion(msg.data1, 127, HEMISPHERE_MAX_CV);
                    if (!log_skip) log_this = 1;
                }
                break;
            }
            case usbMIDI.PitchBend: {
                if (map.function == HEM_MIDI_LEARN) {
                  map.function = HEM_MIDI_PB_OUT;
                  map.channel = msg.channel - 1;
                }
                if (map.function == HEM_MIDI_PB_OUT) {
                    int data = (msg.data2 << 7) + msg.data1 - 8192;
                    map.output = Proportion(data, 8192, HEMISPHERE_3V_CV);
                    if (!log_skip) log_this = 1;
                }
                break;
            }
        }
        if (log_this) UpdateLog(msg);
    }
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
