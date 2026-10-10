/*
 *  *****    output_i2s_f32.h  *****
 * 
 * Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 /* 
 *  Extended by Chip Audette, OpenAudio, May 2019
 *  Converted to F32 and to variable audio block length
 *	The F32 conversion is under the MIT License.  Use at your own risk.
 */
// Updated OpenAudio F32 with this version from Chip Audette's Tympan Library Jan 2021 RSL

#ifndef _OUTPUT_I2S2_F32_H_
#define _OUTPUT_I2S2_F32_H_


#include <Arduino.h>
#include <arm_math.h>
#include "AudioStream_F32.h"
#include "DMAChannel.h"

// TODO: Add optional MCLK disable

class AudioOutputI2S2_F32 : public AudioStream_F32
{
//GUI: inputs:2, outputs:0  //this line used for automatic generation of GUI node
public:
    //uses default AUDIO_SAMPLE_RATE and BLOCK_SIZE_SAMPLES from AudioStream.h:
	AudioOutputI2S2_F32(bool mclkEn=true) : AudioStream_F32(2, inputQueueArray)	{ begin(mclkEn);} 
	// Allow variable sample rate and block size:
	AudioOutputI2S2_F32(const AudioSettings_F32 &settings, bool mclkEn=true) : AudioStream_F32(2, inputQueueArray)
	{ 
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		begin(mclkEn); 	
	}
	virtual void update(void);
	//void begin(void);
	void begin(bool mclk_enable);
	friend class AudioInputI2S2_F32;

	friend class AudioOutputI2SQuad_F32;
	friend class AudioInputI2SQuad_F32;
	bool get_update_responsibility() { return update_responsibility;}
	void set_channel_swap(bool sw) { channel_swap = sw ? 1 : 0;}
	bool get_channel_swap() {return (bool)channel_swap;}
protected:
	AudioOutputI2S2_F32(int dummy): AudioStream_F32(2, inputQueueArray) {} // to be used only inside AudioOutputI2Sslave !!
	static void config_i2s(void);
	static void config_i2s(bool);
	static void config_i2s(float);
	static void config_i2s(bool, float);

	static audio_block_f32_t *block_left_1st;
	static audio_block_f32_t *block_right_1st;
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr(void);
private:
	static audio_block_f32_t *block_left_2nd;
	static audio_block_f32_t *block_right_2nd;
	static uint16_t block_left_offset;
	static uint16_t block_right_offset;
	audio_block_f32_t *inputQueueArray[2];
	static float sample_rate_Hz;
	static int audio_block_samples;
	volatile uint8_t enabled = 1;
	uint8_t channel_swap = 0;
};
 
class AudioOutputI2S2slave_F32 : public AudioOutputI2S2_F32
{
public:
	AudioOutputI2S2slave_F32(void) : AudioOutputI2S2_F32(0) { begin(); } ;
	void begin(void);
	friend class AudioInputI2S2slave_F32;
protected:
	static void config_i2s(void);
};

#endif // _OUTPUT_I2S_F32_H_
