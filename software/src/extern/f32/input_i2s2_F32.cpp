// Xenomorphicle: whole-file guard, this driver is T4.1-only (see README.md)
#ifdef ARDUINO_TEENSY41
/*
 *      input_i2s2_f32.cpp
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

#include <Arduino.h> //do we really need this? (Chip: 2020-10-31)
#include "input_i2s2_F32.h"
#include "output_i2s2_F32.h"
#include "basic_DSPutils.h"
#include <arm_math.h>

// DMAMEM __attribute__((aligned(32)))
static uint64_t i2s2_rx_buffer[AUDIO_BLOCK_SAMPLES] __attribute__((aligned(32))); // good for 16-bit audio samples coming in from teh AIC.  32-bit transfers will need this to be bigger.
audio_block_f32_t *AudioInputI2S2_F32::block_left_f32 = NULL;
audio_block_f32_t *AudioInputI2S2_F32::block_right_f32 = NULL;
uint16_t AudioInputI2S2_F32::block_offset = 0;
bool AudioInputI2S2_F32::update_responsibility = false;
DMAChannel AudioInputI2S2_F32::dma(false);

int AudioInputI2S2_F32::flag_out_of_memory = 0;
unsigned long AudioInputI2S2_F32::update_counter = 0;

float AudioInputI2S2_F32::sample_rate_Hz = AUDIO_SAMPLE_RATE;
int AudioInputI2S2_F32::audio_block_samples = AUDIO_BLOCK_SAMPLES;

#define I2S2_BUFFER_TO_USE_BYTES (AudioOutputI2S2_F32::audio_block_samples * sizeof(i2s2_rx_buffer[0]))

// --------------------------------------------------------------------------------
void AudioInputI2S2_F32::begin()
{
	dma.begin(true); // Allocate the DMA channel first

	AudioOutputI2S2_F32::sample_rate_Hz = sample_rate_Hz;			// these were given in the AudioSettings in the contructor
	AudioOutputI2S2_F32::audio_block_samples = audio_block_samples; // these were given in the AudioSettings in the contructor

	// TODO: should we set & clear the I2S_RCSR_SR bit here?
	AudioOutputI2S2_F32::config_i2s();

	CORE_PIN5_CONFIG = 2;				   // EMC_08, 2=SAI2_RX_DATA, page 434
	IOMUXC_SAI2_RX_DATA0_SELECT_INPUT = 0; // 0=GPIO_EMC_08_ALT2, page 876

	dma.TCD->SADDR = (void *)((uint32_t)&I2S2_RDR0);
	dma.TCD->SOFF = 0;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	dma.TCD->NBYTES_MLNO = 4;
	dma.TCD->SLAST = 0;
	dma.TCD->DADDR = i2s2_rx_buffer;
	dma.TCD->DOFF = 4;
	dma.TCD->CITER_ELINKNO = I2S2_BUFFER_TO_USE_BYTES / 4;
	dma.TCD->DLASTSGA = -I2S2_BUFFER_TO_USE_BYTES;
	dma.TCD->BITER_ELINKNO = I2S2_BUFFER_TO_USE_BYTES / 4;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI2_RX);

	I2S2_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR; // page 2099

	update_responsibility = update_setup();
	dma.enable();
	dma.attachInterrupt(isr);

	update_counter = 0;
}
// ------------------------------ RX DMA ISR ------------------------------------------
void AudioInputI2S2_F32::isr(void)
{
	uint32_t daddr, offset;
	const int32_t *src, *end;
	float32_t *dest_left_f32, *dest_right_f32;
	audio_block_f32_t *left_f32, *right_f32;

	daddr = (uint32_t)(dma.TCD->DADDR);
	dma.clearInterrupt();
	if (daddr < (uint32_t)i2s2_rx_buffer + I2S2_BUFFER_TO_USE_BYTES / 2)
	{
		// DMA is receiving to the first half of the buffer
		// need to remove data from the second half
		src = (int32_t *)&i2s2_rx_buffer[audio_block_samples / 2];
		end = (int32_t *)&i2s2_rx_buffer[audio_block_samples];
		update_counter++; // let's increment the counter here to ensure that we get every ISR resulting in audio
		if (AudioInputI2S2_F32::update_responsibility) AudioStream_F32::update_all();
	}
	else
	{
		// DMA is receiving to the second half of the buffer
		// need to remove data from the first half
		src = (int32_t *)&i2s2_rx_buffer[0];
		end = (int32_t *)&i2s2_rx_buffer[audio_block_samples / 2];
	}
	left_f32 = AudioInputI2S2_F32::block_left_f32;
	right_f32 = AudioInputI2S2_F32::block_right_f32;
	if (left_f32 != NULL && right_f32 != NULL)
	{
		offset = AudioInputI2S2_F32::block_offset;
		if (offset <= ((uint32_t)audio_block_samples / 2))
		{
			dest_left_f32 = &(left_f32->data[offset]);
			dest_right_f32 = &(right_f32->data[offset]);
			AudioInputI2S2_F32::block_offset = offset + audio_block_samples / 2;
			do
			{
				*dest_left_f32++ = (float32_t)*src++;
				*dest_right_f32++ = (float32_t)*src++;
			} while (src < end);
		}
	}
}
// --------------------------------------------------------------------------------
void AudioInputI2S2_F32::update_1chan(int chan, audio_block_f32_t *&out_f32)
{
	if (!out_f32)
		return;
	// scale the float values so that the maximum possible audio values span -1.0 to + 1.0
	arm_scale_f32(out_f32->data, I32_TO_F32_NORM_FACTOR, out_f32->data, audio_block_samples);
	// prepare to transmit by setting the update_counter (which helps tell if data is skipped or out-of-order)
	out_f32->id = update_counter;
	AudioStream_F32::transmit(out_f32, chan);	// transmit the f32 data!
	AudioStream_F32::release(out_f32);			// release the memory blocks
}
// --------------------------------------------------------------------------------
void AudioInputI2S2_F32::update(void)
{
	static bool flag_beenSuccessfullOnce = false;
	audio_block_f32_t *new_left = NULL, *new_right = NULL, *out_left = NULL, *out_right = NULL;

	new_left = AudioStream_F32::allocate_f32();
	new_right = AudioStream_F32::allocate_f32();
	if ((!new_left) || (!new_right))
	{
		// ran out of memory.  Clear and return!
		if (new_left) 	AudioStream_F32::release(new_left);
		if (new_right)	AudioStream_F32::release(new_right);
		new_left = NULL;
		new_right = NULL;
		flag_out_of_memory = 1;
		if (flag_beenSuccessfullOnce)
			Serial.println("Input_I2S_F32: update(): WARNING!!! Out of Memory.");
	}
	else {flag_beenSuccessfullOnce = true; }

	__disable_irq();
	if (block_offset >= audio_block_samples)
	{
		// the DMA filled 2 blocks, so grab them and get the
		// 2 new blocks to the DMA, as quickly as possible
		out_left = block_left_f32;
		block_left_f32 = new_left;
		out_right = block_right_f32;
		block_right_f32 = new_right;
		block_offset = 0;
		__enable_irq();
		update_1chan(0^channel_swap, out_left);	// uses audio_block_samples and update_counter
		update_1chan(1^channel_swap, out_right); // uses audio_block_samples and update_counter
	}
	else if (new_left != NULL)
	{
		// the DMA didn't fill blocks, but we allocated blocks
		if (block_left_f32 == NULL)
		{
			// the DMA doesn't have any blocks to fill, so
			// give it the ones we just allocated
			block_left_f32 = new_left;
			block_right_f32 = new_right;
			block_offset = 0;
			__enable_irq();
		}
		else
		{
			// the DMA already has blocks, doesn't need these
			__enable_irq();
			AudioStream_F32::release(new_left);
			AudioStream_F32::release(new_right);
		}
	}
	else
	{
		// The DMA didn't fill blocks, and we could not allocate
		// memory... the system is likely starving for memory!
		// Sadly, there's nothing we can do.
		__enable_irq();
	}
}

// --------------------------------------------------------------------------------

void AudioInputI2S2slave_F32::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	AudioOutputI2S2slave_F32::config_i2s();
}

#endif // ARDUINO_TEENSY41
