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

const byte VO_SEGMENT_COUNT = 64; // The total number of segments in user memory
const byte VO_MAX_SEGMENTS = 12; // The maximum number of segments in a waveform

/*
 * The VOSegment is a single segment of the VectorOscillator that specifies a target
 * level and relative time.
 *
 * A waveform is constructed of two or more VOSegments.
 *
 */
struct VOSegment {
    byte level;
    byte time;

    bool IsTOC() {return (time == 0xff && level > 0);}
    void SetTOC(byte segments) {
        time = 0xff;
        level = segments;
    }

    /* If this is a TOC segment, Segments() will return how many segments are in the waveform */
    byte Segments() {return level;}

};

VOSegment user_waveforms[VO_SEGMENT_COUNT];

}; // namespace HS

#define int2signal(x) (x << 10)
#define signal2int(x) (x >> 10)
typedef int32_t vosignal_t;

enum {
    VO_TRIANGLE,
    VO_SAWTOOTH,
    VO_SQUARE,
    VO_NUMBER_OF_WAVEFORMS
};

typedef HS::VOSegment VOSegment;

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
        rise = calculate_rise(segment_index);
//        if (rise == 0) countdown = 1;
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
    void SetSegment(byte ix, HS::VOSegment segment) {
        ix = constrain(ix, 0, segment_count - 1);
        total_time -= segments[ix].time;
        memcpy(&segments[ix], &segment, sizeof(segments[ix]));
        total_time += segments[ix].time;
        if (ix == segment_count) segment_count++;
    }

    HS::VOSegment GetSegment(byte ix) {
        ix = constrain(ix, 0, segment_count - 1);
        return segments[ix];
    }

    void SetScale(uint16_t scale_) {scale = scale_;}

    /* frequency is centihertz (e.g., 440 Hz is 44000) */
    void SetFrequency(uint32_t frequency_) {
        if (frequency_ != frequency) {
            frequency = frequency_;
            rise = calculate_rise(segment_index);
        }
    }

    bool GetEOC() {return eoc;}

    byte TotalTime() {return total_time;}

    byte SegmentCount() {return segment_count;}

    void Start() {
        Reset();
        eoc = 0;
    }

    void Reset() {
        segment_index = 0;
        signal = scale_level(segments[segment_count - 1].level);
        rise = calculate_rise(segment_index);
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
				if (rise) {
					signal += rise;
                    if (rise > 0 && signal >= target) advance_segment();
                    else if (rise < 0 && signal <= target) advance_segment();
				} else {
					if (countdown) {
						--countdown;
						if (countdown == 0) advance_segment();
					}
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
    		byte time_index = Proportion(degrees, 3600, total_time);
    		byte segment = 0;
    		byte time = 0;
    		for (byte ix = 0; ix < segment_count; ix++)
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
    byte segment_count = 0; // Number of segments
    int total_time = 0; // Sum of time values for all segments
    vosignal_t signal = 0; // Current scaled signal << 10 for more precision
    vosignal_t target = 0; // Target scaled signal. When the target is reached, the Oscillator moves to the next segment.
    bool eoc = 1; // The most recent tick's next() read was the end of a cycle
    byte segment_index = 0; // Which segment the Oscillator is currently traversing
    vosignal_t rise; // The amount (per tick) the signal must rise to reach the target
    uint32_t frequency; // In centihertz
    uint16_t scale; // The maximum (and minimum negative) output for this Oscillator
    uint32_t countdown; // Ticks left for a segment with a rise of 0
    bool cycle = 1; // Waveform will cycle
    int32_t offset = 0; // Amount added to each voltage output (e.g., to make it unipolar)
    bool sustain = 0; // Waveform stops when it reaches the end of the penultimate stage
    bool sustained = 0; // Current state of sustain. Only active when sustain = 1

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
    vosignal_t scale_level(byte level) {
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
            } else rise = calculate_rise(segment_index);
            sustained = 0;
        }
    }

    vosignal_t calculate_rise(byte ix) {
        // Determine the target level for this segment
        byte level = segments[ix].level;
        int time = static_cast<uint32_t>(segments[ix].time);
        target = scale_level(level);

        // Determine the starting level of this segment to get the total segment rise
        if (ix > 0) ix--;
        else ix = segment_count - 1;
        level = segments[ix].level;
        vosignal_t starting = scale_level(level);

        // How many ticks should a complete cycle last? cycle_ticks is 10 times that number.
        int32_t cycle_ticks = 16666667 / frequency;

        // How many ticks should the current segment last?
        int32_t segment_ticks = Proportion(time, total_time, cycle_ticks);

        // The total difference between the target and the current signal, divided by how many ticks
        // it should take to get there, is the rise. The / 10 is to cancel the extra precision
        // from the previous two calculations.
        vosignal_t new_rise = 0;
        if (segment_ticks > 0) {
            new_rise = ((target - starting) * 10) / segment_ticks;
            if (new_rise == 0) {
                uint32_t prev_countdown = countdown;
                countdown = segment_ticks / 10;
                if (prev_countdown > 0 && prev_countdown < countdown) countdown = prev_countdown;
            }

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
            else if ((signal2int(target) - signal2int(starting)) * (signal2int(target) - signal2int(signal)) < 0) new_rise = -new_rise;
        } else {
            signal = target;
            countdown = 1;
        }

        return new_rise;
    }
};

#endif // HS_VECTOR_OSCILLATOR
