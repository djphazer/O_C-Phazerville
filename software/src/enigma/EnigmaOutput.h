// Copyright (c) 2018, Jason Justian
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

#ifndef ENIGMAOUTPUT_H
#define ENIGMAOUTPUT_H

#include "../braids_quantizer.h"
#include "../braids_quantizer_scales.h"
#include "../OC_scales.h"

enum EnigmaOutputType {
    NOTE3,
    NOTE4,
    NOTE5,
    NOTE6,
    NOTE7,
    MODULATION,
    EXPRESSION,
    TRIGGER,
    GATE,
};
const char* enigma_type_names[] = {"Note 3-Bit", "Note 4-Bit", "Note 5-Bit", "Note 6-Bit", "Note 7-Bit", "Modulation", "Expression", "Trigger", "Gate"};
const char* enigma_type_short_names[] = {"Note-3", "Note-4", "Note-5", "Note-6", "Note-7", "Mod", "Expr", "Trigger", "Gate"};

class EnigmaOutput {
private:
    // Internal data
    byte out; // Output 0-4
    braids::Quantizer quantizer;
    int last_note = -1;
    int deferred_note = -1;

public:
    // Saved data
    byte tk; // Track 0-4
    byte ty; // Type (see enum above)
    byte sc; // Scale
    byte mc; // MIDI channel (1-16, 0=off)

    void InitAs(byte o) {
        out = o;
        tk = o;
        sc = 5;
        mc = 0;

        // Initialize quantizer
        quantizer.Init();
        quantizer.Configure(OC::Scales::GetScale(sc), 0xffff);

        // Set type based on output initialization
        if (o == 0) set_type(EnigmaOutputType::NOTE5);
        if (o == 1) set_type(EnigmaOutputType::MODULATION);
        if (o == 2) set_type(EnigmaOutputType::TRIGGER);
        if (o == 3) set_type(EnigmaOutputType::GATE);
    }

    // Getters
    byte output() {return out;}
    byte track() {return tk;}
    byte type() {return ty;}
    byte scale() {return sc;}
    byte midi_channel() {return mc;}
    int GetDeferredNote() {return deferred_note;}

    // Setters
    void set_output(byte output_) {out = constrain(output_, 0, 3);}
    void set_track(byte track_) {tk = constrain(track_, 0, 3);}
    void set_type(byte type_) {ty = constrain(type_, 0, EnigmaOutputType::GATE);}
    void set_scale(byte scale_) {
        sc = constrain(scale_, 0, OC::Scales::NUM_SCALES - 1);
        quantizer.Init();
        quantizer.Configure(OC::Scales::GetScale(sc), 0xffff);
    }
    void set_midi_channel(byte midi_channel_) {mc = constrain(midi_channel_, 0, 16);}
    void SetDeferredNote(int midi_note_) {deferred_note = midi_note_;}

    /* Sends data to an output based on the current output type. The I/O methods
     * are public methods of the app, so make sure to send a type that supports
     * Out, ClockOut, and GateOut (i.e. HSApplication, or HemisphereApplet)
     */
    template <class C>
    void SendToDAC(C *app, uint16_t reg, int transpose = 0) {

        // Quantize a note based on how many bits
        if (ty <= EnigmaOutputType::NOTE7) {
            byte bits = ty + 3; // Number of bits
            uint8_t mask = 0;
            for (byte s = 0; s < bits; s++) mask |= (0x01 << s);
            int note_shift = ty == EnigmaOutputType::NOTE7 ? 0 : 64; // Note types under 7-bit start at Middle C
            int note_number = (reg & mask) + note_shift;
            note_number = constrain(note_number, 0, 127);
            app->Out(out, quantizer.Lookup(note_number) + transpose);
        }

        // Modulation based on low 8 bits
        if (ty == EnigmaOutputType::MODULATION || ty == EnigmaOutputType::EXPRESSION) {
            app->Out(out, (reg & 0x00ff) * 6);
        }

        // Trigger sends a clock when low bit is high
        bool clock = reg & 0x0001;
        if (ty == EnigmaOutputType::TRIGGER && clock) {
            app->ClockOut(out);
        }

        // Gate goes high or low based on the low bit, and stays there until changed
        if (ty == EnigmaOutputType::GATE) {
            app->GateOut(out, clock);
        }
    }

    /* Sends data vi MIDI based on the current output type. */
    void SendToMIDI(uint16_t reg, int transpose = 0) {
        // Quantize a note based on how many bits
        if (ty <= EnigmaOutputType::NOTE7) {
            byte bits = ty + 3; // Number of bits
            uint8_t mask = 0;
            for (byte s = 0; s < bits; s++) mask |= (0x01 << s);
            int note_shift = ty == EnigmaOutputType::NOTE7 ? 0 : 60; // Note types under 7-bit start at Middle C
            int note_number = (reg & mask) + note_shift + (transpose / 128);
            note_number = constrain(note_number, 0, 127);

            if (midi_channel()) {
                if (last_note > -1) usbMIDI.sendNoteOn(last_note, 0, midi_channel());
                usbMIDI.sendNoteOn(note_number, 0x60, midi_channel());
                last_note = note_number;
            } else {
                deferred_note = note_number;
            }
        }

        // Modulation based on low 8 bits, shifted right for MIDI range
        if (ty == EnigmaOutputType::MODULATION && midi_channel()) {
            usbMIDI.sendControlChange(1, (reg & 0x00ff) >> 1, midi_channel());
        }

        // Expression based on low 8 bits; for MIDI, expression is a percentage of channel volume
        if (ty == EnigmaOutputType::EXPRESSION && midi_channel()) {
            usbMIDI.sendControlChange(11, (reg & 0x00ff) >> 1, midi_channel());
        }

        // Trigger and Gate behave the same way with MIDI; They'll use the last note that wasn't sent
        // out via MIDI on its own output. If no such note is available, then Trigger/Gate will do nothing.
        if ((ty == EnigmaOutputType::TRIGGER || ty == EnigmaOutputType::TRIGGER) && midi_channel()) {
            if (deferred_note >  -1 && (reg & 0x01)) {
                if (last_note > -1) usbMIDI.sendNoteOff(last_note, 0, midi_channel());
                usbMIDI.sendNoteOn(deferred_note, 0x60, midi_channel());
                last_note = deferred_note;
                deferred_note = -1;
            }
        }

        usbMIDI.send_now();
    }

    void NoteOff() {
        if (midi_channel()) {
            if (last_note > -1) usbMIDI.sendNoteOn(last_note, 0, midi_channel());
            last_note = -1;
            deferred_note = -1;
        }
    }
};

#endif // ENIGMAOUTPUT_H
