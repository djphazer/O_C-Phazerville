// Xenomorphicle: whole-file guard, F32 core is used on T4.1 audio builds only (see README.md)
#ifdef ARDUINO_TEENSY41
/* Fix 1 to n problem Bob Larkin June 2020
 * Adapted to Chip Audette's Tympan routine. Allows random channels.
 * Class name does not have "_OA" to be backward compatible.
 * 
 * MIT License.  use at your own risk.
*/

#include "AudioMixer_F32.h"

void AudioMixer4_F32::update(void) {
  audio_block_f32_t *in, *out=NULL;
  int channel = 0;
  
  //get the first available channel
  while  (channel < 4) {
	  out = receiveWritable_f32(channel);
	  if (out) break;
	  channel++;
  }
  if (!out) return;  //there was no data output array available, so exit.
  arm_scale_f32(out->data, multiplier[channel], out->data, out->length);
  
  //add in the remaining channels, as available
  channel++;
  while  (channel < 4) {
    in = receiveReadOnly_f32(channel);
    if (in) {
		audio_block_f32_t *tmp = allocate_f32();
		// pool exhaustion must not hard-fault the audio ISR: skip the
		// channel this update rather than dereference NULL
		if (tmp) {
			arm_scale_f32(in->data, multiplier[channel], tmp->data, tmp->length);
			arm_add_f32(out->data, tmp->data, out->data, tmp->length);
			AudioStream_F32::release(tmp);
		}
		AudioStream_F32::release(in);
	} else {
		//do nothing, this vector is empty
	}
	channel++;
  }
  AudioStream_F32::transmit(out);
  AudioStream_F32::release(out);
}

void AudioMixer8_F32::update(void) {
  audio_block_f32_t *in, *out=NULL;

  //get the first available channel
  int channel = 0;
  while  (channel < 8) {
	  out = receiveWritable_f32(channel);
	  if (out) break;
	  channel++;
  }
  if (!out) return;  //there was no data output array.  so exit.
  arm_scale_f32(out->data, multiplier[channel], out->data, out->length); 
  
  //add in the remaining channels, as available
  channel++;
  while  (channel < 8) {
    in = receiveReadOnly_f32(channel);
    if (in) {
		audio_block_f32_t *tmp = allocate_f32();
		// pool exhaustion must not hard-fault the audio ISR: skip the
		// channel this update rather than dereference NULL
		if (tmp) {
			arm_scale_f32(in->data, multiplier[channel], tmp->data, tmp->length);
			arm_add_f32(out->data, tmp->data, out->data, tmp->length);
			AudioStream_F32::release(tmp);
		}
		AudioStream_F32::release(in);
	} else {
		//do nothing, this vector is empty
	}
	channel++;
  }
  AudioStream_F32::transmit(out);
  AudioStream_F32::release(out);
}

#endif // ARDUINO_TEENSY41
