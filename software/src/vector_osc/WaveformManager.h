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

#ifndef WAVEFORM_MANAGER_H
#define WAVEFORM_MANAGER_H

#include "waveform_library.h"

class WaveformManager {
public:
    /*
     * The segment at user_waveforms[0] should have a level byte of 0xfc, and the time
     * byte should have a value of 0xe2. This indicates that the memory is set up for
     * segment storage. If Validate() is false, then Setup() should be executed.
     */
    bool static Validate() {
        return (HS::user_waveforms[0].level == 0xfc && HS::user_waveforms[0].time == 0xe2);
    }

    /* Add a triangle and sawtooth waveform */
    void static Setup() {
        HS::user_waveforms[0] = VOSegment {0xfc, 0xe2};
        HS::user_waveforms[1] = VOSegment {0x02, 0xff}; // TOC entry: 2 steps
        HS::user_waveforms[2] = VOSegment {0xff, 0x01}; // First segment of triangle
        HS::user_waveforms[3] = VOSegment {0x00, 0x01}; // Second segment of triangle
        HS::user_waveforms[4] = VOSegment {0x02, 0xff}; // TOC entry: 2 steps
        HS::user_waveforms[5] = VOSegment {0xff, 0x00}; // First segment of sawtooth
        HS::user_waveforms[6] = VOSegment {0x00, 0x01}; // Second segment of sawtooth
        for (byte i = 7; i < 64; i++) HS::user_waveforms[i] = VOSegment {0x00, 0xff};
    }

    byte static WaveformCount() {
        byte count = 0;
        for (byte i = 0; i < HS::VO_SEGMENT_COUNT; i++)
        {
            if (HS::user_waveforms[i].IsTOC()) count++;
        }
        return count;
    }

    /* Allows a client application to navigate back and forth between the user waveforms and library
     * waveforms without having to think about the spaces between them.
     *
     * waveform_number is the starting waveform, and direction is -1 or 1, which way you want to move
     * in the list.
     */
    byte static GetNextWaveform(byte waveform_number, int direction) {
        int new_number = waveform_number + direction;
        byte count = WaveformCount();
        if (new_number < 0) new_number = 0;
        if (new_number == count) new_number = 32; // Move from last user waveform to first library waveform
        if (new_number <= 31 && new_number >= count) new_number = count - 1; // Move from first library waveform to last user waveform
        if (new_number >= (HS::WAVEFORM_LIBRARY_COUNT + 32)) new_number = HS::WAVEFORM_LIBRARY_COUNT + 31;
        return new_number;
    }

    byte static SegmentsRemaining() {
        byte segment_count = 1; // Include validation segment
        for (byte i = 0; i < HS::VO_SEGMENT_COUNT; i++)
        {
            if (HS::user_waveforms[i].IsTOC()) {
                segment_count += HS::user_waveforms[i].Segments();
            }
        }
        return (64 - segment_count);
    }

    VectorOscillator static VectorOscillatorFromWaveform(byte waveform_number) {
        VectorOscillator osc;
        if (waveform_number >= 32) { // Library waveforms start at 32
            osc = VectorOscillatorFromLibrary(waveform_number);
        } else {
            byte count = 0;
            for (byte i = 0; i < HS::VO_SEGMENT_COUNT; i++)
            {
                if (HS::user_waveforms[i].IsTOC()) {
                    if (count == waveform_number) {
                        for (int s = 0; s < HS::user_waveforms[i].Segments(); s++)
                        {
                            osc.SetSegment(HS::user_waveforms[i + s + 1]);
                        }
                        break;
                    }
                    count++;
                }
            }
        }
        return osc;
    }

    VectorOscillator static VectorOscillatorFromLibrary(byte waveform_number) {
        waveform_number = waveform_number - 32; // Library waveforms start at 32
        if (waveform_number >= HS::WAVEFORM_LIBRARY_COUNT) waveform_number = HS::WAVEFORM_LIBRARY_COUNT - 1;
        VectorOscillator osc;
        byte count = 0;
        for (byte i = 0; i < ARRAY_SIZE(HS::library_waveforms); i++)
        {
            if (HS::library_waveforms[i].IsTOC()) {
                if (count == waveform_number) {
                    for (int s = 0; s < HS::library_waveforms[i].Segments(); s++)
                    {
                        osc.SetSegment(HS::library_waveforms[i + s + 1]);
                    }
                    break;
                }
                count++;
            }
        }
        return osc;
    }

    byte static GetSegmentIndex(byte waveform_number, byte segment_number, int8_t direction = 0) {
        byte count = 0;
        byte segment_index = 0; // Index from which to copy

        // Find the waveform that's the target of the add operation
        for (byte i = 0; i < HS::VO_SEGMENT_COUNT; i++)
        {
            if (HS::user_waveforms[i].IsTOC() && count++ == waveform_number) {
                segment_index = i + segment_number + 1;
                HS::user_waveforms[i].SetTOC(HS::user_waveforms[i].Segments() + direction);
                break;
            }
        }

        return segment_index;
    }

    void static AddSegmentToWaveformAtSegmentIndex(byte waveform_number, byte segment_number) {
        byte insert_point = GetSegmentIndex(waveform_number, segment_number, 1);

        // If the waveform was found, move the remaining steps to insert a new segment. The
        // newly-inserted step should be a copy of the insert point.
        if (insert_point) {
            for (int i = (HS::VO_SEGMENT_COUNT - 1); i > insert_point ; i--)
            {
                memcpy(&HS::user_waveforms[i], &HS::user_waveforms[i - 1], sizeof(HS::user_waveforms[i - 1]));
            }
            if (HS::user_waveforms[insert_point + 1].time == 0) HS::user_waveforms[insert_point + 1].time = 1;
        }
    }

    void static DeleteSegmentFromWaveformAtSegmentIndex(byte waveform_number, byte segment_number) {
        byte delete_point = GetSegmentIndex(waveform_number, segment_number, -1);

        // If the waveform was found, move the remaining steps to overwrite the deleted segment.
        if (delete_point) {
            for (int i = delete_point; i < (HS::VO_SEGMENT_COUNT - 1); i++)
            {
                memcpy(&HS::user_waveforms[i], &HS::user_waveforms[i + 1], sizeof(HS::user_waveforms[i + 1]));
            }
        }
    }

    void static Update(byte waveform_number, byte segment_number, VOSegment *segment) {
        byte ix = GetSegmentIndex(waveform_number, segment_number);
        if (ix) {
            HS::user_waveforms[ix].level = segment->level;
            HS::user_waveforms[ix].time = segment->time;
        }
    }

    void static AddWaveform() {
        byte ix = 1;
        for (byte i = 0; i < HS::VO_SEGMENT_COUNT; i++)
        {
            if (HS::user_waveforms[i].IsTOC()) {
                ix = i + HS::user_waveforms[i].Segments() + 1;
            }
        }

        // If there's enough room, add a new triangle waveform
        if (ix < 61) {
            HS::user_waveforms[ix] = VOSegment {0x02, 0xff}; // TOC entry: 2 steps
            HS::user_waveforms[ix + 1] = VOSegment {0xff, 0x01}; // First segment of triangle
            HS::user_waveforms[ix + 2] = VOSegment {0x00, 0x01}; // Second segment of triangle
        }
    }

    void static DeleteWaveform(byte waveform_number) {
        // Index of first segment minus one is the TOC record
        byte ix = GetSegmentIndex(waveform_number, 0) - 1;
        if (ix) {
            byte offset = HS::user_waveforms[ix].Segments() + 1;

            // Move the data downward to delete
            for (byte i = ix; i < HS::VO_SEGMENT_COUNT - offset; i++)
            {
                memcpy(&HS::user_waveforms[i], &HS::user_waveforms[i + offset], sizeof(HS::user_waveforms[i + offset]));
            }

            // Fill in freed memory with empty segments
            for (byte i = HS::VO_SEGMENT_COUNT - offset; i < HS::VO_SEGMENT_COUNT; i++)
            {
                HS::user_waveforms[i] = VOSegment {0x00, 0xff};
            }
        }
    }
};

#endif // WAVEFORM_MANAGER_H
