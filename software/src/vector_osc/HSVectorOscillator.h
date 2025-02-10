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
        sustained = 0;
        segment_index = segment_count - 1;
        calculate_rise(segment_index);
        total_run = 0;
    }

    /* The offset amount will be added to each voltage output */
    void Offset(int32_t offset_) {offset = offset_;}

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
        SetFrequency_4dec(100 * frequency_);
    }

    void SetFrequency_4dec(uint32_t frequency_) {
        frequency = frequency_;
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
        signal = scale_level(segments[segment_count - 1].level);
        calculate_rise(segment_index);
        total_run = 0;
        sustained = 0;
        eoc = !cycle;
    }

    int32_t Next() {
        // For non-cycling waveforms, send the level of the last step if eoc
        if (eoc && cycle == 0) {
            vosignal_t nr_signal = scale_level(segments[segment_count - 1].level);
            return signal2int(nr_signal) + offset;
        }
        if (!sustained) { // Observe sustain state
            eoc = 0;
            if (validate()) {
                vosignal_t overflow = step_by(vosignal_t(frequency));
                if (overflow > 0) {
                    // If we overflow again, just add it to run_delta to be taken care of in next iteration.
                    // We could while-loop this instead, but it really shouldn't happen (as segments are
                    // longer than 9990, the max frequency in available apps) and I was observing occassional
                    // infinite loops when switch shapes.
                    run_delta += step_by(overflow);
                }
			}
        }
        return signal2int(signal) + offset;
    }

    /* Get the value of the waveform at a specific phase. Degrees are expressed in tenths of a degree */
    int32_t Phase(int degrees) {
    		degrees = degrees % 3600;
    		degrees = abs(degrees);

    		// I need to find out which segment the specified phase occurs in
    		uint8_t time_index = Proportion(degrees, 3600, total_time);
    		uint8_t segment = 0;
    		uint8_t time = 0;
    		for (uint8_t ix = 0; ix < segment_count; ix++)
    		{
    			time += segments[ix].time;
    			if (time > time_index) {
    				segment = ix;
    				break;
    			}
    		}

    		// Where does this segment start, and how many degrees does it span?
    		int start_degree = Proportion(time - segments[segment].time, total_time, 3600);
    		int segment_degrees = Proportion(segments[segment].time, total_time, 3600);

    		// Start and end point of the total segment
    		int start = signal2int(scale_level(segment == 0 ? segments[segment_count - 1].level : segments[segment - 1].level));
    		int end = signal2int(scale_level(segments[segment].level));

    		// Determine the signal based on the levels and the position within the segment
    		int signal = Proportion(degrees - start_degree, segment_degrees, end - start) + start;

        return signal + offset;
    }

private:
    VOSegment segments[12]; // Array of segments in this Oscillator
    uint8_t segment_count = 0; // Number of segments
    int total_time = 0; // Sum of time values for all segments
    vosignal_t signal = 0; // Current scaled signal << 10 for more precision
    vosignal_t target = 0; // Target scaled signal. When the target is reached, the Oscillator moves to the next segment.
    bool eoc = 1; // The most recent tick's next() read was the end of a cycle
    uint8_t segment_index = 0; // Which segment the Oscillator is currently traversing
    uint32_t frequency; // In centihertz
    uint16_t scale; // The maximum (and minimum negative) output for this Oscillator
    bool cycle = 1; // Waveform will cycle
    int32_t offset = 0; // Amount added to each voltage output (e.g., to make it unipolar)
    bool sustain = 0; // Waveform stops when it reaches the end of the penultimate stage
    bool sustained = 0; // Current state of sustain. Only active when sustain = 1

    vosignal_t rise; // The amount the signal is expected to rise this segment
    vosignal_t run; // The expect duration of the segment (in ticks)
    vosignal_t run_delta; // amount of temporal change without level change
    vosignal_t run_mod; // Corrects for division errors
    vosignal_t rise_mod; // Corrects for division errors
    vosignal_t total_run; // How many ticks we've been in this segment

    /*
     * The Oscillator can only oscillate if the following conditions are true:
     *     (1) The frequency must be greater than 0
     *     (2) There must be more than one steps
     *     (3) The total time must be greater than 0
     *     (4) The scale is greater than 0
     */
    bool validate() {
        bool valid = 1;
        if (frequency == 0) valid = 0;
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

    /*
     * Provide a signal value based on a segment level. The segment level is internally
     * 0-255, and this is converted to a bipolar value by subtracting 128.
     */
    vosignal_t scale_level(uint8_t level) {
        int b_level = constrain(level, 0, 255) - 128;
        int scaled = Proportion(b_level, 127, scale);
        vosignal_t scaled_level = int2signal(scaled);
        return scaled_level;
    }

    void advance_segment() {
        if (sustain && segment_index == segment_count - 2) {
            sustained = 1;
        } else {
            if (++segment_index >= segment_count) {
                if (cycle) Reset();
                eoc = 1;
            } else {
                calculate_rise(segment_index);
                total_run = 0;
            }
            sustained = 0;
        }
    }

    vosignal_t step_by(vosignal_t delta) {
        run_delta += delta;
        delta = 0;
        // Dividing run by run_delta (as opposed to the more typical multiplying) avoids
        // possibilities of overflow. We can then also correct for error by including it in then
        // next pass (via run_mod).
        vosignal_t denom = (run + run_mod) / run_delta;
        run_mod = (run + run_mod) % run_delta;
        vosignal_t rise_delta = target - signal;
        if (denom != 0) {
            rise_delta = (rise + rise_mod) / denom;
            // If rise_delta is 0, we're going to accumulate it anyway, so there's no rise error to
            // correct for.
            rise_mod = rise_delta == 0 ? 0 : (rise + rise_mod) % denom;
        }
        if (rise_delta != 0) {
            signal += rise_delta;
            total_run += run_delta;
            run_delta = 0;
        }
        bool at_target = (rise > 0 && signal >= target) || (rise < 0 && signal <= target);
        vosignal_t total = total_run + run_delta;

        // Advancing segment by time is way more accurate than by value, but doesn't work with sustain.
        if (((!sustain || rise == 0) && total > run) || (rise != 0 && sustain && at_target)) {
            run_delta = 0;
            rise_mod = 0;
            if (!sustain && total > run) {
                delta = sustain ? 0 : total - run;
            }
            signal = target;
            advance_segment();
        }
        return delta;
    }

    void calculate_rise(uint8_t ix) {
        // Determine the target level for this segment
        uint8_t level = segments[ix].level;
        int time = static_cast<uint32_t>(segments[ix].time);
        target = scale_level(level);

        // Determine the starting level of this segment to get the total segment rise
        if (ix > 0) ix--;
        else ix = segment_count - 1;
        level = segments[ix].level;
        vosignal_t starting = scale_level(level);

        //run = Proportion(time, total_time, 1666667);
        run = time * 166666667 / total_time;
        rise = (target - starting);
        // The following line is here to deal with the cases where the signal is coming from a different
        // direction than it would be coming from if it were coming from the previous segment. This can
        // only happen when the Vector Oscillator is being used as an envelope generator with Sustain/Release,
        // and the envelope is ungated prior to the sustain (penultimate) segment, and one of these happens:
        //
        // (1) The signal level at release is lower than the final signal level, but the sustain segment's
        //     level is higher, OR
        // (2) The signal level at release is higher than the final signal level, but the sustain segment's
        //     level is lower.
        //
        // This scenario would result in a rise with the wrong polarity; that is, the signal would move away
        // from the target instead of toward it. The remedy, as the Third Doctor would say, is to Reverse
        // The Polarity. So I test for a difference in sign via multiplication. A negative result (value < 0)
        // indicates that the rise is the reverse of what it should be:
        if ((signal2int(target) - signal2int(starting)) * (signal2int(target) - signal2int(signal)) < 0) rise = -rise;
    }
};

#endif // HS_VECTOR_OSCILLATOR
