// Copyright (c) 2010, 2012, 2013, 2018, Jason Justian
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

//////////////////////////////////////////////////////////////////////////
// This file contains MIDI utilities for Hemisphere Suite, including:
// * System Exclusive Handler base class
// * Data packing structure
// * Teensyduino USB MIDI message number table
// * MIDI note number-name and MIDI channel tables
// * MIDI note/CV quantizer functions
//////////////////////////////////////////////////////////////////////////

#ifndef HSMIDI_H
#define HSMIDI_H

// Teensyduino USB MIDI Library message numbers
// See https://www.pjrc.com/teensy/td_midi.html

#define HEM_MIDI_NOTE_ON usbMIDI.NoteOn
#define HEM_MIDI_NOTE_OFF usbMIDI.NoteOff
#define HEM_MIDI_CC usbMIDI.ControlChange
#define HEM_MIDI_AFTERTOUCH usbMIDI.AfterTouchChannel
#define HEM_MIDI_PITCHBEND usbMIDI.PitchBend
#define HEM_MIDI_SYSEX usbMIDI.SystemExclusive

#define HEM_MIDI_CLOCK usbMIDI.Clock
#define HEM_MIDI_START usbMIDI.Start
#define HEM_MIDI_STOP usbMIDI.Stop

#define HEM_MIDI_CLOCK_DIVISOR 12

const char* const midi_note_numbers[128] = {
    "C-1","C#-1","D-1","D#-1","E-1","F-1","F#-1","G-1","G#-1","A-1","A#-1","B-1",
    "C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
    "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
    "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
    "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
    "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
    "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
    "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
    "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
    "C8","C#8","D8","D#8","E8","F8","F#8","G8","G#8","A8","A#8","B8",
    "C9","C#9","D9","D#9","E9","F9","F#9","G9"
};

const char* const midi_channels[17] = {
    "Off", " 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8",
    " 9", "10", "11", "12", "13", "14", "15", "16"
};

// The functions available for each output
enum MIDIFunctions {
    HEM_MIDI_NOOP = 0,
    HEM_MIDI_NOTE_OUT,
    HEM_MIDI_TRIG_OUT,
    HEM_MIDI_GATE_OUT,
    HEM_MIDI_VEL_OUT,
    HEM_MIDI_CC_OUT,
    HEM_MIDI_AT_OUT,
    HEM_MIDI_PB_OUT,
    HEM_MIDI_CLOCK_OUT,
    HEM_MIDI_START_OUT,

    HEM_MIDI_MAX_FUNCTION = HEM_MIDI_START_OUT
};
const char* const midi_fn_name[HEM_MIDI_MAX_FUNCTION + 1] = {"None", "Note#", "Trig", "Gate", "Veloc", "CC#", "Aft", "Bend", "Clock", "Start"};


/* Hemisphere Suite Data Packing
 *
 * Hemisphere Suite sysex data is arranged in 8-byte packets. The first
 * byte in each packet is a composite of the high bits of the next
 * seven data bytes.
 *
 * The structure does two basic operations:
 *
 * pack_data() converts an array of sysex data bytes into the packed format,
 * so that it may be transmitted via system exclusive to the appropriate
 * synth.

 * unpack_data() is the opposite operation; it converts a single sysex dump
 * into a series of data bytes, so that the data may be manipulated and/or
 * used to populate a data model.
 *
 * This software is based on my dsi_packing.h library, which was made to
 * manipulate sysex data of Dave Smith Instruments synthesizers. I used the
 * same scheme for Hemisphere because (1) I already had the code and (2) It's
 * probably the most efficient way to pack sysex data, barring of some type
 * of compression. And Teensy really needs efficiency.
 */

#define SYSEX_DATA_MAX_SIZE 60

/*
 * SysExData is a sort of generic data structure, containing a size, and a fixed-length
 * array of data. Since it's possible to use the same data structure to represent both packed
 * and unpacked data, several typedefs are included to disambiguate the role of the data in
 * your code.
 */
typedef struct _SysExData {
    int size;
    uint8_t data[SYSEX_DATA_MAX_SIZE];

    void set_data(int size_, uint8_t data_[])
    {
        size = size_;
        for (int i = 0; i < size; i++) data[i] = data_[i];
    }

    /*
     * Given packed voice data (for example, the data that would come directly from an instrument's
     * system exclusive dump), unpack_data() returns an UnpackedData, whose data property contains a
     * one-byte-per-parameter representation of the voice data. The UnpackedData will be an easy way
     * to examine, manipulate, and modify voice data.  Example:
     *
     *   (Accept a system exclusive dump. Process the sysex header yourself, and then put the data into
     *      data, an array of unsigned ints. Store the size of the data array in int size)
     *   PackedData packed_sysex;
     *   set_sysex_data(&packed_sysex, size, data);
     *   UnpackededVoice mopho_voice = unpack_data(packed_sysex);
     *   int cutoff_frequency = mopho_voice.data[20];
     */
    _SysExData unpack()
    {
        uint8_t udata[SYSEX_DATA_MAX_SIZE];
        uint8_t packbyte = 0;  /* Composite of high bits of next 7 bytes */
        uint8_t pos = 0;       /* Current position of 7 */
        uint8_t usize = 0;     /* Unpacked voice size */
        uint8_t c;             /* Current source byte */
        for (int ixp = 0; ixp < size; ixp++)
        {
            c = data[ixp];
            if (pos == 0) {
                packbyte = c;
            } else {
                if (packbyte & (1 << (pos - 1))) {c |= 0x80;}
                udata[usize++] = c;
            }
            pos++;
            pos &= 0x07;
            if (usize > SYSEX_DATA_MAX_SIZE) break;
        }

        _SysExData unpacked;
        unpacked.set_data(usize, udata);
        return unpacked;
    }

    /*
     * Given unpacked voice data (for example, data that might be modified or created by calling software),
     * pack_data() returns a PackedData, whose data property contains a packed representation of the
     * voice data. The PackedData is suitable for sending back to a SysEx instrument.  To send the packed
     * data back via a file or direct I/O call, see dump_voice(); or roll your own I/O to MIDI.  Don't forget
     * the appropriate system exclusive header. Example:
     *
     *   (Let's continue the example from unpack_data() above, by opening the filter all the way. As you
     *     may recall, mopho_voice_data is an UnpackedData)
     *   mopho_voice.data[20] = 164;
     *   PackedData mopho_sysex = pack_data(mopho_voice);
     */
    _SysExData pack()
    {
        uint8_t pdata[SYSEX_DATA_MAX_SIZE];
        uint8_t packbyte = 0;  /* Composite of high bits of next 7 bytes */
        uint8_t pos = 0;       /* Current position of 7 */
        uint8_t psize = 0;     /* Packed voice size */
        uint8_t packet[7];     /* Current packet */
        uint8_t c;             /* Current source byte */
        for (int ixu = 0; ixu < size; ixu++)
        {
            c = data[ixu];
            if (pos == 7) {
                pdata[psize++] = packbyte;
                for (int i = 0; i < pos; i++) pdata[psize++] = packet[i];
                packbyte = 0;
                pos = 0;
            }
            if (c & 0x80) {
                packbyte += (1 << pos);
                c &= 0x7f;
            }
            packet[pos] = c;
            pos++;
            if ((psize + 8) > SYSEX_DATA_MAX_SIZE) break;
        }
        pdata[psize++] = packbyte;
        for (int i = 0; i < pos; i++) pdata[psize++] = packet[i];
        _SysExData packed;
        packed.set_data(psize, pdata);
        return packed;
    }
} SysExData, UnpackedData, PackedData;

/*
 * Base class for applications that need MIDI SysEx support.
 *
 * Teensy 3.2's MIDI library limits the size of received system
 * exclusive dumps to 60 bytes. To maximize this space, a packed
 * byte system is used, in which each byte's bit 7 is packed into
 * a leading byte to form 8-byte "packets."
 *
 * Four bytes are reserved for the sysex header (the MIDI status
 * byte, the non-commercial manufacturer ID, the Beige Maze code,
 * and target application byte), and one for the end-of-sysex
 * byte. This leaves 55 bytes for application data. However, these
 * are 7-bit MIDI data bytes. Once the bytes are packed, each app
 * may transmit up to 48 bytes, or up to 24 16-bit words.
 */
class SystemExclusiveHandler {
public:

    /* OnSendSysEx() is called when there's a request to send system exclusive data, usually
     * in response to the suspension of the app. In OnSendSysEx(), the app is responsible for
     * generating an UnpackedData instance, which contains an array of up to 48 uint8_t bytes,
     * converting it to a PackedData instance, and passing that PackedData to SysExSend().
     */
    virtual void OnSendSysEx();

    /* OnReciveSysEx() is called when a system exclusive message comes in. In OnReceiveSysEx(),
     * the app is responsible for converting a PackedData instance into an UnpackedData instance,
     * which contains an array of up to 48 uint8_t bytes, and putting that data into the app's
     * internal data system.
     */
    virtual void OnReceiveSysEx();

protected:
    /* ListenForSysEx() is for use by apps that don't otherwise deal with listening to MIDI input.
     * A call to ListenForSysEx() is placed in the ISR. When SysEx is recieved, ListenForSysEx()
     * calls OnReceiveSysEx().
     *
     * IMPORTANT! Do not use ListenForSysEx() in apps that use MIDI in, because ListenForSysEx()
     * will devour most of the incoming MIDI events, and MIDI in won't work.
     */
    bool ListenForSysEx() {
        bool heard_sysex = 0;
        if (usbMIDI.read()) {
            if (usbMIDI.getType() == usbMIDI.SystemExclusive) {
                OnReceiveSysEx();
                heard_sysex = 1;
            }
        }
        return heard_sysex;
    }

    void SendSysEx(PackedData packed, char target_id) {
        uint8_t sysex[SYSEX_DATA_MAX_SIZE];
        uint8_t size = 0;
        sysex[size++] = 0xf0;      // Start of exclusive
        sysex[size++] = 0x7d;      // Non-Commercial Manufacturer
        sysex[size++] = 0x62;      // Beige Maze
        sysex[size++] = target_id; // Target product
        for (uint8_t i = 0; i < packed.size; i++)
        {
            sysex[size++] = packed.data[i];
        }
        sysex[size++] = 0xf7; // End of exclusive
        usbMIDI.sendSysEx(size, sysex);
        usbMIDI.send_now();
    }

    bool ExtractSysExData(uint8_t *V, char target_id) {
        // Get the full sysex dump from the MIDI library
        uint8_t *sysex = usbMIDI.getSysExArray();

        bool verify = (sysex[1] == 0x7d && sysex[2] == 0x62 && sysex[3] == target_id);
        if (verify) { // Does the received SysEx belong to this app?
            // Strip the header and end-of-exclusive byte to reveal packed data
            PackedData packed;
            uint8_t psize = 0;
            uint8_t data[SYSEX_DATA_MAX_SIZE];
            for (int i = 0; i < SYSEX_DATA_MAX_SIZE; i++)
            {
                uint8_t b = sysex[i + 4]; // Getting packed bytes past the header
                if (b == 0xf7) break;
                data[psize++] = b;
            }
            packed.set_data(psize, data);

            // Unpack the data and set the value array
            UnpackedData unpacked = packed.unpack();
            for (int i = 0; i < unpacked.size; i++)
            {
                V[i] = unpacked.data[i];
            }
            last_app_code = target_id;
        } else {
            if (sysex[1] == 0x7d && sysex[2] == 0x62) {
                last_app_code = sysex[3];
            } else last_app_code = 0; // Unknown application
        }
        return verify;
    }

    char LastSysExApplicationCode() {return last_app_code;}

private:
    char last_app_code; // The most recent application code received
};

/*
 * MIDI Quantizer: Converts pitch CV values to MIDI note numbers,
 * and vice versa. CV values are (12 << 7) steps per volt, or
 * 128 steps per semitone.
 */
class MIDIQuantizer {
public:
    /* Given a pitch CV value, return the MIDI note number */
    static uint8_t NoteNumber(int cv, int transpose = 0) {
        // CV controllers might be right on the border between voltages, so provide 1/4 tone offset
        if (cv > 0) cv += 32;
        if (cv < 0) cv -= 32;
        int octave = cv / (12 << 7);
        int semitone = (cv % (12 << 7)) / 128;
        int midi_note_number = (octave * 12) + semitone + transpose + 60;
        if (midi_note_number > 127) midi_note_number = 127;
        if (midi_note_number < 0) midi_note_number = 0;
        return static_cast<uint8_t>(midi_note_number);
    }

    /* Given a MIDI note number, return the pitch CV value */
    static int CV(uint8_t midi_note_number, int transpose = 0) {
        int octave = midi_note_number / 12;
        int semitone = midi_note_number % 12;
        int cv = (octave * (12 << 7)) + (semitone * 128) + (transpose * 128) - (5 * (12 << 7));
        return cv;
    }
};

#endif /* HSMIDI_H */
