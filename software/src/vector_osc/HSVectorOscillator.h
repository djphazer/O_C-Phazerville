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

#ifndef HS_VECTOR_OSCILLATOR
#define HS_VECTOR_OSCILLATOR

#include "util/util_math.h"

namespace HS {

const uint8_t VO_SEGMENT_COUNT = 64; // The total number of segments in user memory
const uint8_t VO_MAX_SEGMENTS = 12; // The maximum number of segments in a waveform

/*
 * The VOSegment is a single segment of the VectorOscillator that specifies a target
 * level and relative time.
 *
 * A waveform is constructed of two or more VOSegments.
 *
 */
struct VOSegment {
    uint8_t level;
    uint8_t time;

    bool IsTOC() {return (time == 0xff && level > 0);}
    void SetTOC(uint8_t segments) {
        time = 0xff;
        level = segments;
    }

    /* If this is a TOC segment, Segments() will return how many segments are in the waveform */
    uint8_t Segments() {return level;}

};

DMAMEM VOSegment user_waveforms[VO_SEGMENT_COUNT];

}; // namespace HS

#define int2signal(x) (x << 10)
#define signal2int(x) (x >> 10)
using vosignal_t = int32_t;

enum {
    VO_TRIANGLE,
    VO_SAWTOOTH,
    VO_SQUARE,
    VO_NUMBER_OF_WAVEFORMS
};

using VOSegment = HS::VOSegment;

class VectorOscillator {
public:
    /* Oscillator defaults to cycling. Turn off cycling for EGs, etc */
    void Cycle(bool cycle_ = 1) {cycle = cycle_;}

    /* Oscillator defaults to non-sustaining. Turing on for EGs, etc. */
    void Sustain(bool sustain_ = 1) {sustain = sustain_;}

    /* Move to the release stage after sustain */
    void Release() {
        segment_start_level = update_current();
        sustained = 0;
        uint32_t tu = time_unit();
        segment_index = segment_count - 1;
        segment_time = total_time - segments[segment_index].time;
        phase = segment_time * tu;
    }

    /* The offset amount will be added to each voltage output */
    void Offset(int16_t offset_) {offset = offset_;}

    /* Add a new segment to the end */
    void SetSegment(HS::VOSegment segment) {
        if (segment_count < HS::VO_MAX_SEGMENTS) {
            memcpy(&segments[segment_count], &segment, sizeof(segments[segment_count]));
            total_time += segments[segment_count].time;
            segment_count++;
        }
    }

    /* Update an existing segment */
    void SetSegment(uint8_t ix, HS::VOSegment segment) {
        ix = constrain(ix, 0, segment_count - 1);
        total_time -= segments[ix].time;
        memcpy(&segments[ix], &segment, sizeof(segments[ix]));
        total_time += segments[ix].time;
        if (ix == segment_count) segment_count++;
    }

    HS::VOSegment GetSegment(uint8_t ix) {
        ix = constrain(ix, 0, segment_count - 1);
        return segments[ix];
    }

    void SetScale(uint16_t scale_) {scale = scale_;}

    /* frequency is centihertz (e.g., 440 Hz is 44000) */
    void SetFrequency(uint32_t frequency_) {
        SetPhaseIncrement(0xffffffff / 1666667 * frequency_);
    }

    // Frequency in 10ths of millihertz
    // TODO: This is going to be quite innaccurate and should just be removed
    void SetFrequency_4dec(uint32_t frequency_) {
        phase_increment = 0xffffffff / 166666667 * frequency_;
    }
    
    void SetPhaseIncrement(uint32_t phase_inc) {
        phase_increment = phase_inc;
    }

    bool GetEOC() {return eoc;}

    uint8_t TotalTime() {return total_time;}

    uint8_t SegmentCount() {return segment_count;}

    void Start() {
        Reset();
        eoc = 0;
    }

    void Reset() {
        segment_index = 0;
        segment_time = 0;
        segment_start_level = (segments[segment_count - 1].level - 128) * 256;
        phase = 0;
        sustained = 0;
        eoc = !cycle;
    }

    int32_t Next() {
        // For non-cycling waveforms, send the level of the last step if eoc
        if (eoc && cycle == 0) {
            return Phase32(segment_count - 1, 0xffffffff);
        }
        uint32_t old_phase = phase;
        if (!sustained) { // Observe sustain state
            eoc = 0;
            if (validate()) {
                phase += phase_increment;
            }
        }
        if (!cycle && phase < old_phase) {
            phase = 0xffffffff;
            eoc = true;
            return Phase32(segment_count - 1, 0xffffffff);
        }
        return rescale(update_current());
    }

    /* Get the value of the waveform at a specific phase. Degrees are expressed in tenths of a degree */
    int32_t Phase(int degrees) {
        uint32_t phase = 0xffffffff / 3600 * degrees;
        uint8_t segment = 0;
        uint8_t segment_start = 0;
        uint32_t segment_phase = 0;
        int16_t ignored;
        find_segment(
            phase, false, segment, segment_start, ignored, segment_phase
        );
        return Phase32(segment, segment_phase);
    }

    int32_t Phase32(uint8_t segment, uint32_t segment_phase) {
        // Start and end point of the total segment
        int start = segments[segment == 0 ? segment_count - 1 : segment - 1].level - 128;
        int end = segments[segment].level - 128;
        return rescale(InterpLinear16(start * 256, end * 256, segment_phase >> 16));
    }

private:
    VOSegment segments[12]; // Array of segments in this Oscillator
    uint8_t segment_count = 0; // Number of segments
    int total_time = 0; // Sum of time values for all segments
    bool eoc = 1; // The most recent tick's next() read was the end of a cycle
    bool cycle = 1; // Waveform will cycle
    bool sustain = 0; // Waveform stops when it reaches the end of the penultimate stage
    bool sustained = 0; // Current state of sustain. Only active when sustain = 1
    
    uint32_t phase = 0;
    uint32_t phase_increment = 0;
    int16_t segment_start_level = 0; // Needed for when sustain ends before reaching penultimate stage
    uint8_t segment_index = 0; // Which segment the Oscillator is currently traversing
    uint8_t segment_time = 0; // Start time of current segment (out of total_time)
    uint16_t scale; // The maximum (and minimum negative) output for this Oscillator
    int16_t offset = 0; // Amount added to each voltage output (e.g., to make it unipolar)

    /*
     * The Oscillator can only oscillate if the following conditions are true:
     *     (1) The frequency must be greater than 0
     *     (2) There must be more than one steps
     *     (3) The total time must be greater than 0
     *     (4) The scale is greater than 0
     */
    bool validate() {
        bool valid = 1;
        if (phase_increment == 0) valid = 0;
        if (segment_count < 2) valid = 0;
        if (total_time == 0) valid = 0;
        if (scale == 0) valid = 0;
        return valid;
    }

    int32_t Proportion(int numerator, int denominator, int max_value) {
        vosignal_t proportion = int2signal((int32_t)numerator) / (int32_t)denominator;
        int32_t scaled = signal2int(proportion * max_value);
        return scaled;
    }

    inline uint32_t time_unit() {
        return 0xffffffff / total_time;
    }

    void find_segment(
        uint32_t phase,
        bool sustain,
        uint8_t& segment,
        uint8_t& segment_start,
        int16_t& segment_start_level,
        uint32_t& segment_phase
    ) {
        if (total_time == 0) return; // vector osc hasn't been setup yet so bail
        const uint32_t tu = time_unit();
        uint32_t start_phase = tu * segment_start;
        uint32_t end_phase = start_phase + tu * segments[segment].time;
        // Note: unless a segment transition has occurred, you won't enter this
        // loop. Even then, it will normally only loop once unless a segment is
        // 0 length or frequency is extremely high
        while (!(start_phase <= phase && phase < end_phase)) {
            if (sustain && segment == segment_count - 2) return;
            segment_start_level = (segments[segment].level - 128) * 256;
            segment_start += segments[segment].time;
            segment++;
            if (segment >= segment_count) {
                segment = 0;
                segment_start = 0;
            }
            start_phase = tu * segment_start;
            end_phase = start_phase + tu * segments[segment].time;
        }
        segment_phase = ((phase - start_phase) / ((end_phase - start_phase) >> 16)) << 16;
    }

    // Rescales full 16 bit signed valued based on scale and offset
    int16_t rescale(int16_t unscaled) {
        int32_t mult = unscaled * scale;
        return mult / 32768 + offset;
    }
    
    int16_t update_current() {
        uint32_t segment_phase;
        find_segment(
            phase,
            sustain,
            segment_index,
            segment_time,
            segment_start_level,
            segment_phase
        );
        if (sustain && segment_index == segment_count - 2) {
            sustained = true;
            phase = segment_time * time_unit();
        }
        return InterpLinear16(
            segment_start_level,
            (segments[segment_index].level - 128) * 255,
            segment_phase >> 16
        );
    }
};
#endif // HS_VECTOR_OSCILLATOR
